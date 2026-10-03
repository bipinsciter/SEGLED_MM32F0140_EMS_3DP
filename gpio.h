/***********************************************************************************************************************
    @file    gpio_led_toggle.h
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
#ifndef _GPIO_LED_TOGGLE_H_
#define _GPIO_LED_TOGGLE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Files include */
#include "hal_conf.h"

/* Exported types *****************************************************************************************************/

/* Exported constants *************************************************************************************************/
/**
  * @addtogroup GPIO
  * @{
  */
#define LED1_GREEN_ON		GPIO_ResetBits(GPIOA, GPIO_Pin_5)
#define LED1_GREEN_OFF		GPIO_SetBits(GPIOA, GPIO_Pin_5)
#define LED1_GREEN_TOGGLE	GPIO_WriteBit(GPIOA, GPIO_Pin_5, GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_5) ? Bit_RESET : Bit_SET);

#define LED1_RED_ON			GPIO_ResetBits(GPIOA, GPIO_Pin_6)
#define LED1_RED_OFF		GPIO_SetBits(GPIOA, GPIO_Pin_6)
#define LED1_RED_TOGGLE		GPIO_WriteBit(GPIOA, GPIO_Pin_6, GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_6) ? Bit_RESET : Bit_SET);

#define LED2_GREEN_ON		GPIO_ResetBits(GPIOA, GPIO_Pin_7)
#define LED2_GREEN_OFF		GPIO_SetBits(GPIOA, GPIO_Pin_7)
#define LED2_GREEN_TOGGLE	GPIO_WriteBit(GPIOA, GPIO_Pin_7, GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_7) ? Bit_RESET : Bit_SET);

#define LED2_RED_ON			GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define LED2_RED_OFF		GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define LED2_RED_TOGGLE		GPIO_WriteBit(GPIOB, GPIO_Pin_0, GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_0) ? Bit_RESET : Bit_SET);

#define BUZZER_ON		GPIO_SetBits(GPIOB, GPIO_Pin_11)
#define BUZZER_OFF		GPIO_ResetBits(GPIOB, GPIO_Pin_11)
#define BUZZER_TOGGLE	GPIO_WriteBit(GPIOB, GPIO_Pin_11, GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_11) ? Bit_RESET : Bit_SET);

#define XBEE_RST_HIGH	GPIO_SetBits(GPIOB, GPIO_Pin_4)
#define XBEE_RST_LOW	GPIO_ResetBits(GPIOB, GPIO_Pin_4)

#define RELAY1_ON		GPIO_SetBits(GPIOA, GPIO_Pin_3)
#define RELAY1_OFF		GPIO_ResetBits(GPIOA, GPIO_Pin_3)
#define RELAY1_STAT  	GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3)
#define RELAY1_TOGGLE	GPIO_WriteBit(GPIOA, GPIO_Pin_3, GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_3) ? Bit_RESET : Bit_SET);

#define RELAY2_ON		GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define RELAY2_OFF		GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define RELAY2_STAT  	GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4)
#define RELAY2_TOGGLE	GPIO_WriteBit(GPIOA, GPIO_Pin_4, GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_4) ? Bit_RESET : Bit_SET);

#define RS485_TX_ENB	GPIO_SetBits(GPIOB, GPIO_Pin_5)
#define RS485_RX_ENB	GPIO_ResetBits(GPIOB, GPIO_Pin_5)

#define DOOR_SENSE  	GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10)

#define PROG_ENT_KEY			GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_3)
#define PARA_SELECT_KEY			GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_15)
#define UP_KEY					GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_3)
#define DN_KEY					GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_2)

#define RESERVE_KEY				GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1)
#define DP_MANIP_KEY			GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_2)

#define INPUT1_SENSE			GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1)
#define INPUT2_SENSE			GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2)

/* Exported macro *****************************************************************************************************/

/* Exported variables *************************************************************************************************/

/* Exported functions *************************************************************************************************/

void GPIO_Configure(void);

void ADC_Configure(void);

void GPIO_Configure_Input(GPIO_TypeDef* gpio,u16 pin);
	
void GPIO_Configure_Output(GPIO_TypeDef* gpio,u16 pin);
	

#ifdef __cplusplus
}
#endif

#endif /* _GPIO_LED_TOGGLE_H_ */

/********************************************** (C) Copyright MindMotion **********************************************/

