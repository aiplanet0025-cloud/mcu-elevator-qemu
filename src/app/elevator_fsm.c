#include "elevator_fsm.h"
#include <stdio.h>

void Elevator_FsmInit(ElevatorFsm *fsm, int id) {
    fsm->elevator_id = id;
    fsm->current_floor = 1;
    fsm->target_floor = 1;
    fsm->state = ELEVATOR_STATE_IDLE;
    fsm->timer_ms = 0;
}

void Elevator_FsmSetTarget(ElevatorFsm *fsm, int target) {
    if (fsm->state == ELEVATOR_STATE_IDLE && target >= 1 && target <= MAX_FLOORS) {
        fsm->target_floor = target;
    }
}

bool Elevator_FsmTick(ElevatorFsm *fsm, int tick_ms, char *log_output, int log_size) {
    log_output[0] = '\0';
    bool state_changed = false;

    switch (fsm->state) {
        case ELEVATOR_STATE_IDLE:
            if (fsm->current_floor != fsm->target_floor) {
                if (fsm->target_floor > fsm->current_floor) {
                    fsm->state = ELEVATOR_STATE_MOVING_UP;
                    snprintf(log_output, log_size, "[Elevator %d] Call Accepted! Target set to %d. Motor: STARTING UP...\r\n", fsm->elevator_id, fsm->target_floor);
                } else {
                    fsm->state = ELEVATOR_STATE_MOVING_DOWN;
                    snprintf(log_output, log_size, "[Elevator %d] Call Accepted! Target set to %d. Motor: STARTING DOWN...\r\n", fsm->elevator_id, fsm->target_floor);
                }
                state_changed = true;
            }
            break;

        case ELEVATOR_STATE_MOVING_UP:
            fsm->current_floor++;
            snprintf(log_output, log_size, "[Elevator %d] >> Arrived at floor %d.\r\n", fsm->elevator_id, fsm->current_floor);
            state_changed = true;
            
            if (fsm->current_floor == fsm->target_floor) {
                fsm->state = ELEVATOR_STATE_DOOR_OPENING;
            }
            break;

        case ELEVATOR_STATE_MOVING_DOWN:
            fsm->current_floor--;
            snprintf(log_output, log_size, "[Elevator %d] >> Arrived at floor %d.\r\n", fsm->elevator_id, fsm->current_floor);
            state_changed = true;
            
            if (fsm->current_floor == fsm->target_floor) {
                fsm->state = ELEVATOR_STATE_DOOR_OPENING;
            }
            break;

        case ELEVATOR_STATE_DOOR_OPENING:
            snprintf(log_output, log_size, "[Elevator %d] Door: OPENING at floor %d.\r\n", fsm->elevator_id, fsm->current_floor);
            state_changed = true;
            fsm->state = ELEVATOR_STATE_DOOR_OPEN;
            fsm->timer_ms = 0;
            break;

        case ELEVATOR_STATE_DOOR_OPEN:
            fsm->timer_ms += tick_ms;
            if (fsm->timer_ms >= 2000) { // 开门保持 2 秒后关门
                fsm->state = ELEVATOR_STATE_DOOR_CLOSING;
                snprintf(log_output, log_size, "[Elevator %d] Door: CLOSING...\r\n", fsm->elevator_id);
                state_changed = true;
            }
            break;

        case ELEVATOR_STATE_DOOR_CLOSING:
            snprintf(log_output, log_size, "[Elevator %d] Door: CLOSED. Elevator is now IDLE.\r\n", fsm->elevator_id);
            state_changed = true;
            fsm->state = ELEVATOR_STATE_IDLE;
            break;
    }
    return state_changed;
}