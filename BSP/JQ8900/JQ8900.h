#ifndef JQ8900_H
#define JQ8900_H

#include "main.h"
#include <stdint.h>

typedef struct
{
  UART_HandleTypeDef *uart;
  uint32_t timeout_ms;
} JQ8900_Handle;

void JQ8900_Init(JQ8900_Handle *handle, UART_HandleTypeDef *uart);
void JQ8900_SetTimeout(JQ8900_Handle *handle, uint32_t timeout_ms);

void JQ8900_Play(JQ8900_Handle *handle);
void JQ8900_Pause(JQ8900_Handle *handle);
void JQ8900_Stop(JQ8900_Handle *handle);
void JQ8900_Next(JQ8900_Handle *handle);
void JQ8900_Previous(JQ8900_Handle *handle);
void JQ8900_PlayIndex(JQ8900_Handle *handle, uint16_t index);
void JQ8900_SetVolume(JQ8900_Handle *handle, uint8_t volume);

#endif
