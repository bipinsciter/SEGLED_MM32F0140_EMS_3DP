////////////////////////////////////////////////////////////////////////////////
/// @file     hal_pwr.c
/// @author   AE TEAM
/// @brief    THIS FILE PROVIDES ALL THE PWR FIRMWARE FUNCTIONS.
////////////////////////////////////////////////////////////////////////////////
/// @attention
///
/// THE EXISTING FIRMWARE IS ONLY FOR REFERENCE, WHICH IS DESIGNED TO PROVIDE
/// CUSTOMERS WITH CODING INFORMATION ABOUT THEIR PRODUCTS SO THEY CAN SAVE
/// TIME. THEREFORE, MINDMOTION SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT OR
/// CONSEQUENTIAL DAMAGES ABOUT ANY CLAIMS ARISING OUT OF THE CONTENT OF SUCH
/// HARDWARE AND/OR THE USE OF THE CODING INFORMATION CONTAINED HEREIN IN
/// CONNECTION WITH PRODUCTS MADE BY CUSTOMERS.
///
/// <H2><CENTER>&COPY; COPYRIGHT MINDMOTION </CENTER></H2>
////////////////////////////////////////////////////////////////////////////////

// Define to prevent recursive inclusion
#define __HAL_PWR_C_

// Files includes
#include "hal_pwr.h"
#include "hal_rcc.h"
////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MM32_Hardware_Abstract_Layer
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PWR_HAL
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PWR_Exported_Functions
/// @{

