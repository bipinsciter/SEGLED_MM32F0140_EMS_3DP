/***********************************************************************************************************************
    @file    spi_master_polling.c
    @author  FAE Team
    @date    25-May-2023
    @brief   THIS FILE PROVIDES ALL THE SYSTEM FUNCTIONS.
  **********************************************************************************************************************
    @attention

    <h2><center>&copy; Copyright(c) <2023> <MindMotion></center></h2>

      Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
    following conditions are met:
    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
       the following disclaimer in the documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
       promote products derived from this software without specific prior written permission.

      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *********************************************************************************************************************/

/* Define to prevent recursive inclusion */
#define _SPI_MASTER_POLLING_C_

/* Files include */
#include <stdio.h>
#include "platform.h"
#include "spi_master_polling.h"

/**
  * @addtogroup MM32F0140_LibSamples
  * @{
  */

/**
  * @addtogroup SPI
  * @{
  */

/**
  * @addtogroup SPI_Master_Polling
  * @{
  */

/* Private typedef ****************************************************************************************************/

/* Private define *****************************************************************************************************/

/* Private macro ******************************************************************************************************/


/* Private variables **************************************************************************************************/

/* Private functions **************************************************************************************************/

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void SPI_Configure(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    SPI_InitTypeDef  SPI_InitStruct;

    RCC_APB1PeriphClockCmd(RCC_APB1ENR_SPI2, ENABLE);

    SPI_StructInit(&SPI_InitStruct);
    SPI_InitStruct.SPI_Mode      = SPI_Mode_Master;
    SPI_InitStruct.SPI_DataSize  = SPI_DataSize_8b;
    SPI_InitStruct.SPI_DataWidth = 8;
    SPI_InitStruct.SPI_CPOL      = SPI_CPOL_Low;
    SPI_InitStruct.SPI_CPHA      = SPI_CPHA_1Edge;
    SPI_InitStruct.SPI_NSS       = SPI_NSS_Soft;
    //SPI2 hangs off APB1.  SystemInit() runs the core at 72 MHz with HCLK = SYSCLK
    //and PCLK1 = HCLK/2, so the SPI kernel clock is 36 MHz and /2 (18 MHz) is the
    //fastest SCK this MCU can produce on this bus.  It used to run at /256 = 140 kHz,
    //where a single 256-byte page transfer took about 15 ms on its own.
    //
    //This bus carries nothing but the data flash, and 18 MHz is well inside both
    //parts' ratings:
    //
    //  AT45DB321D  fCAR1  66 MHz   continuous array read, opcode 0xE8 - the one the
    //                              driver uses.  (fCAR2, the 0x03 low-frequency read,
    //                              is 33 MHz, but that opcode is never issued here.)
    //  XM25QH128A  fR     50 MHz   Read Data 0x03.  Everything else on that part -
    //                              program, erase, status, ID - is rated fC = 104 MHz,
    //                              so the plain read is the binding limit at 50 MHz.
    //
    //18 MHz is therefore about a third of the lower of the two ceilings, which leaves
    //room for board parasitics.  Drop to _4 (9 MHz) or _8 (4.5 MHz) if a board ever
    //shows read/write corruption - XM25_ENABLE_SELFTEST in sb_const.h builds the
    //end-to-end check that will catch it.
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStruct.SPI_FirstBit  = SPI_FirstBit_MSB;
    SPI_Init(SPI2, &SPI_InitStruct);

    SPI_BiDirectionalLineConfig(SPI2, SPI_Direction_Rx);
    SPI_BiDirectionalLineConfig(SPI2, SPI_Direction_Tx);

    RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOB, ENABLE);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF_0);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_0);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_0);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_0);

    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_14;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    SPI_Cmd(SPI2, ENABLE);
}

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void SPI_RxData_Polling(uint8_t *Buffer, uint16_t Length)
{
    uint16_t i = 0, Data = 0;

    for (i = 0; i < Length; i++)
    {
        SPI_SendData(SPI2, Data);

        while (RESET == SPI_GetFlagStatus(SPI2, SPI_FLAG_TXEPT))
        {
        }

        while (RESET == SPI_GetFlagStatus(SPI2, SPI_FLAG_RXAVL))
        {
        }

        Buffer[i] = SPI_ReceiveData(SPI2);
    }
}

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void SPI_TxData_Polling(uint8_t *Buffer, uint16_t Length)
{
    uint16_t i = 0;

    for (i = 0; i < Length; i++)
    {
        SPI_SendData(SPI2, Buffer[i]);

        while (RESET == SPI_GetFlagStatus(SPI2, SPI_FLAG_TXEPT))
        {
        }

        while (RESET == SPI_GetFlagStatus(SPI2, SPI_FLAG_RXAVL))
        {
        }

        SPI_ReceiveData(SPI2);
    }
}

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void SPI_ReadBuffer(uint8_t *Buffer, uint8_t Length)
{
    if (Length)
    {
        SPI_FLASH_CS_L();
        SPI_RxData_Polling(Buffer, Length);
        SPI_FLASH_CS_H();
    }
}

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void SPI_WriteBuffer(uint8_t *Buffer, uint8_t Length)
{
    if (Length)
    {
        SPI_FLASH_CS_L();
        SPI_TxData_Polling(Buffer, Length);
        SPI_FLASH_CS_H();
    }
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/********************************************** (C) Copyright MindMotion **********************************************/

