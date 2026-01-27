#ifndef BT_H
#define BT_H

#include "main.h"

void BT_Init(void);
void BT_SendByte(uint8_t byte);
void BT_SendBytes(const uint8_t *data, uint16_t len);
uint16_t BT_Available(void);
int16_t BT_Read(void);
uint16_t BT_ReadBytes(uint8_t *out, uint16_t len);

#endif
