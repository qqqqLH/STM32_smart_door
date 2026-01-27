#include "BT.h"
#include "usart.h"

#define BT_RX_BUF_SIZE 256U		// 缓冲区大小
#define BT_TX_TIMEOUT_MS 100U		//发送超时时间：100ms

static uint8_t s_rx_byte;			// 中断接收存储字节
static volatile uint8_t s_rx_buf[BT_RX_BUF_SIZE];		// 缓冲区数组
static volatile uint16_t s_rx_head; //头指针
static volatile uint16_t s_rx_tail;//尾指针

//计算指针的下一个位置
static uint16_t BT_NextIndex(uint16_t idx)
{
  return (uint16_t)((idx + 1U) % BT_RX_BUF_SIZE);
}

// 初始化头尾指针，开启中断
void BT_Init(void)
{
  s_rx_head = 0;
  s_rx_tail = 0;
  HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

// 蓝牙发送一个字节
void BT_SendByte(uint8_t byte)
{
  (void)HAL_UART_Transmit(&huart1, &byte, 1, BT_TX_TIMEOUT_MS);
}

// 蓝牙发送指定长度的字节
void BT_SendBytes(const uint8_t *data, uint16_t len)
{
  if (data == NULL || len == 0)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, BT_TX_TIMEOUT_MS);
}


// 判断是否有数据，并且返回数据长度，如果s_rx_head >= s_rx_tail那么数据长度为s_rx_head - s_rx_tail
// 如果s_rx_head < s_rx_tail,那么说明发生了回绕，长度为 Size - tail + head
uint16_t BT_Available(void)
{
  if (s_rx_head >= s_rx_tail)
  {
    return (uint16_t)(s_rx_head - s_rx_tail);
  }

  return (uint16_t)(BT_RX_BUF_SIZE - s_rx_tail + s_rx_head);
}

// 读取一个字节
int16_t BT_Read(void)
{
  uint8_t data;
	// 如果头等于尾说明没有数据，返回
  if (s_rx_head == s_rx_tail)
  {
    return -1;
  }
	
	//读取数据
  data = s_rx_buf[s_rx_tail];
	//更新尾指针
  s_rx_tail = BT_NextIndex(s_rx_tail);
  return (int16_t)data;//返回读取到的数据
}


//读取指定字节
uint16_t BT_ReadBytes(uint8_t *out, uint16_t len)
{
  uint16_t count = 0;

  if (out == NULL || len == 0)
  {
    return 0;
  }
	//循环读取指定字节
  while (count < len)
  {
    int16_t ch = BT_Read();
    if (ch < 0)
    {
      break;
    }
    out[count++] = (uint8_t)ch;
  }

  return count;
}


//这里会有一字节浪费因为判断的是头指针的下一位是否与尾指针相同，说明中间永远会空出来一个字节不被填写，视频里面讲错了，但是这种环形缓冲区是正确的因为只损耗了一个字节
//但是换来了更加简单的代码且更容易维护
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uint16_t next = BT_NextIndex(s_rx_head);
    if (next != s_rx_tail)
    {
      s_rx_buf[s_rx_head] = s_rx_byte;
      s_rx_head = next;
    }
    //重新开启 UART 的接收中断，准备接收接下来的这 1 个字节，并将其保存到 s_rx_byte 变量中
    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
  }
}
