#include "oled.h"
#include "oledfont.h"

static void OLED_I2C_Delay(void)
{
  for (volatile uint32_t i = 0; i < 30; i++)
  {
    __NOP();
  }
}

void delay_ms(unsigned int ms)
{
  HAL_Delay(ms);
}

void OLED_GPIO_Init(void)
{
  OLED_SCLK_Set();
  OLED_SDIN_Set();
}

void IIC_Start(void)
{
  OLED_SDIN_Set();
  OLED_SCLK_Set();
  OLED_I2C_Delay();
  OLED_SDIN_Set();
  OLED_I2C_Delay();
  OLED_SDIN_Clr();
  OLED_I2C_Delay();
  OLED_SCLK_Clr();
  OLED_I2C_Delay();
}

void IIC_Stop(void)
{
  OLED_SDIN_Clr();
  OLED_I2C_Delay();
  OLED_SCLK_Set();
  OLED_I2C_Delay();
  OLED_SDIN_Set();
  OLED_I2C_Delay();
}

void IIC_Wait_Ack(void)
{
  OLED_SDIN_Set();
  OLED_I2C_Delay();
  OLED_SCLK_Set();
  OLED_I2C_Delay();
  OLED_SCLK_Clr();
  OLED_I2C_Delay();
}

void Write_IIC_Byte(unsigned char IIC_Byte)
{
  unsigned char i;
  unsigned char m,da;
  da=IIC_Byte;
  OLED_SCLK_Clr();
  for(i=0;i<8;i++)
  {
      m=da;
    m=m&0x80;
    if(m==0x80)
    {OLED_SDIN_Set();}
    else OLED_SDIN_Clr();
      da=da<<1;
    OLED_I2C_Delay();
    OLED_SCLK_Set();
    OLED_I2C_Delay();
    OLED_SCLK_Clr();
    OLED_I2C_Delay();
    }


}

void Write_IIC_Command(unsigned char IIC_Command)
{
   IIC_Start();
   Write_IIC_Byte(0x78);
  IIC_Wait_Ack();
   Write_IIC_Byte(0x00);
  IIC_Wait_Ack();
   Write_IIC_Byte(IIC_Command);
  IIC_Wait_Ack();
   IIC_Stop();
}

void Write_IIC_Data(unsigned char IIC_Data)
{
   IIC_Start();
   Write_IIC_Byte(0x78);
  IIC_Wait_Ack();
   Write_IIC_Byte(0x40);
  IIC_Wait_Ack();
   Write_IIC_Byte(IIC_Data);
  IIC_Wait_Ack();
   IIC_Stop();
}
void OLED_WR_Byte(unsigned dat,unsigned cmd)
{
  if(cmd)
  {

   Write_IIC_Data(dat);

  }
  else 
  {
    Write_IIC_Command(dat);
  }


}

void fill_picture(unsigned char fill_Data)
{
  unsigned char m,n;
  for(m=0;m<8;m++)
  {
    OLED_WR_Byte(0xb0+m,0);
    OLED_WR_Byte(0x02,0);
    OLED_WR_Byte(0x10,0);
    for(n=0;n<128;n++)
      {
        OLED_WR_Byte(fill_Data,1);
      }
  }
}


void Delay_50ms(unsigned int Del_50ms)
{
  while (Del_50ms--)
  {
    HAL_Delay(50);
  }
}

void Delay_1ms(unsigned int Del_1ms)
{
  HAL_Delay(Del_1ms);
}
  void OLED_Set_Pos(unsigned char x, unsigned char y)
{   OLED_WR_Byte(0xb0+y,OLED_CMD);
  OLED_WR_Byte((((x+2)&0xf0)>>4)|0x10,OLED_CMD);
  OLED_WR_Byte(((x+2)&0x0f),OLED_CMD);
}

void OLED_Display_On(void)
{
  OLED_WR_Byte(0X8D,OLED_CMD);
  OLED_WR_Byte(0X14, OLED_CMD); // DCDC ON
  OLED_WR_Byte(0XAF,OLED_CMD);  //DISPLAY ON
}
//¹رՏLEDϔʾ
void OLED_Display_Off(void)
{
  OLED_WR_Byte(0X8D,OLED_CMD);  //SET DCDCüÁ  OLED_WR_Byte(0X10,OLED_CMD);  //DCDC OFF
  OLED_WR_Byte(0X10, OLED_CMD); // DCDC OFF
  OLED_WR_Byte(0XAE,OLED_CMD);  //DISPLAY OFF
}
//ǥƁº¯ʽ,ǥͪƁ,ջ¸öƁĻʇºډ«µġº̓»µぁһѹ!!!
void OLED_Clear(void)
{
  u8 i,n;
  for(i=0;i<8;i++)
  {
    OLED_WR_Byte (0xb0+i,OLED_CMD);    //ɨփҳµؖ·£¨0~7£©
    OLED_WR_Byte (0x02,OLED_CMD);      //ɨփϔʾλփ¡ªÁе͵ؖ·
    OLED_WR_Byte (0x10,OLED_CMD);      //ɨփϔʾλփ¡ªÁиߵؖ·
    for(n=0;n<128;n++)OLED_WR_Byte(0,OLED_DATA);
  } //¸üЂϔʾ
}
void OLED_On(void)
{
  u8 i,n;
  for(i=0;i<8;i++)
  {
    OLED_WR_Byte (0xb0+i,OLED_CMD);    //ɨփҳµؖ·£¨0~7£©
    OLED_WR_Byte (0x02,OLED_CMD);      //ɨփϔʾλփ¡ªÁе͵ؖ·
    OLED_WR_Byte (0x10,OLED_CMD);      //ɨփϔʾλփ¡ªÁиߵؖ·
    for(n=0;n<128;n++)OLED_WR_Byte(1,OLED_DATA);
  } //¸üЂϔʾ
}
//Ԛָ¶¨λփϔʾһ¸öז·û,°üÀ¨²¿·ַ֗û
//x:0~127
//y:0~63
//mode:0,·´°׏Ԋ¾;1,ս³£ϔʾ
//size:ѡԱז̥ 16/12
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 Char_Size)
{
  unsigned char c=0,i=0;
    c=chr-' ';//µõ½ƫ҆º󵄖µ
    if(x>Max_Column-1){x=0;y=y+2;}
    if(Char_Size ==16)
      {
      OLED_Set_Pos(x,y);
      for(i=0;i<8;i++)
      OLED_WR_Byte(F8X16[c*16+i],OLED_DATA);
      OLED_Set_Pos(x,y+1);
      for(i=0;i<8;i++)
      OLED_WR_Byte(F8X16[c*16+i+8],OLED_DATA);
      }
      else {
        OLED_Set_Pos(x,y);
        for(i=0;i<6;i++)
        OLED_WR_Byte(F6x8[c][i],OLED_DATA);

      }
}
//m^nº¯ʽ
u32 oled_pow(u8 m,u8 n)
{
  u32 result=1;
  while(n--)result*=m;
  return result;
}

