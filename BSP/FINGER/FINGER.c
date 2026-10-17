#include "FINGER.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define PKT_HEADER_MSB     0xEFu//包头高8位
#define PKT_HEADER_LSB     0x01u//包头低八位
#define PKT_CMD            0x01u//命令包格式包标识
#define PKT_ACK            0x07u//指令应答包标识

#define CMD_GET_IMAGE      0x01u
#define CMD_GEN_CHAR       0x02u
#define CMD_SEARCH         0x04u
#define CMD_REG_MODEL      0x05u
#define CMD_STORE_CHAR     0x06u
#define CMD_DELETE_CHAR    0x0Cu
#define CMD_EMPTY          0x0Du
#define CMD_READ_INDEX     0x1Fu

#define ACK_OK             0x00u//表示指令执行完毕或OK
#define ACK_NO_FINGER      0x02u//表示传感器上没有手指
#define ACK_NOT_FOUND      0x09u//表示没有搜索到指纹

#define MAX_PAYLOAD        64u
#define MAX_PARAM          8u
#define DEFAULT_TIMEOUT_MS 1000u

typedef struct
{
  uint8_t confirm;
  uint16_t data_len;
  uint8_t data[MAX_PAYLOAD];
} FingerAck;

__weak void FINGER_DelayMs(uint32_t ms)
{
  HAL_Delay(ms);
}

__weak void FINGER_PromptSecondPress(void)
{
}

//写入16bit，高位和低位位置对调
static void WriteU16(uint8_t *out, uint16_t value)
{
  out[0] = (uint8_t)(value >> 8);
  out[1] = (uint8_t)(value & 0xFFu);
}

//写入32bit
static void WriteU32(uint8_t *out, uint32_t value)
{
  out[0] = (uint8_t)(value >> 24);
  out[1] = (uint8_t)(value >> 16);
  out[2] = (uint8_t)(value >> 8);
  out[3] = (uint8_t)(value & 0xFFu);
}

//读取16bit
static uint16_t ReadU16(const uint8_t *in)
{
  return (uint16_t)((uint16_t)in[0] << 8) | (uint16_t)in[1];
}

//校验和是从包标识至校验和之间所有字节之和，包含包标识不包含校验和。
//包标识 + 包长度的高字节 + 包长度的低字节 + 指令/参数里的每一个字节
static uint16_t CalcSum(uint8_t type, uint16_t length, const uint8_t *payload, uint16_t payload_len)
{
  uint32_t sum = type;
  sum += (uint8_t)(length >> 8);
  sum += (uint8_t)(length & 0xFFu);
  for (uint16_t i = 0; i < payload_len; i++)
  {
    sum += payload[i];
  }
  return (uint16_t)sum;
}

/* A single worker owns this transport. RX stays armed while that worker sleeps.
 * USART3 RX/TX: DMA1 channels 3/2; HT, TC and IDLE only publish progress.
 */
#define RX_DMA_SIZE 256u
static FINGER_Handle *s_owner;
static TaskHandle_t s_waiter;
static uint8_t s_rx_dma[RX_DMA_SIZE];
static uint8_t s_tx_dma[9u + 1u + MAX_PARAM + 2u];
static volatile uint32_t s_rx_produced;
static uint32_t s_rx_consumed;
static uint16_t s_rx_position;
static volatile uint8_t s_io_error;
static uint8_t s_resync;
static uint32_t s_command_start;
FINGER_Diagnostics g_finger_diag;

