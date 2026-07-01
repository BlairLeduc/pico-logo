//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  mklfsimg core: mirror a host directory tree into a fresh in-RAM LittleFS
//  volume, then emit it via the shared logo_lfs_backup() so the result is
//  byte-for-byte the format the device restores. Host-only (uses dirent/stat).
//

#include "mklfsimg.h"

#include "third_party/littlefs/lfs.h"
#include "devices/stream.h"
#include "devices/lfs_backup.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// LittleFS RAM cache/lookahead sizing. These are host-side buffers only (they
// do not affect the on-disk layout), so they need not match the device exactly;
// the values below mirror the device config for familiarity.
#define MKLFSIMG_CACHE_SIZE     256u
#define MKLFSIMG_LOOKAHEAD_SIZE 32u
#define MKLFSIMG_PATH_MAX       1024u

// ---------------------------------------------------------------------------
// RAM block device
// ---------------------------------------------------------------------------

static int ram_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                    void *buffer, lfs_size_t size)
{
    memcpy(buffer, (uint8_t *)c->context + (size_t)block * c->block_size + off, size);
    return LFS_ERR_OK;
}

static int ram_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                    const void *buffer, lfs_size_t size)
{
    memcpy((uint8_t *)c->context + (size_t)block * c->block_size + off, buffer, size);
    return LFS_ERR_OK;
}

static int ram_erase(const struct lfs_config *c, lfs_block_t block)
{
    memset((uint8_t *)c->context + (size_t)block * c->block_size, 0xff, c->block_size);
    return LFS_ERR_OK;
}

static int ram_sync(const struct lfs_config *c)
{
    (void)c;
    return LFS_ERR_OK;
}

// ---------------------------------------------------------------------------
// FILE-backed output stream (write-only), as logo_lfs_backup() expects
// ---------------------------------------------------------------------------

static void file_write_bytes(LogoStream *s, const char *buffer, size_t len)
{
    FILE *fp = (FILE *)s->context;
    if (fwrite(buffer, 1, len, fp) != len)
    {
        s->write_error = true;
    }
}

static void file_flush(LogoStream *s)
{
    FILE *fp = (FILE *)s->context;
    if (fflush(fp) != 0)
    {
        s->write_error = true;
    }
}

static const LogoStreamOps file_sink_ops = {
    .write_bytes = file_write_bytes,
    .flush = file_flush,
};

// ---------------------------------------------------------------------------
// Directory walk
// ---------------------------------------------------------------------------

static void set_err(char *errbuf, size_t errbuf_len, const char *fmt, const char *arg)
{
    if (errbuf && errbuf_len)
    {
        snprintf(errbuf, errbuf_len, fmt, arg);
    }
}

// Join `dir` and `name` into `dst`. For the LittleFS root ("/") the result is
// "/name"; otherwise "dir/name". Returns false if the result would overflow.
static bool join_path(char *dst, size_t dst_len, const char *dir, const char *name)
{
    int n;
    if (strcmp(dir, "/") == 0)
    {
        n = snprintf(dst, dst_len, "/%s", name);
    }
    else
    {
        n = snprintf(dst, dst_len, "%s/%s", dir, name);
    }
    return n > 0 && (size_t)n < dst_len;
}

// Copy the regular file at `host_path` to `lfs_path` inside `lfs`.
static bool copy_file(lfs_t *lfs, const char *host_path, const char *lfs_path,
                      char *errbuf, size_t errbuf_len)
{
    FILE *in = fopen(host_path, "rb");
    if (!in)
    {
        set_err(errbuf, errbuf_len, "cannot open source file %s", host_path);
        return false;
    }

    uint8_t file_buf[MKLFSIMG_CACHE_SIZE];
    lfs_file_t f;
    struct lfs_file_config fc = {.buffer = file_buf};
    if (lfs_file_opencfg(lfs, &f, lfs_path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC, &fc) < 0)
    {
        fclose(in);
        set_err(errbuf, errbuf_len, "cannot create %s in image", lfs_path);
        return false;
    }

    bool ok = true;
    uint8_t chunk[4096];
    size_t got;
    while ((got = fread(chunk, 1, sizeof(chunk), in)) > 0)
    {
        if (lfs_file_write(lfs, &f, chunk, (lfs_size_t)got) != (lfs_ssize_t)got)
        {
            set_err(errbuf, errbuf_len, "write failed for %s (image full?)", lfs_path);
            ok = false;
            break;
        }
    }
    if (ok && ferror(in))
    {
        set_err(errbuf, errbuf_len, "read error on %s", host_path);
        ok = false;
    }

    if (lfs_file_close(lfs, &f) < 0 && ok)
    {
        set_err(errbuf, errbuf_len, "close failed for %s", lfs_path);
        ok = false;
    }
    fclose(in);
    return ok;
}

