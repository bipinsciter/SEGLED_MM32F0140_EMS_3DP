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
//---------------------------------------------------------------------------------
// A hard fault used to sit here in an empty loop until the watchdog noticed.
//
// That is indistinguishable, from outside, from any other hang: the device simply
// stops answering and comes back ten seconds later with the watchdog flag set.  The
// stacked exception frame holds the address of the instruction that faulted, which
// names the function once it is looked up in the .map, so send it before the reset
// rather than throwing it away.
//
// Everything below pokes the UART directly.  SendToUART() cannot be used here: it
// touches globals that the faulting code may have been halfway through updating, and
// this code has to work when the rest of the firmware is not trustworthy.
//---------------------------------------------------------------------------------
volatile uint32_t gu32_faultPC = 0;
volatile uint32_t gu32_faultLR = 0;

static void FaultPutByte(uint8_t b)
{
    uint32_t spin = 200000UL;

    UART1->ICR = UART_ICR_TX;
    UART1->TDR = b;
    while(spin-- && !(UART1->ISR & UART_ISR_TX)) {}
}

static void FaultPutHex(uint32_t v)
{
    static const char digits[] = "0123456789ABCDEF";
    int8_t i;

    for(i = 28; i >= 0; i -= 4) FaultPutByte((uint8_t)digits[(v >> i) & 0x0F]);
}

void HardFaultReport(uint32_t *frame)
{
    const char *tag = "\r\nHARDFAULT PC=";
    uint32_t spin;

    //Exception entry stacks r0 r1 r2 r3 r12 LR PC xPSR, so the faulting address is
    //the seventh word and the return address of its caller the sixth.
    gu32_faultPC = frame[6];
    gu32_faultLR = frame[5];

    //Drive the RS485 transceiver ourselves - PB5, the same pin RS485_TX_ENB uses.
    GPIOB->BSRR = (1u << 5);
    for(spin = 0; spin < 2000; spin++) { __NOP(); }

    while(*tag) FaultPutByte((uint8_t)*tag++);
    FaultPutHex(gu32_faultPC);
    FaultPutByte(' '); FaultPutByte('L'); FaultPutByte('R'); FaultPutByte('=');
    FaultPutHex(gu32_faultLR);
    FaultPutByte('\r'); FaultPutByte('\n');

    //Let the last byte leave before releasing the driver, then stop.  The watchdog
    //restarts the device, which is what used to happen anyway - only now the host
    //has been told why.
    spin = 200000UL;
    while(spin-- && !(UART1->ISR & UART_ISR_TXC)) {}
    GPIOB->BRR = (1u << 5);

    while(1) {}
}

__attribute__((naked)) void HardFault_Handler(void)
{
    //No RTOS here, so the frame is always on MSP.  bl reaches anywhere in a 4 MB
    //image, which b on a Cortex-M0 does not.
    __asm volatile ("mrs r0, msp\n\t"
                    "bl  HardFaultReport\n\t");
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

