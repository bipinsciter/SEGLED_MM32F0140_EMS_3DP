/***********************************************************************************************************************
    @file    mm32f0140_it.c
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
#define _MM32F0140_IT_C_

/* Files include */
#include "platform.h"
#include "mm32f0140_it.h"
#include "sb_global_var.h"

/**
  * @addtogroup MM32F0140_LibSamples
  * @{
  */

/**
  * @addtogroup GPIO
  * @{
  */

/**
  * @addtogroup GPIO_LED_Toggle
  * @{
  */

/* Private typedef ****************************************************************************************************/

/* Private define *****************************************************************************************************/

/* Private macro ******************************************************************************************************/

/* Private variables **************************************************************************************************/
volatile bool TIM1_UpdateFlag;

/* Private functions **************************************************************************************************/

/***********************************************************************************************************************
  * @brief  This function handles NMI exception
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void NMI_Handler(void)
{
}

/***********************************************************************************************************************
  * @brief  This function handles Hard Fault exception
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void HardFault_Handler(void)
{
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1)
    {
    }
}

/***********************************************************************************************************************
  * @brief  This function handles SVCall exception
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void SVC_Handler(void)
{
}

/***********************************************************************************************************************
  * @brief  This function handles PendSVC exception
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void PendSV_Handler(void)
{
}

/***********************************************************************************************************************
  * @brief  This function handles SysTick Handler
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void SysTick_Handler(void)
{
    if (0 != PLATFORM_DelayTick)
    {
        PLATFORM_DelayTick--;
    }
}

/***********************************************************************************************************************
  * @brief  This function handles TIM1_BRK_UP_TRG_COM Handler
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    if (RESET != TIM_GetITStatus(TIM1, TIM_IT_Update))
    {
		static uint8_t mcnt=0,mcnt1=0,mcnt2=0;
	
		bool_keyScan_flag = 1;		//key scan runs on the 50 ms tick
		
		//Free-running 50 ms tick.  Never reset and never read here - whileTask()
		//tracks the DIFFERENCE since it last looked, so a pass that overruns a tick
		//catches up instead of losing it.  The boolean flags above cannot do that:
		//setting one that is already set is a no-op, so the tick simply vanishes.
		gu16_tick50++;
		
		//Buzzer cadence is timed here, not in whileTask().  Those flags above are
		//booleans: a pass of whileTask() takes about 85 ms because it refreshes the
		//display every time, so ticks are regularly missed and anything counting
		//them runs slow.  BuzzerTick() only decrements counters and toggles a GPIO.
		BuzzerTick();
		NearBlinkTick();
		
		mcnt2++;
		if(mcnt2>=5)
		{
			mcnt2=0;
			bool_msec250_flag = 1;
		}
	
		mcnt1++;
		if(mcnt1>=10)
		{
			mcnt1=0;		
			bool_mec500_blink_flag ^= 1;
			
			bool_dp_sw_factor_add[DP1] = 1;
			bool_dp_sw_factor_add[DP2] = 1;
			bool_dp_sw_factor_add[DP3] = 1;
		}
		
		//---------------------------------------------
		mcnt++;
		if(mcnt>=20)
		{
			mcnt=0;
			bool_sec_flag=1;
		}
		//---------------------------------------------
	
        //TIM1_UpdateFlag = 1;

        TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
    }
}

/***********************************************************************************************************************
  * @brief  This function handles UART1 Handler
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void UART1_IRQHandler(void)
{
    uint8_t RxData = 0;

    if (RESET != UART_GetITStatus(UART1, UART_IT_RXIEN))
    {
        RxData = (uint8_t)UART_ReceiveData(UART1);

        UART_ClearITPendingBit(UART1, UART_IT_RXIEN);
		
		if(!bool_msgRcvOK)
		{
			if((RxData==0xFF) && (!gu8_rxMode))
			{
				RxBuffer1[RxInd++]=RxData;
				gu8_rxMode=1;
				RxTimeout=4;
			}
			else if((RxData==0xEA) && (!gu8_rxMode))
			{
				RxBuffer1[RxInd++]=RxData;
				gu8_rxMode=2;
				RxTimeout=4;	
			}
			else if(gu8_rxMode==1)
			{
				if((RxData==DeviceID) || (RxData==0x00))
				{
					RxBuffer1[RxInd++]=RxData;
					gu8_rxMode=3;
					RxTimeout=4;
				}
				else
				{
					gu8_rxMode=0;
					RxTimeout=0;
					RxInd=0;
				}
			}
			else if(gu8_rxMode==2)
			{
				RxBuffer1[RxInd++]=RxData;
				
				if(RxInd==9)
				{
					if(!memcmp(&RxBuffer1[1],&gu8ar_SrNumber[8],8))
					{
						gu8_rxMode=3;
						RxInd=1;
						RxTimeout=4;
					}
					else
					{
						gu8_rxMode=0;
						RxTimeout=0;
						RxInd=0;
					}
				}
			}
			else if(gu8_rxMode==3)
			{
				RxBuffer1[RxInd++]=RxData;
				RxTimeout=4;

				if((RxData==0xFE) || (RxData==0xEB))
				{
					bool_msgRcvOK=1;
				}
			}
			if(RxInd>=RX_IND_MAX) RxInd=0;
		}
    }

//    if (RESET != UART_GetITStatus(UART1, UART_IT_TXIEN))
//    {
//        UART_ClearITPendingBit(UART1, UART_IT_TXIEN);

//        if (0 == UART_TxStruct.CompleteFlag)
//        {
//            UART_SendData(UART1, UART_TxStruct.Buffer[UART_TxStruct.CurrentCount++]);

//            if (UART_TxStruct.CurrentCount == UART_TxStruct.Length)
//            {
//                UART_TxStruct.CompleteFlag = 1;

//                UART_ITConfig(UART1, UART_IT_TXIEN, DISABLE);
//            }
//        }
//    }
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

