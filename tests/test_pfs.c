//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the file-server example (logo/fileserver).
//
//  The example is pure Logo; the whole file is loaded into the interpreter
//  (proving it parses and its helpers run on the mock device) and then driven
//  end to end: real HTTP requests are queued through the mock connection,
//  pumped until pending, routed with `fs.handle`, and the raw response bytes
//  are asserted. The source path is passed in as a compile definition.
//
//  The shared mock filesystem (test_mock_fs.h) is flat: its list_directory
//  ignores the path and returns every entry. To exercise real subdirectory
//  browsing this test overrides just that one op with a hierarchical variant
//  that returns the direct children of a path under their bare names, exactly
//  as littlefs does on the device -- leaving the shared mock untouched.
//

#include "test_scaffold.h"
#include "test_mock_fs.h"
#include "mock_device.h"
#include "core/httpd.h"
#include "core/repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FILESERVER_PATH
#error "FILESERVER_PATH must be defined (path to logo/fileserver)"
#endif

// ---------------------------------------------------------------------------
// Hierarchical directory listing over the flat mock store.
//
// Return entries whose stored full path is a direct child of `dir`, passing
// the callback the bare child name (like the device backend). Root is "/".
// ---------------------------------------------------------------------------
static bool fs_list_children(const char *dir, LogoDirCallback cb, void *ud,
                             const char *filter)
{
    size_t dl = strlen(dir);
    for (int i = 0; i < MOCK_MAX_FILES; i++)
    {
        if (!mock_files[i].exists)
            continue;
        const char *name = mock_files[i].name;   // full path, e.g. "/sub/a.txt"
        if (strncmp(name, dir, dl) != 0)
            continue;
        const char *rest = name + dl;
        // Skip the separating slash for a non-root parent ("/sub" + "/a.txt").
        if (dl > 0 && dir[dl - 1] != '/')
        {
            if (*rest != '/')
                continue;                        // "/subother" is not under "/sub"
            rest++;
        }
        if (*rest == '\0')
            continue;                            // the directory itself
        if (strchr(rest, '/') != NULL)
            continue;                            // a deeper descendant, not a child
        if (filter && strcmp(filter, "*") != 0)
        {
            const char *ext = strrchr(rest, '.');
            if (!ext || strcmp(ext + 1, filter) != 0)
                continue;
        }
        LogoEntryType type = mock_files[i].is_directory ? LOGO_ENTRY_DIRECTORY
                                                        : LOGO_ENTRY_FILE;
        if (!cb(rest, type, ud))
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Load the whole fileserver file, defining its procedures (same line-buffering
// as the `load` primitive and the galaxian test). Fails on any error.
// ---------------------------------------------------------------------------
static void load_fileserver(void)
{
    FILE *f = fopen(FILESERVER_PATH, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "cannot open " FILESERVER_PATH);

    char line[512];
    char proc[8192];
    size_t proc_len = 0;
    bool in_def = false;

    while (fgets(line, sizeof(line), f))
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;

        if (!in_def && repl_line_starts_with_to(line))
        {
            in_def = true;
            memcpy(proc, line, len);
            proc[len] = '\n';
            proc_len = len + 1;
            continue;
        }
        if (in_def)
        {
            if (repl_line_is_end(line))
            {
                TEST_ASSERT_MESSAGE(proc_len + 4 <= sizeof(proc), "procedure too big");
                memcpy(proc + proc_len, "end", 3);
                proc[proc_len + 3] = '\0';
                in_def = false;
                Result r = proc_define_from_text(proc);
                TEST_ASSERT_MESSAGE(r.status != RESULT_ERROR, proc);
            }
            else
            {
                TEST_ASSERT_MESSAGE(proc_len + len + 1 < sizeof(proc), "procedure too big");
                memcpy(proc + proc_len, line, len);
                proc[proc_len + len] = '\n';
                proc_len += len + 1;
            }
            continue;
        }

        Result r = run_string(line);
        TEST_ASSERT_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, line);
    }
    fclose(f);
}

void setUp(void)
{
    test_scaffold_setUp();
    mock_device_init();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    mock_storage_ops.list_directory = fs_list_children;  // hierarchical listing
    logo_io_init(&mock_io, &mock_console, &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    mock_device_set_wifi_connected(true);   // http.listen requires a connection
    set_mock_ticks(0);
    load_fileserver();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// ---------------------------------------------------------------------------
// Request helpers (mirroring test_httpd.c).
// ---------------------------------------------------------------------------
static void pump(int n)
{
    for (int i = 0; i < n; i++) httpd_poll();
}

// NUL-terminated copy of connection slot 0's recorded response text.
static const char *resp_str(void)
{
    static char buf[4096];
    int len = 0;
    const char *r = mock_httpd_conn_response(0, &len);
    if (len > (int)sizeof(buf) - 1) len = (int)sizeof(buf) - 1;
    memcpy(buf, r, (size_t)len);
    buf[len] = '\0';
    return buf;
}

// Queue one request, pump until pending, route it with fs.handle, return the
// raw response text. Resets the server first so each request reuses slot 0,
// letting a single test issue several requests in sequence.
static const char *handle(const char *req)
{
    httpd_reset();
    eval_string("http.listen 80");
    mock_httpd_queue_connection(req, strlen(req));
    pump(3);
    TEST_ASSERT_TRUE_MESSAGE(httpd_request_pending(), req);
    Result r = eval_string("fs.handle");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, req);
    return resp_str();
}

static bool status_is(const char *resp, int code)
{
    char prefix[24];
    int plen = snprintf(prefix, sizeof(prefix), "HTTP/1.1 %d", code);
    return strncmp(resp, prefix, (size_t)plen) == 0;
}

// A small tree: files and a subdirectory with its own child.
static void seed_tree(void)
{
    mock_fs_create_file("/index.html", "<h1>home</h1>");
    mock_fs_create_file("/notes.txt", "hello");
    mock_fs_create_file("/logo.png", "\x89PNG\r\n");
    mock_fs_create_file("/data", "raw-bytes");
    mock_fs_create_dir("/sub");
    mock_fs_create_file("/sub/child.txt", "kid");
}

// Assert a Logo word expression evaluates to the given string.
static void assert_word(const char *expr, const char *expected)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, value_to_string(r.value), expr);
}

