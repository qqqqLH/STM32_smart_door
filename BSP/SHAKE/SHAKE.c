#include "SHAKE.h"


uint8_t SHAKE_IsActive(void)
{
  return (HAL_GPIO_ReadPin(SHAKE_DO_GPIO_Port, SHAKE_DO_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
}
