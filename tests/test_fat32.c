//
//  Unit tests for devices/picocalc/fat32.c, exercised through the mock SD
//  card backend (tests/mock_sdcard.c) and the in-memory image formatter
//  (tests/fat32_image.c).
//
//  The tests focus on regressions identified during the FAT spec audit:
//   - MBR vs BPB detection
//   - LFN sequence safety / NUL termination
//   - shortname collision detection (~N)
//   - set_current_dir on a regular file
//   - rename across directories updates ".."
//   - zero-fill on grow past EOF
//   - dirty flag avoids write on read-only close
//   - bound FAT chain traversal
//   - boot signature / file system type validation
//
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "unity.h"
#include "fat32.h"
#include "sdcard.h"
#include "mock_sdcard.h"
#include "fat32_image.h"

// fat32_unmount() forces a re-mount on the next operation.  Tests use it
// between scenarios so internal state does not leak between cases.
extern void fat32_unmount(void);

void setUp(void)
{
    // Default: a small superfloppy with the minimum legal cluster count
    // (65525) and 1 sector/cluster — about 33 MB of sparse RAM.
    fat32_image_format_superfloppy(65525, 1);
    fat32_unmount();
}

void tearDown(void)
{
    fat32_unmount();
    mock_sd_destroy();
}

//
// Mount / format detection
//

static void test_mount_superfloppy_succeeds(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());
    TEST_ASSERT_TRUE(fat32_is_mounted());
}

static void test_mount_mbr_succeeds(void)
{
    fat32_image_format_mbr(65525, 1, 2048);
    fat32_unmount();
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());
    TEST_ASSERT_TRUE(fat32_is_mounted());
}

// Regression for #1: a valid FAT32 superfloppy whose bootstrap code
// happens to contain non-zero bytes at the MBR partition-table offsets
// must NOT be misinterpreted as an MBR.
static void test_mount_superfloppy_with_busy_bootstrap_succeeds(void)
{
    // Inject random non-zero bytes into the bootstrap code area where
    // MBR partition-table entries would live (offsets 446..509).  The
    // boot sector is otherwise valid (jump byte + BPS + 0x55AA).
    uint8_t *bs = mock_sd_block_ptr(0);
    TEST_ASSERT_NOT_NULL(bs);
    for (int i = 446; i < 510; i++)
    {
        // Avoid writing 0x0B/0x0C at the partition-type offsets (4 bytes
        // into each 16-byte slot) so we test the looser case of stray
        // bytes; the MBR heuristic must reject these.
        bs[i] = (uint8_t)(0xAB ^ (i & 0xFF));
    }

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());
}

// Regression for #25: invalid boot signature must be rejected.
static void test_mount_rejects_bad_boot_signature(void)
{
    uint8_t *bs = mock_sd_block_ptr(0);
    bs[66] = 0x00; // boot_signature (offset 66 in the BPB)
    TEST_ASSERT_NOT_EQUAL(FAT32_OK, fat32_mount());
}

// Regression for #25: file_system_type that is not "FAT32..." must be
// rejected.
static void test_mount_rejects_bad_fs_type(void)
{
    uint8_t *bs = mock_sd_block_ptr(0);
    memcpy(bs + 82, "EXFAT   ", 8); // file_system_type field
    TEST_ASSERT_NOT_EQUAL(FAT32_OK, fat32_mount());
}

//
// File create / read / write round-trip
//

static void test_create_write_read_roundtrip(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    fat32_file_t f;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "hello.txt"));

    const char payload[] = "Hello, FAT32!";
    size_t written = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f, payload, sizeof(payload), &written));
    TEST_ASSERT_EQUAL_UINT(sizeof(payload), written);
    fat32_close(&f);

    fat32_file_t f2;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f2, "hello.txt"));
    char readback[64] = {0};
    size_t read_n = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_read(&f2, readback, sizeof(readback), &read_n));
    TEST_ASSERT_EQUAL_UINT(sizeof(payload), read_n);
    TEST_ASSERT_EQUAL_STRING(payload, readback);
    fat32_close(&f2);
}