static void WakeWorker(void)
{
  BaseType_t higher = pdFALSE;
  if (s_waiter != NULL)
  {
    vTaskNotifyGiveFromISR(s_waiter, &higher);
    portYIELD_FROM_ISR(higher);
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *uart, uint16_t size)
{
  (void)size;
  if (s_owner != NULL && uart == s_owner->uart)
  {
    /* Read the live cursor: a delayed HT IRQ may follow a newer IDLE IRQ. */
    uint16_t pos = (uint16_t)(RX_DMA_SIZE - __HAL_DMA_GET_COUNTER(uart->hdmarx));
    pos %= RX_DMA_SIZE;
    s_rx_produced += (uint16_t)((pos + RX_DMA_SIZE - s_rx_position) % RX_DMA_SIZE);
    s_rx_position = pos;
    WakeWorker();
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
  if (s_owner != NULL && uart == s_owner->uart) { WakeWorker(); }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  if (s_owner != NULL && uart == s_owner->uart)
  {
    g_finger_diag.uart_errors++;
    g_finger_diag.last_uart_error = uart->ErrorCode;
    if ((uart->ErrorCode & HAL_UART_ERROR_ORE) != 0u) { g_finger_diag.overruns++; }
    s_io_error = 1u;
    WakeWorker();
  }
}

static uint8_t StartReceiver(FINGER_Handle *handle)
{
  if (handle == NULL || handle->uart == NULL || handle->uart->hdmarx == NULL ||
      handle->uart->hdmatx == NULL) { return 0u; }
  (void)HAL_UART_AbortTransmit(handle->uart);
  (void)HAL_UART_AbortReceive(handle->uart);
  __HAL_UART_CLEAR_OREFLAG(handle->uart);
  s_rx_produced = 0u;
  s_rx_consumed = 0u;
  s_rx_position = 0u;
  s_io_error = 0u;
  (void)ulTaskNotifyTake(pdTRUE, 0);
  if (HAL_UARTEx_ReceiveToIdle_DMA(handle->uart, s_rx_dma, RX_DMA_SIZE) != HAL_OK)
  {
    s_io_error = 1u;
    return 0u;
  }
  return 1u;
}

static uint32_t TimeLeft(uint32_t timeout_ms)
{
  uint32_t elapsed = HAL_GetTick() - s_command_start;
  return (elapsed < timeout_ms) ? timeout_ms - elapsed : 0u;
}

static uint8_t UartRead(FINGER_Handle *handle, uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
  if (handle == NULL || handle != s_owner || s_io_error != 0u) { return 0u; }
  while (len != 0u)
  {
    uint32_t available = s_rx_produced - s_rx_consumed;
    if (s_io_error != 0u) { return 0u; }
    if (available > RX_DMA_SIZE)
    {
      g_finger_diag.ring_overflows++;
      s_io_error = 1u;
      return 0u;
    }
    if (available != 0u)
    {
      *buf++ = s_rx_dma[s_rx_consumed++ % RX_DMA_SIZE];
      len--;
    }
    else
    {
      uint32_t remaining = TimeLeft(timeout_ms);
      if (remaining == 0u) { return 0u; }
      (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(remaining));
    }
  }
  return 1u;
}

static uint8_t ReadAck(FINGER_Handle *handle, FingerAck *ack, uint32_t timeout_ms)
{
  uint8_t packet[9u + MAX_PAYLOAD + 2u];
  uint16_t used = 0u;
  uint8_t address[4];
  WriteU32(address, handle->address);
  while (TimeLeft(timeout_ms) != 0u)
  {
    if (!UartRead(handle, &packet[used++], 1u, timeout_ms)) { return 0u; }
    /* Slide past noise, invalid lengths, bad addresses and bad checksums. */
    while (used != 0u)
    {
      uint16_t length;
      if (packet[0] != PKT_HEADER_MSB) { goto discard; }
      if (used < 2u) { break; }
      if (packet[1] != PKT_HEADER_LSB) { goto discard; }
      if (used < 9u) { break; }
      length = ReadU16(&packet[7]);
      if (memcmp(&packet[2], address, 4u) != 0 || packet[6] != PKT_ACK ||
          length < 3u || length > MAX_PAYLOAD + 2u) { goto discard; }
      if (used < 9u + length) { break; }
      if (ReadU16(&packet[7u + length]) !=
          CalcSum(PKT_ACK, length, &packet[9], (uint16_t)(length - 2u)))
      {
        g_finger_diag.bad_packets++;
        goto discard;
      }
      ack->confirm = packet[9];
      ack->data_len = (uint16_t)(length - 3u);
      memcpy(ack->data, &packet[10], ack->data_len);
      g_finger_diag.received_packets++;
      return 1u;
    discard:
      memmove(packet, &packet[1], --used);
    }
  }
  return 0u;
}

static uint8_t SendCmd(FINGER_Handle *handle, uint8_t cmd, const uint8_t *params, uint16_t param_len)
{
  // 9 = 包头(2) + 设备地址(4) + 包标识(1) + 包长度(2)
  // 1 = 指令码
  // MAX_PARAM = 参数的最大可能长度
  // 2 = 校验和
  uint8_t *buf = s_tx_dma;
  //包长度 = 指令+参数+校验和
  uint16_t length = (uint16_t)(1u + param_len + 2u);
  uint16_t idx = 0;

  if (handle == NULL || handle->uart == NULL || param_len > MAX_PARAM)
  {
    return 0u;
  }

  //填写包头
  buf[idx++] = PKT_HEADER_MSB;//高8位
  buf[idx++] = PKT_HEADER_LSB;//低8位
  WriteU32(&buf[idx], handle->address);//写入设备地址
  idx += 4u;//增加数组指针地址

  //写入包标识，命令包格式位01
  buf[idx++] = PKT_CMD;
  //写入包长度
  WriteU16(&buf[idx], length);
  idx += 2u;//增加两字节，因为包长度两字节
  buf[idx++] = cmd;//写入指令
  //循环写入参数
  for (uint16_t i = 0; i < param_len; i++)
  {
    buf[idx++] = params[i];
  }
  //计算校验和并且把校验和写入数组
  uint16_t sum = CalcSum(PKT_CMD, length, &buf[9], (uint16_t)(1u + param_len));
  WriteU16(&buf[idx], sum);
  idx += 2u;
  //打包好数组一次性全部发送
  if (HAL_UART_Transmit_DMA(handle->uart, buf, idx) != HAL_OK) { return 0u; }
  /* DMA owns the static buffer until the UART TC interrupt (last stop bit). */
  while (handle->uart->gState != HAL_UART_STATE_READY)
  {
    uint32_t remaining = TimeLeft(handle->timeout_ms);
    if (s_io_error != 0u || remaining == 0u)
    {
      (void)HAL_UART_AbortTransmit(handle->uart);
      return 0u;
    }
    (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(remaining));
  }
  return 1u;
}

// 命令函数
static uint8_t Command(FINGER_Handle *handle, uint8_t cmd, const uint8_t *params,
                       uint16_t param_len, FingerAck *ack)
{
  if (handle == NULL || handle != s_owner || handle->uart == NULL) { return 0u; }
  if (s_resync != 0u || s_io_error != 0u || handle->uart->RxState != HAL_UART_STATE_BUSY_RX)
  {
    /* Protocol has no transaction ID. Quarantine late ACKs after a failed command. */
    FINGER_DelayMs(handle->timeout_ms);
    if (!StartReceiver(handle)) { return 0u; }
    s_resync = 0u;
    g_finger_diag.recoveries++;
  }
  taskENTER_CRITICAL();
  s_rx_consumed = s_rx_produced;
  taskEXIT_CRITICAL();
  (void)ulTaskNotifyTake(pdTRUE, 0);
  s_command_start = HAL_GetTick();
  if (SendCmd(handle, cmd, params, param_len) && ReadAck(handle, ack, handle->timeout_ms))
  {
    return 1u;
  }
  if (TimeLeft(handle->timeout_ms) == 0u) { g_finger_diag.timeouts++; }
  (void)HAL_UART_AbortTransmit(handle->uart);
  s_resync = 1u;
  return 0u;
}

//等待手指按下
static FINGER_Result WaitImage(FINGER_Handle *handle, uint32_t timeout_ms)
{
  //在一定时间内循环发送命令
  //3.3.1通用指令集
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < timeout_ms)
  {
    FingerAck ack;
    if (Command(handle, CMD_GET_IMAGE, NULL, 0, &ack))
    {
      if (ack.confirm == ACK_OK)
      {
        return FINGER_OK;
      }
      if (ack.confirm == ACK_NO_FINGER)
      {
        FINGER_DelayMs(50);
        continue;
      }
      return FINGER_FAIL;
    }
    FINGER_DelayMs(50);
  }
  return FINGER_TIMEOUT;
}

//与上一个函数一致不过一个是等待有手指一个是等待没有手指
static FINGER_Result WaitNoFinger(FINGER_Handle *handle, uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < timeout_ms)
  {
    FingerAck ack;
    if (Command(handle, CMD_GET_IMAGE, NULL, 0, &ack))
    {
      if (ack.confirm == ACK_NO_FINGER)
      {
        return FINGER_OK;
      }
      if (ack.confirm == ACK_OK)
      {
        FINGER_DelayMs(50);
        continue;
      }
      return FINGER_FAIL;
    }
    FINGER_DelayMs(50);
  }
  return FINGER_TIMEOUT;
}

