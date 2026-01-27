#include "JQ8900.h"

#define JQ8900_START_BYTE 0xAAu

#define JQ8900_CMD_PLAY          0x02u
#define JQ8900_CMD_PAUSE         0x03u
#define JQ8900_CMD_STOP          0x04u
#define JQ8900_CMD_PREVIOUS      0x05u
#define JQ8900_CMD_NEXT          0x06u
#define JQ8900_CMD_PLAY_INDEX    0x07u
#define JQ8900_CMD_VOLUME_SET    0x13u

#define JQ8900_DEFAULT_TIMEOUT_MS 100u

static uint8_t JQ8900_Checksum(const uint8_t *data, uint8_t len)
{
  uint16_t sum = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    sum += data[i];
  }
  return (uint8_t)sum;
}

static void JQ8900_Send(JQ8900_Handle *handle, uint8_t cmd, const uint8_t *payload, uint8_t len)
{
  // 起始位+指令类型+数据长度=3
  // 数据 N=8
  // 校验和=1
  uint8_t buf[3 + 8 + 1];
  uint8_t idx = 0;

  if (handle == NULL || handle->uart == NULL)
  {
    return;
  }

  buf[idx++] = JQ8900_START_BYTE;
  buf[idx++] = cmd;
  buf[idx++] = len;
  for (uint8_t i = 0; i < len; i++)
  {
    buf[idx++] = payload[i];
  }
  buf[idx++] = JQ8900_Checksum(buf, idx);

  (void)HAL_UART_Transmit(handle->uart, buf, idx, handle->timeout_ms);
}

void JQ8900_Init(JQ8900_Handle *handle, UART_HandleTypeDef *uart)
{
  if (handle == NULL)
  {
    return;
  }
  handle->uart = uart;
  handle->timeout_ms = JQ8900_DEFAULT_TIMEOUT_MS;
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
  JQ8900_Send(handle, JQ8900_CMD_PLAY, NULL, 0);
}

void JQ8900_Pause(JQ8900_Handle *handle)
{
  JQ8900_Send(handle, JQ8900_CMD_PAUSE, NULL, 0);
}

void JQ8900_Stop(JQ8900_Handle *handle)
{
  JQ8900_Send(handle, JQ8900_CMD_STOP, NULL, 0);
}

void JQ8900_Next(JQ8900_Handle *handle)
{
  JQ8900_Send(handle, JQ8900_CMD_NEXT, NULL, 0);
}

void JQ8900_Previous(JQ8900_Handle *handle)
{
  JQ8900_Send(handle, JQ8900_CMD_PREVIOUS, NULL, 0);
}

void JQ8900_PlayIndex(JQ8900_Handle *handle, uint16_t index)
{
  uint8_t payload[2];
  payload[0] = (uint8_t)(index >> 8);
  payload[1] = (uint8_t)(index & 0xFFu);
  JQ8900_Send(handle, JQ8900_CMD_PLAY_INDEX, payload, 2);
}

void JQ8900_SetVolume(JQ8900_Handle *handle, uint8_t volume)
{
  uint8_t payload[1];
  payload[0] = volume;
  JQ8900_Send(handle, JQ8900_CMD_VOLUME_SET, payload, 1);
}
