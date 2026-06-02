#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "elevator_fsm.h"
#include "elevator_event.h"
#include "logger.h"

extern QueueHandle_t xFloorQueue;

// 任务：单个电梯的运行逻辑
void vTaskElevator(void *pvParameters) {
    ElevatorFsm *fsm = (ElevatorFsm *)pvParameters;
    Elevator_FsmInit(fsm, fsm->elevator_id);
    
    ElevatorEvent xReceivedEvent;
    char log_buf[128];

    for (;;) {
        // 如果电梯处于待机状态，则挂起任务，阻塞等待特定给该电梯的事件
        // (简单实现：假设事件只发给它，或者所有电梯共享队列)
        // 实际应用需要更复杂的队列调度，这里简化为所有任务从同一队列读取，检查 ID
        
        if (fsm->state == ELEVATOR_STATE_IDLE) {
            if (xQueueReceive(xFloorQueue, &xReceivedEvent, portMAX_DELAY) == pdTRUE) {
                if (xReceivedEvent.elevator_id == fsm->elevator_id) {
                    Elevator_FsmSetTarget(fsm, xReceivedEvent.targetFloor);
                } else {
                    // 不是发给我的事件，放回队列（注意：实际应该有独立的队列或调度器）
                    // 简化版：这里简单忽略，因为是广播模型
                }
            }
        }

        int tick_time_ms = 1000;
        if (fsm->state == ELEVATOR_STATE_DOOR_OPEN) {
            tick_time_ms = 500;
        }

        // 迭代状态机
        if (Elevator_FsmTick(fsm, tick_time_ms, log_buf, sizeof(log_buf))) {
            if (log_buf[0] != '\0') {
                Logger_Info(log_buf); 
            }
        }

        vTaskDelay(pdMS_TO_TICKS(tick_time_ms));
    }
}
