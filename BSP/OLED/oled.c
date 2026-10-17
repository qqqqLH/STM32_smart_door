#include "oled.h"
#include "oledfont.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

/* PC14/PC15 cannot use hardware I2C. TIM1_UP -> DMA1 channel 5 writes only
 * those two GPIOC BSRR bits. Each I2C bit has setup/high/hold phases of 2 us
 * (about 167 kHz). The display must not stretch SCL; ACK is released, as in
 * the original driver. Do not enable USART1_RX/TIM4_CH3 DMA on channel 5.
 */
#define OLED_BLOCK_BYTES 16u
#define OLED_WAVE_WORDS 640u
#define OLED_PHASE_US 2u
#define OLED_NOTIFY_DRAW 1u
#define OLED_NOTIFY_DMA  2u
#define OLED_DMA_TIMEOUT_MS 20u
#define OLED_COLUMN_OFFSET 2u

static uint8_t s_frame[8][128];
static uint8_t s_dirty[8];
static uint32_t s_wave[OLED_WAVE_WORDS];
static uint16_t s_wave_length;
static uint8_t s_cursor_x, s_cursor_page;
static SemaphoreHandle_t s_frame_mutex;
static TaskHandle_t s_display_task;
static volatile uint8_t s_ready, s_dma_busy, s_dma_failed;
OLED_Diagnostics g_oled_diag;

static void OLED_I2C_Delay(void)
{
  for (volatile uint32_t i = 0; i < 30u; i++) { __NOP(); }
}

