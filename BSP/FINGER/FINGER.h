#ifndef FINGER_H
#define FINGER_H

#include "main.h"
#include <stdint.h>

#define FINGER_ADDR_DEFAULT 0xFFFFFFFFu

typedef enum
{
  FINGER_OK = 0,
  FINGER_FAIL = 1,
  FINGER_TIMEOUT = 2,
  FINGER_NO_FINGER = 3,
  FINGER_NOT_FOUND = 4
} FINGER_Result;

typedef struct
{
  UART_HandleTypeDef *uart;
  uint32_t address;
  uint32_t timeout_ms;
} FINGER_Handle;

void FINGER_Init(FINGER_Handle *handle, UART_HandleTypeDef *uart);
void FINGER_SetAddress(FINGER_Handle *handle, uint32_t address);
void FINGER_SetTimeout(FINGER_Handle *handle, uint32_t timeout_ms);

FINGER_Result FINGER_Identify(FINGER_Handle *handle, uint16_t start_page, uint16_t page_num,
                              uint16_t *page_id, uint16_t *score, uint32_t timeout_ms);
FINGER_Result FINGER_Enroll(FINGER_Handle *handle, uint16_t page_id, uint32_t timeout_ms);
FINGER_Result FINGER_Delete(FINGER_Handle *handle, uint16_t page_id, uint16_t count);
FINGER_Result FINGER_Empty(FINGER_Handle *handle);
FINGER_Result FINGER_ReadIndexTable(FINGER_Handle *handle, uint8_t page, uint8_t *out, uint16_t out_len);

void FINGER_DelayMs(uint32_t ms);

#endif
