//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the LittleFS-backed LogoStorage (devices/lfs_storage.c),
//  driven against a RAM block device so the file/dir semantics are exercised
//  natively (no flash hardware).
//

#include "unity.h"
#include "devices/lfs_storage.h"
#include "devices/storage.h"
#include "devices/stream.h"
#include "third_party/littlefs/lfs.h"

#include <stdio.h>
#include <string.h>

//============================================================================
// RAM block device + mounted lfs instance
//============================================================================

#define BS 4096u
#define BC 64u
#define CS 256u
#define LA 32u

static uint8_t ram[BS * BC];

// Fault injection: `read_seen` records which blocks are actually read (the walk
// enumerates a file's data blocks without reading them, so only the blocks it
// reads are worth breaking), and `read_fail` makes reads of a block fail —
// standing in for a block that has gone bad on the device.
static bool read_seen[BC];
static bool read_fail[BC];

static int bd_read(const struct lfs_config *c, lfs_block_t b, lfs_off_t o,
                   void *buf, lfs_size_t s)
{
    if (b < BC)
    {
        read_seen[b] = true;
        if (read_fail[b])
        {
            return LFS_ERR_CORRUPT;
        }
    }
    memcpy(buf, &ram[b * c->block_size + o], s);
    return 0;
}
static int bd_prog(const struct lfs_config *c, lfs_block_t b, lfs_off_t o,
                   const void *buf, lfs_size_t s)
{
    memcpy(&ram[b * c->block_size + o], buf, s);
    return 0;
}
static int bd_erase(const struct lfs_config *c, lfs_block_t b)
{
    memset(&ram[b * c->block_size], 0xff, c->block_size);
    return 0;
}
static int bd_sync(const struct lfs_config *c)
{
    (void)c;
    return 0;
}

static uint8_t rbuf[CS], pbuf[CS], lbuf[LA];
static const struct lfs_config cfg = {
    .read = bd_read, .prog = bd_prog, .erase = bd_erase, .sync = bd_sync,
    .read_size = CS, .prog_size = CS, .block_size = BS, .block_count = BC,
    .block_cycles = 500, .cache_size = CS, .lookahead_size = LA,
    .read_buffer = rbuf, .prog_buffer = pbuf, .lookahead_buffer = lbuf,
};

static lfs_t lfs;
static LogoStorage storage;

void setUp(void)
{
    memset(read_seen, 0, sizeof(read_seen));
    memset(read_fail, 0, sizeof(read_fail));
    memset(ram, 0xff, sizeof(ram));
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &cfg));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &cfg));
    logo_lfs_storage_init(&storage, &lfs);
}

void tearDown(void)
{
    lfs_unmount(&lfs);
}

// Helper: write a whole string to a freshly opened file and close it.
static void write_file(const char *path, const char *text)
{
    LogoStream *s = storage.ops->open(path);
    TEST_ASSERT_NOT_NULL(s);
    s->ops->write(s, text);
    s->ops->close(s);
    free(s);
}

//============================================================================
// File round-trip + metadata
//============================================================================

static void test_write_then_read_back(void)
{
    write_file("/hello.txt", "Hello, Logo!");

    LogoStream *s = storage.ops->open("/hello.txt");
    TEST_ASSERT_NOT_NULL(s);
    char buf[64];
    int n = s->ops->read_chars(s, buf, sizeof(buf) - 1);
    TEST_ASSERT_EQUAL_INT(12, n);
    buf[n] = '\0';
    TEST_ASSERT_EQUAL_STRING("Hello, Logo!", buf);
    s->ops->close(s);
    free(s);
}

