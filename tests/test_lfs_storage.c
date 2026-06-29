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

#include <string.h>

//============================================================================
// RAM block device + mounted lfs instance
//============================================================================

#define BS 4096u
#define BC 64u
#define CS 256u
#define LA 32u

static uint8_t ram[BS * BC];

static int bd_read(const struct lfs_config *c, lfs_block_t b, lfs_off_t o,
                   void *buf, lfs_size_t s)
{
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
    return UNITY_END();
}
