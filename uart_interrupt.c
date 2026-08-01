/***********************************************************************************************************************
    @file    uart_interrupt.c
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
#define _UART_INTERRUPT_C_

/* Files include */
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "uart_interrupt.h"
#include "sb_global_var.h"

/**
  * @addtogroup MM32F0140_LibSamples
  * @{
  */

/**
  * @addtogroup UART
  * @{
  */

/**
  * @addtogroup UART_Interrupt
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
void UART_Configure(uint8_t Baud)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    UART_InitTypeDef UART_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2ENR_UART1, ENABLE);
	
	uint32_t Baudrate=56700;
	
	switch(Baud)
	{
		case BAUD_1200:		Baudrate=1200;		break;
		case BAUD_2400:		Baudrate=2400;		break;
		case BAUD_4800:		Baudrate=4800;		break;
		case BAUD_9600:		Baudrate=9600;		break;
		case BAUD_14400:	Baudrate=14400;		break;
		case BAUD_19200:	Baudrate=19200;		break;
		case BAUD_28800:	Baudrate=28800;		break;
		case BAUD_38400:	Baudrate=38400;		break;
		case BAUD_57600:	Baudrate=57600;		break;
		case BAUD_115200:	Baudrate=115200;	break;
		default:			Baudrate=57600;		break;
	}

    UART_StructInit(&UART_InitStruct);
    UART_InitStruct.BaudRate      = Baudrate;
    UART_InitStruct.WordLength    = UART_WordLength_8b;
    UART_InitStruct.StopBits      = UART_StopBits_1;
    UART_InitStruct.Parity        = UART_Parity_No;
    UART_InitStruct.HWFlowControl = UART_HWFlowControl_None;
    UART_InitStruct.Mode          = UART_Mode_Rx | UART_Mode_Tx;
    UART_Init(UART1, &UART_InitStruct);
	
    RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOB, ENABLE);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_0);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_0);

    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_6;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    NVIC_InitStruct.NVIC_IRQChannel = UART1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 0x01;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
	
	UART_ITConfig(UART1, UART_IT_RXIEN, ENABLE);
	
    UART_Cmd(UART1, ENABLE);
}

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void UART_RxData_Interrupt(uint8_t Length)
{
    uint8_t i = 0;

    for (i = 0; i < Length; i++)
    {
        UART_RxStruct.Buffer[i] = 0;
    }

    UART_RxStruct.Length = Length;
    UART_RxStruct.CurrentCount = 0;
    UART_RxStruct.CompleteFlag = 0;

    UART_ITConfig(UART1, UART_IT_RXIEN, ENABLE);
}

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void UART_TxData_Interrupt(uint8_t *Buffer, uint8_t Length)
{
    uint8_t i = 0;

    for (i = 0; i < Length; i++)
    {
        UART_TxStruct.Buffer[i] = Buffer[i];
    }

    UART_TxStruct.Length = Length;
    UART_TxStruct.CurrentCount = 0;
    UART_TxStruct.CompleteFlag = 0;

    UART_ITConfig(UART1, UART_IT_TXIEN, ENABLE);
}

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void UART_Interrupt_Sample(void)
{
    printf("\r\nTest %s", __FUNCTION__);

    UART_RxStruct.CompleteFlag = 0;
    UART_TxStruct.CompleteFlag = 1;

    UART_Configure(BAUD_115200);

    UART_RxData_Interrupt(10);

    printf("\r\nSend 10 bytes to UART every time");

    while (1)
    {
        if (0 != UART_RxStruct.CompleteFlag)
        {
            UART_TxData_Interrupt((uint8_t *)UART_RxStruct.Buffer, UART_RxStruct.Length);

            while (0 == UART_TxStruct.CompleteFlag)
            {
            }

            UART_RxData_Interrupt(10);
        }
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