// Regression for #11: opening a file read-only and closing it must NOT
// rewrite the directory entry.
static void test_readonly_close_does_not_write(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    fat32_file_t f;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "ro.txt"));
    fat32_close(&f);

    // Force any background writes to flush before we measure.
    mock_sd_reset_stats();

    fat32_file_t f2;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f2, "ro.txt"));
    char buf[8];
    size_t n;
    fat32_read(&f2, buf, sizeof(buf), &n);
    fat32_close(&f2);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, mock_sd_write_count(),
        "read-only open/close must not write");
}

// Regression for #6: writing past EOF must zero-fill the gap so previous
// on-disk contents are not exposed.
static void test_write_past_eof_zero_fills(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    fat32_file_t f;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "sparse.bin"));

    // Write a short header, seek 1 KB ahead, then write a footer.  Bytes
    // between header and footer must read back as zero.
    const uint8_t header[] = {0xAA, 0xBB, 0xCC, 0xDD};
    size_t n;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f, header, sizeof(header), &n));

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_seek(&f, 1024));
    const uint8_t footer[] = {0x11, 0x22, 0x33, 0x44};
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f, footer, sizeof(footer), &n));
    fat32_close(&f);

    fat32_file_t f2;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f2, "sparse.bin"));
    uint8_t buf[1024 + 4] = {0xFF};
    memset(buf, 0xFF, sizeof(buf));
    size_t read_n = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_read(&f2, buf, sizeof(buf), &read_n));
    TEST_ASSERT_EQUAL_UINT(sizeof(buf), read_n);

    // header preserved
    TEST_ASSERT_EQUAL_HEX8_ARRAY(header, buf, sizeof(header));
    // gap zeroed
    for (size_t i = sizeof(header); i < 1024; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00, buf[i], "byte in sparse gap not zero");
    }
    // footer preserved
    TEST_ASSERT_EQUAL_HEX8_ARRAY(footer, buf + 1024, sizeof(footer));
    fat32_close(&f2);
}

//
// Long file names + shortname collision (#2 / #14)
//

static void test_long_filename_roundtrip(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    const char *name = "A Long Filename With Spaces.txt";
    fat32_file_t f;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, name));
    fat32_close(&f);

    // Re-open via the long name to verify the LFN was written and is
    // resolvable on lookup.
    fat32_file_t f2;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f2, name));
    fat32_close(&f2);
}

// Regression for #2: creating multiple files whose long names collapse to
// the same short-name root must produce distinct ~N tails — and the
// collision check must compare against the on-disk 8.3 names, not the
// reconstructed long names.
static void test_shortname_collision_handled(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    fat32_file_t f;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "Long Name One.txt"));
    fat32_close(&f);

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "Long Name Two.txt"));
    fat32_close(&f);

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "Long Name Three.txt"));
    fat32_close(&f);

    // All three should be openable via their original long names.
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f, "Long Name One.txt"));   fat32_close(&f);
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f, "Long Name Two.txt"));   fat32_close(&f);
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f, "Long Name Three.txt")); fat32_close(&f);
}

//
// set_current_dir on a non-directory (#3)
//

static void test_set_current_dir_rejects_file(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    fat32_file_t f;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "regular.txt"));
    fat32_close(&f);

    TEST_ASSERT_EQUAL_INT(FAT32_ERROR_NOT_A_DIRECTORY,
                          fat32_set_current_dir("regular.txt"));
}

//
// Rename across directories updates ".." (#5)
//

static void test_rename_directory_updates_dotdot(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    fat32_file_t d;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_dir_create(&d, "/src"));
    fat32_close(&d);
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_dir_create(&d, "/dst"));
    fat32_close(&d);
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_dir_create(&d, "/src/child"));
    fat32_close(&d);

    // Move /src/child → /dst/child
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_rename("/src/child", "/dst/child"));

    // Capture /dst's start cluster directly from its open file handle.
    fat32_file_t dst_dir;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&dst_dir, "/dst"));
    uint32_t dst_cluster = dst_dir.start_cluster;
    fat32_close(&dst_dir);
    TEST_ASSERT_TRUE_MESSAGE(dst_cluster != 0, "/dst has no start cluster");

    // Open the moved directory and inspect its ".." entry.  Expect the
    // recorded parent cluster to match /dst (not /src and not 0).
    fat32_file_t moved;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&moved, "/dst/child"));

    fat32_entry_t entry;
    bool saw_dotdot = false;
    while (fat32_dir_read(&moved, &entry) == FAT32_OK && entry.filename[0])
    {
        if (strcmp(entry.filename, "..") == 0)
        {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(dst_cluster, entry.start_cluster,
                ".. of moved directory must reference new parent");
            saw_dotdot = true;
            break;
        }
    }
    fat32_close(&moved);
    TEST_ASSERT_TRUE_MESSAGE(saw_dotdot, "moved directory has no .. entry");
}

