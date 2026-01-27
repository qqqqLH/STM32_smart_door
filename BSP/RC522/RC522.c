#include "RC522.h"

static void Delay_us(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000U) * us / 4U;
    while (cycles--)
    {
        __NOP();
    }
}

void RC522_Init(void)
{
        HAL_GPIO_WritePin(GPIO_CS_Port, GPIO_CS_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIO_SCK_Port, GPIO_SCK_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIO_MOSI_Port, GPIO_MOSI_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIO_RST_Port, GPIO_RST_Pin, GPIO_PIN_SET);
}

void RC522_SPI_SendByte( uint8_t byte )
{
        uint8_t n;
        for( n=0;n<8;n++ )
        {
                if( byte&0x80 )
                        RC522_MOSI_1();
                else
                        RC522_MOSI_0();

                Delay_us(5);
                RC522_SCK_0();
                Delay_us(5);
                RC522_SCK_1();
                Delay_us(5);

                byte<<=1;
        }
}

uint8_t RC522_SPI_ReadByte( void )
{
        uint8_t n,data;
        for( n=0;n<8;n++ )
        {
                data<<=1;
                RC522_SCK_0();
                Delay_us(5);

                if( RC522_MISO_GET()==1 )
                        data|=0x01;

                Delay_us(5);
                RC522_SCK_1();
                Delay_us(5);

        }
        return data;
}

uint8_t RC522_Read_Register( uint8_t Address )
{
        uint8_t data,Addr;

        Addr = ( (Address<<1)&0x7E )|0x80;

        RC522_CS_Enable();
        RC522_SPI_SendByte( Addr );
        data = RC522_SPI_ReadByte();
        RC522_CS_Disable();

        return data;
}

void RC522_Write_Register( uint8_t Address, uint8_t data )
{
        uint8_t Addr;

        Addr = ( Address<<1 )&0x7E;

        RC522_CS_Enable();
        RC522_SPI_SendByte( Addr );
        RC522_SPI_SendByte( data );
        RC522_CS_Disable();

}

void RC522_SetBit_Register( uint8_t Address, uint8_t mask )
{
        uint8_t temp;

        temp = RC522_Read_Register( Address );

        RC522_Write_Register( Address, temp|mask );
}

void RC522_ClearBit_Register( uint8_t Address, uint8_t mask )
{
        uint8_t temp;

        temp = RC522_Read_Register( Address );

        RC522_Write_Register( Address, temp&(~mask) );
}

void RC522_Antenna_On( void )
{
        uint8_t k;
        k = RC522_Read_Register( TxControlReg );

        if( !( k&0x03 ) )
                RC522_SetBit_Register( TxControlReg, 0x03 );
}

void RC522_Antenna_Off( void )
{

        RC522_ClearBit_Register( TxControlReg, 0x03 );
}

void RC522_Rese( void )
{
  RC522_Reset_Disable();
  Delay_us ( 1 );
  RC522_Reset_Enable();
  Delay_us ( 1 );
  RC522_Reset_Disable();
  Delay_us ( 1 );
  RC522_Write_Register( CommandReg, 0x0F );
  while( RC522_Read_Register( CommandReg )&0x10 )
      ;

  Delay_us ( 1 );
  RC522_Write_Register( ModeReg, 0x3D );
  RC522_Write_Register( TReloadRegL, 30 );
  RC522_Write_Register( TReloadRegH, 0 );
  RC522_Write_Register( TModeReg, 0x8D );
  RC522_Write_Register( TPrescalerReg, 0x3E );
  RC522_Write_Register( TxAutoReg, 0x40 );
}

void RC522_Config_Type( char Type )
{
  if( Type=='A' )
  {
    RC522_ClearBit_Register( Status2Reg, 0x08 );
    RC522_Write_Register( ModeReg, 0x3D );
    RC522_Write_Register( RxSelReg, 0x86 );
    RC522_Write_Register( RFCfgReg, 0x7F );
    RC522_Write_Register( TReloadRegL, 30 );
    RC522_Write_Register( TReloadRegH, 0 );
    RC522_Write_Register( TModeReg, 0x8D );
    RC522_Write_Register( TPrescalerReg, 0x3E );
    Delay_us(2);

    RC522_Antenna_On();
  }
}

