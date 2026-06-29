//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the storage router (devices/storage_router.c): path dispatch
//  between the root filesystem and the "/sd" mount, prefix stripping, stream
//  name preservation, synthetic "sd" entry on root listing, and cross-mount
//  rename rejection.
//

#include "unity.h"
#include "devices/storage_router.h"
#include "devices/stream.h"

#include <string.h>
#include <stdlib.h>

//============================================================================
// Spy backends: each records the last path(s) it received and a call count, so
// a test can assert which backend handled a call and with what (stripped) path.
//============================================================================

typedef struct Spy
{
    char last[LOGO_STREAM_NAME_MAX];
    char last2[LOGO_STREAM_NAME_MAX]; // second path for rename
    int open, exists, dir_exists, file_delete, dir_create, dir_delete;
    int rename, file_size, list;
} Spy;

static Spy root_spy, sd_spy;

static void spy_reset(void)
{
    memset(&root_spy, 0, sizeof(root_spy));
    memset(&sd_spy, 0, sizeof(sd_spy));
}

// A backend "open" returns a heap LogoStream whose name is the path the backend
// actually received (the router is expected to overwrite it with the full path).
static LogoStream *make_stream(const char *name)
{
    LogoStream *s = calloc(1, sizeof(LogoStream));
    strncpy(s->name, name, LOGO_STREAM_NAME_MAX - 1);
    s->is_open = true;
    return s;
}

// Generate a full ops table writing into the given Spy.
#define DEFINE_SPY_OPS(PFX, SP)                                                     \
    static LogoStream *PFX##_open(const char *p)                                   \
    {                                                                              \
        SP.open++;                                                                 \
        strncpy(SP.last, p, LOGO_STREAM_NAME_MAX - 1);                             \
        return make_stream(p);                                                     \
    }                                                                              \
    static bool PFX##_exists(const char *p)                                        \
    {                                                                              \
        SP.exists++;                                                               \
        strncpy(SP.last, p, LOGO_STREAM_NAME_MAX - 1);                             \
        return true;                                                               \
    }                                                                              \
    static bool PFX##_dir_exists(const char *p)                                    \
    {                                                                              \
        SP.dir_exists++;                                                           \
        strncpy(SP.last, p, LOGO_STREAM_NAME_MAX - 1);                             \
        return true;                                                               \
    }                                                                              \
    static bool PFX##_file_delete(const char *p)                                   \
    {                                                                              \
        SP.file_delete++;                                                          \
        strncpy(SP.last, p, LOGO_STREAM_NAME_MAX - 1);                             \
        return true;                                                               \
    }                                                                              \
    static bool PFX##_dir_create(const char *p)                                    \
    {                                                                              \
        SP.dir_create++;                                                           \
        strncpy(SP.last, p, LOGO_STREAM_NAME_MAX - 1);                             \
        return true;                                                               \
    }                                                                              \
    static bool PFX##_dir_delete(const char *p)                                    \
    {                                                                              \
        SP.dir_delete++;                                                           \
        strncpy(SP.last, p, LOGO_STREAM_NAME_MAX - 1);                             \
        return true;                                                               \
    }                                                                              \
    static bool PFX##_rename(const char *a, const char *b)                         \
    {                                                                              \
        SP.rename++;                                                               \
        strncpy(SP.last, a, LOGO_STREAM_NAME_MAX - 1);                             \
        strncpy(SP.last2, b, LOGO_STREAM_NAME_MAX - 1);                            \
        return true;                                                               \
    }                                                                              \
    static long PFX##_file_size(const char *p)                                     \
    {                                                                              \
        SP.file_size++;                                                            \
        strncpy(SP.last, p, LOGO_STREAM_NAME_MAX - 1);                             \
        return 42;                                                                 \
    }                                                                              \
    static bool PFX##_list(const char *p, LogoDirCallback cb, void *ud,            \
                           const char *filter)                                     \
    {                                                                              \
        (void)filter;                                                             \
        SP.list++;                                                                 \
        strncpy(SP.last, p, LOGO_STREAM_NAME_MAX - 1);                             \
        cb(#PFX "_entry", LOGO_ENTRY_FILE, ud);                                    \
        return true;                                                              \
    }                                                                              \
    static const LogoStorageOps PFX##_ops = {                                      \
        .open = PFX##_open,                                                        \
        .file_exists = PFX##_exists,                                               \
        .dir_exists = PFX##_dir_exists,                                            \
        .file_delete = PFX##_file_delete,                                          \
        .dir_create = PFX##_dir_create,                                            \
        .dir_delete = PFX##_dir_delete,                                            \
        .rename = PFX##_rename,                                                    \
        .file_size = PFX##_file_size,                                              \
        .list_directory = PFX##_list,                                              \
    }

