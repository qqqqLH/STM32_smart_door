/* Executes the production UART transport with a deterministic HAL/RTOS model. */
#include <stdio.h>
#include <string.h>
#include "../BSP/FINGER/FINGER.c"

static DMA_Channel_TypeDef rx_channel, tx_channel;
static DMA_HandleTypeDef rx = { &rx_channel }, tx = { &tx_channel };
static UART_HandleTypeDef uart = { NULL, &rx, &tx, 0, 0, 0 };
static uint32_t now, notifications;
static uint8_t response[1024], sent[32];
static uint16_t response_size, response_pos, chunk_size, write_pos, sent_size;
static int tx_pending, tx_stall, rx_error, flood;
static FINGER_Handle finger;

uint32_t HAL_GetTick(void) { return now; }
void HAL_Delay(uint32_t ms) { now += ms; }
void vTaskDelay(TickType_t ticks) { now += ticks; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)1; }
void vTaskNotifyGiveFromISR(TaskHandle_t t, BaseType_t *h) { (void)t; (void)h; notifications++; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u) { u->gState = HAL_UART_STATE_READY; tx_pending = 0; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u) { u->RxState = HAL_UART_STATE_READY; return HAL_OK; }
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *u, uint8_t *data, uint16_t size)
{
  assert(data == s_rx_dma && size == RX_DMA_SIZE);
  u->RxState = HAL_UART_STATE_BUSY_RX; write_pos = 0; rx_channel.CNDTR = size;
  return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *u, uint8_t *data, uint16_t size)
{
  assert(u->RxState == HAL_UART_STATE_BUSY_RX); /* RX must precede TX. */
  assert(size <= sizeof(sent)); memcpy(sent, data, size); sent_size = size;
  u->gState = 2; tx_pending = 1;
  return HAL_OK;
}
static void deliver(uint16_t count)
{
  while (count-- && response_pos < response_size)
  {
    s_rx_dma[write_pos++] = response[response_pos++];
    write_pos %= RX_DMA_SIZE;
    rx_channel.CNDTR = RX_DMA_SIZE - write_pos;
    if (write_pos == RX_DMA_SIZE / 2u || write_pos == 0u)
      HAL_UARTEx_RxEventCallback(&uart, write_pos ? write_pos : RX_DMA_SIZE);
  }
  HAL_UARTEx_RxEventCallback(&uart, write_pos);
}
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t timeout)
{
  (void)clear;
  if (!notifications && timeout)
  {
    if (tx_pending && !tx_stall)
    {
      now++; tx_pending = 0; uart.gState = HAL_UART_STATE_READY;
      HAL_UART_TxCpltCallback(&uart);
    }
    else if (rx_error)
    {
      now++; rx_error = 0; uart.ErrorCode = HAL_UART_ERROR_ORE;
      uart.RxState = HAL_UART_STATE_READY; HAL_UART_ErrorCallback(&uart);
    }
    else if (response_pos < response_size && !tx_stall)
    {
      now++; deliver(flood ? response_size : chunk_size);
    }
    else { now += timeout; }
  }
  uint32_t result = notifications; notifications = 0; return result;
}
static void reset(void)
{
  now = notifications = 0; response_size = response_pos = 0;
  chunk_size = 64; tx_stall = rx_error = flood = 0;
  FINGER_Init(&finger, &uart);
}
static void ack(uint8_t confirm, uint16_t data_size)
{
  /* Independent wire-format fixture. */
  uint8_t packet[80] = {0xEF, 1, 0xFF, 0xFF, 0xFF, 0xFF, 7, 0, 0};
  uint16_t length = data_size + 3, sum = 7 + length;
  packet[8] = (uint8_t)length; packet[9] = confirm; sum += confirm;
  for (uint16_t i = 0; i < data_size; i++) { packet[10 + i] = (uint8_t)(i + 1); sum += i + 1; }
  packet[10 + data_size] = (uint8_t)(sum >> 8); packet[11 + data_size] = (uint8_t)sum;
  memcpy(response + response_size, packet, data_size + 12); response_size += data_size + 12;
}
int main(void)
{
  FingerAck result;
  uint8_t table[32];
  reset(); ack(0, 32); chunk_size = 1;
  assert(FINGER_ReadIndexTable(&finger, 0, table, sizeof(table)) == FINGER_OK);
  assert(table[0] == 1 && table[31] == 32);
  const uint8_t expected[] = {0xEF,1,0xFF,0xFF,0xFF,0xFF,1,0,4,0x1F,0,0,0x24};
  assert(sent_size == sizeof(expected) && memcmp(sent, expected, sizeof(expected)) == 0);
  puts("PASS fragmented ACK + command checksum + receive-before-transmit");

  reset(); memset(response, 0x55, 250); response_size = 250; ack(0, 4); chunk_size = 31;
  assert(Command(&finger, CMD_SEARCH, NULL, 0, &result));
  assert(result.data_len == 4 && result.data[3] == 4 && write_pos < 250);
  puts("PASS noise resynchronization and DMA ring wrap");

  reset(); ack(0, 0); response[8] = 255; /* impossible length */
  ack(0, 0); response[12 + 2] = 0; /* wrong address */
  ack(0, 0); response[35] ^= 1; /* wrong checksum */
  ack(0, 4); chunk_size = 3;
  assert(Command(&finger, CMD_SEARCH, NULL, 0, &result));
  assert(result.data_len == 4 && g_finger_diag.bad_packets == 1);
  puts("PASS invalid length/address/checksum followed by a valid packet");

  reset(); ack(0, 2); memset(table, 0xAA, sizeof(table));
  assert(FINGER_ReadIndexTable(&finger, 0, table, sizeof(table)) == FINGER_FAIL);
  assert(table[0] == 0xAA && table[31] == 0xAA);
  puts("PASS truncated index table rejected without exposing uninitialized bytes");

  reset(); ack(0, 4); response_size -= 2; chunk_size = 1;
  assert(!Command(&finger, CMD_SEARCH, NULL, 0, &result));
  assert(now == 1000 && g_finger_diag.timeouts == 1);
  response_size = response_pos = 0; ack(0, 0);
  assert(Command(&finger, CMD_EMPTY, NULL, 0, &result));
  assert(g_finger_diag.recoveries == 1 && now >= 2000);
  puts("PASS one timeout budget across fragments and recovery before next command");

  reset(); rx_error = 1;
  assert(!Command(&finger, CMD_EMPTY, NULL, 0, &result));
  assert(g_finger_diag.overruns == 1);
  ack(0, 0); assert(Command(&finger, CMD_EMPTY, NULL, 0, &result));
  assert(g_finger_diag.recoveries == 1 && !s_io_error);
  puts("PASS ORE reporting and RX DMA restart");

  reset(); tx_stall = 1;
  assert(!Command(&finger, CMD_EMPTY, NULL, 0, &result));
  assert(now == 1000 && uart.gState == HAL_UART_STATE_READY);
  puts("PASS missing TX-complete interrupt times out and aborts DMA");

  reset(); flood = 1; memset(response, 0x55, 300); response_size = 300;
  assert(!Command(&finger, CMD_EMPTY, NULL, 0, &result));
  assert(g_finger_diag.ring_overflows == 1);
  puts("PASS RX ring overflow fails closed");

  reset(); deliver(0); s_command_start = UINT32_MAX - 20; now = 10;
  assert(TimeLeft(1000) == 969);
  /* A stale HT callback after IDLE must not publish phantom bytes. */
  response_size = 140; memset(response, 0x55, 140); deliver(140);
  uint32_t produced = s_rx_produced;
  HAL_UARTEx_RxEventCallback(&uart, RX_DMA_SIZE / 2);
  assert(s_rx_produced == produced);
  puts("PASS tick rollover and delayed HT notification");
  return 0;
}