//==========================================================================
// Pure helpers: path handling and content types
//==========================================================================

void test_fs_trim_normalizes_trailing_slash(void)
{
    assert_word("fs.trim \"/", "/");
    assert_word("fs.trim \"/sub", "/sub");
    assert_word("fs.trim \"/sub/", "/sub");
}

void test_fs_join_avoids_double_slash(void)
{
    assert_word("fs.join \"/ \"a.txt", "/a.txt");
    assert_word("fs.join \"/sub \"a.txt", "/sub/a.txt");
}

void test_fs_parent(void)
{
    assert_word("fs.parent \"/sub", "/");
    assert_word("fs.parent \"/a/b", "/a");
}

void test_fs_ext_takes_last_segment_lowercased(void)
{
    assert_word("fs.ext \"index.html", "html");
    assert_word("fs.ext \"a.tar.GZ", "gz");
    assert_word("fs.ext \"noext", "");
}

void test_fs_mime_maps_known_types(void)
{
    assert_word("fs.mime \"page.html", "text/html");
    assert_word("fs.mime \"style.css", "text/css");
    assert_word("fs.mime \"pic.PNG", "image/png");     // case-insensitive
    assert_word("fs.mime \"photo.jpeg", "image/jpeg");
    assert_word("fs.mime \"vec.svg", "image/svg+xml"); // '+' survives (bar-quoted)
    assert_word("fs.mime \"data", "application/octet-stream");
}

//==========================================================================
// GET: directory listing
//==========================================================================

