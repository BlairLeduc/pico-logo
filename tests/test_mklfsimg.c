//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Round-trip test for the mklfsimg host tool: build an image from a real host
//  directory tree, restore it into a fresh LittleFS volume via the same
//  logo_lfs_restore() the device uses, and verify the tree came back intact.
//

#include "unity.h"
#include "mklfsimg.h"
#include "third_party/littlefs/lfs.h"
#include "devices/stream.h"
#include "devices/lfs_backup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Restore-side RAM block device, sized to the image geometry.
#define BS MKLFSIMG_BLOCK_SIZE
#define BC MKLFSIMG_BLOCK_COUNT
#define CS 256u
#define LA 32u

static uint8_t ram[BS * BC];

static int bd_read(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, void *buf, lfs_size_t s)
{ memcpy(buf, &ram[b * c->block_size + o], s); return 0; }
static int bd_prog(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, const void *buf, lfs_size_t s)
{ memcpy(&ram[b * c->block_size + o], buf, s); return 0; }
static int bd_erase(const struct lfs_config *c, lfs_block_t b)
{ memset(&ram[b * c->block_size], 0xff, c->block_size); return 0; }
static int bd_sync(const struct lfs_config *c) { (void)c; return 0; }

static uint8_t rbuf[CS], pbuf[CS], lbuf[LA];

static struct lfs_config make_cfg(void)
{
    struct lfs_config cfg = {
        .read = bd_read, .prog = bd_prog, .erase = bd_erase, .sync = bd_sync,
        .read_size = CS, .prog_size = CS, .block_size = BS, .block_count = BC,
        .block_cycles = 500, .cache_size = CS, .lookahead_size = LA,
        .read_buffer = rbuf, .prog_buffer = pbuf, .lookahead_buffer = lbuf,
    };
    return cfg;
}

// Seekable in-memory stream over the image file, as logo_lfs_restore() expects.
typedef struct { uint8_t *buf; size_t len; size_t rpos; } Blob;

static int blob_read_chars(LogoStream *s, char *b, int n)
{
    Blob *B = s->context;
    size_t avail = B->len - B->rpos;
    size_t k = ((size_t)n < avail) ? (size_t)n : avail;
    memcpy(b, B->buf + B->rpos, k);
    B->rpos += k;
    return (int)k;
}
static long blob_get_read_pos(LogoStream *s) { return (long)((Blob *)s->context)->rpos; }
static bool blob_set_read_pos(LogoStream *s, long p)
{
    Blob *B = s->context;
    if (p < 0 || (size_t)p > B->len) return false;
    B->rpos = (size_t)p;
    return true;
}
static const LogoStreamOps blob_ops = {
    .read_chars = blob_read_chars,
    .get_read_pos = blob_get_read_pos,
    .set_read_pos = blob_set_read_pos,
};

// ---------------------------------------------------------------------------

static char tmp_root[256];

void setUp(void)
{
    memset(ram, 0xff, sizeof(ram));
    // A fresh, unique temp directory per test via mkdtemp, so contents never
    // leak between tests (a fixed name would be reused on the second setUp).
    snprintf(tmp_root, sizeof(tmp_root), "%s/mklfsimg_XXXXXX",
             getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    TEST_ASSERT_NOT_NULL(mkdtemp(tmp_root));
}

void tearDown(void) {}

static void write_host_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

static void path_in(char *dst, size_t n, const char *rel)
{
    snprintf(dst, n, "%s/%s", tmp_root, rel);
}

static void check_lfs_file(lfs_t *lfs, const char *name, const char *expected)
{
    lfs_file_t f;
    uint8_t fbuf[CS];
    struct lfs_file_config fc = {.buffer = fbuf};
    char got[256] = {0};
    TEST_ASSERT_EQUAL_INT(0, lfs_file_opencfg(lfs, &f, name, LFS_O_RDONLY, &fc));
    lfs_ssize_t r = lfs_file_read(lfs, &f, got, sizeof(got) - 1);
    TEST_ASSERT_TRUE(r >= 0);
    lfs_file_close(lfs, &f);
    got[r] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, got);
}

static void test_build_restore_round_trip(void)
{
    // Build a small host tree: a root file, a subdirectory with a file, and a
    // hidden file that must be skipped.
    char p[512];
    path_in(p, sizeof(p), "startup");
    write_host_file(p, "to greet pr [hi] end");
    path_in(p, sizeof(p), ".DS_Store");
    write_host_file(p, "junk");
    path_in(p, sizeof(p), "themes");
    mkdir(p, 0777);
    path_in(p, sizeof(p), "themes/default.theme");
    write_host_file(p, "fg=white bg=black");

    char img[512];
    path_in(img, sizeof(img), "logo.img");

    char err[256] = {0};
    bool built = mklfsimg_build(tmp_root, img, err, sizeof(err));
    TEST_ASSERT_TRUE_MESSAGE(built, err);

    // Read the image into memory and restore it into a fresh volume.
    FILE *f = fopen(img, "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    TEST_ASSERT_TRUE(sz > 0);
    uint8_t *bytes = malloc((size_t)sz);
    TEST_ASSERT_EQUAL_INT(sz, (long)fread(bytes, 1, (size_t)sz, f));
    fclose(f);

    Blob blob = {.buf = bytes, .len = (size_t)sz, .rpos = 0};
    LogoStream in = {.type = LOGO_STREAM_FILE, .ops = &blob_ops, .context = &blob, .is_open = true};

    struct lfs_config cfg = make_cfg();
    lfs_t lfs;
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &cfg));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &cfg));
    TEST_ASSERT_TRUE(logo_lfs_restore(&lfs, &in));

    // The tree is intact...
    check_lfs_file(&lfs, "/startup", "to greet pr [hi] end");
    check_lfs_file(&lfs, "/themes/default.theme", "fg=white bg=black");

    // ...and the hidden file was skipped.
    struct lfs_info info;
    TEST_ASSERT_EQUAL_INT(LFS_ERR_NOENT, lfs_stat(&lfs, "/.DS_Store", &info));

    lfs_unmount(&lfs);
    free(bytes);
}

static void test_missing_source_dir_reports_error(void)
{
    char img[512];
    path_in(img, sizeof(img), "nope.img");
    char err[256] = {0};
    TEST_ASSERT_FALSE(mklfsimg_build("/no/such/dir/here", img, err, sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_restore_round_trip);
    RUN_TEST(test_missing_source_dir_reports_error);
    return UNITY_END();
}