DEFINE_SPY_OPS(root, root_spy);
DEFINE_SPY_OPS(sd, sd_spy);

// SD availability is controlled per test.
static bool sd_present_flag;
static bool sd_present(void) { return sd_present_flag; }

static LogoStorage router;

void setUp(void)
{
    spy_reset();
    sd_present_flag = true;
    logo_storage_router_init(&router, &root_ops, &sd_ops, sd_present);
}

void tearDown(void) {}

// Collects entries reported by a directory listing.
typedef struct
{
    char names[8][LOGO_STREAM_NAME_MAX];
    LogoEntryType types[8];
    int count;
    int stop_after; // return false after this many (0 = never stop)
} Collector;

static bool collect_cb(const char *name, LogoEntryType type, void *ud)
{
    Collector *c = (Collector *)ud;
    if (c->count < 8)
    {
        strncpy(c->names[c->count], name, LOGO_STREAM_NAME_MAX - 1);
        c->types[c->count] = type;
        c->count++;
    }
    if (c->stop_after && c->count >= c->stop_after)
    {
        return false;
    }
    return true;
}

//============================================================================
// Dispatch + prefix stripping
//============================================================================

static void test_root_path_routes_to_root(void)
{
    TEST_ASSERT_TRUE(router.ops->file_exists("/foo"));
    TEST_ASSERT_EQUAL_INT(1, root_spy.exists);
    TEST_ASSERT_EQUAL_INT(0, sd_spy.exists);
    TEST_ASSERT_EQUAL_STRING("/foo", root_spy.last);
}

static void test_sd_path_strips_prefix_to_sd(void)
{
    TEST_ASSERT_TRUE(router.ops->file_exists("/sd/foo"));
    TEST_ASSERT_EQUAL_INT(0, root_spy.exists);
    TEST_ASSERT_EQUAL_INT(1, sd_spy.exists);
    TEST_ASSERT_EQUAL_STRING("/foo", sd_spy.last);
}

static void test_sd_root_maps_to_slash(void)
{
    TEST_ASSERT_TRUE(router.ops->dir_exists("/sd"));
    TEST_ASSERT_EQUAL_INT(1, sd_spy.dir_exists);
    TEST_ASSERT_EQUAL_STRING("/", sd_spy.last);
}

static void test_sdcard_is_not_the_mount(void)
{
    // "/sdcard" is a root file, not the "/sd" mount.
    TEST_ASSERT_TRUE(router.ops->file_exists("/sdcard"));
    TEST_ASSERT_EQUAL_INT(1, root_spy.exists);
    TEST_ASSERT_EQUAL_INT(0, sd_spy.exists);
    TEST_ASSERT_EQUAL_STRING("/sdcard", root_spy.last);
}

static void test_sd_match_is_case_insensitive(void)
{
    TEST_ASSERT_TRUE(router.ops->file_exists("/SD/foo"));
    TEST_ASSERT_EQUAL_INT(1, sd_spy.exists);
    TEST_ASSERT_EQUAL_STRING("/foo", sd_spy.last);
}

static void test_nested_sd_path(void)
{
    TEST_ASSERT_EQUAL_INT(42, router.ops->file_size("/sd/a/b/c.txt"));
    TEST_ASSERT_EQUAL_INT(1, sd_spy.file_size);
    TEST_ASSERT_EQUAL_STRING("/a/b/c.txt", sd_spy.last);
}

static void test_nondir_ops_dispatch(void)
{
    router.ops->dir_create("/sd/newdir");
    TEST_ASSERT_EQUAL_INT(1, sd_spy.dir_create);
    TEST_ASSERT_EQUAL_STRING("/newdir", sd_spy.last);

    router.ops->dir_delete("/proj");
    TEST_ASSERT_EQUAL_INT(1, root_spy.dir_delete);
    TEST_ASSERT_EQUAL_STRING("/proj", root_spy.last);

    router.ops->file_delete("/sd/junk");
    TEST_ASSERT_EQUAL_INT(1, sd_spy.file_delete);
    TEST_ASSERT_EQUAL_STRING("/junk", sd_spy.last);
}

//============================================================================
// open: stream name must reflect the full (routed) path
//============================================================================

static void test_open_preserves_full_path_name(void)
{
    LogoStream *s = router.ops->open("/sd/foo");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(1, sd_spy.open);
    TEST_ASSERT_EQUAL_STRING("/foo", sd_spy.last);    // backend saw stripped path
    TEST_ASSERT_EQUAL_STRING("/sd/foo", s->name);     // router restored full path
    free(s);
}

static void test_open_root_keeps_name(void)
{
    LogoStream *s = router.ops->open("/notes");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(1, root_spy.open);
    TEST_ASSERT_EQUAL_STRING("/notes", s->name);
    free(s);
}