void test_root_lists_files_and_folders(void)
{
    seed_tree();
    const char *resp = handle("GET / HTTP/1.1\r\n\r\n");
    TEST_ASSERT_TRUE(status_is(resp, 200));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Type: text/html\r\n"));
    // Direct children appear...
    TEST_ASSERT_NOT_NULL(strstr(resp, "href=/notes.txt"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "href=/index.html"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "href=/sub/"));      // folder link
    // ...but a grandchild does not.
    TEST_ASSERT_NULL(strstr(resp, "child.txt"));
    // Upload + delete client script is present.
    TEST_ASSERT_NOT_NULL(strstr(resp, "<script"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Upload"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "method:'PUT'"));
    // The page is assembled from a flat stream, not nested Logo lists.
    TEST_ASSERT_NULL(strstr(resp, "<main class=app>]"));
    // Bar-quoted Logo words cannot safely contain JavaScript's `||`.
    TEST_ASSERT_NULL(strstr(resp, "!nbusy"));
    // Root has no parent link.
    TEST_ASSERT_NULL(strstr(resp, "Parent directory"));
}

void test_subdir_lists_only_its_children_with_parent_link(void)
{
    seed_tree();
    const char *resp = handle("GET /sub/ HTTP/1.1\r\n\r\n");
    TEST_ASSERT_TRUE(status_is(resp, 200));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Type: text/html\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "href=/sub/child.txt"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Parent directory"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "href=/"));          // parent points to root
    // Files from the parent directory must not leak in.
    TEST_ASSERT_NULL(strstr(resp, "notes.txt"));
}

//==========================================================================
// GET: file downloads with content types
//==========================================================================

void test_download_text_file_with_type_and_body(void)
{
    seed_tree();
    const char *resp = handle("GET /notes.txt HTTP/1.1\r\n\r\n");
    TEST_ASSERT_TRUE(status_is(resp, 200));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Type: text/plain\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\r\n\r\nhello"));
}

void test_download_html_and_png_and_binary_types(void)
{
    seed_tree();
    TEST_ASSERT_NOT_NULL(strstr(handle("GET /index.html HTTP/1.1\r\n\r\n"),
                                "Content-Type: text/html\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(handle("GET /logo.png HTTP/1.1\r\n\r\n"),
                                "Content-Type: image/png\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(handle("GET /data HTTP/1.1\r\n\r\n"),
                                "Content-Type: application/octet-stream\r\n"));
}

void test_missing_file_is_404(void)
{
    seed_tree();
    TEST_ASSERT_TRUE(status_is(handle("GET /nope.txt HTTP/1.1\r\n\r\n"), 404));
}

//==========================================================================
// PUT: upload, DELETE: remove
//==========================================================================

void test_put_stores_the_body(void)
{
    const char *resp = handle("PUT /up.txt HTTP/1.1\r\nContent-Length: 5\r\n\r\nworld");
    TEST_ASSERT_TRUE(status_is(resp, 201));
    // The raw body landed in the file system under the request path.
    assert_word("file? \"/up.txt", "true");
    MockFile *up = mock_fs_get_file("/up.txt", false);
    TEST_ASSERT_NOT_NULL(up);
    TEST_ASSERT_EQUAL_UINT(5, up->size);
    TEST_ASSERT_EQUAL_INT(0, memcmp(up->data, "world", 5));
}

void test_delete_removes_a_file(void)
{
    seed_tree();
    TEST_ASSERT_TRUE(status_is(handle("DELETE /notes.txt HTTP/1.1\r\n\r\n"), 200));
    assert_word("file? \"/notes.txt", "false");
}

void test_delete_missing_is_404(void)
{
    seed_tree();
    TEST_ASSERT_TRUE(status_is(handle("DELETE /gone.txt HTTP/1.1\r\n\r\n"), 404));
}

void test_unsupported_method_is_405(void)
{
    seed_tree();
    TEST_ASSERT_TRUE(status_is(handle("POST /x HTTP/1.1\r\nContent-Length: 0\r\n\r\n"), 405));
}

//==========================================================================
// A large listing streams past the single-word (255-byte) limit.
//==========================================================================

void test_many_files_stream_in_one_page(void)
{
    for (int i = 0; i < 8; i++)
    {
        char name[32];
        snprintf(name, sizeof(name), "/file%d.txt", i);
        mock_fs_create_file(name, "x");
    }
    const char *resp = handle("GET / HTTP/1.1\r\n\r\n");
    TEST_ASSERT_TRUE(status_is(resp, 200));
    TEST_ASSERT_NOT_NULL(strstr(resp, "href=/file0.txt"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "href=/file7.txt"));
    // The body comfortably exceeds any single interned word.
    TEST_ASSERT_TRUE(strlen(resp) > 300);
}

void test_repeated_pages_recycle_temporary_lists(void)
{
    seed_tree();
    for (int i = 0; i < 80; i++)
    {
        const char *resp = handle("GET / HTTP/1.1\r\n\r\n");
        TEST_ASSERT_TRUE_MESSAGE(status_is(resp, 200), "directory page ran out of nodes");
    }
}

void test_fileserver_can_be_restarted(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, eval_string("fileserver").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, eval_string("fileserver").status);
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fs_trim_normalizes_trailing_slash);
    RUN_TEST(test_fs_join_avoids_double_slash);
    RUN_TEST(test_fs_parent);
    RUN_TEST(test_fs_ext_takes_last_segment_lowercased);
    RUN_TEST(test_fs_mime_maps_known_types);
    RUN_TEST(test_root_lists_files_and_folders);
    RUN_TEST(test_subdir_lists_only_its_children_with_parent_link);
    RUN_TEST(test_download_text_file_with_type_and_body);
    RUN_TEST(test_download_html_and_png_and_binary_types);
    RUN_TEST(test_missing_file_is_404);
    RUN_TEST(test_put_stores_the_body);
    RUN_TEST(test_delete_removes_a_file);
    RUN_TEST(test_delete_missing_is_404);
    RUN_TEST(test_unsupported_method_is_405);
    RUN_TEST(test_many_files_stream_in_one_page);
    RUN_TEST(test_repeated_pages_recycle_temporary_lists);
    RUN_TEST(test_fileserver_can_be_restarted);
    return UNITY_END();
}