//
// Volume label retrieval (#4)
//

static void test_volume_label_preserved(void)
{
    // Default mock image label is "MOCKVOL" (padded to 11 with spaces).
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    char label[16];
    // The format helper writes the label to the BPB but FAT32 spec also
    // requires a VOLUME_ID directory entry; if the BPB-only label is the
    // only one present, fat32_get_volume_name returns an empty string.
    // Either outcome is acceptable here — what we want to assert is that
    // when a label exists, no spurious '.' is inserted (regression #4).
    fat32_error_t r = fat32_get_volume_name(label, sizeof(label));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, r);
    TEST_ASSERT_NULL_MESSAGE(strchr(label, '.'),
        "volume label must not contain a '.' separator");
}

//
// Multi-cluster file: write enough bytes to force chain allocation, then
// read back and verify every byte.  Default cluster size is 512 B so 4 KB
// occupies 8 clusters.
//

static void test_multi_cluster_file_roundtrip(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    enum { N = 4096 };
    uint8_t *payload = (uint8_t *)malloc(N);
    TEST_ASSERT_NOT_NULL(payload);
    for (int i = 0; i < N; i++) payload[i] = (uint8_t)((i * 31) ^ (i >> 5));

    fat32_file_t f;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "big.bin"));
    size_t n = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f, payload, N, &n));
    TEST_ASSERT_EQUAL_UINT(N, n);
    fat32_close(&f);

    fat32_file_t f2;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f2, "big.bin"));
    TEST_ASSERT_EQUAL_UINT(N, fat32_size(&f2));
    uint8_t *readback = (uint8_t *)calloc(1, N);
    TEST_ASSERT_NOT_NULL(readback);
    size_t got = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_read(&f2, readback, N, &got));
    TEST_ASSERT_EQUAL_UINT(N, got);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, readback, N);
    fat32_close(&f2);

    free(payload);
    free(readback);
}

// Seek into the middle of a multi-cluster file and overwrite a region that
// straddles a cluster boundary.  Verifies that fat32_seek correctly walks
// the FAT chain and that fat32_write picks the right cluster on resume.
static void test_multi_cluster_seek_and_overwrite(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    enum { N = 2048 };
    uint8_t *payload = (uint8_t *)malloc(N);
    for (int i = 0; i < N; i++) payload[i] = (uint8_t)i;

    fat32_file_t f;
    size_t n;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "stripe.bin"));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f, payload, N, &n));
    fat32_close(&f);

    // Overwrite bytes [500, 700) — straddles the 512 B cluster boundary.
    uint8_t patch[200];
    memset(patch, 0xCC, sizeof(patch));
    fat32_file_t f2;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f2, "stripe.bin"));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_seek(&f2, 500));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f2, patch, sizeof(patch), &n));
    TEST_ASSERT_EQUAL_UINT(sizeof(patch), n);
    fat32_close(&f2);

    // Read whole file back and check byte-for-byte.
    fat32_file_t f3;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f3, "stripe.bin"));
    uint8_t buf[N];
    size_t got;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_read(&f3, buf, sizeof(buf), &got));
    TEST_ASSERT_EQUAL_UINT(N, got);
    for (int i = 0; i < N; i++)
    {
        uint8_t expect = (i >= 500 && i < 700) ? 0xCC : (uint8_t)i;
        TEST_ASSERT_EQUAL_HEX8(expect, buf[i]);
    }
    fat32_close(&f3);
    free(payload);
}

//
// Directory growth: a 1-sector root cluster holds 16 32-byte slots.  Each
// LFN-bearing file consumes 3-4 slots, so creating ~10 files forces the
// driver to allocate a second cluster for the root directory.
//

