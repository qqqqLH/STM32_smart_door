#ifndef __SERVO_H
#define __SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tim.h"

#define SERVO_STOP_PULSE_US     1500u
#define SERVO_FORWARD_PULSE_US  2000u
#define SERVO_REVERSE_PULSE_US  1000u

void Servo_Forward(TIM_HandleTypeDef *htim, uint32_t channel);
void Servo_Reverse(TIM_HandleTypeDef *htim, uint32_t channel);
void Servo_Stop(TIM_HandleTypeDef *htim, uint32_t channel);

#ifdef __cplusplus
}
#endif

#endif