void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size2)
{
  u8 t,temp;
  u8 enshow=0;
  for(t=0;t<len;t++)
  {
    temp=(num/oled_pow(10,len-t-1))%10;
    if(enshow==0&&t<(len-1))
    {
      if(temp==0)
      {
        OLED_ShowChar(x+(size2/2)*t,y,' ',size2);
        continue;
      }else enshow=1;

    }
     OLED_ShowChar(x+(size2/2)*t,y,temp+'0',size2);
  }
}
//ϔʾһ¸öז·ûºŴ®
void OLED_ShowString(u8 x,u8 y,u8 *chr,u8 Char_Size)
{
  unsigned char j=0;
  while (chr[j]!='\0')
  {    OLED_ShowChar(x,y,chr[j],Char_Size);
      x+=8;
    if(x>120){x=0;y+=2;}
      j++;
  }
}
//ϔʾººז
void OLED_ShowCHinese(u8 x,u8 y,u8 no)
{
  u8 t,adder=0;
  OLED_Set_Pos(x,y);
    for(t=0;t<16;t++)
    {
        OLED_WR_Byte(Hzk[2*no][t],OLED_DATA);
        adder+=1;
     }
    OLED_Set_Pos(x,y+1);
    for(t=0;t<16;t++)
      {
        OLED_WR_Byte(Hzk[2*no+1][t],OLED_DATA);
        adder+=1;
      }
}
/***********¹¦Ĝèʶ£ºϔʾϔʾBMPͼƬ128¡Á64ưʼµ㗸±꨸,y),xµķ¶Χ0¡«127£¬yΪҳµķ¶Χ0¡«7*****************/
void OLED_DrawBMP(unsigned char x0, unsigned char y0,unsigned char x1, unsigned char y1,unsigned char BMP[])
{
 unsigned int j=0;
 unsigned char x,y;

  if(y1%8==0) y=y1/8;
  else y=y1/8+1;
  for(y=y0;y<y1;y++)
  {
    OLED_Set_Pos(x0,y);
    for(x=x0;x<x1;x++)
      {
        OLED_WR_Byte(BMP[j++],OLED_DATA);
      }
  }
}

//³õʼ»¯SSD1306
void OLED_Init(void)
{
  OLED_GPIO_Init();
  delay_ms(50);

  OLED_WR_Byte(0xAE,OLED_CMD);//--display off
  OLED_WR_Byte(0x00,OLED_CMD);//---set low column address
  OLED_WR_Byte(0x10,OLED_CMD);//---set high column address
  OLED_WR_Byte(0x40,OLED_CMD);//--set start line address
  OLED_WR_Byte(0xB0,OLED_CMD);//--set page address
  OLED_WR_Byte(0x81,OLED_CMD); // contract control
  OLED_WR_Byte(0xFF,OLED_CMD);//--128
  OLED_WR_Byte(0xA1,OLED_CMD);//set segment remap
  OLED_WR_Byte(0xA6,OLED_CMD);//--normal / reverse
  OLED_WR_Byte(0xA8,OLED_CMD);//--set multiplex ratio(1 to 64)
  OLED_WR_Byte(0x3F,OLED_CMD);//--1/32 duty
  OLED_WR_Byte(0xC8,OLED_CMD);//Com scan direction
  OLED_WR_Byte(0xD3,OLED_CMD);//-set display offset
  OLED_WR_Byte(0x00,OLED_CMD);//

  OLED_WR_Byte(0xD5,OLED_CMD);//set osc division
  OLED_WR_Byte(0x80,OLED_CMD);//

  OLED_WR_Byte(0xD8,OLED_CMD);//set area color mode off
  OLED_WR_Byte(0x05,OLED_CMD);//

  OLED_WR_Byte(0xD9,OLED_CMD);//Set Pre-Charge Period
  OLED_WR_Byte(0xF1,OLED_CMD);//

  OLED_WR_Byte(0xDA,OLED_CMD);//set com pin configuartion
  OLED_WR_Byte(0x12,OLED_CMD);//

  OLED_WR_Byte(0xDB,OLED_CMD);//set Vcomh
  OLED_WR_Byte(0x30,OLED_CMD);//

  OLED_WR_Byte(0x8D,OLED_CMD);//set charge pump enable
  OLED_WR_Byte(0x14,OLED_CMD);//

  OLED_WR_Byte(0xAF,OLED_CMD);//--turn on oled panel
}
