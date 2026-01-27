/**
  ******************************************************************************
  * @file    FLASH.h
  * @author  Driver Generator
  * @brief   STM32F103C8T6 内部 Flash 读写驱动头文件
  ******************************************************************************
  */

#ifndef __FLASH_H
#define __FLASH_H


/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported constants --------------------------------------------------------*/
/* STM32F103C8T6 (Medium Density) Specifics */
#define FLASH_PAGE_SIZE         0x400U      /* 1KB per page */
#define FLASH_START_ADDR        0x08000000U /* Start address of Flash */
#define FLASH_END_ADDR          0x08010000U /* End address (64KB) */

/* Error Codes */
#define FLASH_OK                0
#define FLASH_ERR_ERASE         1
#define FLASH_ERR_WRITE         2
#define FLASH_ERR_ADDR_OUT      3

/* Exported functions ------------------------------------------------------- */

/**
  * @brief  读取指定地址的字 (32-bit)
  * @param  Address: 读取地址
  * @return 读取到的数据
  */
uint32_t Flash_ReadWord(uint32_t Address);

/**
  * @brief  读取指定地址的半字 (16-bit)
  * @param  Address: 读取地址
  * @return 读取到的数据
  */
uint16_t Flash_ReadHalfWord(uint32_t Address);

/**
  * @brief  擦除指定的 Flash 页
  * @param  PageAddress: 该页内的任意地址 (e.g. 0x0800FC00)
  * @return 0: 成功, 其他: 失败代码
  */
uint8_t Flash_ErasePage(uint32_t PageAddress);

/**
  * @brief  在指定地址写入一个字 (32-bit)
  * @note   写入前请确保该地址已被擦除 (值为 0xFFFFFFFF)
  * @param  Address: 写入地址
  * @param  Data: 要写入的数据
  * @return 0: 成功, 其他: 失败代码
  */
uint8_t Flash_WriteWord(uint32_t Address, uint32_t Data);

/**
  * @brief  在指定地址写入一组字 (32-bit Array)
  * @param  StartAddress: 起始写入地址
  * @param  pData: 数据指针
  * @param  Length: 数据长度 (以字为单位, 不是字节)
  * @return 0: 成功, 其他: 失败代码
  */
uint8_t Flash_WriteArray(uint32_t StartAddress, uint32_t *pData, uint16_t Length);

/**
  * @brief  获取地址所在的页码
  * @param  Address: Flash 地址
  * @return 页码 (0 - 63)
  */
uint32_t Flash_GetPage(uint32_t Address);


#endif
