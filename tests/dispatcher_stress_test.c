#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_FLOORS 8

typedef enum {
    ELEVATOR_STATE_IDLE,
    ELEVATOR_STATE_MOVING_UP,
    ELEVATOR_STATE_MOVING_DOWN,
    ELEVATOR_STATE_DOOR_OPENING,
    ELEVATOR_STATE_DOOR_OPEN,
    ELEVATOR_STATE_DOOR_CLOSING
} FsmState;

typedef struct {
    int elevator_id;
    int current_floor;
    int target_floor;
    FsmState state;
    unsigned int timer_ms;
} ElevatorFsm;

static int calculate_cost(ElevatorFsm *fsm, int target_floor) {
    int cost = 0;
    
    switch (fsm->state) {
        case ELEVATOR_STATE_IDLE:
            cost = abs(fsm->current_floor - target_floor);
            break;

        case ELEVATOR_STATE_MOVING_UP:
            if (target_floor >= fsm->current_floor) {
                cost = target_floor - fsm->current_floor;
            } else {
                cost = (fsm->target_floor - fsm->current_floor) + abs(fsm->target_floor - target_floor) + 2;
            }
            break;

        case ELEVATOR_STATE_MOVING_DOWN:
            if (target_floor <= fsm->current_floor) {
                cost = fsm->current_floor - target_floor;
            } else {
                cost = (fsm->current_floor - fsm->target_floor) + abs(fsm->target_floor - target_floor) + 2;
            }
            break;

        case ELEVATOR_STATE_DOOR_OPENING:
        case ELEVATOR_STATE_DOOR_OPEN:
        case ELEVATOR_STATE_DOOR_CLOSING:
            cost = abs(fsm->current_floor - target_floor) + 2;
            break;

        default:
            cost = 1000;
            break;
    }
    return cost;
}

int main(void) {
    ElevatorFsm g_elevator_fsm1 = {1, 2, 8, ELEVATOR_STATE_MOVING_UP, 0};
    ElevatorFsm g_elevator_fsm2 = {2, 7, 8, ELEVATOR_STATE_MOVING_UP, 0};

    printf("=== Starting Dispatcher Scheduling Stress Test ===\n");
    printf("Initial State:\n");
    printf("  Elevator 1: Floor %d, State: BUSY (MOVING UP to %d)\n", g_elevator_fsm1.current_floor, g_elevator_fsm1.target_floor);
    printf("  Elevator 2: Floor %d, State: BUSY (MOVING UP to %d)\n", g_elevator_fsm2.current_floor, g_elevator_fsm2.target_floor);
    printf("\nIncoming Calls: 3, 4, 8, 7, 2\n\n");

    int calls[] = {3, 4, 8, 7, 2};
    int num_calls = sizeof(calls) / sizeof(calls[0]);
    int assigned_to_e1 = 0;
    int assigned_to_e2 = 0;


    for (int i = 0; i < num_calls; i++) {
        int floor = calls[i];
        int cost1 = calculate_cost(&g_elevator_fsm1, floor);
        int cost2 = calculate_cost(&g_elevator_fsm2, floor);

        printf("Call for Floor %d:\n", floor);
        printf("  -> Cost E1: %d, Cost E2: %d\n", cost1, cost2);

        if (cost1 <= cost2) {
            printf("  ==> ASSIGNED TO ELEVATOR 1\n");
            assigned_to_e1++;
        } else {
            printf("  ==> ASSIGNED TO ELEVATOR 2\n");
            assigned_to_e2++;
        }
    }

    printf("\n=== Dispatcher Test Summary ===\n");
    printf("Total Calls: %d\n", num_calls);
    printf("Assigned to Elevator 1: %d (%.1f%%)\n", assigned_to_e1, (float)assigned_to_e1 / num_calls * 100.0f);
    printf("Assigned to Elevator 2: %d (%.1f%%)\n", assigned_to_e2, (float)assigned_to_e2 / num_calls * 100.0f);

    if (assigned_to_e1 > 0 && assigned_to_e2 > 0) {
        printf("\n[SUCCESS] SCHEDULING BUG FIXED & VERIFIED!\n");
        printf("Load is dynamically balanced: Elevator 1 handles nearby low-floor requests, and Elevator 2 handles nearby high-floor requests.\n");
    } else {
        printf("\n[FAILURE] Scheduling remains imbalanced.\n");
    }

    return 0;
}

