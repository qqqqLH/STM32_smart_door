#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
#include <stddef.h>
#define __weak __attribute__((weak))
#define __NOP() ((void)0)
#define __DMB() ((void)0)
typedef struct { volatile uint32_t CCR, CNDTR, CPAR, CMAR; } DMA_Channel_TypeDef;
typedef struct { DMA_Channel_TypeDef *Instance; } DMA_HandleTypeDef;
typedef struct { void *Instance; DMA_HandleTypeDef *hdmarx, *hdmatx; volatile uint32_t gState, RxState, ErrorCode; } UART_HandleTypeDef;
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
#define HAL_UART_STATE_READY 0u
#define HAL_UART_STATE_BUSY_RX 1u
#define HAL_UART_ERROR_ORE 8u
#define __HAL_DMA_GET_COUNTER(d) ((d)->Instance->CNDTR)
#define __HAL_UART_CLEAR_OREFLAG(u) ((u)->ErrorCode = 0u)
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *u, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *u, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u, uint8_t *data, uint16_t size, uint32_t timeout);
typedef struct { volatile uint32_t BSRR; } GPIO_TypeDef;
typedef struct { volatile uint32_t CR1, DIER, CNT, SR, PSC, ARR, RCR, EGR; } TIM_TypeDef;
typedef struct { volatile uint32_t ISR, IFCR; } DMA_TypeDef;
typedef struct { volatile uint32_t CFGR; } RCC_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
extern GPIO_TypeDef test_gpio;
extern TIM_TypeDef test_tim;
extern DMA_TypeDef test_dma;
extern DMA_Channel_TypeDef test_channel;
extern RCC_TypeDef test_rcc;
#define GPIOC (&test_gpio)
#define TIM1 (&test_tim)
#define DMA1 (&test_dma)
#define DMA1_Channel5 (&test_channel)
#define RCC (&test_rcc)
#define GPIO_PIN_14 (1u << 14)
#define GPIO_PIN_15 (1u << 15)
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define GPIO_MODE_OUTPUT_OD 1
#define GPIO_PULLUP 1
#define GPIO_SPEED_FREQ_LOW 1
#define TIM_DIER_UDE (1u << 8)
#define TIM_CR1_CEN 1u
#define TIM_EGR_UG 1u
#define RCC_CFGR_PPRE2 (7u << 11)
#define DMA_CCR_EN 1u
#define DMA_CCR_TCIE 2u
#define DMA_CCR_TEIE 8u
#define DMA_CCR_DIR 16u
#define DMA_CCR_MINC 128u
#define DMA_CCR_PSIZE_1 512u
#define DMA_CCR_MSIZE_1 2048u
#define DMA_ISR_TCIF5 (1u << 17)
#define DMA_ISR_TEIF5 (1u << 19)
#define DMA_IFCR_CGIF5 (1u << 16)
#define DMA1_Channel5_IRQn 15u
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_DMA1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM1_CLK_ENABLE() ((void)0)
static inline uint32_t HAL_RCC_GetPCLK2Freq(void) { return 72000000u; }
static inline void HAL_GPIO_Init(GPIO_TypeDef *g, GPIO_InitTypeDef *i) { (void)g; (void)i; }
static inline void HAL_GPIO_WritePin(GPIO_TypeDef *g, uint32_t p, uint32_t v) { g->BSRR = v ? p : p << 16; }
static inline void HAL_NVIC_SetPriority(uint32_t n, uint32_t p, uint32_t s) { (void)n; (void)p; (void)s; }
static inline void HAL_NVIC_EnableIRQ(uint32_t n) { (void)n; }
#endif
