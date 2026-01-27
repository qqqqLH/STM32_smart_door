//定时器产生20ms的PWM波
//0.5ms全速正传
//1.5ms停止
//2.5ms全速反转

#include "servo.h"

void Servo_Forward(TIM_HandleTypeDef *htim, uint32_t channel)
{
  if (htim == NULL)
  {
    return;
  }

  HAL_TIM_PWM_Start(htim, channel);
  __HAL_TIM_SET_COMPARE(htim, channel, SERVO_FORWARD_PULSE_US);
}

void Servo_Reverse(TIM_HandleTypeDef *htim, uint32_t channel)
{
  if (htim == NULL)
  {
    return;
  }

  HAL_TIM_PWM_Start(htim, channel);
  __HAL_TIM_SET_COMPARE(htim, channel, SERVO_REVERSE_PULSE_US);
}

void Servo_Stop(TIM_HandleTypeDef *htim, uint32_t channel)
{
  if (htim == NULL)
  {
    return;
  }

  HAL_TIM_PWM_Start(htim, channel);
  __HAL_TIM_SET_COMPARE(htim, channel, SERVO_STOP_PULSE_US);
}