void delay_ms(unsigned int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
void Delay_50ms(unsigned int n) { delay_ms(n * 50u); }
void Delay_1ms(unsigned int n) { delay_ms(n); }
void OLED_GPIO_Init(void) { OLED_SCLK_Set(); OLED_SDIN_Set(); }

/* Only panel initialization uses CPU-driven I2C. Rendering never calls it. */
void IIC_Start(void)
{
  OLED_SDIN_Set(); OLED_SCLK_Set(); OLED_I2C_Delay();
  OLED_SDIN_Clr(); OLED_I2C_Delay(); OLED_SCLK_Clr();
}
void IIC_Stop(void)
{
  OLED_SDIN_Clr(); OLED_I2C_Delay(); OLED_SCLK_Set();
  OLED_I2C_Delay(); OLED_SDIN_Set(); OLED_I2C_Delay();
}
void IIC_Wait_Ack(void)
{
  OLED_SDIN_Set(); OLED_I2C_Delay(); OLED_SCLK_Set();
  OLED_I2C_Delay(); OLED_SCLK_Clr(); OLED_I2C_Delay();
}
void Write_IIC_Byte(unsigned char value)
{
  for (uint8_t bit = 0u; bit < 8u; bit++)
  {
    if ((value & 0x80u) != 0u) { OLED_SDIN_Set(); } else { OLED_SDIN_Clr(); }
    OLED_I2C_Delay(); OLED_SCLK_Set(); OLED_I2C_Delay();
    OLED_SCLK_Clr(); OLED_I2C_Delay(); value <<= 1;
  }
}
void Write_IIC_Command(unsigned char command)
{
  IIC_Start(); Write_IIC_Byte(0x78u); IIC_Wait_Ack();
  Write_IIC_Byte(0x00u); IIC_Wait_Ack();
  Write_IIC_Byte(command); IIC_Wait_Ack(); IIC_Stop();
}

static void PutByte(uint8_t x, uint8_t page, uint8_t value)
{
  if (x < 128u && page < 8u && s_frame[page][x] != value)
  {
    s_frame[page][x] = value;
    s_dirty[page] |= (uint8_t)(1u << (x / OLED_BLOCK_BYTES));
  }
}

void Write_IIC_Data(unsigned char value)
{
  PutByte(s_cursor_x, s_cursor_page, value);
  if (s_cursor_x < 128u) { s_cursor_x++; }
}
void OLED_WR_Byte(unsigned value, unsigned command)
{
  if (command == OLED_DATA) { Write_IIC_Data((uint8_t)value); }
  else { Write_IIC_Command((uint8_t)value); }
}
void OLED_Set_Pos(unsigned char x, unsigned char page)
{
  s_cursor_x = x; s_cursor_page = page;
}
void fill_picture(unsigned char value)
{
  for (uint8_t page = 0u; page < 8u; page++)
    for (uint8_t x = 0u; x < 128u; x++) { PutByte(x, page, value); }
}
void OLED_Clear(void) { fill_picture(0u); }
void OLED_On(void) { fill_picture(0xFFu); }
void OLED_Display_On(void)
{
  Write_IIC_Command(0x8Du); Write_IIC_Command(0x14u); Write_IIC_Command(0xAFu);
}
void OLED_Display_Off(void)
{
  Write_IIC_Command(0x8Du); Write_IIC_Command(0x10u); Write_IIC_Command(0xAEu);
}
void OLED_ShowChar(u8 x, u8 page, u8 chr, u8 size)
{
  if (chr < ' ' || chr > '~' || x >= 128u || page >= 8u) { return; }
  uint16_t c = (uint16_t)(chr - ' ');
  if (size != 16u && c >= sizeof(F6x8) / sizeof(F6x8[0])) { c = '?' - ' '; }
  for (uint8_t i = 0u; i < 8u && (uint16_t)x + i < 128u; i++)
  {
    if (size == 16u)
    {
      PutByte((uint8_t)(x + i), page, F8X16[c * 16u + i]);
      PutByte((uint8_t)(x + i), (uint8_t)(page + 1u), F8X16[c * 16u + i + 8u]);
    }
    else
    {
      PutByte((uint8_t)(x + i), page, (i < 6u) ? F6x8[c][i] : 0u);
    }
  }
}
u32 oled_pow(u8 m, u8 n)
{
  u32 result = 1u;
  while (n-- != 0u) { result *= m; }
  return result;
}
void OLED_ShowNum(u8 x, u8 page, u32 num, u8 len, u8 size)
{
  uint8_t shown = 0u;
  if (len > 10u) { return; }
  for (uint8_t i = 0u; i < len; i++)
  {
    uint8_t digit = (uint8_t)((num / oled_pow(10u, (uint8_t)(len - i - 1u))) % 10u);
    if (digit != 0u || i == len - 1u) { shown = 1u; }
    OLED_ShowChar((u8)(x + (size / 2u) * i), page, shown ? (u8)('0' + digit) : ' ', size);
  }
}
void OLED_ShowString(u8 x, u8 page, u8 *text, u8 size)
{
  if (text == NULL) { return; }
  while (*text != 0u && page < 8u)
  {
    OLED_ShowChar(x, page, *text++, size);
    if (x >= 120u) { x = 0u; page += 2u; } else { x += 8u; }
  }
}
void OLED_ShowCHinese(u8 x, u8 page, u8 no)
{
  if ((uint16_t)no * 2u + 1u >= sizeof(Hzk) / sizeof(Hzk[0])) { return; }
  for (uint8_t i = 0u; i < 16u && (uint16_t)x + i < 128u; i++)
  {
    PutByte((uint8_t)(x + i), page, Hzk[2u * no][i]);
    PutByte((uint8_t)(x + i), (uint8_t)(page + 1u), Hzk[2u * no + 1u][i]);
  }
}
void OLED_DrawBMP(u8 x0, u8 y0, u8 x1, u8 y1, unsigned char bitmap[])
{
  uint16_t index = 0u;
  if (bitmap == NULL) { return; }
  for (uint16_t page = y0; page < y1; page++)
    for (uint16_t x = x0; x < x1; x++) { PutByte((u8)x, (u8)page, bitmap[index++]); }
}
void OLED_DrawPoint(u8 x, u8 y, u8 on)
{
  if (x < 128u && y < 64u)
  {
    uint8_t mask = (uint8_t)(1u << (y % 8u));
    uint8_t value = s_frame[y / 8u][x];
    PutByte(x, (uint8_t)(y / 8u), on ? (uint8_t)(value | mask) : (uint8_t)(value & ~mask));
  }
}
void OLED_Fill(u8 x1, u8 y1, u8 x2, u8 y2, u8 on)
{
  for (uint16_t y = y1; y <= y2 && y < 64u; y++)
    for (uint16_t x = x1; x <= x2 && x < 128u; x++) { OLED_DrawPoint((u8)x, (u8)y, on); }
}

uint8_t OLED_IsReady(void) { return s_ready; }
void OLED_BeginFrame(void) { (void)xSemaphoreTake(s_frame_mutex, portMAX_DELAY); }
void OLED_EndFrame(void)
{
  (void)xSemaphoreGive(s_frame_mutex);
  (void)xTaskNotify(s_display_task, OLED_NOTIFY_DRAW, eSetBits);
}

/* Three phases avoid changing SDA on the SCL falling edge. BSRR leaves every
 * other GPIOC pin untouched; the immutable waveform is the DMA snapshot. */
static void WaveLevel(uint8_t scl, uint8_t sda)
{
  uint32_t set = (scl ? OLED_SCL_PIN : 0u) | (sda ? OLED_SDA_PIN : 0u);
  uint32_t reset = (scl ? 0u : OLED_SCL_PIN) | (sda ? 0u : OLED_SDA_PIN);
  configASSERT(s_wave_length < OLED_WAVE_WORDS);
  s_wave[s_wave_length++] = set | (reset << 16);
}
static void WaveStart(void) { WaveLevel(1u, 1u); WaveLevel(1u, 0u); WaveLevel(0u, 0u); }
static void WaveStop(void)
{
  WaveLevel(0u, 0u); WaveLevel(1u, 0u); WaveLevel(1u, 1u); WaveLevel(1u, 1u);
}
static void WaveByte(uint8_t byte)
{
  for (uint8_t bit = 0u; bit < 9u; bit++)
  {
    uint8_t data = (bit == 8u || (byte & 0x80u) != 0u) ? 1u : 0u;
    WaveLevel(0u, data); WaveLevel(1u, data); WaveLevel(0u, data);
    byte <<= 1;
  }
}
static void BuildWave(uint8_t page, uint8_t x, const uint8_t *data)
{
  uint8_t column = (uint8_t)(x + OLED_COLUMN_OFFSET);
  s_wave_length = 0u;
  WaveStart(); WaveByte(0x78u); WaveByte(0x00u);
  WaveByte((uint8_t)(0xB0u + page));
  WaveByte((uint8_t)(0x10u | (column >> 4))); WaveByte((uint8_t)(column & 0x0Fu));
  WaveStop();
  WaveStart(); WaveByte(0x78u); WaveByte(0x40u);
  for (uint8_t i = 0u; i < OLED_BLOCK_BYTES; i++) { WaveByte(data[i]); }
  WaveStop();
}
static void StopDMA(void)
{
  TIM1->DIER &= ~TIM_DIER_UDE;
  TIM1->CR1 &= ~TIM_CR1_CEN;
  DMA1_Channel5->CCR &= ~DMA_CCR_EN;
  DMA1->IFCR = DMA_IFCR_CGIF5;
  GPIOC->BSRR = OLED_SCL_PIN | OLED_SDA_PIN;
  s_dma_busy = 0u;
}
void OLED_DMA_IRQHandler(void)
{
  uint32_t status = DMA1->ISR;
  if ((status & (DMA_ISR_TCIF5 | DMA_ISR_TEIF5)) != 0u)
  {
    BaseType_t higher = pdFALSE;
    if ((status & DMA_ISR_TEIF5) != 0u) { s_dma_failed = 1u; }
    StopDMA();
    if (s_display_task != NULL)
    {
      (void)xTaskNotifyFromISR(s_display_task, OLED_NOTIFY_DMA, eSetBits, &higher);
      portYIELD_FROM_ISR(higher);
    }
  }
}
static uint8_t SendWave(void)
{
  uint32_t flags;
  TickType_t start = xTaskGetTickCount();
  s_dma_failed = 0u;
  s_dma_busy = 1u;
  DMA1->IFCR = DMA_IFCR_CGIF5;
  DMA1_Channel5->CPAR = (uint32_t)&GPIOC->BSRR;
  DMA1_Channel5->CMAR = (uint32_t)s_wave;
  DMA1_Channel5->CNDTR = s_wave_length;
  DMA1_Channel5->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_PSIZE_1 |
                       DMA_CCR_MSIZE_1 | DMA_CCR_TCIE | DMA_CCR_TEIE;
  TIM1->CNT = 0u;
  TIM1->SR = 0u;
  __DMB();
  DMA1_Channel5->CCR |= DMA_CCR_EN;
  TIM1->DIER |= TIM_DIER_UDE;
  TIM1->CR1 |= TIM_CR1_CEN;
  while (s_dma_busy != 0u)
  {
    TickType_t elapsed = xTaskGetTickCount() - start;
    if (elapsed >= pdMS_TO_TICKS(OLED_DMA_TIMEOUT_MS))
    {
      taskENTER_CRITICAL();
      StopDMA(); s_dma_failed = 1u;
      taskEXIT_CRITICAL();
      break;
    }
    (void)xTaskNotifyWait(0u, UINT32_MAX, &flags, pdMS_TO_TICKS(OLED_DMA_TIMEOUT_MS) - elapsed);
  }
  if (s_dma_failed != 0u) { g_oled_diag.dma_errors++; return 0u; }
  g_oled_diag.blocks_sent++;
  g_oled_diag.data_bytes += OLED_BLOCK_BYTES;
  return 1u;
}

static void OLED_PanelInit(void)
{
  OLED_GPIO_Init();
  delay_ms(50);

  Write_IIC_Command(0xAE);//--display off
  Write_IIC_Command(0x00);//---set low column address
  Write_IIC_Command(0x10);//---set high column address
  Write_IIC_Command(0x40);//--set start line address
  Write_IIC_Command(0xB0);//--set page address
  Write_IIC_Command(0x81); // contract control
  Write_IIC_Command(0xFF);//--128
  Write_IIC_Command(0xA1);//set segment remap
  Write_IIC_Command(0xA6);//--normal / reverse
  Write_IIC_Command(0xA8);//--set multiplex ratio(1 to 64)
  Write_IIC_Command(0x3F);//--1/32 duty
  Write_IIC_Command(0xC8);//Com scan direction
  Write_IIC_Command(0xD3);//-set display offset
  Write_IIC_Command(0x00);//

  Write_IIC_Command(0xD5);//set osc division
  Write_IIC_Command(0x80);//

  Write_IIC_Command(0xD8);//set area color mode off
  Write_IIC_Command(0x05);//

  Write_IIC_Command(0xD9);//Set Pre-Charge Period
  Write_IIC_Command(0xF1);//

  Write_IIC_Command(0xDA);//set com pin configuartion
  Write_IIC_Command(0x12);//

  Write_IIC_Command(0xDB);//set Vcomh
  Write_IIC_Command(0x30);//

  Write_IIC_Command(0x8D);//set charge pump enable
  Write_IIC_Command(0x14);//

  Write_IIC_Command(0xAF);//--turn on oled panel
}


void OLED_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t clock = HAL_RCC_GetPCLK2Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE2) != 0u) { clock *= 2u; }
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();
  gpio.Pin = OLED_SCL_PIN | OLED_SDA_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW; /* PC13..15: use the 2 MHz output setting. */
  HAL_GPIO_Init(GPIOC, &gpio);
  TIM1->CR1 = 0u; TIM1->DIER = 0u;
  TIM1->PSC = clock / 1000000u - 1u;
  TIM1->ARR = OLED_PHASE_US - 1u;
  TIM1->RCR = 0u; TIM1->EGR = TIM_EGR_UG; TIM1->SR = 0u;
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 6u, 0u);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  memset(s_frame, 0, sizeof(s_frame));
  memset(s_dirty, 0xFF, sizeof(s_dirty));
  OLED_PanelInit();
}

