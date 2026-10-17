#ifndef JQ8900_H
#define JQ8900_H

#include "main.h"
#include <stdint.h>

/* Conservative startup margin, not a module-ready acknowledgement. */
#define JQ8900_BOOT_WAIT_MS 1500u
#define JQ8900_COMMAND_GAP_MS 20u

typedef struct
{
  UART_HandleTypeDef *uart;
  uint32_t timeout_ms;
  uint32_t init_tick;
  uint32_t last_tx_tick;
  uint32_t tx_errors;
  uint16_t pending_index;
  uint8_t volume;
  uint8_t volume_valid;
  uint8_t volume_repeats;
  uint8_t pending_play;
  uint8_t has_tx;
} JQ8900_Handle;

void JQ8900_Init(JQ8900_Handle *handle, UART_HandleTypeDef *uart);
void JQ8900_SetTimeout(JQ8900_Handle *handle, uint32_t timeout_ms);

/* Single task owns the handle. Call periodically to service startup/retries.
 * During startup, only the latest requested playback is retained. */
void JQ8900_Process(JQ8900_Handle *handle);
void JQ8900_DelayMs(uint32_t ms);

void JQ8900_Play(JQ8900_Handle *handle);
void JQ8900_Pause(JQ8900_Handle *handle);
void JQ8900_Stop(JQ8900_Handle *handle);
void JQ8900_Next(JQ8900_Handle *handle);
void JQ8900_Previous(JQ8900_Handle *handle);
void JQ8900_PlayIndex(JQ8900_Handle *handle, uint16_t index);
void JQ8900_SetVolume(JQ8900_Handle *handle, uint8_t volume);

#endif