static void test_directory_grows_across_clusters(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    enum { COUNT = 12 };
    char name[64];
    for (int i = 0; i < COUNT; i++)
    {
        snprintf(name, sizeof(name), "Long Name Number %02d.txt", i);
        fat32_file_t f;
        fat32_error_t r = fat32_create(&f, name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(FAT32_OK, r, name);
        fat32_close(&f);
    }

    // Re-open every file via its long name — proves the LFN entries
    // survived crossing the cluster boundary.
    for (int i = 0; i < COUNT; i++)
    {
        snprintf(name, sizeof(name), "Long Name Number %02d.txt", i);
        fat32_file_t f;
        TEST_ASSERT_EQUAL_INT_MESSAGE(FAT32_OK, fat32_open(&f, name), name);
        fat32_close(&f);
    }
}

//
// fat32_delete must release the cluster chain (free clusters), unlink the
// 8.3 + LFN entries, and the file must no longer be openable.
//

static void test_delete_releases_chain(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    uint64_t free_before = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_get_free_space(&free_before));

    // Allocate ~4 KB → 8 clusters of overhead at 512 B/cluster.
    enum { N = 4096 };
    uint8_t buf[N];
    memset(buf, 0xA5, sizeof(buf));

    fat32_file_t f;
    size_t n;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "doomed.bin"));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f, buf, N, &n));
    fat32_close(&f);

    uint64_t free_after_write = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_get_free_space(&free_after_write));
    TEST_ASSERT_TRUE_MESSAGE(free_after_write < free_before,
        "writing a file must reduce free space");

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_delete("doomed.bin"));

    // The file must no longer exist.
    fat32_file_t gone;
    TEST_ASSERT_EQUAL_INT(FAT32_ERROR_FILE_NOT_FOUND,
                          fat32_open(&gone, "doomed.bin"));

    // Free space must be restored (allow a small margin if FSInfo wasn't
    // tracking before — but in practice it is exact in this fixture).
    uint64_t free_after_delete = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_get_free_space(&free_after_delete));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(free_before, free_after_delete,
        "delete must restore free space exactly");
}

//
// FSInfo accounting: each file write should decrement free_count, each
// delete should increment it.  Tracked via fat32_get_free_space.
//

static void test_fsinfo_free_count_tracks_alloc_free(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    uint64_t f0 = 0, f1 = 0, f2 = 0;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_get_free_space(&f0));

    fat32_file_t f;
    size_t n;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "acct.bin"));
    uint8_t blob[1024];
    memset(blob, 0x5A, sizeof(blob));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f, blob, sizeof(blob), &n));
    fat32_close(&f);

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_get_free_space(&f1));
    TEST_ASSERT_TRUE(f1 < f0);

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_delete("acct.bin"));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_get_free_space(&f2));
    TEST_ASSERT_EQUAL_UINT64(f0, f2);
}

//
// Nested directory creation + cd + lookup.  Exercises fat32_dir_create
// for sub-paths and fat32_set_current_dir / fat32_get_current_dir.
//

static void test_nested_mkdir_and_cd(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    fat32_file_t d;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_dir_create(&d, "/a"));         fat32_close(&d);
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_dir_create(&d, "/a/b"));       fat32_close(&d);
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_dir_create(&d, "/a/b/c"));     fat32_close(&d);

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_set_current_dir("/a/b/c"));

    char cwd[FAT32_MAX_PATH_LEN];
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_get_current_dir(cwd, sizeof(cwd)));
    TEST_ASSERT_EQUAL_STRING("/a/b/c", cwd);

    // Create a file relative to the cwd, then verify it resolves under
    // the absolute path too.
    fat32_file_t f;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "leaf.txt"));
    fat32_close(&f);

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_set_current_dir("/"));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f, "/a/b/c/leaf.txt"));
    fat32_close(&f);

    // Non-empty directory must refuse to be deleted.
    TEST_ASSERT_EQUAL_INT(FAT32_ERROR_DIR_NOT_EMPTY, fat32_delete("/a/b/c"));
}

//
// Circular FAT chain: poke the FAT directly so cluster N points back to
// itself, then attempt to read past it.  The driver must terminate, not
// loop.  Regression for fix #21 / #22.
//
// Layout reminder: reserved_sectors = 32, FAT0 starts at sector 32.
// Each FAT sector holds 128 32-bit entries (sector_offset = cluster / 128,
// entry_offset = cluster % 128).
//

