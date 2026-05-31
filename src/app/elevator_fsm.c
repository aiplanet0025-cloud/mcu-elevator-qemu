#include "elevator_fsm.h"
#include <stdio.h>

void Elevator_FsmInit(ElevatorFsm *fsm) {
    fsm->current_floor = 1;
    fsm->target_floor = 1;
    fsm->state = ELEVATOR_STATE_IDLE;
}

bool Elevator_FsmTick(ElevatorFsm *fsm, int tick_ms, char *log_output, int log_size) {
    log_output[0] = '\0';
    
    switch (fsm->state) {
        case ELEVATOR_STATE_IDLE:
            if (fsm->current_floor != fsm->target_floor) {
                fsm->state = ELEVATOR_STATE_MOVING;
                snprintf(log_output, log_size, "[FSM] Target changed to %d. Starting Motor...\r\n", fsm->target_floor);
                return true;
            }
            break;

        case ELEVATOR_STATE_MOVING:
            if (fsm->current_floor < fsm->target_floor) {
                fsm->current_floor++;
                snprintf(log_output, log_size, "[FSM] Elevator moving UP. Passing floor %d.\r\n", fsm->current_floor);
            } else if (fsm->current_floor > fsm->target_floor) {
                fsm->current_floor--;
                snprintf(log_output, log_size, "[FSM] Elevator moving DOWN. Passing floor %d.\r\n", fsm->current_floor);
            }
            
            if (fsm->current_floor == fsm->target_floor) {
                fsm->state = ELEVATOR_STATE_DOOR_OPEN;
            }
            return true;

        case ELEVATOR_STATE_DOOR_OPEN:
            snprintf(log_output, log_size, "[FSM] Arrived at %d. Door Open -> Door Close.\r\n", fsm->current_floor);
            fsm->state = ELEVATOR_STATE_IDLE;
            return true;
    }
    return false;
}