//生成特征
static FINGER_Result GenChar(FINGER_Handle *handle, uint8_t buffer_id)
{
  //command命令生成特征在bufferid1
  uint8_t params[1];
  FingerAck ack;
  params[0] = buffer_id;
  if (!Command(handle, CMD_GEN_CHAR, params, 1, &ack))
  {
    return FINGER_FAIL;
  }
  return (ack.confirm == ACK_OK) ? FINGER_OK : FINGER_FAIL;
}

//搜索指纹
static FINGER_Result Search(FINGER_Handle *handle, uint8_t buffer_id, uint16_t start_page,
                            uint16_t page_num, uint16_t *page_id, uint16_t *score)
{
  //五个参数：缓冲区号+ (2)StartPage + (2)PageNum
  uint8_t params[5];
  FingerAck ack;

  params[0] = buffer_id;
  WriteU16(&params[1], start_page);
  WriteU16(&params[3], page_num);

  //发送搜索指纹命令
  if (!Command(handle, CMD_SEARCH, params, sizeof(params), &ack))
  {
    return FINGER_FAIL;
  }

  //解析结果
  if (ack.confirm == ACK_OK && ack.data_len >= 4u)
  {
    if (page_id != NULL)
    {
      *page_id = ReadU16(&ack.data[0]);
    }
    if (score != NULL)
    {
      *score = ReadU16(&ack.data[2]);
    }
    return FINGER_OK;
  }

  if (ack.confirm == ACK_NOT_FOUND)
  {
    return FINGER_NOT_FOUND;
  }

  return FINGER_FAIL;
}

