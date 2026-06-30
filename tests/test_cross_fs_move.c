//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for cross-filesystem move in the storage router: moving a file between
//  the root backend (a RAM-backed LittleFS) and the "/sd" backend (a tiny
//  in-memory FS), including directory rejection, overwrite, content integrity,
//  and rollback when the copy fails.
//

#include "unity.h"
#include "devices/storage_router.h"
#include "devices/lfs_storage.h"
#include "devices/storage.h"
#include "devices/stream.h"
#include "devices/io.h"
#include "third_party/littlefs/lfs.h"

#include <string.h>
#include <stdlib.h>

//============================================================================
// Root backend: RAM-backed LittleFS (reuses the geometry from the FS tests)
//============================================================================

#define BS 4096u
#define BC 64u
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
static const struct lfs_config cfg = {
    .read = bd_read, .prog = bd_prog, .erase = bd_erase, .sync = bd_sync,
    .read_size = CS, .prog_size = CS, .block_size = BS, .block_count = BC,
    .block_cycles = 500, .cache_size = CS, .lookahead_size = LA,
    .read_buffer = rbuf, .prog_buffer = pbuf, .lookahead_buffer = lbuf,
};
static lfs_t lfs;
static LogoStorage root_backend;

//============================================================================
// "/sd" backend: a tiny in-memory filesystem with a per-file capacity, so a
// too-large copy triggers a write error (exercising rollback).
//============================================================================

#define MEM_MAX_FILES 8
#define MEM_FILE_CAP 1024

typedef struct
{
    char name[64];
    char data[MEM_FILE_CAP];
    size_t size;
    bool used;
} MemFile;

static MemFile mem_files[MEM_MAX_FILES];

static MemFile *mem_find(const char *name)
{
    for (int i = 0; i < MEM_MAX_FILES; i++)
    {
        if (mem_files[i].used && strcmp(mem_files[i].name, name) == 0)
        {
            return &mem_files[i];
        }
    }
    return NULL;
}

static MemFile *mem_create(const char *name)
{
    for (int i = 0; i < MEM_MAX_FILES; i++)
    {
        if (!mem_files[i].used)
        {
            mem_files[i].used = true;
            strncpy(mem_files[i].name, name, sizeof(mem_files[i].name) - 1);
            mem_files[i].name[sizeof(mem_files[i].name) - 1] = '\0';
            mem_files[i].size = 0;
            return &mem_files[i];
        }
    }
    return NULL;
}

typedef struct
{
    MemFile *f;
    size_t read_pos;
} MemCtx;

static int mem_read_chars(LogoStream *s, char *buf, int count)
{
    MemCtx *c = (MemCtx *)s->context;
    if (!c->f) return -1;
    size_t avail = c->f->size - c->read_pos;
    size_t n = ((size_t)count < avail) ? (size_t)count : avail;
    memcpy(buf, c->f->data + c->read_pos, n);
    c->read_pos += n;
    return (int)n;
}
static void mem_write_bytes(LogoStream *s, const char *buf, size_t len)
{
    MemCtx *c = (MemCtx *)s->context;
    if (!c->f) return;
    size_t room = MEM_FILE_CAP - c->f->size;
    size_t n = (len < room) ? len : room;
    memcpy(c->f->data + c->f->size, buf, n);
    c->f->size += n;
    if (n < len) s->write_error = true; // capacity exceeded
}
static void mem_write(LogoStream *s, const char *text)
{
    mem_write_bytes(s, text, strlen(text));
}
static void mem_flush(LogoStream *s) { (void)s; }
static void mem_close(LogoStream *s)
{
    free(s->context);
    s->context = NULL;
    s->is_open = false;
}
static const LogoStreamOps mem_stream_ops = {
    .read_chars = mem_read_chars, .write = mem_write,
    .write_bytes = mem_write_bytes, .flush = mem_flush,
    .close = mem_close,
};

