//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for whole-filesystem image backup/restore (devices/lfs_backup.c):
//  sparse round-trip, validate-before-flash on a corrupt image, growing a
//  small image onto a larger device, and rejecting an image too large to fit.
//

#include "unity.h"
#include "devices/lfs_backup.h"
#include "devices/stream.h"
#include "third_party/littlefs/lfs.h"

#include <string.h>
#include <stdlib.h>

//============================================================================
// RAM block device shared by every mount (sized for the largest test device).
//============================================================================

#define BS 4096u
#define CS 256u
#define LA 32u
#define BC_SMALL 16u
#define BC_BIG 48u

static uint8_t ram[BS * BC_BIG];

static int bd_read(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, void *buf, lfs_size_t s)
{ memcpy(buf, &ram[b * c->block_size + o], s); return 0; }
static int bd_prog(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, const void *buf, lfs_size_t s)
{ memcpy(&ram[b * c->block_size + o], buf, s); return 0; }
static int bd_erase(const struct lfs_config *c, lfs_block_t b)
{ memset(&ram[b * c->block_size], 0xff, c->block_size); return 0; }
static int bd_sync(const struct lfs_config *c) { (void)c; return 0; }

static uint8_t rbuf[CS], pbuf[CS], lbuf[LA];

static struct lfs_config make_cfg(lfs_size_t block_count)
{
    struct lfs_config cfg = {
        .read = bd_read, .prog = bd_prog, .erase = bd_erase, .sync = bd_sync,
        .read_size = CS, .prog_size = CS, .block_size = BS, .block_count = block_count,
        .block_cycles = 500, .cache_size = CS, .lookahead_size = LA,
        .read_buffer = rbuf, .prog_buffer = pbuf, .lookahead_buffer = lbuf,
    };
    return cfg;
}

//============================================================================
// Seekable in-memory blob: stands in for the SD-card backup file.
//============================================================================

#define BLOB_CAP (BS * BC_BIG + 65536u)

typedef struct
{
    uint8_t buf[BLOB_CAP];
    size_t len;
    size_t rpos;
} Blob;

static int blob_read_chars(LogoStream *s, char *b, int n)
{
    Blob *B = (Blob *)s->context;
    size_t avail = B->len - B->rpos;
    size_t k = ((size_t)n < avail) ? (size_t)n : avail;
    memcpy(b, B->buf + B->rpos, k);
    B->rpos += k;
    return (int)k;
}
static void blob_write_bytes(LogoStream *s, const char *b, size_t n)
{
    Blob *B = (Blob *)s->context;
    if (B->len + n > BLOB_CAP) { s->write_error = true; return; }
    memcpy(B->buf + B->len, b, n);
    B->len += n;
}
static void blob_flush(LogoStream *s) { (void)s; }
static long blob_get_read_pos(LogoStream *s) { return (long)((Blob *)s->context)->rpos; }
static bool blob_set_read_pos(LogoStream *s, long p)
{
    Blob *B = (Blob *)s->context;
    if (p < 0 || (size_t)p > B->len) return false;
    B->rpos = (size_t)p;
    return true;
}
static const LogoStreamOps blob_ops = {
    .read_chars = blob_read_chars,
    .write_bytes = blob_write_bytes,
    .flush = blob_flush,
    .get_read_pos = blob_get_read_pos,
    .set_read_pos = blob_set_read_pos,
};

static Blob blob;
static LogoStream blob_stream;

static void blob_reset_for_write(void)
{
    blob.len = 0;
    blob.rpos = 0;
    blob_stream.type = LOGO_STREAM_FILE;
    blob_stream.ops = &blob_ops;
    blob_stream.context = &blob;
    blob_stream.is_open = true;
    blob_stream.write_error = false;
}
static void blob_rewind_for_read(void) { blob.rpos = 0; blob_stream.write_error = false; }

//============================================================================
// Helpers
//============================================================================

static void write_file(lfs_t *lfs, const char *name, const char *content)
{
    lfs_file_t f;
    uint8_t fbuf[CS];
    struct lfs_file_config fc = {.buffer = fbuf};
    TEST_ASSERT_EQUAL_INT(0, lfs_file_opencfg(lfs, &f, name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC, &fc));
    lfs_size_t n = (lfs_size_t)strlen(content);
    TEST_ASSERT_EQUAL_INT((int)n, lfs_file_write(lfs, &f, content, n));
    TEST_ASSERT_EQUAL_INT(0, lfs_file_close(lfs, &f));
}

static void check_file(lfs_t *lfs, const char *name, const char *expected)
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

void setUp(void) { memset(ram, 0xff, sizeof(ram)); }
void tearDown(void) {}

//============================================================================
// Tests
//============================================================================