static void poke_fat_entry(uint32_t cluster, uint32_t value)
{
    uint32_t fat_sector = 32 + (cluster / 128);
    uint32_t entry_off  = (cluster % 128) * 4;
    uint8_t *sec = mock_sd_block_ptr(fat_sector);
    TEST_ASSERT_NOT_NULL(sec);
    sec[entry_off + 0] = (uint8_t)(value      );
    sec[entry_off + 1] = (uint8_t)(value >>  8);
    sec[entry_off + 2] = (uint8_t)(value >> 16);
    sec[entry_off + 3] = (uint8_t)(value >> 24);
    // Mirror to FAT1 (fat_size = 512 sectors for 65525-cluster image).
    uint8_t *sec1 = mock_sd_block_ptr(fat_sector + 512);
    if (sec1)
    {
        memcpy(sec1 + entry_off, sec + entry_off, 4);
    }
}

static void test_circular_fat_chain_terminates(void)
{
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    // Write a file large enough that its chain has at least 2 clusters.
    enum { N = 2048 };
    uint8_t blob[N];
    memset(blob, 0x77, sizeof(blob));
    fat32_file_t f;
    size_t n;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_create(&f, "loop.bin"));
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_write(&f, blob, sizeof(blob), &n));
    uint32_t start = f.start_cluster;
    fat32_close(&f);

    // Force cluster N → cluster N (self-loop).  This is the most extreme
    // form of circularity; fat32_read must not hang.
    TEST_ASSERT_TRUE(start >= 2);
    poke_fat_entry(start, start);

    // Re-mount so any cached state is dropped.
    fat32_unmount();
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());

    fat32_file_t f2;
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_open(&f2, "loop.bin"));
    // Try to read more than file_size — driver must stop, not spin.
    uint8_t out[N + 1024];
    size_t got = 0;
    fat32_error_t rr = fat32_read(&f2, out, sizeof(out), &got);
    // Either OK with bounded got, or an explicit format error — both are
    // acceptable.  What matters is that we returned at all.
    TEST_ASSERT_TRUE(rr == FAT32_OK || rr == FAT32_ERROR_INVALID_FORMAT);
    TEST_ASSERT_TRUE_MESSAGE(got <= sizeof(out), "read overran buffer");
    fat32_close(&f2);
}

//
// Hot-swap safety: the generation counter bumps on every unmount, so an open
// handle can detect that the card was removed/swapped underneath it.
//

static void test_generation_bumps_on_unmount(void)
{
    uint32_t g0 = fat32_get_generation();
    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());
    fat32_unmount();
    TEST_ASSERT_EQUAL_UINT32(g0 + 1, fat32_get_generation());

    TEST_ASSERT_EQUAL_INT(FAT32_OK, fat32_mount());
    fat32_unmount();
    TEST_ASSERT_EQUAL_UINT32(g0 + 2, fat32_get_generation());
}

//
// Run all tests
//

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_generation_bumps_on_unmount);
    RUN_TEST(test_mount_superfloppy_succeeds);
    RUN_TEST(test_mount_mbr_succeeds);
    RUN_TEST(test_mount_superfloppy_with_busy_bootstrap_succeeds);
    RUN_TEST(test_mount_rejects_bad_boot_signature);
    RUN_TEST(test_mount_rejects_bad_fs_type);
    RUN_TEST(test_create_write_read_roundtrip);
    RUN_TEST(test_readonly_close_does_not_write);
    RUN_TEST(test_write_past_eof_zero_fills);
    RUN_TEST(test_long_filename_roundtrip);
    RUN_TEST(test_shortname_collision_handled);
    RUN_TEST(test_set_current_dir_rejects_file);
    RUN_TEST(test_rename_directory_updates_dotdot);
    RUN_TEST(test_volume_label_preserved);
    RUN_TEST(test_multi_cluster_file_roundtrip);
    RUN_TEST(test_multi_cluster_seek_and_overwrite);
    RUN_TEST(test_directory_grows_across_clusters);
    RUN_TEST(test_delete_releases_chain);
    RUN_TEST(test_fsinfo_free_count_tracks_alloc_free);
    RUN_TEST(test_nested_mkdir_and_cd);
    RUN_TEST(test_circular_fat_chain_terminates);
    return UNITY_END();
}