static LogoStream *mem_open(const char *path)
{
    MemFile *f = mem_find(path);
    if (!f) f = mem_create(path);
    if (!f) return NULL;
    MemCtx *ctx = calloc(1, sizeof(MemCtx));
    ctx->f = f;
    LogoStream *s = calloc(1, sizeof(LogoStream));
    s->type = LOGO_STREAM_FILE;
    s->ops = &mem_stream_ops;
    s->context = ctx;
    s->is_open = true;
    // Deliberately leave write_error set, mimicking a backend (like the FAT one)
    // whose open() does not initialise this field. cross_fs_move must clear it
    // before relying on it, or content copies would spuriously "fail".
    s->write_error = true;
    strncpy(s->name, path, LOGO_STREAM_NAME_MAX - 1);
    return s;
}
static bool mem_file_exists(const char *p) { return mem_find(p) != NULL; }
static bool mem_dir_exists(const char *p) { return p[0] == '/' && p[1] == '\0'; }
static bool mem_file_delete(const char *p)
{
    MemFile *f = mem_find(p);
    if (!f) return false;
    f->used = false;
    return true;
}
static bool mem_dir_create(const char *p) { (void)p; return false; }
static bool mem_dir_delete(const char *p) { (void)p; return false; }
static bool mem_rename(const char *a, const char *b) { (void)a; (void)b; return false; }
static long mem_file_size(const char *p)
{
    MemFile *f = mem_find(p);
    return f ? (long)f->size : -1;
}
static bool mem_list(const char *p, LogoDirCallback cb, void *ud, const char *fl)
{ (void)p; (void)cb; (void)ud; (void)fl; return true; }

static const LogoStorageOps mem_ops = {
    .open = mem_open, .file_exists = mem_file_exists, .dir_exists = mem_dir_exists,
    .file_delete = mem_file_delete, .dir_create = mem_dir_create,
    .dir_delete = mem_dir_delete, .rename = mem_rename, .file_size = mem_file_size,
    .list_directory = mem_list,
};

//============================================================================
// Fixture
//============================================================================

static LogoStorage router;
static bool sd_always(void) { return true; }

void setUp(void)
{
    memset(ram, 0xff, sizeof(ram));
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &cfg));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &cfg));
    logo_lfs_storage_init(&root_backend, &lfs);

    memset(mem_files, 0, sizeof(mem_files));

    logo_storage_router_init(&router, root_backend.ops, &mem_ops, sd_always);
}
void tearDown(void) { lfs_unmount(&lfs); }

// Write a whole string through the router (creates+writes+closes).
static void put(const char *path, const char *text)
{
    LogoStream *s = router.ops->open(path);
    TEST_ASSERT_NOT_NULL(s);
    s->ops->write(s, text);
    s->ops->close(s);
    free(s);
}

// Read a whole file's content through the router into buf.
static void get(const char *path, char *buf, size_t size)
{
    LogoStream *s = router.ops->open(path);
    TEST_ASSERT_NOT_NULL(s);
    int n = s->ops->read_chars(s, buf, (int)size - 1);
    if (n < 0) n = 0;
    buf[n] = '\0';
    s->ops->close(s);
    free(s);
}

//============================================================================
// Tests
//============================================================================