char PcdComMF522 ( uint8_t ucCommand, uint8_t * pInData, uint8_t ucInLenByte, uint8_t * pOutData, uint32_t * pOutLenBit )
{
    char cStatus = MI_ERR;
    uint8_t ucIrqEn   = 0x00;
    uint8_t ucWaitFor = 0x00;
    uint8_t ucLastBits;
    uint8_t ucN;
    uint32_t ul;

    switch ( ucCommand )
    {
       case PCD_AUTHENT:
          ucIrqEn   = 0x12;
          ucWaitFor = 0x10;
          break;

       case PCD_TRANSCEIVE:
          ucIrqEn   = 0x77;
          ucWaitFor = 0x30;
          break;

       default:
         break;

    }

    RC522_Write_Register ( ComIEnReg, ucIrqEn | 0x80 );
    RC522_ClearBit_Register ( ComIrqReg, 0x80 );
    RC522_Write_Register ( CommandReg, PCD_IDLE );
    RC522_SetBit_Register ( FIFOLevelReg, 0x80 );

    for ( ul = 0; ul < ucInLenByte; ul ++ )
                  RC522_Write_Register ( FIFODataReg, pInData [ ul ] );

    RC522_Write_Register ( CommandReg, ucCommand );

    if ( ucCommand == PCD_TRANSCEIVE )
                        RC522_SetBit_Register(BitFramingReg,0x80);

    ul = 1000;

    do
    {
         ucN = RC522_Read_Register ( ComIrqReg );
         ul --;
    } while ( ( ul != 0 ) && ( ! ( ucN & 0x01 ) ) && ( ! ( ucN & ucWaitFor ) ) );

    RC522_ClearBit_Register ( BitFramingReg, 0x80 );

    if ( ul != 0 )
    {
    if ( ! ( RC522_Read_Register ( ErrorReg ) & 0x1B ) )
    {
      cStatus = MI_OK;

      if ( ucN & ucIrqEn & 0x01 )
        cStatus = MI_NOTAGERR;

      if ( ucCommand == PCD_TRANSCEIVE )
      {
          ucN = RC522_Read_Register ( FIFOLevelReg );

          ucLastBits = RC522_Read_Register ( ControlReg ) & 0x07;

          if ( ucLastBits )
              * pOutLenBit = ( ucN - 1 ) * 8 + ucLastBits;
          else
              * pOutLenBit = ucN * 8;

        if ( ucN == 0 )ucN = 1;

        if ( ucN > MAXRLEN )ucN = MAXRLEN;

        for ( ul = 0; ul < ucN; ul ++ )pOutData [ ul ] = RC522_Read_Register ( FIFODataReg );
        }
      }
  else
    cStatus = MI_ERR;
    }

   RC522_SetBit_Register ( ControlReg, 0x80 );
   RC522_Write_Register ( CommandReg, PCD_IDLE );

   return cStatus;
}

char PcdRequest ( uint8_t ucReq_code, uint8_t * pTagType )
{
   char cStatus;
   uint8_t ucComMF522Buf [ MAXRLEN ];
   uint32_t ulLen;

   RC522_ClearBit_Register ( Status2Reg, 0x08 );
   RC522_Write_Register ( BitFramingReg, 0x07 );
   RC522_SetBit_Register ( TxControlReg, 0x03 );

   ucComMF522Buf [ 0 ] = ucReq_code;

   cStatus = PcdComMF522 ( PCD_TRANSCEIVE,        ucComMF522Buf, 1, ucComMF522Buf, & ulLen );

   if ( ( cStatus == MI_OK ) && ( ulLen == 0x10 ) )
   {

       * pTagType = ucComMF522Buf [ 0 ];
       * ( pTagType + 1 ) = ucComMF522Buf [ 1 ];
   }
   else
     cStatus = MI_ERR;
     return cStatus;
}

char PcdAnticoll ( uint8_t * pSnr )
{
    char cStatus;
    uint8_t uc, ucSnr_check = 0;
    uint8_t ucComMF522Buf [ MAXRLEN ];
          uint32_t ulLen;

    RC522_ClearBit_Register ( Status2Reg, 0x08 );
    RC522_Write_Register ( BitFramingReg, 0x00);
    RC522_ClearBit_Register ( CollReg, 0x80 );

    ucComMF522Buf [ 0 ] = 0x93;
    ucComMF522Buf [ 1 ] = 0x20;

    cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 2, ucComMF522Buf, & ulLen);

    if ( cStatus == MI_OK)
    {
                        for ( uc = 0; uc < 4; uc ++ )
                        {
         * ( pSnr + uc )  = ucComMF522Buf [ uc ];
         ucSnr_check ^= ucComMF522Buf [ uc ];
      }

      if ( ucSnr_check != ucComMF522Buf [ uc ] )
                                cStatus = MI_ERR;
    }
    RC522_SetBit_Register ( CollReg, 0x80 );
    return cStatus;
}