//特征合并
static FINGER_Result RegModel(FINGER_Handle *handle)
{
  FingerAck ack;
  if (!Command(handle, CMD_REG_MODEL, NULL, 0, &ack))
  {
    return FINGER_FAIL;
  }
  return (ack.confirm == ACK_OK) ? FINGER_OK : FINGER_FAIL;
}

//存储模板
static FINGER_Result StoreChar(FINGER_Handle *handle, uint8_t buffer_id, uint16_t page_id)
{
  uint8_t params[3];
  FingerAck ack;

  params[0] = buffer_id;
  WriteU16(&params[1], page_id);
  if (!Command(handle, CMD_STORE_CHAR, params, sizeof(params), &ack))
  {
    return FINGER_FAIL;
  }
  return (ack.confirm == ACK_OK) ? FINGER_OK : FINGER_FAIL;
}

//将uart绑定到指纹模块，初始化指纹模块
void FINGER_Init(FINGER_Handle *handle, UART_HandleTypeDef *uart)
{
  if (handle == NULL)
  {
    return;
  }
  handle->uart = uart;
  handle->address = FINGER_ADDR_DEFAULT;
  handle->timeout_ms = DEFAULT_TIMEOUT_MS;
  s_owner = handle;
  s_waiter = xTaskGetCurrentTaskHandle();
  s_resync = 0u;
  memset(&g_finger_diag, 0, sizeof(g_finger_diag));
  (void)StartReceiver(handle);
}

