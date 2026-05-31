#ifndef ELEVATOR_FSM_H
#define ELEVATOR_FSM_H

typedef enum {
    ELEVATOR_STATE_IDLE,
    ELEVATOR_STATE_MOVING,
    ELEVATOR_STATE_DOOR_OPEN
} FsmState;

typedef struct {
    int current_floor;
    int target_floor;
    FsmState state;
} ElevatorFsm;

void Elevator_FsmInit(ElevatorFsm *fsm);
// 核心状态机迭代函数，返回当前是否还在动作
bool Elevator_FsmTick(ElevatorFsm *fsm, int tick_ms, char *log_output, int log_size);

#endif