static void test_backup_restore_round_trip(void)
{
    struct lfs_config cfg = make_cfg(BC_BIG);
    lfs_t lfs;
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &cfg));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &cfg));
    write_file(&lfs, "alpha.lgo", "to alpha pr 1 end");
    write_file(&lfs, "beta.txt", "the quick brown fox");

    blob_reset_for_write();
    TEST_ASSERT_TRUE(logo_lfs_backup(&lfs, &blob_stream));

    // Wipe the filesystem completely, then restore over the blank volume.
    TEST_ASSERT_EQUAL_INT(0, lfs_unmount(&lfs));
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &cfg));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &cfg));

    blob_rewind_for_read();
    TEST_ASSERT_TRUE(logo_lfs_restore(&lfs, &blob_stream));

    check_file(&lfs, "alpha.lgo", "to alpha pr 1 end");
    check_file(&lfs, "beta.txt", "the quick brown fox");
    lfs_unmount(&lfs);
}

static void test_backup_is_sparse(void)
{
    struct lfs_config cfg = make_cfg(BC_BIG);
    lfs_t lfs;
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &cfg));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &cfg));
    write_file(&lfs, "small.txt", "hi");

    blob_reset_for_write();
    TEST_ASSERT_TRUE(logo_lfs_backup(&lfs, &blob_stream));
    lfs_unmount(&lfs);

    // A nearly-empty 48-block volume must image to far less than the full size.
    TEST_ASSERT_TRUE(blob.len < (size_t)(BC_BIG * BS) / 2);
}

static void test_corrupt_image_rejected_fs_intact(void)
{
    struct lfs_config cfg = make_cfg(BC_BIG);
    lfs_t lfs;
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &cfg));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &cfg));
    write_file(&lfs, "keep.txt", "original");

    blob_reset_for_write();
    TEST_ASSERT_TRUE(logo_lfs_backup(&lfs, &blob_stream));

    // Corrupt a data byte well past the header so a block CRC fails.
    blob.buf[40] ^= 0xFF;

    blob_rewind_for_read();
    TEST_ASSERT_FALSE(logo_lfs_restore(&lfs, &blob_stream));
    // Validation fails before any flash is touched: fs still mounted and intact.
    check_file(&lfs, "keep.txt", "original");
    lfs_unmount(&lfs);
}

static void test_restore_grows_to_larger_device(void)
{
    // Back up a small (16-block) filesystem.
    struct lfs_config small = make_cfg(BC_SMALL);
    lfs_t lfs;
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &small));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &small));
    write_file(&lfs, "carry.txt", "survives the grow");
    blob_reset_for_write();
    TEST_ASSERT_TRUE(logo_lfs_backup(&lfs, &blob_stream));
    lfs_unmount(&lfs);

    // Restore onto a fresh 48-block device.
    struct lfs_config big = make_cfg(BC_BIG);
    memset(ram, 0xff, sizeof(ram));
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &big));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &big));

    blob_rewind_for_read();
    TEST_ASSERT_TRUE(logo_lfs_restore(&lfs, &blob_stream));

    // Content survives and the filesystem now spans the whole larger device.
    check_file(&lfs, "carry.txt", "survives the grow");
    struct lfs_fsinfo info;
    TEST_ASSERT_EQUAL_INT(0, lfs_fs_stat(&lfs, &info));
    TEST_ASSERT_EQUAL_UINT32(BC_BIG, info.block_count);
    lfs_unmount(&lfs);
}

static void test_restore_too_large_rejected(void)
{
    // Image taken from a 48-block filesystem...
    struct lfs_config big = make_cfg(BC_BIG);
    lfs_t lfs;
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &big));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &big));
    write_file(&lfs, "big.txt", "from the large volume");
    blob_reset_for_write();
    TEST_ASSERT_TRUE(logo_lfs_backup(&lfs, &blob_stream));
    lfs_unmount(&lfs);

    // ...cannot be restored onto a 16-block device (no shrink).
    struct lfs_config small = make_cfg(BC_SMALL);
    memset(ram, 0xff, sizeof(ram));
    TEST_ASSERT_EQUAL_INT(0, lfs_format(&lfs, &small));
    TEST_ASSERT_EQUAL_INT(0, lfs_mount(&lfs, &small));
    write_file(&lfs, "marker.txt", "still here");

    blob_rewind_for_read();
    TEST_ASSERT_FALSE(logo_lfs_restore(&lfs, &blob_stream));
    // Rejected before flashing: the small filesystem is untouched.
    check_file(&lfs, "marker.txt", "still here");
    lfs_unmount(&lfs);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_backup_restore_round_trip);
    RUN_TEST(test_backup_is_sparse);
    RUN_TEST(test_corrupt_image_rejected_fs_intact);
    RUN_TEST(test_restore_grows_to_larger_device);
    RUN_TEST(test_restore_too_large_rejected);
    return UNITY_END();
}
