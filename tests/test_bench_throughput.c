// P10 M0 -- interpreter throughput benchmark and regression guard.
// See docs/interpreter-throughput-design.md section 6 (M0).
//
// Times the interpreter on the host against the mock device, where drawing
// is a recorded command rather than a rasterised one, so these numbers
// isolate interpreting from plotting (the method of design section 2.1).
//
// Absolute per-iteration times are printed (BENCH lines) for the record --
// the design doc keeps the baseline, and M1/M2 before/afters read straight
// off this output.  The ctest assertions are on RELATIVE numbers only: each
// scenario against an in-process calibration loop that slows down with the
// machine exactly as the interpreter does, plus the workspace-scan ratio.
// A loaded CI box inflates numerator and denominator together, so the guard
// does not flap; a real regression moves the ratio.
//
// Bounds are ~3x the ratios measured at the current baseline, so they catch
// a 2x interpreter regression while tolerating machine variance.  They were
// re-tightened when M1 and M2 landed; do the same for any later milestone.

#include "test_scaffold.h"
#include "core/repl.h"
#include "core/error.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef TRAILS_SOURCE
#error "TRAILS_SOURCE must be defined"
#endif
#ifndef CHECKRUN_SOURCE
#error "CHECKRUN_SOURCE must be defined"
#endif

// Relative bounds (scenario time / calibration-loop time), set at ~3x the
// baseline ratios recorded in the design doc.
#define BOUND_REPEAT_ITER_X_CAL   700.0    // M2 baseline x~215
#define BOUND_PROC1_ITER_X_CAL    300.0    // M2 baseline x~91
#define BOUND_PROC_SCAN_RATIO     2.0      // 128- vs 1-proc; M2 flattened this to 1.00
#define BOUND_TRAILS_FRAME_X_CAL  5.5e5    // M2 baseline x181k
#define BOUND_TRAILS_BOARD_X_CAL  3.0e6    // P9 M3 baseline x~0.9M (was x~16M)
#define BOUND_CHECKRUN_FRAME_X_CAL 1.8e6   // M2 baseline x587k

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

// The relative-guard denominator: a volatile float-add loop, timed in ns per
// iteration.  It scales with the machine the same way the interpreter does,
// so scenario/calibration ratios are stable where absolute times are not.
static double calibrate_ns(void)
{
    const int n = 20 * 1000 * 1000;
    volatile float x = 0.0f;
    double t0 = now_ms();
    for (int i = 0; i < n; i++)
        x = x + 1.0f;
    double t1 = now_ms();
    (void)x;
    return (t1 - t0) * 1e6 / n;
}

// Run a line and return its wall time in ms.
static double time_code_ms(const char *code)
{
    double t0 = now_ms();
    Result r = run_string(code);
    double t1 = now_ms();
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));
    return t1 - t0;
}

//==========================================================================
// Scenario 1: the pure repeat loop of the design's profile (section 2.2)
//==========================================================================

void test_bench_repeat_loop(void)
{
    double cal = calibrate_ns();
    run_string("make \"x 0");

    const int iters = 200000;
    time_code_ms("repeat 20000 [make \"x (:x + 1)]");   // warm-up: intern, grow
    double ms = time_code_ms("repeat 200000 [make \"x (:x + 1)]");
    double per_iter_ns = ms * 1e6 / iters;
    double ratio = per_iter_ns / cal;

    printf("BENCH repeat.loop      %8.1f ns/iter   x%.0f cal (cal %.2f ns)\n",
           per_iter_ns, ratio, cal);
    TEST_ASSERT_TRUE_MESSAGE(ratio < BOUND_REPEAT_ITER_X_CAL,
                             "repeat-loop iteration regressed vs calibration");
}

//==========================================================================
// Scenario 2: user-procedure call cost at several workspace sizes.
// The target is defined LAST, so the linear scan in find_procedure_index_n
// pays its worst case -- the number M2's binding cache should flatten.
// Sizes are 1 / 64 / 127: MAX_PROCEDURES is 128, so 127 fillers + target
// is the fullest workspace a stock build allows.
//==========================================================================

