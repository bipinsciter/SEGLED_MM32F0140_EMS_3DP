/***********************************************************************************************************************
    @file    iwdg_systemmonitor.c
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
#define _IWDG_SYSTEMMONITOR_C_

/* Files include */
#include <stdio.h>
#include "platform.h"
#include "iwdg_systemmonitor.h"

/**
  * @addtogroup MM32F0140_LibSamples
  * @{
  */

/**
  * @addtogroup IWDG
  * @{
  */

/**
  * @addtogroup IWDG_SystemMonitor
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
void IWDG_Configure(uint32_t Timeout)
{
    //Timeout is in MILLISECONDS and now actually means that.
    //
    //This divided by 32 while setting the prescaler to 256, so every requested timeout
    //came out EIGHT TIMES too long: IWDG_Configure(1250) asked for 1.25 s and armed a
    //watchdog of very nearly ten seconds.  Nothing depended on the real figure, so it
    //went unnoticed - but it meant the one number describing how long the firmware may
    //stall was wrong by almost an order of magnitude.
    //
    //  period = Reload * Prescaler / LSI_VALUE
    //
    //The reload register is 12 bits, so with /256 and a 40 kHz LSI the longest possible
    //period is 4095 * 256 / 40000 = 26.2 s.  Anything longer is clamped rather than
    //silently wrapping to a very short one.
    uint32_t lu32_reload = ((uint32_t)LSI_VALUE / 1000UL) * Timeout / 256UL;
    uint16_t Reload;

    if(lu32_reload < 1UL)        lu32_reload = 1UL;
    if(lu32_reload > 0x0FFFUL)   lu32_reload = 0x0FFFUL;
    Reload = (uint16_t)lu32_reload;

    RCC_APB1PeriphClockCmd(RCC_APB1ENR_IWDG, ENABLE);

    RCC_LSICmd(ENABLE);

    while (RESET == RCC_GetFlagStatus(RCC_FLAG_LSIRDY))
    {
    }

    PVU_CheckStatus();
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    PVU_CheckStatus();

    RVU_CheckStatus();
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetReload(Reload);
    RVU_CheckStatus();

    IWDG_ReloadCounter();

    IWDG_Enable();
}

/***********************************************************************************************************************
  * @brief
  * @note   none
  * @param  none
  * @retval none
  *********************************************************************************************************************/
void IWDG_SystemMonitor_Sample(void)
{
    printf("\r\nTest %s", __FUNCTION__);

    IWDG_Configure(500);

    while (1)
    {
        PLATFORM_LED_Toggle(LED1);
        PLATFORM_DelayMS(100);

        IWDG_ReloadCounter();
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