//============================================================================
// list_directory + synthetic "sd" entry
//============================================================================

static void test_root_listing_injects_sd_when_present(void)
{
    sd_present_flag = true;
    Collector c = {0};
    router.ops->list_directory("/", collect_cb, &c, "*");
    // "sd" (synthetic) then the root backend's entry.
    TEST_ASSERT_EQUAL_INT(2, c.count);
    TEST_ASSERT_EQUAL_STRING("sd", c.names[0]);
    TEST_ASSERT_EQUAL_INT(LOGO_ENTRY_DIRECTORY, c.types[0]);
    TEST_ASSERT_EQUAL_STRING("root_entry", c.names[1]);
    TEST_ASSERT_EQUAL_INT(1, root_spy.list);
}

static void test_root_listing_no_sd_when_absent(void)
{
    sd_present_flag = false;
    Collector c = {0};
    router.ops->list_directory("/", collect_cb, &c, "*");
    TEST_ASSERT_EQUAL_INT(1, c.count);
    TEST_ASSERT_EQUAL_STRING("root_entry", c.names[0]);
}

static void test_sd_listing_routes_and_no_injection(void)
{
    Collector c = {0};
    router.ops->list_directory("/sd", collect_cb, &c, "*");
    TEST_ASSERT_EQUAL_INT(1, sd_spy.list);
    TEST_ASSERT_EQUAL_STRING("/", sd_spy.last);
    TEST_ASSERT_EQUAL_INT(1, c.count);
    TEST_ASSERT_EQUAL_STRING("sd_entry", c.names[0]);
}

static void test_nonroot_dir_does_not_inject_sd(void)
{
    Collector c = {0};
    router.ops->list_directory("/subdir", collect_cb, &c, "*");
    TEST_ASSERT_EQUAL_INT(1, c.count);
    TEST_ASSERT_EQUAL_STRING("root_entry", c.names[0]);
}

//============================================================================
// rename: same-backend delegates natively; cross-mount goes through
// cross_fs_move (copy+delete), exercised against real backends in
// test_cross_fs_move.c — here we only check the router does not call a native
// rename for a cross-mount move.
//============================================================================

static void test_rename_within_root(void)
{
    TEST_ASSERT_TRUE(router.ops->rename("/a", "/b"));
    TEST_ASSERT_EQUAL_INT(1, root_spy.rename);
    TEST_ASSERT_EQUAL_STRING("/a", root_spy.last);
    TEST_ASSERT_EQUAL_STRING("/b", root_spy.last2);
}

static void test_rename_within_sd_strips_both(void)
{
    TEST_ASSERT_TRUE(router.ops->rename("/sd/a", "/sd/b"));
    TEST_ASSERT_EQUAL_INT(1, sd_spy.rename);
    TEST_ASSERT_EQUAL_STRING("/a", sd_spy.last);
    TEST_ASSERT_EQUAL_STRING("/b", sd_spy.last2);
}

static void test_cross_mount_rename_not_native(void)
{
    // A cross-mount rename is dispatched to cross_fs_move, not to either
    // backend's native rename. With these spies the move bails out early (the
    // destination is reported as a directory), so it returns false — but the
    // point here is that no native rename runs. The actual copy+delete behaviour
    // is covered by test_cross_fs_move.c against real backends.
    TEST_ASSERT_FALSE(router.ops->rename("/sd/a", "/b"));
    TEST_ASSERT_FALSE(router.ops->rename("/a", "/sd/b"));
    TEST_ASSERT_EQUAL_INT(0, root_spy.rename);
    TEST_ASSERT_EQUAL_INT(0, sd_spy.rename);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_root_path_routes_to_root);
    RUN_TEST(test_sd_path_strips_prefix_to_sd);
    RUN_TEST(test_sd_root_maps_to_slash);
    RUN_TEST(test_sdcard_is_not_the_mount);
    RUN_TEST(test_sd_match_is_case_insensitive);
    RUN_TEST(test_nested_sd_path);
    RUN_TEST(test_nondir_ops_dispatch);
    RUN_TEST(test_open_preserves_full_path_name);
    RUN_TEST(test_open_root_keeps_name);
    RUN_TEST(test_root_listing_injects_sd_when_present);
    RUN_TEST(test_root_listing_no_sd_when_absent);
    RUN_TEST(test_sd_listing_routes_and_no_injection);
    RUN_TEST(test_nonroot_dir_does_not_inject_sd);
    RUN_TEST(test_rename_within_root);
    RUN_TEST(test_rename_within_sd_strips_both);
    RUN_TEST(test_cross_mount_rename_not_native);
    return UNITY_END();
}