// Define `count-1` empty filler procedures, then the call target.
static void define_workspace(int count)
{
    Result r = run_string("erall");
    TEST_ASSERT_TRUE(r.status == RESULT_NONE || r.status == RESULT_OK);
    char text[64];
    for (int i = 0; i < count - 1; i++)
    {
        snprintf(text, sizeof(text), "to bench.fill.%d\nend", i);
        r = proc_define_from_text(text);
        TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
    }
    r = proc_define_from_text("to bench.target\nend");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
}

// ns per call of bench.target with `procs` procedures defined.
static double time_proc_call_ns(int procs)
{
    define_workspace(procs);
    const int iters = 100000;
    time_code_ms("repeat 10000 [bench.target]");        // warm-up
    double ms = time_code_ms("repeat 100000 [bench.target]");
    return ms * 1e6 / iters;
}

void test_bench_proc_call_workspace_scaling(void)
{
    double cal = calibrate_ns();
    double t1 = time_proc_call_ns(1);
    double t64 = time_proc_call_ns(64);
    double t128 = time_proc_call_ns(128);
    double scan_ratio = t128 / t1;

    printf("BENCH proc.call.1      %8.1f ns/call   x%.0f cal\n", t1, t1 / cal);
    printf("BENCH proc.call.64     %8.1f ns/call\n", t64);
    printf("BENCH proc.call.128    %8.1f ns/call   x%.2f of 1-proc\n",
           t128, scan_ratio);

    TEST_ASSERT_TRUE_MESSAGE(t1 / cal < BOUND_PROC1_ITER_X_CAL,
                             "procedure call regressed vs calibration");
    TEST_ASSERT_TRUE_MESSAGE(scan_ratio < BOUND_PROC_SCAN_RATIO,
                             "workspace-size scan cost regressed");
}

//==========================================================================
// Scenario 3/4: the shipped games' play.frame on the mock device.
// Loading mirrors test_trails.c's loader, which mirrors prim_load.
//==========================================================================

#define TEST_LOAD_MAX_LINE 256

static void load_game(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    char line[1024], buf[8192];
    size_t used = 0;
    bool in_def = false;

    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        TEST_ASSERT_LESS_THAN_MESSAGE(TEST_LOAD_MAX_LINE, n, line);
        if (n == 0) continue;

        if (!in_def && repl_line_starts_with_to(line)) {
            in_def = true;
            used = 0;
        }

        if (in_def) {
            if (repl_line_is_end(line)) {
                memcpy(buf + used, "end", 4);
                in_def = false;
                Result r = proc_define_from_text(buf);
                TEST_ASSERT_NOT_EQUAL_MESSAGE(RESULT_ERROR, r.status, buf);
                used = 0;
            } else {
                TEST_ASSERT_LESS_THAN(sizeof(buf) - n - 2, used);
                memcpy(buf + used, line, n);
                used += n;
                buf[used++] = '\n';
            }
            continue;
        }

        Result r = run_string(line);
        TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, line);
    }
    TEST_ASSERT_FALSE_MESSAGE(in_def, "file ends inside a procedure definition");
    fclose(f);
}

// Time `frames` play.frames after `setup` and report ms per frame.
static double time_game_frames_ms(const char *setup, int frames)
{
    Result r = run_string(setup);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));
    reset_output();
    run_string("play.frame");                            // warm-up
    char code[64];
    snprintf(code, sizeof(code), "repeat %d [play.frame]", frames);
    return time_code_ms(code) / frames;
}

void test_bench_trails_play_frame(void)
{
    double cal = calibrate_ns();
    load_game(TRAILS_SOURCE);
    double ms = time_game_frames_ms(
        "setup.palette setup.shapes setup.turtles setup.tiles setup.sound "
        "init.game setup.level setrefresh \"manual", 30);

    printf("BENCH trails.frame     %8.3f ms/frame  x%.1fk cal\n",
           ms, ms * 1e6 / cal / 1e3);
    TEST_ASSERT_TRUE_MESSAGE(ms * 1e6 / cal < BOUND_TRAILS_FRAME_X_CAL,
                             "Turtle Trails play.frame regressed vs calibration");

    // The level build -- P9 M3 turned this from decoding a map into 1,050
    // cons cells and carving it with the pen into two map passes and one
    // stampmap.  It is not a frame cost, but it is the largest single stall
    // the game has, so it belongs in the record beside the frame.
    double board = time_code_ms("setup.level");
    printf("BENCH trails.board     %8.3f ms/build x%.1fM cal\n",
           board, board * 1e6 / cal / 1e6);
    TEST_ASSERT_TRUE_MESSAGE(board * 1e6 / cal < BOUND_TRAILS_BOARD_X_CAL,
                             "Turtle Trails level build regressed vs calibration");
}

