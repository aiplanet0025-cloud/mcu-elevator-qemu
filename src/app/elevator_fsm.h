#ifndef ELEVATOR_FSM_H
#define ELEVATOR_FSM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ELEVATOR_STATE_IDLE,
    ELEVATOR_STATE_MOVING_UP,
    ELEVATOR_STATE_MOVING_DOWN,
    ELEVATOR_STATE_DOOR_OPENING,
    ELEVATOR_STATE_DOOR_OPEN,
    ELEVATOR_STATE_DOOR_CLOSING
} FsmState;

typedef struct {
    int current_floor;
    int target_floor;
    FsmState state;
    uint32_t timer_ms; // 模拟开门等待时间
} ElevatorFsm;

void Elevator_FsmInit(ElevatorFsm *fsm);
void Elevator_FsmSetTarget(ElevatorFsm *fsm, int target);
// 迭代状态机：返回 true 代表状态发生改变，并将打印日志写入 log_output
bool Elevator_FsmTick(ElevatorFsm *fsm, int tick_ms, char *log_output, int log_size);

#endif