#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "elevator_fsm.h"
#include "elevator_event.h"
#include "dispatcher_event.h"
#include "logger.h"
#include <stdlib.h>

extern QueueHandle_t xDispatcherQueue;
extern QueueHandle_t xFloorQueue; // Reused for sending to specific elevators
extern ElevatorFsm g_elevator_fsm1;
extern ElevatorFsm g_elevator_fsm2;

static int calculate_cost(ElevatorFsm *fsm, int target_floor) {
    if (fsm->state != ELEVATOR_STATE_IDLE) {
        return 1000; // Busy elevators are high cost
    }
    return abs(fsm->current_floor - target_floor);
}

void vTaskDispatcher(void *pvParameters) {
    DispatcherEvent xReceivedEvent;
    
    for (;;) {
        if (xQueueReceive(xDispatcherQueue, &xReceivedEvent, portMAX_DELAY) == pdTRUE) {
            int cost1 = calculate_cost(&g_elevator_fsm1, xReceivedEvent.targetFloor);
            int cost2 = calculate_cost(&g_elevator_fsm2, xReceivedEvent.targetFloor);
            
            ElevatorEvent xAssignEvent;
            xAssignEvent.targetFloor = xReceivedEvent.targetFloor;
            
            if (cost1 <= cost2) {
                xAssignEvent.elevator_id = 1;
                Logger_Info("[Dispatcher] Assigning floor %d to Elevator 1\r\n", xReceivedEvent.targetFloor);
            } else {
                xAssignEvent.elevator_id = 2;
                Logger_Info("[Dispatcher] Assigning floor %d to Elevator 2\r\n", xReceivedEvent.targetFloor);
            }
            
            xQueueSend(xFloorQueue, &xAssignEvent, 0);
        }
    }
}