// P10 M5: the cost of an expression, by shape. The board profiler
// (logo/tests/p10prof) times the same shapes there; the pair is what turns
// "this statement is slow" into "slow relative to what", and it is how the
// grouping paren was separated from the infix operator and the operands.
// Printed for the record, like the other BENCH lines; the guard is on the
// one ratio a regression would move.
void test_bench_expr_shapes(void)
{
    static const char *const shape[] = {
        "[]", "[ignore 1]", "[ignore :x]", "[ignore sum 1 1]",
        "[ignore (1 + 1)]", "[ignore (:x + :x)]", "[ignore (sum 1 1)]",
        "[make \"x 1]", "[make \"x (:x + 1)]", "[make \"x :x + 1]",
    };
    const int iters = 200000;
    run_string("make \"x 1");

    double bare = 0, paren = 0, bare_make = 0;
    for (unsigned i = 0; i < sizeof(shape) / sizeof(*shape); i++)
    {
        char code[128];
        snprintf(code, sizeof(code), "repeat %d %s", iters, shape[i]);
        Result r = run_string(code);
        TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                                 shape[i]);
        double ns = time_code_ms(code) * 1e6 / iters;
        printf("SHAPE %-22s %7.1f ns\n", shape[i], ns);
        if (i == 0) bare = ns;
        if (strcmp(shape[i], "[make \"x (:x + 1)]") == 0) paren = ns;
        if (strcmp(shape[i], "[make \"x :x + 1]") == 0) bare_make = ns;
    }

    // The outermost paren of `make "v (expr)` is redundant -- the last
    // argument absorbs the expression anyway -- and it is not free. If this
    // ever reaches parity the game's de-parenthesising is pointless; if it
    // grows past a third, something in the grouping path regressed.
    double cost = (paren - bare_make) / (bare_make - bare);
    printf("SHAPE redundant-paren overhead %.1f %%\n", cost * 100.0);
    TEST_ASSERT_TRUE_MESSAGE(cost < 0.5, "grouping-paren overhead regressed");
}

void test_bench_checkrun_play_frame(void)
{
    double cal = calibrate_ns();
    load_game(CHECKRUN_SOURCE);
    double ms = time_game_frames_ms(
        "setup.sound init.game start.round setrefresh \"manual", 20);

    printf("BENCH checkrun.frame   %8.3f ms/frame  x%.1fk cal\n",
           ms, ms * 1e6 / cal / 1e3);
    TEST_ASSERT_TRUE_MESSAGE(ms * 1e6 / cal < BOUND_CHECKRUN_FRAME_X_CAL,
                             "Checkpoint Run play.frame regressed vs calibration");
}

//==========================================================================
// The hardware script: logo/tests/p10m0 must run end to end on the mock,
// so a script that fails half way through cannot waste a hardware session
// (the p9m0 convention).
//==========================================================================

void test_p10m0_script_runs(void)
{
    load_game(P10M0_SOURCE);
    run_string("make \"p10.n 100");   // dial down: correctness, not timing
    Result r = run_string("p10m0");
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(output_buffer, "repeat loop"), output_buffer);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(output_buffer, "full ws"), output_buffer);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bench_repeat_loop);
    RUN_TEST(test_bench_proc_call_workspace_scaling);
    RUN_TEST(test_bench_expr_shapes);
    RUN_TEST(test_bench_trails_play_frame);
    RUN_TEST(test_bench_checkrun_play_frame);
    RUN_TEST(test_p10m0_script_runs);
    return UNITY_END();
}
