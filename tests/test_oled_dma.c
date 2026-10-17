/* Decode the actual DMA waveform and test the production framebuffer. */
#include <stdio.h>
#include <string.h>
#include "../BSP/OLED/oled.c"
GPIO_TypeDef test_gpio;
TIM_TypeDef test_tim;
DMA_TypeDef test_dma;
DMA_Channel_TypeDef test_channel;
RCC_TypeDef test_rcc;
static uint32_t now;
static int dma_result;
uint32_t HAL_GetTick(void) { return now; }
TickType_t xTaskGetTickCount(void) { return now; }
void HAL_Delay(uint32_t ms) { now += ms; }
void vTaskDelay(TickType_t ms) { now += ms; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)1; }
BaseType_t xTaskNotify(TaskHandle_t t, uint32_t b, int a) { (void)t; (void)b; (void)a; return pdTRUE; }
BaseType_t xTaskNotifyFromISR(TaskHandle_t t, uint32_t b, int a, BaseType_t *h)
{ (void)h; return xTaskNotify(t, b, a); }
BaseType_t xTaskNotifyWait(uint32_t e, uint32_t x, uint32_t *bits, TickType_t timeout)
{
  (void)e; (void)x; *bits = 0;
  if (dma_result)
  {
    now += 2;
    test_dma.ISR = dma_result == 1 ? DMA_ISR_TCIF5 : DMA_ISR_TEIF5;
    OLED_DMA_IRQHandler(); test_dma.ISR = 0; return pdTRUE;
  }
  now += timeout; return pdFALSE;
}
static void decode(uint8_t bytes[2][32], uint8_t lengths[2])
{
  uint32_t pins = OLED_SDA_PIN | OLED_SCL_PIN;
  int transaction = -1, active = 0, pending = 0, bit = 0;
  uint8_t byte = 0;
  memset(lengths, 0, 2);
  for (uint16_t i = 0; i < s_wave_length; i++)
  {
    uint32_t word = s_wave[i], mask = OLED_SDA_PIN | OLED_SCL_PIN;
    assert((word & ~(mask | (mask << 16))) == 0); /* no collateral GPIO writes */
    assert(((word & 0xFFFFu) & (word >> 16)) == 0);
    uint32_t next = (pins | (word & 0xFFFFu)) & ~(word >> 16);
    int scl = !!(pins & OLED_SCL_PIN), sda = !!(pins & OLED_SDA_PIN);
    int nscl = !!(next & OLED_SCL_PIN), nsda = !!(next & OLED_SDA_PIN);
    if (scl && nscl && sda != nsda)
    {
      if (!nsda)
      {
        assert(!active); active = 1; pending = bit = 0; transaction++;
        assert(transaction < 2);
      }
      else { assert(active && bit == 0); active = pending = 0; }
    }
    else if (!scl && nscl && active) { pending = 1; }
    else if (scl && !nscl && active && pending)
    {
      pending = 0;
      if (bit < 8) { byte = (uint8_t)((byte << 1) | sda); }
      else
      {
        assert(sda == 1); /* master releases SDA during ACK */
        bytes[transaction][lengths[transaction]++] = byte; byte = 0;
      }
      bit = (bit + 1) % 9;
    }
    pins = next;
  }
  assert(!active && transaction == 1 && pins == (OLED_SDA_PIN | OLED_SCL_PIN));
}
int main(void)
{
  uint8_t data[16], decoded[2][32], lengths[2];
  for (uint8_t i = 0; i < sizeof(data); i++) { data[i] = (uint8_t)(i * 17u); }
  BuildWave(7, 112, data); decode(decoded, lengths);
  const uint8_t expected[] = {0x78, 0, 0xB7, 0x17, 0x02};
  assert(lengths[0] == 5 && memcmp(decoded[0], expected, 5) == 0);
  assert(lengths[1] == 18 && decoded[1][0] == 0x78 && decoded[1][1] == 0x40);
  assert(memcmp(&decoded[1][2], data, sizeof(data)) == 0);
  assert(s_wave_length == 635 && s_wave_length <= OLED_WAVE_WORDS);
  puts("PASS DMA waveform: address, page, column offset, 16 payload bytes, ACK slots, STOP");

  memset(s_frame, 0, sizeof(s_frame)); memset(s_dirty, 0, sizeof(s_dirty));
  OLED_ShowString(0, 0, (u8 *)"PASSWORD        ", 14);
  assert(s_dirty[0] == 0x0F && s_dirty[1] == 0);
  memset(s_dirty, 0, sizeof(s_dirty));
  OLED_ShowString(0, 0, (u8 *)"PASSWORD        ", 14);
  assert(s_dirty[0] == 0); /* unchanged screen causes no transfer */
  OLED_ShowString(0, 0, (u8 *)"PASS            ", 14);
  assert(s_dirty[0] == 0x0C);
  for (uint8_t x = 32; x < 128; x++) { assert(s_frame[0][x] == 0); }
  puts("PASS identical frame sends nothing; shorter text only dirties erased blocks");

  memset(s_dirty, 0, sizeof(s_dirty));
  PutByte(127, 7, 0xFF); PutByte(128, 7, 0xFF); PutByte(0, 8, 0xFF);
  assert(s_dirty[7] == 0x80 && s_frame[7][127] == 0xFF);
  OLED_DrawPoint(127, 63, 0); assert(s_frame[7][127] == 0x7F);
  puts("PASS framebuffer edges and point masks");

  /* The bundled 6x8 font ends before '~'; unsupported glyphs use '?'. */
  OLED_ShowChar(0, 6, '?', 14);
  uint8_t replacement[8]; memcpy(replacement, s_frame[6], sizeof(replacement));
  OLED_ShowChar(0, 6, '~', 14);
  assert(memcmp(replacement, s_frame[6], sizeof(replacement)) == 0);

  uint32_t saved[OLED_WAVE_WORDS];
  memcpy(data, &s_frame[7][112], sizeof(data)); s_dirty[7] = 0;
  BuildWave(7, 112, data); memcpy(saved, s_wave, sizeof(saved));
  PutByte(127, 7, 0x31);
  assert(s_dirty[7] == 0x80 && memcmp(saved, s_wave, sizeof(saved)) == 0);
  puts("PASS drawing during DMA preserves the active snapshot and marks a later refresh");

  dma_result = 1; now = 0; assert(SendWave());
  assert(!s_dma_busy && !(TIM1->CR1 & TIM_CR1_CEN) && !(DMA1_Channel5->CCR & DMA_CCR_EN));
  assert(g_oled_diag.blocks_sent == 1 && g_oled_diag.data_bytes == 16);
  dma_result = 2; assert(!SendWave()); assert(g_oled_diag.dma_errors == 1);
  dma_result = 0; now = 0; assert(!SendWave());
  assert(now == 20 && g_oled_diag.dma_errors == 2 && !s_dma_busy);
  puts("PASS DMA completion, DMA error and lost-interrupt timeout stop the timer safely");
  return 0;
}
