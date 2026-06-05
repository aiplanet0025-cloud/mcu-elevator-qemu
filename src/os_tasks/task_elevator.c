#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "elevator_fsm.h"
#include "elevator_event.h"
#include "logger.h"

extern QueueHandle_t xElevator1Queue;
extern QueueHandle_t xElevator2Queue;

// 任务：单个电梯的运行逻辑
void vTaskElevator(void *pvParameters) {
    ElevatorFsm *fsm = (ElevatorFsm *)pvParameters;
    Elevator_FsmInit(fsm, fsm->elevator_id);
    
    ElevatorEvent xReceivedEvent;
    QueueHandle_t elevatorQueue = (fsm->elevator_id == 1) ? xElevator1Queue : xElevator2Queue;
    char log_buf[128];

    for (;;) {
        // 如果电梯处于待机状态，则阻塞等待调度器发往本电梯专属队列的事件。
        // 每部电梯使用独立队列，避免共享队列中某部电梯误取并丢弃另一部电梯的 call 命令。
        if (fsm->state == ELEVATOR_STATE_IDLE) {
            if (xQueueReceive(elevatorQueue, &xReceivedEvent, portMAX_DELAY) == pdTRUE) {
                Elevator_FsmSetTarget(fsm, xReceivedEvent.targetFloor);
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
