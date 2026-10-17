#include "JQ8900.h"
#include <string.h>

#define JQ8900_START_BYTE 0xAAu

#define JQ8900_CMD_PLAY          0x02u
#define JQ8900_CMD_PAUSE         0x03u
#define JQ8900_CMD_STOP          0x04u
#define JQ8900_CMD_PREVIOUS      0x05u
#define JQ8900_CMD_NEXT          0x06u
#define JQ8900_CMD_PLAY_INDEX    0x07u
#define JQ8900_CMD_VOLUME_SET    0x13u

#define JQ8900_DEFAULT_TIMEOUT_MS 100u

__weak void JQ8900_DelayMs(uint32_t ms)
{
  HAL_Delay(ms);
}

static uint8_t JQ8900_Ready(const JQ8900_Handle *handle)
{
  return (handle != NULL && handle->uart != NULL &&
          (uint32_t)(HAL_GetTick() - handle->init_tick) >= JQ8900_BOOT_WAIT_MS);
}

static uint8_t JQ8900_Checksum(const uint8_t *data, uint8_t len)
{
  uint16_t sum = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    sum += data[i];
  }
  return (uint8_t)sum;
}

static HAL_StatusTypeDef JQ8900_Send(JQ8900_Handle *handle, uint8_t cmd, const uint8_t *payload, uint8_t len)
{
  // 起始位+指令类型+数据长度=3
  // 数据 N=8
  // 校验和=1
  uint8_t buf[3 + 8 + 1];
  uint8_t idx = 0;
  HAL_StatusTypeDef result;

  if (!JQ8900_Ready(handle) || len > 8u || (len != 0u && payload == NULL))
  {
    return HAL_ERROR;
  }

  if (handle->has_tx != 0u)
  {
    uint32_t elapsed = HAL_GetTick() - handle->last_tx_tick;
    if (elapsed < JQ8900_COMMAND_GAP_MS)
    {
      JQ8900_DelayMs(JQ8900_COMMAND_GAP_MS - elapsed);
    }
  }

  buf[idx++] = JQ8900_START_BYTE;
  buf[idx++] = cmd;
  buf[idx++] = len;
  for (uint8_t i = 0; i < len; i++)
  {
    buf[idx++] = payload[i];
  }
  /* Do not read idx and modify idx unsequenced in the same expression. */
  buf[idx] = JQ8900_Checksum(buf, idx);
  idx++;

  result = HAL_UART_Transmit(handle->uart, buf, idx, handle->timeout_ms);
  handle->last_tx_tick = HAL_GetTick();
  handle->has_tx = 1u;
  if (result != HAL_OK) { handle->tx_errors++; }
  return result;
}

void JQ8900_Init(JQ8900_Handle *handle, UART_HandleTypeDef *uart)
{
  if (handle == NULL)
  {
    return;
  }
  memset(handle, 0, sizeof(*handle));
  handle->uart = uart;
  handle->timeout_ms = JQ8900_DEFAULT_TIMEOUT_MS;
  handle->init_tick = HAL_GetTick();
}

void JQ8900_SetTimeout(JQ8900_Handle *handle, uint32_t timeout_ms)
{
  if (handle == NULL)
  {
    return;
  }
  handle->timeout_ms = timeout_ms;
}

void JQ8900_Play(JQ8900_Handle *handle)
{
  if (handle == NULL) { return; }
  handle->pending_play = JQ8900_CMD_PLAY;
  JQ8900_Process(handle);
}

void JQ8900_Pause(JQ8900_Handle *handle)
{
  if (handle == NULL) { return; }
  handle->pending_play = 0u;
  (void)JQ8900_Send(handle, JQ8900_CMD_PAUSE, NULL, 0);
}

void JQ8900_Stop(JQ8900_Handle *handle)
{
  if (handle == NULL) { return; }
  handle->pending_play = 0u;
  (void)JQ8900_Send(handle, JQ8900_CMD_STOP, NULL, 0);
}

void JQ8900_Next(JQ8900_Handle *handle)
{
  if (handle == NULL) { return; }
  handle->pending_play = JQ8900_CMD_NEXT;
  JQ8900_Process(handle);
}

void JQ8900_Previous(JQ8900_Handle *handle)
{
  if (handle == NULL) { return; }
  handle->pending_play = JQ8900_CMD_PREVIOUS;
  JQ8900_Process(handle);
}

void JQ8900_PlayIndex(JQ8900_Handle *handle, uint16_t index)
{
  if (handle == NULL) { return; }
  handle->pending_index = index;
  handle->pending_play = JQ8900_CMD_PLAY_INDEX;
  JQ8900_Process(handle);
}

void JQ8900_SetVolume(JQ8900_Handle *handle, uint8_t volume)
{
  if (handle == NULL) { return; }
  handle->volume = (volume <= 30u) ? volume : 30u;
  /* Repeat the initial restoration; later menu changes apply immediately. */
  handle->volume_repeats = (handle->volume_valid == 0u) ? 3u : 1u;
  handle->volume_valid = 1u;
  JQ8900_Process(handle);
}

void JQ8900_Process(JQ8900_Handle *handle)
{
  if (!JQ8900_Ready(handle) || handle->volume_valid == 0u) { return; }
  if (handle->volume_repeats == 0u && handle->pending_play == 0u) { return; }

  /* The module powers up at volume 20 and does not acknowledge volume writes.
   * Always restore the saved volume before PLAY, even after a module reset. */
  if (JQ8900_Send(handle, JQ8900_CMD_VOLUME_SET, &handle->volume, 1u) != HAL_OK) { return; }
  if (handle->volume_repeats != 0u) { handle->volume_repeats--; }
  if (handle->pending_play != 0u)
  {
    uint8_t payload[2];
    uint8_t len = (handle->pending_play == JQ8900_CMD_PLAY_INDEX) ? 2u : 0u;
    payload[0] = (uint8_t)(handle->pending_index >> 8);
    payload[1] = (uint8_t)handle->pending_index;
    if (JQ8900_Send(handle, handle->pending_play, payload, len) == HAL_OK)
    {
      handle->pending_play = 0u;
    }
  }
}