//设置finger模块地址
void FINGER_SetAddress(FINGER_Handle *handle, uint32_t address)
{
  if (handle == NULL)
  {
    return;
  }
  handle->address = address;
}
//设置超时时间
void FINGER_SetTimeout(FINGER_Handle *handle, uint32_t timeout_ms)
{
  if (handle == NULL)
  {
    return;
  }
  handle->timeout_ms = timeout_ms;
}

//识别指纹，通过按下存入缓冲区，然后比对flash里面的指纹
FINGER_Result FINGER_Identify(FINGER_Handle *handle, uint16_t start_page, uint16_t page_num,
                              uint16_t *page_id, uint16_t *score, uint32_t timeout_ms)
{
  FINGER_Result ret;

  ret = WaitImage(handle, timeout_ms);
  if (ret != FINGER_OK)
  {
    return ret;
  }

  ret = GenChar(handle, 1);
  if (ret != FINGER_OK)
  {
    return ret;
  }

  return Search(handle, 1, start_page, page_num, page_id, score);
}

//录入指纹
FINGER_Result FINGER_Enroll(FINGER_Handle *handle, uint16_t page_id, uint32_t timeout_ms)
{
  FINGER_Result ret;

  ret = WaitImage(handle, timeout_ms);
  if (ret != FINGER_OK)
  {
    return ret;
  }

  ret = GenChar(handle, 1);
  if (ret != FINGER_OK)
  {
    return ret;
  }

  FINGER_DelayMs(300);
  //播放语音请再次按下
  FINGER_PromptSecondPress();
  //等待指纹模块上面没有手指
  ret = WaitNoFinger(handle, timeout_ms);
  if (ret != FINGER_OK)
  {
    return ret;
  }
  //等待手指按下
  ret = WaitImage(handle, timeout_ms);
  if (ret != FINGER_OK)
  {
    return ret;
  }
  //生成特征
  ret = GenChar(handle, 2);
  if (ret != FINGER_OK)
  {
    return ret;
  }
  //特征合并
  ret = RegModel(handle);
  if (ret != FINGER_OK)
  {
    return ret;
  }
  //存储指纹
  return StoreChar(handle, 1, page_id);
}

//删除指纹
FINGER_Result FINGER_Delete(FINGER_Handle *handle, uint16_t page_id, uint16_t count)
{
  uint8_t params[4];
  FingerAck ack;

  WriteU16(&params[0], page_id);
  WriteU16(&params[2], count);
  if (!Command(handle, CMD_DELETE_CHAR, params, sizeof(params), &ack))
  {
    return FINGER_FAIL;
  }
  return (ack.confirm == ACK_OK) ? FINGER_OK : FINGER_FAIL;
}

//清空指纹
FINGER_Result FINGER_Empty(FINGER_Handle *handle)
{
  FingerAck ack;
  if (!Command(handle, CMD_EMPTY, NULL, 0, &ack))
  {
    return FINGER_FAIL;
  }
  return (ack.confirm == ACK_OK) ? FINGER_OK : FINGER_FAIL;
}

//读取录入模板的索引表
//页码0代表模板0-256但是整个模块只有50个指纹
FINGER_Result FINGER_ReadIndexTable(FINGER_Handle *handle, uint8_t page, uint8_t *out, uint16_t out_len)
{
  uint8_t params[1];
  FingerAck ack;

  if (out == NULL || out_len == 0u)
  {
    return FINGER_FAIL;
  }

  params[0] = page;
  if (!Command(handle, CMD_READ_INDEX, params, sizeof(params), &ack))
  {
    return FINGER_FAIL;
  }
  if (ack.confirm != ACK_OK)
  {
    return FINGER_FAIL;
  }
  if (ack.data_len != 32u || ack.data_len > out_len)
  {
    return FINGER_FAIL;
  }

  for (uint16_t i = 0; i < ack.data_len; i++)
  {
    out[i] = ack.data[i];
  }
  return FINGER_OK;
}
