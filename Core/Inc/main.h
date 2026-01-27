/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define OLED_SDA_Pin GPIO_PIN_14
#define OLED_SDA_GPIO_Port GPIOC
#define OLED_SCL_Pin GPIO_PIN_15
#define OLED_SCL_GPIO_Port GPIOC
#define BEEP_Pin GPIO_PIN_0
#define BEEP_GPIO_Port GPIOA
#define ROW1_Pin GPIO_PIN_4
#define ROW1_GPIO_Port GPIOA
#define ROW2_Pin GPIO_PIN_5
#define ROW2_GPIO_Port GPIOA
#define ROW3_Pin GPIO_PIN_6
#define ROW3_GPIO_Port GPIOA
#define ROW4_Pin GPIO_PIN_7
#define ROW4_GPIO_Port GPIOA
#define SHAKE_DO_Pin GPIO_PIN_0
#define SHAKE_DO_GPIO_Port GPIOB
#define FINGER_IRQ_Pin GPIO_PIN_1
#define FINGER_IRQ_GPIO_Port GPIOB
#define FINGER_IRQ_EXTI_IRQn EXTI1_IRQn
#define COL1_Pin GPIO_PIN_12
#define COL1_GPIO_Port GPIOB
#define COL2_Pin GPIO_PIN_13
#define COL2_GPIO_Port GPIOB
#define COL3_Pin GPIO_PIN_14
#define COL3_GPIO_Port GPIOB
#define COL4_Pin GPIO_PIN_15
#define COL4_GPIO_Port GPIOB
#define RFID_RST_Pin GPIO_PIN_11
#define RFID_RST_GPIO_Port GPIOA
#define RFID_MISO_Pin GPIO_PIN_12
#define RFID_MISO_GPIO_Port GPIOA
#define RFID_MOSI_Pin GPIO_PIN_15
#define RFID_MOSI_GPIO_Port GPIOA
#define RFID_SCK_Pin GPIO_PIN_3
#define RFID_SCK_GPIO_Port GPIOB
#define RFID_SDA_Pin GPIO_PIN_4
#define RFID_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