static void test_move_root_to_sd(void)
{
    put("/note.txt", "hello from root");
    TEST_ASSERT_TRUE(router.ops->rename("/note.txt", "/sd/note.txt"));

    TEST_ASSERT_FALSE(router.ops->file_exists("/note.txt"));   // source gone
    TEST_ASSERT_TRUE(router.ops->file_exists("/sd/note.txt")); // dest present
    char buf[64];
    get("/sd/note.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("hello from root", buf);
}

static void test_move_sd_to_root(void)
{
    put("/sd/data.txt", "hello from sd");
    TEST_ASSERT_TRUE(router.ops->rename("/sd/data.txt", "/data.txt"));

    TEST_ASSERT_FALSE(router.ops->file_exists("/sd/data.txt"));
    TEST_ASSERT_TRUE(router.ops->file_exists("/data.txt"));
    char buf[64];
    get("/data.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("hello from sd", buf);
}

static void test_move_overwrites_existing_dest(void)
{
    put("/src.txt", "NEW");
    put("/sd/dst.txt", "OLD-AND-LONGER");
    TEST_ASSERT_TRUE(router.ops->rename("/src.txt", "/sd/dst.txt"));

    char buf[64];
    get("/sd/dst.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("NEW", buf); // fully replaced, no leftover tail
    TEST_ASSERT_FALSE(router.ops->file_exists("/src.txt"));
}

static void test_cross_fs_directory_move_rejected(void)
{
    TEST_ASSERT_TRUE(router.ops->dir_create("/adir"));
    TEST_ASSERT_FALSE(router.ops->rename("/adir", "/sd/adir"));
    TEST_ASSERT_TRUE(router.ops->dir_exists("/adir")); // source dir intact
}

static void test_failed_copy_preserves_source_and_cleans_dest(void)
{
    // Build a source larger than the /sd backend's per-file capacity so the
    // copy fails partway and must roll back.
    static char big[MEM_FILE_CAP + 200];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    put("/big.txt", big);

    TEST_ASSERT_FALSE(router.ops->rename("/big.txt", "/sd/big.txt"));
    TEST_ASSERT_TRUE(router.ops->file_exists("/big.txt"));      // source preserved
    TEST_ASSERT_FALSE(router.ops->file_exists("/sd/big.txt")); // partial dest removed
}

static void test_move_missing_source_fails_and_creates_nothing(void)
{
    // Source does not exist: the move must fail, and must NOT fabricate an empty
    // source or destination via create-on-open.
    TEST_ASSERT_FALSE(router.ops->file_exists("/ghost.txt"));
    TEST_ASSERT_FALSE(router.ops->rename("/ghost.txt", "/sd/ghost.txt"));
    TEST_ASSERT_FALSE(router.ops->file_exists("/ghost.txt"));
    TEST_ASSERT_FALSE(router.ops->file_exists("/sd/ghost.txt"));
}

static void test_move_preserves_binary_content(void)
{
    // A payload with embedded NUL bytes (like an image) must survive a cross-FS
    // move intact — the copy uses write_bytes, not a NUL-terminated write.
    static const char payload[] = {'P', 0x00, 'N', 'G', 0x00, 0x00, 0x7F, 'z'};
    LogoStream *w = router.ops->open("/img.bin");
    TEST_ASSERT_NOT_NULL(w);
    w->ops->write_bytes(w, payload, sizeof(payload));
    w->ops->close(w);
    free(w);

    TEST_ASSERT_TRUE(router.ops->rename("/img.bin", "/sd/img.bin"));
    TEST_ASSERT_FALSE(router.ops->file_exists("/img.bin"));
    TEST_ASSERT_EQUAL_INT((long)sizeof(payload), router.ops->file_size("/sd/img.bin"));

    char buf[16];
    LogoStream *r = router.ops->open("/sd/img.bin");
    TEST_ASSERT_NOT_NULL(r);
    int n = r->ops->read_chars(r, buf, (int)sizeof(buf));
    r->ops->close(r);
    free(r);
    TEST_ASSERT_EQUAL_INT((int)sizeof(payload), n);
    TEST_ASSERT_EQUAL_MEMORY(payload, buf, sizeof(payload));
}

static void test_same_fs_rename_still_native(void)
{
    // A same-backend rename must not go through copy+delete (root side).
    put("/a.txt", "data");
    TEST_ASSERT_TRUE(router.ops->rename("/a.txt", "/b.txt"));
    TEST_ASSERT_FALSE(router.ops->file_exists("/a.txt"));
    TEST_ASSERT_TRUE(router.ops->file_exists("/b.txt"));
}

//============================================================================
// logo_io_copy_file: the io-layer wrapper backing the `copyfile` primitive,
// driven through the same router (root LittleFS + "/sd" mem backend).
//============================================================================

static LogoIO copy_io;

static void copy_setup(void)
{
    logo_io_init(&copy_io, NULL, &router, NULL);
}

static void test_copy_within_root(void)
{
    copy_setup();
    put("/a.txt", "duplicate me");
    TEST_ASSERT_TRUE(logo_io_copy_file(&copy_io, "/a.txt", "/b.txt"));
    TEST_ASSERT_TRUE(router.ops->file_exists("/a.txt")); // source kept (copy, not move)
    char buf[64];
    get("/b.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("duplicate me", buf);
}

static void test_copy_root_to_sd_binary(void)
{
    copy_setup();
    static const char payload[] = {'G', 0x00, 'I', 'F', 0x00, 0x09, 'q'};
    LogoStream *w = router.ops->open("/pic.bin");
    TEST_ASSERT_NOT_NULL(w);
    w->ops->write_bytes(w, payload, sizeof(payload));
    w->ops->close(w);
    free(w);

    TEST_ASSERT_TRUE(logo_io_copy_file(&copy_io, "/pic.bin", "/sd/pic.bin"));
    TEST_ASSERT_TRUE(router.ops->file_exists("/pic.bin")); // source kept
    TEST_ASSERT_EQUAL_INT((long)sizeof(payload), router.ops->file_size("/sd/pic.bin"));

    char buf[16];
    LogoStream *r = router.ops->open("/sd/pic.bin");
    int n = r->ops->read_chars(r, buf, (int)sizeof(buf));
    r->ops->close(r);
    free(r);
    TEST_ASSERT_EQUAL_INT((int)sizeof(payload), n);
    TEST_ASSERT_EQUAL_MEMORY(payload, buf, sizeof(payload));
}

static void test_copy_overwrites_dest(void)
{
    copy_setup();
    put("/src.txt", "NEW");
    put("/dst.txt", "OLD-AND-LONGER");
    TEST_ASSERT_TRUE(logo_io_copy_file(&copy_io, "/src.txt", "/dst.txt"));
    char buf[64];
    get("/dst.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("NEW", buf); // replaced, no stale tail
}

static void test_copy_onto_self_is_noop(void)
{
    copy_setup();
    put("/keep.txt", "unchanged");
    TEST_ASSERT_TRUE(logo_io_copy_file(&copy_io, "/keep.txt", "/keep.txt"));
    char buf[64];
    get("/keep.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("unchanged", buf); // not clobbered to empty
}

static void test_copy_missing_source_fails(void)
{
    copy_setup();
    TEST_ASSERT_FALSE(logo_io_copy_file(&copy_io, "/ghost.txt", "/out.txt"));
    TEST_ASSERT_FALSE(router.ops->file_exists("/out.txt")); // nothing fabricated
}

static void test_copy_directory_source_rejected(void)
{
    copy_setup();
    TEST_ASSERT_TRUE(router.ops->dir_create("/adir"));
    TEST_ASSERT_FALSE(logo_io_copy_file(&copy_io, "/adir", "/adir.copy"));
    TEST_ASSERT_FALSE(router.ops->file_exists("/adir.copy"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_move_root_to_sd);
    RUN_TEST(test_move_sd_to_root);
    RUN_TEST(test_move_overwrites_existing_dest);
    RUN_TEST(test_cross_fs_directory_move_rejected);
    RUN_TEST(test_failed_copy_preserves_source_and_cleans_dest);
    RUN_TEST(test_move_missing_source_fails_and_creates_nothing);
    RUN_TEST(test_move_preserves_binary_content);
    RUN_TEST(test_same_fs_rename_still_native);
    RUN_TEST(test_copy_within_root);
    RUN_TEST(test_copy_root_to_sd_binary);
    RUN_TEST(test_copy_overwrites_dest);
    RUN_TEST(test_copy_onto_self_is_noop);
    RUN_TEST(test_copy_missing_source_fails);
    RUN_TEST(test_copy_directory_source_rejected);
    return UNITY_END();
}
