/*
 * RC522 software SPI driver (GPIO bit-bang).
 * Pin mapping is defined below.
 */

#ifndef _RC522_H
#define _RC522_H

#include "main.h"
#include <stdint.h>

#ifndef u8
#define u8 uint8_t
#endif

#ifndef u16
#define u16 uint16_t
#endif

#ifndef u32
#define u32 uint32_t
#endif

#define GPIO_CS_Pin         GPIO_PIN_4
#define GPIO_CS_Port        GPIOB

#define GPIO_SCK_Pin        GPIO_PIN_3
#define GPIO_SCK_Port       GPIOB

#define GPIO_MOSI_Pin       GPIO_PIN_15
#define GPIO_MOSI_Port      GPIOA

#define GPIO_RST_Pin        GPIO_PIN_11
#define GPIO_RST_Port       GPIOA

#define GPIO_MISO_Pin       GPIO_PIN_12
#define GPIO_MISO_Port      GPIOA

#define   RC522_CS_Enable()         HAL_GPIO_WritePin(GPIO_CS_Port, GPIO_CS_Pin, GPIO_PIN_RESET)
#define   RC522_CS_Disable()        HAL_GPIO_WritePin(GPIO_CS_Port, GPIO_CS_Pin, GPIO_PIN_SET)

#define   RC522_Reset_Enable()      HAL_GPIO_WritePin(GPIO_RST_Port, GPIO_RST_Pin, GPIO_PIN_RESET)
#define   RC522_Reset_Disable()     HAL_GPIO_WritePin(GPIO_RST_Port, GPIO_RST_Pin, GPIO_PIN_SET)

#define   RC522_SCK_0()             HAL_GPIO_WritePin(GPIO_SCK_Port, GPIO_SCK_Pin, GPIO_PIN_RESET)
#define   RC522_SCK_1()             HAL_GPIO_WritePin(GPIO_SCK_Port, GPIO_SCK_Pin, GPIO_PIN_SET)

#define   RC522_MOSI_0()            HAL_GPIO_WritePin(GPIO_MOSI_Port, GPIO_MOSI_Pin, GPIO_PIN_RESET)
#define   RC522_MOSI_1()            HAL_GPIO_WritePin(GPIO_MOSI_Port, GPIO_MOSI_Pin, GPIO_PIN_SET)

#define   RC522_MISO_GET()          (HAL_GPIO_ReadPin(GPIO_MISO_Port, GPIO_MISO_Pin) == GPIO_PIN_SET)

#define PCD_IDLE              0x00
#define PCD_AUTHENT           0x0E
#define PCD_RECEIVE           0x08
#define PCD_TRANSMIT          0x04
#define PCD_TRANSCEIVE        0x0C
#define PCD_RESETPHASE        0x0F
#define PCD_CALCCRC           0x03

#define PICC_REQIDL           0x26
#define PICC_REQALL           0x52
#define PICC_ANTICOLL1        0x93
#define PICC_ANTICOLL2        0x95
#define PICC_AUTHENT1A        0x60
#define PICC_AUTHENT1B        0x61
#define PICC_READ             0x30
#define PICC_WRITE            0xA0
#define PICC_DECREMENT        0xC0
#define PICC_INCREMENT        0xC1
#define PICC_RESTORE          0xC2
#define PICC_TRANSFER         0xB0
#define PICC_HALT             0x50

#define DEF_FIFO_LENGTH       64
#define MAXRLEN  18

#define     RFU00                 0x00
#define     CommandReg            0x01
#define     ComIEnReg             0x02
#define     DivlEnReg             0x03
#define     ComIrqReg             0x04
#define     DivIrqReg             0x05
#define     ErrorReg              0x06
#define     Status1Reg            0x07
#define     Status2Reg            0x08
#define     FIFODataReg           0x09
#define     FIFOLevelReg          0x0A
#define     WaterLevelReg         0x0B
#define     ControlReg            0x0C
#define     BitFramingReg         0x0D
#define     CollReg               0x0E
#define     RFU0F                 0x0F

#define     RFU10                 0x10
#define     ModeReg               0x11
#define     TxModeReg             0x12
#define     RxModeReg             0x13
#define     TxControlReg          0x14
#define     TxAutoReg             0x15
#define     TxSelReg              0x16
#define     RxSelReg              0x17
#define     RxThresholdReg        0x18
#define     DemodReg              0x19
#define     RFU1A                 0x1A
#define     RFU1B                 0x1B
#define     MifareReg             0x1C
#define     RFU1D                 0x1D
#define     RFU1E                 0x1E
#define     SerialSpeedReg        0x1F

#define     RFU20                 0x20
#define     CRCResultRegM         0x21
#define     CRCResultRegL         0x22
#define     RFU23                 0x23
#define     ModWidthReg           0x24
#define     RFU25                 0x25
#define     RFCfgReg              0x26
#define     GsNReg                0x27
#define     CWGsCfgReg            0x28
#define     ModGsCfgReg           0x29
#define     TModeReg              0x2A
#define     TPrescalerReg         0x2B
#define     TReloadRegH           0x2C
#define     TReloadRegL           0x2D
#define     TCounterValueRegH     0x2E
#define     TCounterValueRegL     0x2F

#define     RFU30                 0x30
#define     TestSel1Reg           0x31
#define     TestSel2Reg           0x32
#define     TestPinEnReg          0x33
#define     TestPinValueReg       0x34
#define     TestBusReg            0x35
#define     AutoTestReg           0x36
#define     VersionReg            0x37
#define     AnalogTestReg         0x38
#define     TestDAC1Reg           0x39
#define     TestDAC2Reg           0x3A
#define     TestADCReg            0x3B
#define     RFU3C                 0x3C
#define     RFU3D                 0x3D
#define     RFU3E                 0x3E
#define     RFU3F                                          0x3F

#define         MI_OK                 0x26
#define         MI_NOTAGERR           0xcc
#define         MI_ERR                0xbb

void RC522_Init(void);

void RC522_SPI_SendByte( uint8_t byte );
uint8_t RC522_SPI_ReadByte( void );
uint8_t RC522_Read_Register( uint8_t Address );
void RC522_Write_Register( uint8_t Address, uint8_t data );
void RC522_SetBit_Register( uint8_t Address, uint8_t mask );
void RC522_ClearBit_Register( uint8_t Address, uint8_t mask );

void RC522_Antenna_On( void );
void RC522_Antenna_Off( void );
void RC522_Rese( void );
void RC522_Config_Type( char Type );

char PcdComMF522 ( uint8_t ucCommand, uint8_t * pInData, uint8_t ucInLenByte, uint8_t * pOutData, uint32_t * pOutLenBit );
char PcdRequest ( uint8_t ucReq_code, uint8_t * pTagType );
char PcdAnticoll ( uint8_t * pSnr );
void CalulateCRC ( uint8_t * pIndata, u8 ucLen, uint8_t * pOutData );
char PcdSelect ( uint8_t * pSnr );
char PcdAuthState ( uint8_t ucAuth_mode, uint8_t ucAddr, uint8_t * pKey, uint8_t * pSnr );
char PcdWrite ( uint8_t ucAddr, uint8_t * pData );
char PcdRead ( uint8_t ucAddr, uint8_t * pData );
char PcdHalt( void );

#endif