void CalulateCRC ( uint8_t * pIndata, u8 ucLen, uint8_t * pOutData )
{
    uint8_t uc, ucN;

    RC522_ClearBit_Register(DivIrqReg,0x04);
    RC522_Write_Register(CommandReg,PCD_IDLE);
    RC522_SetBit_Register(FIFOLevelReg,0x80);

    for ( uc = 0; uc < ucLen; uc ++)
            RC522_Write_Register ( FIFODataReg, * ( pIndata + uc ) );

    RC522_Write_Register ( CommandReg, PCD_CALCCRC );

    uc = 0xFF;

    do
    {
        ucN = RC522_Read_Register ( DivIrqReg );
        uc --;
    } while ( ( uc != 0 ) && ! ( ucN & 0x04 ) );

    pOutData [ 0 ] = RC522_Read_Register ( CRCResultRegL );
    pOutData [ 1 ] = RC522_Read_Register ( CRCResultRegM );

}

char PcdSelect ( uint8_t * pSnr )
{
    char ucN;
    uint8_t uc;
          uint8_t ucComMF522Buf [ MAXRLEN ];
    uint32_t  ulLen;

    ucComMF522Buf [ 0 ] = PICC_ANTICOLL1;
    ucComMF522Buf [ 1 ] = 0x70;
    ucComMF522Buf [ 6 ] = 0;

    for ( uc = 0; uc < 4; uc ++ )
    {
            ucComMF522Buf [ uc + 2 ] = * ( pSnr + uc );
            ucComMF522Buf [ 6 ] ^= * ( pSnr + uc );
    }

    CalulateCRC ( ucComMF522Buf, 7, & ucComMF522Buf [ 7 ] );

    RC522_ClearBit_Register ( Status2Reg, 0x08 );

    ucN = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 9, ucComMF522Buf, & ulLen );

    if ( ( ucN == MI_OK ) && ( ulLen == 0x18 ) )
      ucN = MI_OK;
    else
      ucN = MI_ERR;

    return ucN;

}

char PcdAuthState ( uint8_t ucAuth_mode, uint8_t ucAddr, uint8_t * pKey, uint8_t * pSnr )
{
    char cStatus;
          uint8_t uc, ucComMF522Buf [ MAXRLEN ];
    uint32_t ulLen;

    ucComMF522Buf [ 0 ] = ucAuth_mode;
    ucComMF522Buf [ 1 ] = ucAddr;

    for ( uc = 0; uc < 6; uc ++ )
            ucComMF522Buf [ uc + 2 ] = * ( pKey + uc );

    for ( uc = 0; uc < 6; uc ++ )
            ucComMF522Buf [ uc + 8 ] = * ( pSnr + uc );

    cStatus = PcdComMF522 ( PCD_AUTHENT, ucComMF522Buf, 12, ucComMF522Buf, & ulLen );

    if ( ( cStatus != MI_OK ) || ( ! ( RC522_Read_Register ( Status2Reg ) & 0x08 ) ) )
      cStatus = MI_ERR;

    return cStatus;

}

char PcdWrite ( uint8_t ucAddr, uint8_t * pData )
{
    char cStatus;
          uint8_t uc, ucComMF522Buf [ MAXRLEN ];
    uint32_t ulLen;

    ucComMF522Buf [ 0 ] = PICC_WRITE;
    ucComMF522Buf [ 1 ] = ucAddr;

    CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );

    cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, & ulLen );

    if ( ( cStatus != MI_OK ) || ( ulLen != 4 ) || ( ( ucComMF522Buf [ 0 ] & 0x0F ) != 0x0A ) )
      cStatus = MI_ERR;

    if ( cStatus == MI_OK )
    {

      for ( uc = 0; uc < 16; uc ++ )
                          ucComMF522Buf [ uc ] = * ( pData + uc );

      CalulateCRC ( ucComMF522Buf, 16, & ucComMF522Buf [ 16 ] );

      cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 18, ucComMF522Buf, & ulLen );

                        if ( ( cStatus != MI_OK ) || ( ulLen != 4 ) || ( ( ucComMF522Buf [ 0 ] & 0x0F ) != 0x0A ) )
        cStatus = MI_ERR;
    }
    return cStatus;
}

char PcdRead ( uint8_t ucAddr, uint8_t * pData )
{
    char cStatus;
          uint8_t uc, ucComMF522Buf [ MAXRLEN ];
    uint32_t ulLen;

    ucComMF522Buf [ 0 ] = PICC_READ;
    ucComMF522Buf [ 1 ] = ucAddr;

    CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );

    cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, & ulLen );

    if ( ( cStatus == MI_OK ) && ( ulLen == 0x90 ) )
    {
                        for ( uc = 0; uc < 16; uc ++ )
        * ( pData + uc ) = ucComMF522Buf [ uc ];
    }
    else
      cStatus = MI_ERR;

    return cStatus;

}

char PcdHalt( void )
{
        uint8_t ucComMF522Buf [ MAXRLEN ];
        uint32_t  ulLen;

  ucComMF522Buf [ 0 ] = PICC_HALT;
  ucComMF522Buf [ 1 ] = 0;

  CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );
         PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, & ulLen );

  return MI_OK;
}