////////////////////////////////////////////////////////////////////////////////
/// @brief  Deinitializes the PWR peripheral registers to their default reset
/// values.
/// @param  None.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void PWR_DeInit(void)
{
    exRCC_APB1PeriphReset(RCC_APB1ENR_PWR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Enables or disables the Power Voltage Detector(PVD).
/// @param  state: new state of the PVD.
///         This parameter can be: ENABLE or DISABLE.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void PWR_PVDCmd(FunctionalState state)
{
    (state) ? (PWR->CR |= PWR_CR_PVDE) : (PWR->CR &= ~PWR_CR_PVDE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Configures the voltage threshold detected by the Power Voltage
///         Detector(PVD).
/// @param  pvd_level: specifies the PVD detection level
///         This parameter can be one of the following values:
/// @arg    emPVD_LEVEL0 : PVD detection level set to 1.8V
/// @arg    emPVD_LEVEL1 : PVD detection level set to 2.1V
/// @arg    emPVD_LEVEL2 : PVD detection level set to 2.4V
/// @arg    emPVD_LEVEL3 : PVD detection level set to 2.7V
/// @arg    emPVD_LEVEL4 : PVD detection level set to 3.0V
/// @arg    emPVD_LEVEL5 : PVD detection level set to 3.3V
/// @arg    emPVD_LEVEL6 : PVD detection level set to 3.6V
/// @arg    emPVD_LEVEL7 : PVD detection level set to 3.9V
/// @arg    emPVD_LEVEL8 : PVD detection level set to 4.2V
/// @arg    emPVD_LEVEL9 : PVD detection level set to 4.5V
/// @arg    emPVD_LEVEL10: PVD detection level set to 4.8V
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void PWR_PVDLevelConfig(u32 pvd_level)
{
    PWR->CR = (PWR->CR & (~PWR_CR_PLS)) | pvd_level;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Enables or disables the WakeUp Pin functionality.
/// @param  state: new state of the WakeUp Pin functionality.
///         This parameter can be: ENABLE or DISABLE.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void PWR_WakeUpPinCmd(FunctionalState state)
{
    (state != DISABLE) ? (PWR->CSR |= PWR_CSR_EWUP) : (PWR->CSR &= ~PWR_CSR_EWUP);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Enters SLEEP mode.
/// @param  sleep_entry: Select Enter mode of SLEEP to execute related instructions
///         This parameter can be one of the following values:
/// @arg    PWR_SLEEPNOW_WFI: enter SLEEPNOW mode with WFI instruction
/// @arg    PWR_SLEEPNOW_WFE: enter SLEEPNOW mode with WFE instruction
/// @arg    PWR_SLEEPONEXIT : enter SLEEPONEXIT mode with WFI instruction
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void PWR_EnterSLEEPMode(uint32_t sleep_entry)
{
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    if((sleep_entry == PWR_SLEEPNOW_WFE) || (sleep_entry == PWR_SLEEPNOW_WFI))
    {
        SCB->SCR &= ~SCB_SCR_SLEEPONEXIT_Msk;
        if(sleep_entry == PWR_SLEEPNOW_WFI)
        {
            __WFI();
        }
        else
        {
            __SEV();
            __WFE();
            __WFE();
        }
    }
    else if (sleep_entry == PWR_SLEEPONEXIT)
    {
        SCB->SCR |= SCB_SCR_SLEEPONEXIT_Msk;
		__WFI();
    }  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Enters STOP mode.
/// @param  regulator: specifies the regulator state in STOP mode.
///         This parameter can be one of the following values:
/// @arg    PWR_Regulator_ON: STOP mode with regulator ON
/// @arg    PWR_Regulator_LowPower: STOP mode with regulator in low power mode.
/// @param  stop_entry: specifies if STOP mode in entered with WFI or  WFE
///         instruction.
///         This parameter can be one of the following values:
/// @arg    PWR_STOPEntry_WFI: enter STOP mode with WFI instruction
/// @arg    PWR_STOPEntry_WFE: enter STOP mode with WFE instruction
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void PWR_EnterSTOPMode(emPWR_Reg_Stop_mode_Typedef regulator, emPWR_STOP_ModeEn_Typedef stop_entry)
{
    uint32_t systickreg = 0;
    uint32_t nvicReg = 0;
    uint32_t wrIcerReg = 0;
    uint32_t wwdgReg = WWDG->CR;
    uint32_t exti16Reg = 0;
    
    RCC_APB1PeriphClockCmd( RCC_APB1ENR_WWDG, ENABLE);   
       
    systickreg = SysTick->CTRL & 0x01;

    MODIFY_REG(PWR->CR, PWR_CR_LDPS | PWR_CR_PDDS, regulator);
    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    /* By default, clear the enablement of all interrupts except EXTI0 - EXTI15. The interrupt lines with enablement above EXTI16 will maintain their original enabled states */
    nvicReg = NVIC->ISER[0];
    exti16Reg = EXTI->IMR;
    wrIcerReg = nvicReg;
    wrIcerReg &= ~((1UL << EXTI0_1_IRQn)|(1UL << EXTI2_3_IRQn)|(1UL << EXTI4_15_IRQn));
    
    if(exti16Reg & (1UL << 16))
    {
        wrIcerReg &= ~(1UL << PVD_IRQn);
    }
    if(exti16Reg & (1UL << 19))
    {
        wrIcerReg &= ~(1UL << COMP_IRQn);
    }  
    if(exti16Reg & (1UL << 24))
    {
        wrIcerReg &= ~(1UL << WWDG_IWDG_IRQn);
        WWDG->CR &= ~WWDG_CR_WDGA;
    }
    NVIC->ICER[0] = wrIcerReg;
    
    /* Enable HSI */
    RCC->CR |= 0x1;
    while((RCC->CR & 0x2) == 0);
	
    /* To ensure that the instruction is deterministic after awakening, turn off the prefetch function before stop. */
    RCC->CFGR  &= ~(RCC_CFGR_HPRE | RCC_CFGR_SW);
    while((RCC->CFGR & RCC_CFGR_SWS) != 0);
	
    /* Before turning off prefetch, you need to clear the latency configuration */
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR &= ~FLASH_ACR_PRFTBE;
    while ((FLASH->ACR & FLASH_ACR_PRFTBS) != 0)
    {
    }
    
    SysTick->CTRL &= 0xFFFFFFFE;

    /* Disable PLL */
    RCC->CR &= ~RCC_CR_PLLON; 
	
    /* RCC_HSIDLY register */
    *(__IO uint32_t *)(RCC_BASE + 0x44) = 0xFF;	
    
    if(stop_entry == PWR_STOPEntry_WFI)
    {
        /* Request Wait For Interrupt */
        __WFI();
    }
    else
    {
        /* Request Wait For Event */
        __SEV();
        __WFE();
        __WFE();
    }
    NVIC->ISER[0] = nvicReg;
    SysTick->CTRL |= systickreg ;
    
    WWDG->CR = wwdgReg;
        
    CLEAR_BIT(SCB->SCR, ((u32)SCB_SCR_SLEEPDEEP_Msk)); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Enters STANDBY mode.
/// @param  None.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void PWR_EnterSTANDBYMode(void)
{
    uint32_t nvicReg = 0;
    uint32_t wrIcerReg = 0;
    uint32_t wwdgReg = WWDG->CR;
    uint32_t exti16Reg = 0;
    
    RCC_APB1PeriphClockCmd( RCC_APB1ENR_WWDG, ENABLE);    
	   
    /* By default, clear the enablement of all interrupts except EXTI0 - EXTI15. The interrupt lines with enablement above EXTI16 will maintain their original enabled states */
    nvicReg = NVIC->ISER[0];
    exti16Reg = EXTI->IMR;
    wrIcerReg = nvicReg;
    wrIcerReg &= ~((1UL << EXTI0_1_IRQn)|(1UL << EXTI2_3_IRQn)|(1UL << EXTI4_15_IRQn));
      
    if(exti16Reg & (1UL << 16))
    {
        wrIcerReg &= ~(1UL << PVD_IRQn);
    }
    if(exti16Reg & (1UL << 19))
    {
        wrIcerReg &= ~(1UL << COMP_IRQn);
    }  
    if(exti16Reg & (1UL << 24))
    {
        wrIcerReg &= ~(1UL << WWDG_IWDG_IRQn);
        WWDG->CR &= ~WWDG_CR_WDGA;
    }
    NVIC->ICER[0] = wrIcerReg;
    
    /* Enable HSI */
    RCC->CR |= 0x1;
    while((RCC->CR & 0x2) == 0);
	
    /* To ensure that the instruction is deterministic after awakening, turn off the prefetch function before stop. */
    RCC->CFGR  &= ~(RCC_CFGR_HPRE | RCC_CFGR_SW);
    while((RCC->CFGR & RCC_CFGR_SWS) != 0);
	
    /* Before turning off prefetch, you need to clear the latency configuration */
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR &= ~FLASH_ACR_PRFTBE;
    while ((FLASH->ACR & FLASH_ACR_PRFTBS) != 0)
    {
    }
    
    SysTick->CTRL &= 0xFFFFFFFE;

    /* Disable PLL */
    RCC->CR &= ~RCC_CR_PLLON; 
	
    /* RCC_HSIDLY register */
    *(__IO uint32_t *)(RCC_BASE + 0x44) = 0xFF;
    
    PWR->CR |= PWR_CR_PDDS;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
#if defined(__CC_ARM)
    __force_stores();
#endif
    __WFI();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Checks whether the specified PWR flag is set or not.
/// @param  flag: specifies the flag to check.
///         This parameter can be one of the following values:
/// @arg    PWR_FLAG_WU: Wake Up flag
/// @arg    PWR_FLAG_SB: StandBy flag
/// @arg    PWR_FLAG_PVDO: PVD Output
/// @retval The new state of PWR_FLAG (SET or RESET).
////////////////////////////////////////////////////////////////////////////////
FlagStatus PWR_GetFlagStatus(u32 flag)
{
    return (FlagStatus)(PWR->CSR & flag);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Clears the PWR's pending flags.
/// @param  flag: specifies the flag to clear.
///         This parameter can be one of the following values:
/// @arg    PWR_FLAG_WU: Wake Up flag
/// @arg    PWR_FLAG_SB: StandBy flag
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void PWR_ClearFlag(u32 flag)
{
    PWR->CR |= flag << 2;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  Enters STANDBY mode.
/// @param  None.
/// @retval None.
////////////////////////////////////////////////////////////////////////////////
void exPWR_EnterLowPowerMode(emPWR_LP_Mode_Typedef lp_mode, emPWR_Wait_Mode_Typedef wait_mode)
{
    if (lp_mode != LP_SLEEP_MODE) {
        PWR->CR |= lp_mode | PWR_CR_CWUF;
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
        if (wait_mode == LP_WFE) {
            __SEV();
            __WFE();
            __WFE();        //  stop & standby
            return;
        }
    }
    __WFI();        // sleep &  stop & standby

}

/// @}

/// @}

/// @}