// Recursively mirror the host directory `host_dir` into `lfs_dir` within `lfs`.
static bool copy_tree(lfs_t *lfs, const char *host_dir, const char *lfs_dir,
                      char *errbuf, size_t errbuf_len)
{
    DIR *d = opendir(host_dir);
    if (!d)
    {
        set_err(errbuf, errbuf_len, "cannot open directory %s", host_dir);
        return false;
    }

    bool ok = true;
    struct dirent *e;
    while (ok && (e = readdir(d)) != NULL)
    {
        // Skip ".", ".." and hidden entries (e.g. .DS_Store, .git).
        if (e->d_name[0] == '.')
        {
            continue;
        }

        char host_path[MKLFSIMG_PATH_MAX];
        char lfs_path[MKLFSIMG_PATH_MAX];
        if (!join_path(host_path, sizeof(host_path), host_dir, e->d_name) ||
            !join_path(lfs_path, sizeof(lfs_path), lfs_dir, e->d_name))
        {
            set_err(errbuf, errbuf_len, "path too long under %s", host_dir);
            ok = false;
            break;
        }

        struct stat st;
        if (stat(host_path, &st) != 0)
        {
            set_err(errbuf, errbuf_len, "cannot stat %s", host_path);
            ok = false;
            break;
        }

        if (S_ISDIR(st.st_mode))
        {
            int err = lfs_mkdir(lfs, lfs_path);
            if (err < 0 && err != LFS_ERR_EXIST)
            {
                set_err(errbuf, errbuf_len, "cannot create directory %s in image", lfs_path);
                ok = false;
                break;
            }
            ok = copy_tree(lfs, host_path, lfs_path, errbuf, errbuf_len);
        }
        else if (S_ISREG(st.st_mode))
        {
            ok = copy_file(lfs, host_path, lfs_path, errbuf, errbuf_len);
        }
        // Anything else (symlink, device, socket) is silently skipped.
    }

    closedir(d);
    return ok;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

bool mklfsimg_build(const char *src_dir, const char *out_path,
                    char *errbuf, size_t errbuf_len)
{
    if (errbuf && errbuf_len)
    {
        errbuf[0] = '\0';
    }

    size_t volume_bytes = (size_t)MKLFSIMG_BLOCK_COUNT * MKLFSIMG_BLOCK_SIZE;
    uint8_t *ram = malloc(volume_bytes);
    if (!ram)
    {
        set_err(errbuf, errbuf_len, "out of memory (%s)", "RAM volume");
        return false;
    }
    memset(ram, 0xff, volume_bytes);

    uint8_t read_buf[MKLFSIMG_CACHE_SIZE];
    uint8_t prog_buf[MKLFSIMG_CACHE_SIZE];
    uint8_t lookahead_buf[MKLFSIMG_LOOKAHEAD_SIZE];
    struct lfs_config cfg = {
        .context = ram,
        .read = ram_read,
        .prog = ram_prog,
        .erase = ram_erase,
        .sync = ram_sync,
        .read_size = MKLFSIMG_CACHE_SIZE,
        .prog_size = MKLFSIMG_CACHE_SIZE,
        .block_size = MKLFSIMG_BLOCK_SIZE,
        .block_count = MKLFSIMG_BLOCK_COUNT,
        .block_cycles = 500,
        .cache_size = MKLFSIMG_CACHE_SIZE,
        .lookahead_size = MKLFSIMG_LOOKAHEAD_SIZE,
        .read_buffer = read_buf,
        .prog_buffer = prog_buf,
        .lookahead_buffer = lookahead_buf,
    };

    lfs_t lfs;
    if (lfs_format(&lfs, &cfg) < 0 || lfs_mount(&lfs, &cfg) < 0)
    {
        free(ram);
        set_err(errbuf, errbuf_len, "cannot create LittleFS volume%s", "");
        return false;
    }

    bool ok = copy_tree(&lfs, src_dir, "/", errbuf, errbuf_len);

    if (ok)
    {
        FILE *out = fopen(out_path, "wb");
        if (!out)
        {
            set_err(errbuf, errbuf_len, "cannot open output file %s", out_path);
            ok = false;
        }
        else
        {
            LogoStream sink = {
                .type = LOGO_STREAM_FILE,
                .ops = &file_sink_ops,
                .context = out,
                .is_open = true,
            };
            if (!logo_lfs_backup(&lfs, &sink) || sink.write_error)
            {
                set_err(errbuf, errbuf_len, "failed to write image %s", out_path);
                ok = false;
            }
            if (fclose(out) != 0 && ok)
            {
                set_err(errbuf, errbuf_len, "failed to close image %s", out_path);
                ok = false;
            }
            if (!ok)
            {
                remove(out_path);
            }
        }
    }

    lfs_unmount(&lfs);
    free(ram);
    return ok;
}
