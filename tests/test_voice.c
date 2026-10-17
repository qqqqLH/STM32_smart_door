/* Production voice driver with a module that boots slowly at default volume 20. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../BSP/JQ8900/JQ8900.c"
static uint32_t now, module_ready_at;
static uint8_t module_volume, played_volume, wire[32][12];
static uint32_t sent_at[32];
static uint16_t wire_len[32];
static unsigned sends, plays, fail_next;
static UART_HandleTypeDef uart;
static JQ8900_Handle voice;
uint32_t HAL_GetTick(void) { return now; }
void HAL_Delay(uint32_t ms) { now += ms; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u, uint8_t *data, uint16_t size, uint32_t timeout)
{
  (void)timeout;
  assert(u == &uart && size <= 12 && sends < 32);
  assert(size == data[2] + 4u);
  uint8_t sum = 0;
  for (uint16_t i = 0; i < size - 1; i++) { sum += data[i]; }
  assert(sum == data[size - 1]);
  sent_at[sends] = now; wire_len[sends] = size; memcpy(wire[sends++], data, size);
  if (fail_next) { fail_next = 0; return HAL_ERROR; }
  if ((int32_t)(now - module_ready_at) >= 0)
  {
    if (data[1] == 0x13) { module_volume = data[3]; }
    if (data[1] == 0x07 || data[1] == 0x02 || data[1] == 0x05 || data[1] == 0x06)
    { played_volume = module_volume; plays++; }
  }
  return HAL_OK;
}
static void reset(void)
{
  now = sends = plays = fail_next = 0;
  module_ready_at = 1200; module_volume = 20; played_volume = 255;
  memset(wire, 0, sizeof(wire)); JQ8900_Init(&voice, &uart);
}
int main(void)
{
  reset(); JQ8900_SetVolume(&voice, 10); JQ8900_PlayIndex(&voice, 1);
  assert(sends == 0 && now == 0); /* startup guard must not block the UI */
  now = 1499; JQ8900_Process(&voice); assert(sends == 0);
  now = 1500; JQ8900_Process(&voice);
  const uint8_t volume10[] = {0xAA, 0x13, 1, 10, 0xC8};
  const uint8_t play1[] = {0xAA, 0x07, 2, 0, 1, 0xB4};
  assert(sends == 2 && wire_len[0] == 5 && wire_len[1] == 6);
  assert(memcmp(wire[0], volume10, 5) == 0 && memcmp(wire[1], play1, 6) == 0);
  assert(sent_at[1] - sent_at[0] >= 20 && plays == 1 && played_volume == 10);
  puts("PASS cold startup: no early commands; restore saved volume before first playback");

  JQ8900_Process(&voice); JQ8900_Process(&voice);
  assert(voice.volume_repeats == 0 && sends == 4);
  JQ8900_Process(&voice); assert(sends == 4);
  puts("PASS startup restoration retries finish without continuous UART traffic");

  module_volume = 20; /* voice module reset independently, MCU still holds VOL1 */
  unsigned first = sends; JQ8900_PlayIndex(&voice, 2);
  assert(sends == first + 2 && wire[first][1] == 0x13 && wire[first + 1][1] == 0x07);
  assert(played_volume == 10);
  JQ8900_SetVolume(&voice, 15); JQ8900_Play(&voice); assert(played_volume == 15);
  puts("PASS module reset and menu changes: every playback uses the current selected volume");

  reset(); now = 1500; fail_next = 1; JQ8900_SetVolume(&voice, 10);
  assert(voice.tx_errors == 1 && voice.volume_repeats == 3);
  fail_next = 1; JQ8900_PlayIndex(&voice, 1);
  assert(plays == 0 && voice.pending_play == 0x07);
  JQ8900_Process(&voice); assert(plays == 1 && played_volume == 10 && voice.pending_play == 0);
  puts("PASS UART failure suppresses playback and keeps volume/play requests for retry");

  reset(); JQ8900_SetVolume(&voice, 10); JQ8900_PlayIndex(&voice, 1);
  JQ8900_PlayIndex(&voice, 2); JQ8900_Stop(&voice);
  now = 1500; JQ8900_Process(&voice); assert(plays == 0);
  JQ8900_SetVolume(&voice, 255); assert(module_volume == 30);
  JQ8900_SetVolume(&voice, 0); JQ8900_PlayIndex(&voice, 1); assert(played_volume == 0);
  puts("PASS queued startup playback can be cancelled; volume clamp and mute are valid");

  reset(); now = UINT32_MAX - 100; JQ8900_Init(&voice, &uart); JQ8900_SetVolume(&voice, 10);
  now += 1499u; JQ8900_Process(&voice); assert(sends == 0);
  now++; JQ8900_Process(&voice); assert(sends == 1);
  puts("PASS startup deadline across tick rollover");
  return 0;
}
