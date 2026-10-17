#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
TaskHandle_t xTaskGetCurrentTaskHandle(void);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t timeout);
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *higher);
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount(void);
BaseType_t xTaskNotify(TaskHandle_t task, uint32_t bits, int action);
BaseType_t xTaskNotifyFromISR(TaskHandle_t task, uint32_t bits, int action, BaseType_t *higher);
BaseType_t xTaskNotifyWait(uint32_t entry, uint32_t exit, uint32_t *bits, TickType_t timeout);
#endif