// `save` writes many small chunks in one open session, then closes, then a
// later `load` reopens and reads. Reproduce that exact pattern.
static void test_many_writes_one_session_then_reopen(void)
{
    LogoStream *s = storage.ops->open("/proc.lgo");
    TEST_ASSERT_NOT_NULL(s);
    s->ops->write(s, "to hello\n");
    s->ops->write(s, "  pr \"hello\n");
    s->ops->write(s, "end\n");
    s->ops->close(s);
    free(s);

    // The file must now exist and read back exactly what was written.
    TEST_ASSERT_TRUE(storage.ops->file_exists("/proc.lgo"));
    LogoStream *r = storage.ops->open("/proc.lgo");
    TEST_ASSERT_NOT_NULL(r);
    char buf[128];
    int n = r->ops->read_chars(r, buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = '\0';
    TEST_ASSERT_EQUAL_STRING("to hello\n  pr \"hello\nend\n", buf);
    r->ops->close(r);
    free(r);
}

static void test_file_exists_and_size(void)
{
    write_file("/a.txt", "12345");
    TEST_ASSERT_TRUE(storage.ops->file_exists("/a.txt"));
    TEST_ASSERT_FALSE(storage.ops->file_exists("/nope.txt"));
    TEST_ASSERT_EQUAL_INT(5, storage.ops->file_size("/a.txt"));
    TEST_ASSERT_EQUAL_INT(-1, storage.ops->file_size("/nope.txt"));
}

static void test_read_and_write_positions_independent(void)
{
    // Open existing file: read cursor at 0, write cursor at end (append).
    write_file("/log.txt", "abc");
    LogoStream *s = storage.ops->open("/log.txt");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, s->ops->get_read_pos(s));
    TEST_ASSERT_EQUAL_INT(3, s->ops->get_write_pos(s));

    s->ops->write(s, "def"); // appends
    // read from the start still sees the whole file
    char buf[16];
    int n = s->ops->read_chars(s, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    TEST_ASSERT_EQUAL_STRING("abcdef", buf);
    s->ops->close(s);
    free(s);
}

static void test_read_line(void)
{
    write_file("/lines.txt", "one\ntwo\nthree");
    LogoStream *s = storage.ops->open("/lines.txt");
    char buf[32];
    TEST_ASSERT_EQUAL_INT(3, s->ops->read_line(s, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("one", buf);
    TEST_ASSERT_EQUAL_INT(3, s->ops->read_line(s, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("two", buf);
    TEST_ASSERT_EQUAL_INT(5, s->ops->read_line(s, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("three", buf);
    s->ops->close(s);
    free(s);
}

static void test_delete_and_rename(void)
{
    write_file("/old.txt", "data");
    TEST_ASSERT_TRUE(storage.ops->rename("/old.txt", "/new.txt"));
    TEST_ASSERT_FALSE(storage.ops->file_exists("/old.txt"));
    TEST_ASSERT_TRUE(storage.ops->file_exists("/new.txt"));

    TEST_ASSERT_TRUE(storage.ops->file_delete("/new.txt"));
    TEST_ASSERT_FALSE(storage.ops->file_exists("/new.txt"));
    TEST_ASSERT_FALSE(storage.ops->file_delete("/new.txt")); // gone
}

//============================================================================
// Directories
//============================================================================

static void test_dir_create_exists_delete(void)
{
    TEST_ASSERT_TRUE(storage.ops->dir_exists("/")); // root always exists
    TEST_ASSERT_FALSE(storage.ops->dir_exists("/sub"));

    TEST_ASSERT_TRUE(storage.ops->dir_create("/sub"));
    TEST_ASSERT_TRUE(storage.ops->dir_exists("/sub"));
    TEST_ASSERT_FALSE(storage.ops->file_exists("/sub")); // it's a dir, not a file

    TEST_ASSERT_TRUE(storage.ops->dir_delete("/sub"));
    TEST_ASSERT_FALSE(storage.ops->dir_exists("/sub"));
}

static void test_dir_delete_nonempty_fails(void)
{
    TEST_ASSERT_TRUE(storage.ops->dir_create("/d"));
    write_file("/d/inside.txt", "x");
    TEST_ASSERT_FALSE(storage.ops->dir_delete("/d")); // not empty
    TEST_ASSERT_TRUE(storage.ops->file_delete("/d/inside.txt"));
    TEST_ASSERT_TRUE(storage.ops->dir_delete("/d")); // now empty
}

//============================================================================
// Listing (+ extension filter)
//============================================================================

typedef struct
{
    char names[16][64];
    LogoEntryType types[16];
    int count;
} Listing;

static bool list_cb(const char *name, LogoEntryType type, void *ud)
{
    Listing *l = (Listing *)ud;
    if (l->count < 16)
    {
        strncpy(l->names[l->count], name, 63);
        l->types[l->count] = type;
        l->count++;
    }
    return true;
}

static bool listing_has(const Listing *l, const char *name, LogoEntryType t)
{
    for (int i = 0; i < l->count; i++)
    {
        if (strcmp(l->names[i], name) == 0 && l->types[i] == t)
        {
            return true;
        }
    }
    return false;
}

static void test_list_directory_and_filter(void)
{
    write_file("/one.lgo", "a");
    write_file("/two.lgo", "b");
    write_file("/notes.txt", "c");
    TEST_ASSERT_TRUE(storage.ops->dir_create("/dir"));

    // Unfiltered: files + dir, no "." / "..".
    Listing all = {0};
    TEST_ASSERT_TRUE(storage.ops->list_directory("/", list_cb, &all, "*"));
    TEST_ASSERT_TRUE(listing_has(&all, "one.lgo", LOGO_ENTRY_FILE));
    TEST_ASSERT_TRUE(listing_has(&all, "two.lgo", LOGO_ENTRY_FILE));
    TEST_ASSERT_TRUE(listing_has(&all, "notes.txt", LOGO_ENTRY_FILE));
    TEST_ASSERT_TRUE(listing_has(&all, "dir", LOGO_ENTRY_DIRECTORY));
    TEST_ASSERT_EQUAL_INT(4, all.count);

    // Filter by extension "lgo": the two .lgo files pass; notes.txt is excluded.
    // Directories are not subject to the extension filter (matches the FAT
    // backend), so "dir" still appears.
    Listing lgo = {0};
    TEST_ASSERT_TRUE(storage.ops->list_directory("/", list_cb, &lgo, "lgo"));
    TEST_ASSERT_TRUE(listing_has(&lgo, "one.lgo", LOGO_ENTRY_FILE));
    TEST_ASSERT_TRUE(listing_has(&lgo, "two.lgo", LOGO_ENTRY_FILE));
    TEST_ASSERT_TRUE(listing_has(&lgo, "dir", LOGO_ENTRY_DIRECTORY));
    TEST_ASSERT_FALSE(listing_has(&lgo, "notes.txt", LOGO_ENTRY_FILE));
    TEST_ASSERT_EQUAL_INT(3, lgo.count);
}

static void test_open_directory_as_file_fails(void)
{
    TEST_ASSERT_TRUE(storage.ops->dir_create("/adir"));
    TEST_ASSERT_NULL(storage.ops->open("/adir"));
}

//============================================================================
// Consistency walk (fs_check) -- the walk that free-space reporting and block
// allocation both run, which is why a broken one takes all three down (B64).
//============================================================================

// A file big enough to need data blocks of its own, plus a subdirectory: the
// walk reads the directory's metadata pair, which is the part worth breaking.
static void build_filesystem(void)
{
    LogoStream *s = storage.ops->open("/data");
    TEST_ASSERT_NOT_NULL(s);
    char payload[1500];
    memset(payload, 'a', sizeof(payload));
    s->ops->write_bytes(s, payload, sizeof(payload));
    s->ops->close(s);
    free(s);
    TEST_ASSERT_TRUE(storage.ops->dir_create("/sketches"));
}

static void test_fs_check_reports_a_clean_filesystem(void)
{
    build_filesystem();

    uint32_t blocks = 0;
    long last_block = 0;
    int code = -1;
    TEST_ASSERT_TRUE(storage.ops->fs_check(&blocks, &last_block, &code, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(0, code);
    TEST_ASSERT_GREATER_THAN(0, blocks);
}

static void test_fs_check_names_the_error_a_broken_walk_stops_on(void)
{
    build_filesystem();

    // Break the metadata the walk reads, leaving the superblock pair (so the
    // volume still mounts and lists) and every data block (so files still read).
    memset(read_seen, 0, sizeof(read_seen));
    uint32_t blocks = 0;
    long last_block = 0;
    int code = -1;
    TEST_ASSERT_TRUE(storage.ops->fs_check(&blocks, &last_block, &code, NULL, NULL));
    int broken = 0;
    for (lfs_block_t b = 2; b < BC; b++)
    {
        if (read_seen[b])
        {
            read_fail[b] = true;
            broken++;
        }
    }
    TEST_ASSERT_GREATER_THAN(0, broken);

    // The walk stops, and reports the filesystem's own code rather than a bare
    // "something went wrong".
    code = 0;
    TEST_ASSERT_FALSE(storage.ops->fs_check(&blocks, &last_block, &code, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(LFS_ERR_CORRUPT, code);

    // The same fault takes free-space reporting down with it: this pairing is
    // what made a failing `free` and a stalled write one bug rather than two.
    uint32_t free_blocks = 0, block_size = 0;
    TEST_ASSERT_FALSE(storage.ops->free_blocks("/", &free_blocks, &block_size));

    // ...while reading a file, which never walks the filesystem, still works —
    // which is why a damaged volume looks healthy until something allocates.
    LogoStream *r = storage.ops->open("/data");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT(1500, r->ops->get_length(r));
    r->ops->close(r);
    free(r);
}

// Blocks the walk reports as live, so a test can tell a file's data blocks
// (reported but never read) from the metadata it does read.
static bool live_block[BC];

static int mark_live(void *p, lfs_block_t block)
{
    (void)p;
    if (block < BC)
    {
        live_block[block] = true;
    }
    return 0;
}

static bool record_bad_file(const char *path, long size, void *user_data)
{
    char *out = (char *)user_data;
    snprintf(out, LOGO_STREAM_NAME_MAX, "%s:%ld", path, size);
    return true;
}

// The walk never reads a file's data blocks, so damage inside a file is
// invisible to it. That is exactly the gap the per-file scan closes.
static void test_fs_check_scan_names_a_file_it_cannot_read(void)
{
    LogoStream *s = storage.ops->open("/sketches/rocks");
    TEST_ASSERT_NULL(s); // the directory does not exist yet
    TEST_ASSERT_TRUE(storage.ops->dir_create("/sketches"));

    s = storage.ops->open("/sketches/rocks");
    TEST_ASSERT_NOT_NULL(s);
    char payload[1500];
    memset(payload, 'a', sizeof(payload));
    s->ops->write_bytes(s, payload, sizeof(payload));
    s->ops->close(s);
    free(s);

    // A data block is one the walk reports as live but never reads.
    memset(live_block, 0, sizeof(live_block));
    memset(read_seen, 0, sizeof(read_seen));
    TEST_ASSERT_EQUAL_INT(0, lfs_fs_traverse(&lfs, mark_live, NULL));
    lfs_block_t victim = 0;
    for (lfs_block_t b = 2; b < BC; b++)
    {
        if (live_block[b] && !read_seen[b])
        {
            victim = b;
            break;
        }
    }
    TEST_ASSERT_GREATER_THAN(1, victim);
    read_fail[victim] = true;

    // The walk still passes -- it never touches that block -- while the scan
    // finds the file, names it, and reports its recorded length.
    char found[LOGO_STREAM_NAME_MAX] = "";
    uint32_t blocks = 0;
    long last_block = 0;
    int code = -1;
    TEST_ASSERT_TRUE(storage.ops->fs_check(&blocks, &last_block, &code,
                                           record_bad_file, found));
    TEST_ASSERT_EQUAL_INT(0, code);
    TEST_ASSERT_EQUAL_STRING("/sketches/rocks:1500", found);

    // Reporting only: the file it named is still there.
    TEST_ASSERT_TRUE(storage.ops->file_exists("/sketches/rocks"));
}

static void test_fs_check_scan_is_quiet_when_every_file_reads(void)
{
    build_filesystem();

    char found[LOGO_STREAM_NAME_MAX] = "";
    uint32_t blocks = 0;
    long last_block = 0;
    int code = -1;
    TEST_ASSERT_TRUE(storage.ops->fs_check(&blocks, &last_block, &code,
                                           record_bad_file, found));
    TEST_ASSERT_EQUAL_STRING("", found);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_write_then_read_back);
    RUN_TEST(test_many_writes_one_session_then_reopen);
    RUN_TEST(test_file_exists_and_size);
    RUN_TEST(test_read_and_write_positions_independent);
    RUN_TEST(test_read_line);
    RUN_TEST(test_delete_and_rename);
    RUN_TEST(test_dir_create_exists_delete);
    RUN_TEST(test_dir_delete_nonempty_fails);
    RUN_TEST(test_list_directory_and_filter);
    RUN_TEST(test_open_directory_as_file_fails);
    RUN_TEST(test_fs_check_reports_a_clean_filesystem);
    RUN_TEST(test_fs_check_names_the_error_a_broken_walk_stops_on);
    RUN_TEST(test_fs_check_scan_names_a_file_it_cannot_read);
    RUN_TEST(test_fs_check_scan_is_quiet_when_every_file_reads);
    return UNITY_END();
}
