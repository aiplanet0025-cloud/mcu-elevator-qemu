#include "elevator_fsm.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LOG_BUFFER_SIZE 128

static int g_checks_run = 0;
static int g_checks_failed = 0;

#define CHECK_TRUE(condition) check_true((condition), #condition, __FILE__, __LINE__)
#define CHECK_INT_EQ(expected, actual) check_int_eq((expected), (actual), #expected, #actual, __FILE__, __LINE__)
#define CHECK_UINT_EQ(expected, actual) check_uint_eq((expected), (actual), #expected, #actual, __FILE__, __LINE__)
#define CHECK_STR_EQ(expected, actual) check_str_eq((expected), (actual), #expected, #actual, __FILE__, __LINE__)
#define CHECK_STR_CONTAINS(haystack, needle) check_str_contains((haystack), (needle), #haystack, #needle, __FILE__, __LINE__)

static void check_true(bool condition, const char *condition_text, const char *file, int line) {
    g_checks_run++;
    if (!condition) {
        g_checks_failed++;
        printf("%s:%d: expected true: %s\n", file, line, condition_text);
    }
}

static void check_int_eq(int expected, int actual, const char *expected_text, const char *actual_text, const char *file, int line) {
    g_checks_run++;
    if (expected != actual) {
        g_checks_failed++;
        printf("%s:%d: expected %s == %s, got %d != %d\n", file, line, expected_text, actual_text, expected, actual);
    }
}

static void check_uint_eq(unsigned int expected, unsigned int actual, const char *expected_text, const char *actual_text, const char *file, int line) {
    g_checks_run++;
    if (expected != actual) {
        g_checks_failed++;
        printf("%s:%d: expected %s == %s, got %u != %u\n", file, line, expected_text, actual_text, expected, actual);
    }
}

static void check_str_eq(const char *expected, const char *actual, const char *expected_text, const char *actual_text, const char *file, int line) {
    g_checks_run++;
    if (strcmp(expected, actual) != 0) {
        g_checks_failed++;
        printf("%s:%d: expected %s == %s, got \"%s\" != \"%s\"\n", file, line, expected_text, actual_text, expected, actual);
    }
}

static void check_str_contains(const char *haystack, const char *needle, const char *haystack_text, const char *needle_text, const char *file, int line) {
    g_checks_run++;
    if (strstr(haystack, needle) == NULL) {
        g_checks_failed++;
        printf("%s:%d: expected %s to contain %s, got \"%s\" without \"%s\"\n", file, line, haystack_text, needle_text, haystack, needle);
    }
}

static void expect_fsm(const ElevatorFsm *fsm, int current_floor, int target_floor, FsmState state, unsigned int timer_ms) {
    CHECK_INT_EQ(current_floor, fsm->current_floor);
    CHECK_INT_EQ(target_floor, fsm->target_floor);
    CHECK_INT_EQ(state, fsm->state);
    CHECK_UINT_EQ(timer_ms, fsm->timer_ms);
}

static void test_initial_state_is_idle_on_floor_one(void) {
    ElevatorFsm fsm;

    Elevator_FsmInit(&fsm);

    expect_fsm(&fsm, 1, 1, ELEVATOR_STATE_IDLE, 0U);
}

static void test_idle_target_can_be_set_and_same_floor_tick_is_quiet(void) {
    ElevatorFsm fsm;
    char log[LOG_BUFFER_SIZE];

    Elevator_FsmInit(&fsm);
    Elevator_FsmSetTarget(&fsm, 1);

    CHECK_TRUE(!Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 1, 1, ELEVATOR_STATE_IDLE, 0U);
    CHECK_STR_EQ("", log);
}

static void test_full_upward_trip_opens_waits_closes_and_returns_idle(void) {
    ElevatorFsm fsm;
    char log[LOG_BUFFER_SIZE];

    Elevator_FsmInit(&fsm);
    Elevator_FsmSetTarget(&fsm, 3);

    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 1, 3, ELEVATOR_STATE_MOVING_UP, 0U);
    CHECK_STR_CONTAINS(log, "Target set to 3");
    CHECK_STR_CONTAINS(log, "STARTING UP");

    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 2, 3, ELEVATOR_STATE_MOVING_UP, 0U);
    CHECK_STR_EQ("[FSM] >> Elevator arrived at floor 2.\r\n", log);

    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 3, 3, ELEVATOR_STATE_DOOR_OPENING, 0U);
    CHECK_STR_EQ("[FSM] >> Elevator arrived at floor 3.\r\n", log);

    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 3, 3, ELEVATOR_STATE_DOOR_OPEN, 0U);
    CHECK_STR_EQ("[FSM] Door: OPENING at floor 3.\r\n", log);

    CHECK_TRUE(!Elevator_FsmTick(&fsm, 1999, log, sizeof(log)));
    expect_fsm(&fsm, 3, 3, ELEVATOR_STATE_DOOR_OPEN, 1999U);
    CHECK_STR_EQ("", log);

    CHECK_TRUE(Elevator_FsmTick(&fsm, 1, log, sizeof(log)));
    expect_fsm(&fsm, 3, 3, ELEVATOR_STATE_DOOR_CLOSING, 2000U);
    CHECK_STR_EQ("[FSM] Door: CLOSING...\r\n", log);

    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 3, 3, ELEVATOR_STATE_IDLE, 2000U);
    CHECK_STR_EQ("[FSM] Door: CLOSED. Elevator is now IDLE.\r\n", log);
}

static void test_downward_trip_reaches_target_before_opening_door(void) {
    ElevatorFsm fsm;
    char log[LOG_BUFFER_SIZE];

    Elevator_FsmInit(&fsm);
    fsm.current_floor = 3;
    fsm.target_floor = 1;

    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 3, 1, ELEVATOR_STATE_MOVING_DOWN, 0U);
    CHECK_STR_CONTAINS(log, "Target set to 1");
    CHECK_STR_CONTAINS(log, "STARTING DOWN");

    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 2, 1, ELEVATOR_STATE_MOVING_DOWN, 0U);
    CHECK_STR_EQ("[FSM] >> Elevator arrived at floor 2.\r\n", log);

    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));
    expect_fsm(&fsm, 1, 1, ELEVATOR_STATE_DOOR_OPENING, 0U);
    CHECK_STR_EQ("[FSM] >> Elevator arrived at floor 1.\r\n", log);
}

static void test_target_update_is_ignored_while_not_idle(void) {
    ElevatorFsm fsm;
    char log[LOG_BUFFER_SIZE];

    Elevator_FsmInit(&fsm);
    Elevator_FsmSetTarget(&fsm, 3);
    CHECK_TRUE(Elevator_FsmTick(&fsm, 100, log, sizeof(log)));

    Elevator_FsmSetTarget(&fsm, 2);

    expect_fsm(&fsm, 1, 3, ELEVATOR_STATE_MOVING_UP, 0U);
}

int main(void) {
    test_initial_state_is_idle_on_floor_one();
    test_idle_target_can_be_set_and_same_floor_tick_is_quiet();
    test_full_upward_trip_opens_waits_closes_and_returns_idle();
    test_downward_trip_reaches_target_before_opening_door();
    test_target_update_is_ignored_while_not_idle();

    if (g_checks_failed != 0) {
        printf("FAILED: %d/%d checks failed.\n", g_checks_failed, g_checks_run);
        return 1;
    }

    printf("PASSED: %d checks passed.\n", g_checks_run);
    return 0;
}