void OLED_Task(void *argument)
{
  uint32_t flags;
  (void)argument;
  s_display_task = xTaskGetCurrentTaskHandle();
  s_frame_mutex = xSemaphoreCreateMutex();
  configASSERT(s_frame_mutex != NULL);
  OLED_Init();
  s_ready = 1u;
  for (;;)
  {
    uint8_t found = 0u, page = 0u, block = 0u;
    uint8_t snapshot[OLED_BLOCK_BYTES];
    (void)xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
    for (page = 0u; page < 8u; page++)
    {
      for (block = 0u; block < 8u; block++)
      {
        if ((s_dirty[page] & (1u << block)) != 0u)
        {
          memcpy(snapshot, &s_frame[page][block * OLED_BLOCK_BYTES], sizeof(snapshot));
          s_dirty[page] &= (uint8_t)~(1u << block);
          found = 1u;
          break;
        }
      }
      if (found != 0u) { break; }
    }
    (void)xSemaphoreGive(s_frame_mutex);
    if (found == 0u)
    {
      (void)xTaskNotifyWait(0u, UINT32_MAX, &flags, portMAX_DELAY);
      continue;
    }
    BuildWave(page, (uint8_t)(block * OLED_BLOCK_BYTES), snapshot);
    if (SendWave() == 0u)
    {
      /* Restore dirty state. Edits made while DMA was active already set it. */
      (void)xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
      s_dirty[page] |= (uint8_t)(1u << block);
      (void)xSemaphoreGive(s_frame_mutex);
      /* Clock out a partially transmitted byte and restore the bus. */
      OLED_SDIN_Set();
      for (uint8_t i = 0u; i < 9u; i++)
      {
        OLED_SCLK_Clr(); OLED_I2C_Delay(); OLED_SCLK_Set(); OLED_I2C_Delay();
      }
      OLED_SCLK_Clr(); IIC_Stop();
      vTaskDelay(pdMS_TO_TICKS(10u));
    }
  }
}
