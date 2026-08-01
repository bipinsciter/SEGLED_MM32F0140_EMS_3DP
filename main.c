/***********************************************************************************************************************
    @file    main.c
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
#define _MAIN_C_

/* Files include */
#include "math.h"
#include "stdlib.h"
#include "stdio.h"
#include "platform.h"
#include "gpio.h"
#include "TM1680.h"
#include "XGZP6891D.h"
//#include "DS1307.h"
#include "PCF8563.h"
#include "AT45DB321D.h"
//#include "SPI.h"
#include "spi_master_polling.h"
#include "iwdg_systemmonitor.h"
#include "uart_interrupt.h"
#include "tim1_timebase.h"
#include "main.h"
#include "string.h"
#include "sb_variables.h"
#include "i2cmaster.h"
#include "i2c2master.h"
#include "i2c3master.h"
#include "mm32f0140_it.h"

/**
  * @addtogroup MM32F0140_LibSamples
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

/* Private functions **************************************************************************************************/
void ReadADCChannel(void)
{
	uint16_t RVxVoltage[2];
	
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);

	while (RESET == ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
	{
	}

	ADC_ClearFlag(ADC1, ADC_FLAG_EOC);

	RVxVoltage[0] = ADC_GetChannelConvertedValue(ADC1, ADC_Channel_1);
	RVxVoltage[1] = ADC_GetChannelConvertedValue(ADC1, ADC_Channel_2);

	printf("CH1 = %d  \tCH2 = %d\n", RVxVoltage[0], RVxVoltage[1]);
}

//------------------------------------------------------------------------------
// Epoch Time Functions
//------------------------------------------------------------------------------
/* Return 1 if YEAR + TM_YEAR_BASE is a leap year.  */
static inline int leapyear (long int year)
{
  /* Don't add YEAR to TM_YEAR_BASE, as that might overflow.
     Also, work even if YEAR is negative.  */
    return((year & 3) == 0 && (year % 100 != 0 || ((year / 100) & 3) == (- (TM_YEAR_BASE / 100) & 3)));
}

unsigned long ydhms_diff (unsigned long int year1, unsigned long int yday1, unsigned int hour1, unsigned int min1, unsigned int sec1, unsigned int year0, unsigned int yday0, unsigned int hour0, unsigned int min0, unsigned int sec0)
{
    /* Compute intervening leap days correctly even if year is negative.
     Take care to avoid integer overflow here.  */
    int a4 = SHR (year1, 2) + SHR (TM_YEAR_BASE, 2) - ! (year1 & 3);
    int b4 = SHR (year0, 2) + SHR (TM_YEAR_BASE, 2) - ! (year0 & 3);
    int a100 = a4 / 25 - (a4 % 25 < 0);
    int b100 = b4 / 25 - (b4 % 25 < 0);
    int a400 = SHR (a100, 2);
    int b400 = SHR (b100, 2);
    int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
    
    /* Compute the desired time in time_t precision.  Overflow might
     occur here.  */
    unsigned long int tyear1 = year1;
    unsigned long int years = tyear1 - year0;
    unsigned long int days = 365 * years + yday1 - yday0 + intervening_leap_days;
    unsigned long int hours = 24 * days + hour1 - hour0;
    unsigned long int minutes = 60 * hours + min1 - min0;
    unsigned long int seconds = 60 * minutes + sec1 - sec0;
    return seconds;
}

unsigned long get_epoch_time(RTCData t1)
{
    t1.year -= TM_YEAR_BASE;
    t1.month--;

    int mon_remainder1 = t1.month % 12;
    int negative_mon_remainder1 = mon_remainder1 < 0;
    int mon_years1 = t1.month / 12 - negative_mon_remainder1;
    long int lyear_requested1 = t1.year;
    long int year1 = lyear_requested1 + mon_years1;
    
    int mon_yday1 = ((__mon_yday[leapyear (year1)][mon_remainder1 + 12 * negative_mon_remainder1]) - 1);
    long int lmday1 = t1.day;
    long int yday1 = mon_yday1 + lmday1;
    
    return(ydhms_diff (year1, yday1, t1.hour, t1.minute, t1.second,EPOCH_YEAR - TM_YEAR_BASE, 0, 0, 0, 0));
}

void get_date_time(RTCData* t1,unsigned long epoch)
{
	unsigned long dayclock, dayno;
	int year = EPOCH_YEAR;
	
	dayclock = epoch % 86400;
	dayno = epoch / 86400;
	
	t1->second = (uint8_t)(dayclock % 60);
	t1->minute = (uint8_t)((dayclock % 3600) / 60);
	t1->hour = (uint8_t)(dayclock / 3600);
	t1->day = (uint8_t)((dayno + 4) % 7); // Day 0 was a sunday
	while (dayno >= (unsigned long) YEARSIZE(year))
	{
		dayno -= YEARSIZE(year);
		year++;
	}
	t1->year = (uint16_t)(year - TM_YEAR_BASE);
	//date_time->tm_yday = dayno;
	t1->month = 0;
	while (dayno >= (unsigned long) _ytab[LEAPYEAR(year)][t1->month])
	{
		dayno -= _ytab[LEAPYEAR(year)][t1->month];
		t1->month++;
	}
	t1->month++;
	t1->day = (uint8_t)(dayno + 1);
	
	//t1->year += 2000;
}

//void Check_RTC(void)
//{
//	static uint8_t current_sec=0, last_sec=0;
//	
//	RTC_data[0]=Read_byte_PCF8563(RTC_TIMESEC_REG);
//	current_sec=RTC_data[0];
//	
//	if(last_sec != current_sec)
//	{
//		last_sec = current_sec;
//		
//		Read_PCF8563(RTC_TIMEMIN_REG,&RTC_data[1],5);
//		
//		bool_Sec_blink_flag ^= 1;
//		
//		RTC_data[0] &= ~BIT7;
//		
//		RTC_data[0] &= 0x7F;
//		RTC_data[1] &= 0x7F;
//		RTC_data[2] &= 0x3F;
//		RTC_data[3] &= 0x3F;
//		RTC_data[4] &= 0x1F;
//		
//		rtc.second = BCD2HEX(RTC_data[0]);		//Second
//		rtc.minute = BCD2HEX(RTC_data[1]);		//Minute
//		rtc.hour = BCD2HEX(RTC_data[2]);		//Hour
//		rtc.day = BCD2HEX(RTC_data[3]);			//Date
//		rtc.month = BCD2HEX(RTC_data[4]);		//Month
//		rtc.year = BCD2HEX(RTC_data[5]);		//Year
//		
//		if(rtc.hour>=12) 
//		{
//			bool_AM_PM_Flag=0;
//		}
//		else                    
//		{
//			bool_AM_PM_Flag=1;
//		}
//		
//		rtc1.second = rtc.second;				//Second
//		rtc1.minute = rtc.minute;				//Minute
//		rtc1.hour = rtc.hour;					//Hour
//		rtc1.day = rtc.day;						//Date
//		rtc1.month = rtc.month;					//Month
//		rtc1.year = rtc.year + 2000;			//Year

//		ep.currentEpochTime = get_epoch_time(rtc1);
//	}
//}

void Check_RTC(void)
{
//	bool_Sec_blink_flag ^= 1;

//	ep.currentEpochTime++;
//	get_date_time(&rtc,ep.currentEpochTime);
	
	RTC_data[0]=Read_byte_PCF8563(RTC_TIMESEC_REG);
	current_sec=RTC_data[0];
	
	if(last_sec != current_sec)
	{
		last_sec = current_sec;
		
		Read_PCF8563(RTC_TIMEMIN_REG,&RTC_data[1],5);
		
		bool_Sec_blink_flag ^= 1;

		RTC_data[0] &= 0x7F;
		RTC_data[1] &= 0x7F;
		RTC_data[2] &= 0x3F;
		RTC_data[3] &= 0x3F;
		RTC_data[4] &= 0x1F;
		
		rtc.second = BCD2HEX(RTC_data[0]);		//Second
		rtc.minute = BCD2HEX(RTC_data[1]);		//Minute
		rtc.hour = BCD2HEX(RTC_data[2]);		//Hour
		rtc.day = BCD2HEX(RTC_data[3]);			//Date
		rtc.month = BCD2HEX(RTC_data[4]);		//Month
		rtc.year = BCD2HEX(RTC_data[5]);		//Year
		
		if(rtc.hour>=12) 
		{
			bool_AM_PM_Flag=0;
		}
		else                    
		{
			bool_AM_PM_Flag=1;
		}
		
		rtc1.second = rtc.second;				//Second
		rtc1.minute = rtc.minute;				//Minute
		rtc1.hour = rtc.hour;					//Hour
		rtc1.day = rtc.day;						//Date
		rtc1.month = rtc.month;					//Month
		rtc1.year = rtc.year + 2000;			//Year

		ep.currentEpochTime = get_epoch_time(rtc1);
	}
	
	current_min = rtc.minute;
	current_hr = rtc.hour;
	
	//---------------------------------------------------------------
	if(last_min != current_min)
	{
		last_min = current_min;
		
		if(logTimer)
		{
			logTimer--;
			if(!logTimer)
			{
				LogReading(NORMAL_LOG,0,0xFFFF);
				logTimer=LogInterval;
			}
		}
		FillRamBuffer(NORMAL_LOG,0,0xFFFF);
		
		if(gu16_parameterWord & ENABLE_M3LOG)
		{
			if(gu16_parameterWord & ENABLE_DP1)
			{
				HourDP_Mean[DP1] += Dpressure[DP1];
				HrDPSampleInd[DP1]++;
			}
		
			if(gu16_parameterWord & ENABLE_DP2)
			{
				HourDP_Mean[DP2] += Dpressure[DP2];
				HrDPSampleInd[DP2]++;
			}
		
			if(gu16_parameterWord & ENABLE_DP3)
			{
				HourDP_Mean[DP3] += Dpressure[DP3];
				HrDPSampleInd[DP3]++;
			}

			/*opstr("\r\nMinute: ");
			print_float(HourDP_Mean[DP1],test,1);	opstr("      ");
			print_float(HourDP_Mean[DP2],test,1);	opstr("      ");
			print_float(HourDP_Mean[DP3],test,1);
			opstr("\r\n");
			*/
		}
	}
	
	//---------------------------------------------------------------
	if(last_hr != current_hr)
	{
		if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_M3LOG))
		{
			if(gu16_parameterWord & ENABLE_DP1)
			{
				HourDP_Mean[DP1] /= HrDPSampleInd[DP1];
				WriteLog(DP1_CURR_24HR_MEAN_OFFSET,last_hr,(uint8_t*)&HourDP_Mean[DP1],4);
				HourDP_Mean[DP1]=0.0;
				HrDPSampleInd[DP1]=0;
			}
		
			if(gu16_parameterWord & ENABLE_DP2)
			{
				HourDP_Mean[DP2] /= HrDPSampleInd[DP2];
				WriteLog(DP2_CURR_24HR_MEAN_OFFSET,last_hr,(uint8_t*)&HourDP_Mean[DP2],4);
				HourDP_Mean[DP2]=0.0;
				HrDPSampleInd[DP2]=0;
			}
			
			if(gu16_parameterWord & ENABLE_DP3)
			{
				HourDP_Mean[DP3] /= HrDPSampleInd[DP3];
				WriteLog(DP3_CURR_24HR_MEAN_OFFSET,last_hr,(uint8_t*)&HourDP_Mean[DP3],4);
				HourDP_Mean[DP3]=0.0;
				HrDPSampleInd[DP3]=0;
			}
		}
		
		last_hr = current_hr;
	}
			
	if((ep.currentEpochTime % 86400) < 5)
	{
		if(!bool_resetMinMax)
		{
			if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_M3LOG))
			{
				//Store Last Day Epoch with less than 2 minutes
				ep1.currentEpochTime = ep.currentEpochTime - 120;
			
				memcpy(&MinMaxMeanDayLogArr[0],(uint8_t*)&ep1.currentEpochTime,4);
								
				//Find DP1 Mean Value from last 24 Hour and Store it ---------------------------------------------
				if(gu16_parameterWord & ENABLE_DP1)
				{
					ReadMinMaxLog(DP1_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					DP_Mean[DP1]=0;
					a2=0;
					for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
					{
						memcpy((uint8_t*)&tempfloat1,&Buffer1[a2],4);
						a2 += 4;
						DP_Mean[DP1] += tempfloat1;
					}
					DP_Mean[DP1] /= TOTAL_MEAN_HOUR;
			
					memcpy(&MinMaxMeanDayLogArr[4],(uint8_t*)&DP_Min[DP1],4);
					memcpy(&MinMaxMeanDayLogArr[8],(uint8_t*)&DP_Max[DP1],4);
					memcpy(&MinMaxMeanDayLogArr[12],(uint8_t*)&DP_Mean[DP1],4);
					WriteLog(LAST_DP1_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
				}
			
				//Find DP2 Mean Value from last 24 Hour and Store it ---------------------------------------------
				if(gu16_parameterWord & ENABLE_DP2)
				{
					ReadMinMaxLog(DP2_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					DP_Mean[DP2]=0;
					a2=0;
					for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
					{
						memcpy((uint8_t*)&tempfloat1,&Buffer1[a2],4);
						a2 += 4;
						DP_Mean[DP2] += tempfloat1;
					}
					DP_Mean[DP2] /= TOTAL_MEAN_HOUR;
			
					memcpy(&MinMaxMeanDayLogArr[4],(uint8_t*)&DP_Min[DP2],4);
					memcpy(&MinMaxMeanDayLogArr[8],(uint8_t*)&DP_Max[DP2],4);
					memcpy(&MinMaxMeanDayLogArr[12],(uint8_t*)&DP_Mean[DP2],4);
					WriteLog(LAST_DP2_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
				}
			
				//Find DP3 Mean Value from last 24 Hour and Store it ---------------------------------------------
				if(gu16_parameterWord & ENABLE_DP3)
				{
					ReadMinMaxLog(DP3_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					DP_Mean[DP3]=0;
					a2=0;
					for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
					{
						memcpy((uint8_t*)&tempfloat1,&Buffer1[a2],4);
						a2 += 4;
						DP_Mean[DP3] += tempfloat1;
					}
					DP_Mean[DP3] /= TOTAL_MEAN_HOUR;
				
					memcpy(&MinMaxMeanDayLogArr[4],(uint8_t*)&DP_Min[DP3],4);
					memcpy(&MinMaxMeanDayLogArr[8],(uint8_t*)&DP_Max[DP3],4);
					memcpy(&MinMaxMeanDayLogArr[12],(uint8_t*)&DP_Mean[DP3],4);
					WriteLog(LAST_DP3_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
				}
			
				//Clear all Hour mean value for next day
				memset(Buffer1,0,100);
				if(gu16_parameterWord & ENABLE_DP1)
				{
					WriteLog(DP1_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					HourDP_Mean[DP1]=0;
					HrDPSampleInd[DP1]=0;
				}
				if(gu16_parameterWord & ENABLE_DP2)
				{
					WriteLog(DP2_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					HourDP_Mean[DP2]=0;
					HrDPSampleInd[DP2]=0;
				}
				if(gu16_parameterWord & ENABLE_DP3)
				{
					WriteLog(DP3_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					HourDP_Mean[DP3]=0;
					HrDPSampleInd[DP3]=0;
				}
			
				MinMaxMeanDayLogInd++;
				if(MinMaxMeanDayLogInd>=TOTAL_MIN_MAX_MEAN_LOG) MinMaxMeanDayLogInd=0;
				WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
			}
			
			ResetMinMax();
			#ifdef ENABLE_PRINTF
			opstr("DayChange Occure.Min Max Reset\r\n");
			#endif
			bool_resetMinMax=1;
		}
	}
	else
	{
		bool_resetMinMax=0;
	}
//	//---------------------------------------------------------------	
//	if(rtc.hour>=12) 
//	{
//		bool_AM_PM_Flag=0;
//	}
//	else                    
//	{
//		bool_AM_PM_Flag=1;
//	}
	//---------------------------------------------------------------
}

static uint8_t changeNibbles(uint8_t byte)
{
	uint8_t byte1,byte2,byte3;
	
	byte1 = byte >> 4;
	byte2 = byte << 4;
	byte3 = byte1 | byte2;
	
	return byte3;
}

void AllSegment(uint8_t state)
{
	uint8_t disp[32];
	
	if(state == ON)
	{
		memset(disp,0xFF,sizeof(disp));
	}
	else
	{
		memset(disp,0x00,sizeof(disp));
	}

	TM1680WritePage(0, disp, 32);
}

void InitLEDController(void)
{
	uint8_t i=0;
	
	TM1680Configure();
	//----------------------------
	AllSegment(1);
	
	//while(1);
	
	TM1680Blink(BLINK_1SEC);
	
	PLATFORM_DelayMS(2000);
	
	TM1680Blink(BLINK_OFF);
	//-------------------------------------------------------------------
	
	for(i=0;i<NO_DIGIT;i++) data[i]=BLANK;
	data[1] = V;
	data[2] = E;
	data[3] = r;
	
	data[5] = 11;
	data[6] = 0;
						
	disp_value();
	PLATFORM_DelayMS(1000);

	////-------------------------------------------------------------------
	for(i=0;i<NO_DIGIT;i++) data[i]=BLANK;
	data[2] = I;
	data[3] = D;
	convert_char(DeviceID,&data[4],3);
	disp_value();
	PLATFORM_DelayMS(1000);
	
	//-------------------------------------------------------------------
	for(i=0;i<NO_DIGIT;i++) data[i]=BLANK;
	
	data[1] = B;
	data[2] = r;
	data[3] = t;
	
	switch(UART_BaudRate)
	{
		case BAUD_1200:		convert_char(1200,&data[4],4);		break;
		case BAUD_2400:		convert_char(2400,&data[4],4);		break;
		case BAUD_4800:		convert_char(4800,&data[4],4);		break;
		case BAUD_9600:		convert_char(9600,&data[4],4);		break;
		case BAUD_14400:	convert_char(14400,&data[4],5);		break;
		case BAUD_19200:	convert_char(19200,&data[4],5);		break;
		case BAUD_28800:	convert_char(28800,&data[4],5);		break;
		case BAUD_38400:	convert_char(38400,&data[4],5);		break;
		case BAUD_57600:	convert_char(57600,&data[4],5);		break;
		case BAUD_115200:	convert_float(115200,&data[4],0);	break;
		default:			convert_char(9600,&data[4],4);		break; 
	}

	disp_value();
	PLATFORM_DelayMS(1000);
	
//	////-------------------------------------------------------------------
//	for(i=0;i<NO_DIGIT;i++) data[i]=BLANK;
//	data[2] = 5;
//	data[3] = r;

//	convert_float(gu32_SrNumber,&data[4],0);
//	disp_value();
	PLATFORM_DelayMS(2000);
}

void disp_value(void)
{
	for(uint8_t i=1;i<NO_DIGIT;i++) disp_buffer[i]=seg_code[data[i]];
	for(uint8_t i=0;i<32;i++) final_buffer[i]=0;
	
	if(lcd.Sym_RTC_AM) RTC_AM_on;
	if(lcd.Sym_RTC_PM) RTC_PM_on;
	if(lcd.Sym_RTC_BC1) RTC_BC1_on;
	if(lcd.Sym_RTC_COL) RTC_COL_on;
	
	if(disp_buffer[1] & 0x01) final_buffer[16] |= BIT1;//RTC_A2_on;
	if(disp_buffer[1] & 0x02) final_buffer[17] |= BIT1;//RTC_B2_on;
	if(disp_buffer[1] & 0x04) final_buffer[18] |= BIT1;//RTC_C2_on;
	if(disp_buffer[1] & 0x08) final_buffer[19] |= BIT1;//RTC_D2_on;
	if(disp_buffer[1] & 0x10) final_buffer[20] |= BIT1;//RTC_E2_on;
	if(disp_buffer[1] & 0x20) final_buffer[21] |= BIT1;//RTC_F2_on;
	if(disp_buffer[1] & 0x40) final_buffer[22] |= BIT1;//RTC_G2_on;

	if(disp_buffer[2] & 0x01) final_buffer[24] |= BIT0;//RTC_A3_on;
	if(disp_buffer[2] & 0x02) final_buffer[25] |= BIT0;//RTC_B3_on;
	if(disp_buffer[2] & 0x04) final_buffer[26] |= BIT0;//RTC_C3_on;
	if(disp_buffer[2] & 0x08) final_buffer[27] |= BIT0;//RTC_D3_on;
	if(disp_buffer[2] & 0x10) final_buffer[28] |= BIT0;//RTC_E3_on;
	if(disp_buffer[2] & 0x20) final_buffer[29] |= BIT0;//RTC_F3_on;
	if(disp_buffer[2] & 0x40) final_buffer[30] |= BIT0;//RTC_G3_on;

	if(disp_buffer[3] & 0x01) final_buffer[16] |= BIT0;//RTC_A4_on;
	if(disp_buffer[3] & 0x02) final_buffer[17] |= BIT0;//RTC_B4_on;
	if(disp_buffer[3] & 0x04) final_buffer[18] |= BIT0;//RTC_C4_on;
	if(disp_buffer[3] & 0x08) final_buffer[19] |= BIT0;//RTC_D4_on;
	if(disp_buffer[3] & 0x10) final_buffer[20] |= BIT0;//RTC_E4_on;
	if(disp_buffer[3] & 0x20) final_buffer[21] |= BIT0;//RTC_F4_on;
	if(disp_buffer[3] & 0x40) final_buffer[22] |= BIT0;//RTC_G4_on;
	
	if(lcd.Sym_DP_UNIT) DP_UNIT_on;
	
	if(DP_Alrm_ON[DP1]==NO_ALARM)
	{
		if(lcd.Sym_DP_MIN) DP_MIN_on;
		if(lcd.Sym_DP_LOGO) DP_LOGO_on;
		
		if(disp_buffer[4] & 0x01) final_buffer[24] |= BIT4;//DP_A1_on;
		if(disp_buffer[4] & 0x02) final_buffer[25] |= BIT4;//DP_B1_on;
		if(disp_buffer[4] & 0x04) final_buffer[26] |= BIT4;//DP_C1_on;
		if(disp_buffer[4] & 0x08) final_buffer[27] |= BIT4;//DP_D1_on;
		if(disp_buffer[4] & 0x10) final_buffer[28] |= BIT4;//DP_E1_on;
		if(disp_buffer[4] & 0x20) final_buffer[29] |= BIT4;//DP_F1_on;
		if(disp_buffer[4] & 0x40) final_buffer[30] |= BIT4;//DP_G1_on;

		if(disp_buffer[5] & 0x01) final_buffer[16] |= BIT4;//DP_A2_on;
		if(disp_buffer[5] & 0x02) final_buffer[17] |= BIT4;//DP_B2_on;
		if(disp_buffer[5] & 0x04) final_buffer[18] |= BIT4;//DP_C2_on;
		if(disp_buffer[5] & 0x08) final_buffer[19] |= BIT4;//DP_D2_on;
		if(disp_buffer[5] & 0x10) final_buffer[20] |= BIT4;//DP_E2_on;
		if(disp_buffer[5] & 0x20) final_buffer[21] |= BIT4;//DP_F2_on;
		if(disp_buffer[5] & 0x40) final_buffer[22] |= BIT4;//DP_G2_on;
		if(disp_buffer[5] & 0x80) final_buffer[23] |= BIT4;//DP_H2_on;

		if(disp_buffer[6] & 0x01) final_buffer[16] |= BIT2;//DP_A3_on;
		if(disp_buffer[6] & 0x02) final_buffer[17] |= BIT2;//DP_B3_on;
		if(disp_buffer[6] & 0x04) final_buffer[18] |= BIT2;//DP_C3_on;
		if(disp_buffer[6] & 0x08) final_buffer[19] |= BIT2;//DP_D3_on;
		if(disp_buffer[6] & 0x10) final_buffer[20] |= BIT2;//DP_E3_on;
		if(disp_buffer[6] & 0x20) final_buffer[21] |= BIT2;//DP_F3_on;
		if(disp_buffer[6] & 0x40) final_buffer[22] |= BIT2;//DP_G3_on;
	}
	else if(DP_Alrm_ON[DP1]==LOWER_ALARM)
	{
		if(lcd.Sym_DP_MIN_ALM) DP_MIN_ALM_on;
		if(lcd.Sym_DP_LOGO_ALM) DP_LOGO_ALM_on;
		
		if(disp_buffer[4] & 0x01) final_buffer[24] |= BIT5;//DP_A1_on;
		if(disp_buffer[4] & 0x02) final_buffer[25] |= BIT5;//DP_B1_on;
		if(disp_buffer[4] & 0x04) final_buffer[26] |= BIT5;//DP_C1_on;
		if(disp_buffer[4] & 0x08) final_buffer[27] |= BIT5;//DP_D1_on;
		if(disp_buffer[4] & 0x10) final_buffer[28] |= BIT5;//DP_E1_on;
		if(disp_buffer[4] & 0x20) final_buffer[29] |= BIT5;//DP_F1_on;
		if(disp_buffer[4] & 0x40) final_buffer[30] |= BIT5;//DP_G1_on;

		if(disp_buffer[5] & 0x01) final_buffer[16] |= BIT5;//DP_A2_on;
		if(disp_buffer[5] & 0x02) final_buffer[17] |= BIT5;//DP_B2_on;
		if(disp_buffer[5] & 0x04) final_buffer[18] |= BIT5;//DP_C2_on;
		if(disp_buffer[5] & 0x08) final_buffer[19] |= BIT5;//DP_D2_on;
		if(disp_buffer[5] & 0x10) final_buffer[20] |= BIT5;//DP_E2_on;
		if(disp_buffer[5] & 0x20) final_buffer[21] |= BIT5;//DP_F2_on;
		if(disp_buffer[5] & 0x40) final_buffer[22] |= BIT5;//DP_G2_on;
		if(disp_buffer[5] & 0x80) final_buffer[23] |= BIT5;//DP_H2_on;

		if(disp_buffer[6] & 0x01) final_buffer[16] |= BIT3;//DP_A3_on;
		if(disp_buffer[6] & 0x02) final_buffer[17] |= BIT3;//DP_B3_on;
		if(disp_buffer[6] & 0x04) final_buffer[18] |= BIT3;//DP_C3_on;
		if(disp_buffer[6] & 0x08) final_buffer[19] |= BIT3;//DP_D3_on;
		if(disp_buffer[6] & 0x10) final_buffer[20] |= BIT3;//DP_E3_on;
		if(disp_buffer[6] & 0x20) final_buffer[21] |= BIT3;//DP_F3_on;
		if(disp_buffer[6] & 0x40) final_buffer[22] |= BIT3;//DP_G3_on;
	}
	else
	{
		if(lcd.Sym_DP_MIN_ALM) 
		{
			DP_MIN_on;	
			DP_MIN_ALM_on;
		}
		if(lcd.Sym_DP_LOGO_ALM) DP_LOGO_ALM_on;
		
		if(disp_buffer[4] & 0x01) final_buffer[24] |= BIT4;//DP_A1_on;
		if(disp_buffer[4] & 0x02) final_buffer[25] |= BIT4;//DP_B1_on;
		if(disp_buffer[4] & 0x04) final_buffer[26] |= BIT4;//DP_C1_on;
		if(disp_buffer[4] & 0x08) final_buffer[27] |= BIT4;//DP_D1_on;
		if(disp_buffer[4] & 0x10) final_buffer[28] |= BIT4;//DP_E1_on;
		if(disp_buffer[4] & 0x20) final_buffer[29] |= BIT4;//DP_F1_on;
		if(disp_buffer[4] & 0x40) final_buffer[30] |= BIT4;//DP_G1_on;

		if(disp_buffer[5] & 0x01) final_buffer[16] |= BIT4;//DP_A2_on;
		if(disp_buffer[5] & 0x02) final_buffer[17] |= BIT4;//DP_B2_on;
		if(disp_buffer[5] & 0x04) final_buffer[18] |= BIT4;//DP_C2_on;
		if(disp_buffer[5] & 0x08) final_buffer[19] |= BIT4;//DP_D2_on;
		if(disp_buffer[5] & 0x10) final_buffer[20] |= BIT4;//DP_E2_on;
		if(disp_buffer[5] & 0x20) final_buffer[21] |= BIT4;//DP_F2_on;
		if(disp_buffer[5] & 0x40) final_buffer[22] |= BIT4;//DP_G2_on;
		if(disp_buffer[5] & 0x80) final_buffer[23] |= BIT4;//DP_H2_on;

		if(disp_buffer[6] & 0x01) final_buffer[16] |= BIT2;//DP_A3_on;
		if(disp_buffer[6] & 0x02) final_buffer[17] |= BIT2;//DP_B3_on;
		if(disp_buffer[6] & 0x04) final_buffer[18] |= BIT2;//DP_C3_on;
		if(disp_buffer[6] & 0x08) final_buffer[19] |= BIT2;//DP_D3_on;
		if(disp_buffer[6] & 0x10) final_buffer[20] |= BIT2;//DP_E3_on;
		if(disp_buffer[6] & 0x20) final_buffer[21] |= BIT2;//DP_F3_on;
		if(disp_buffer[6] & 0x40) final_buffer[22] |= BIT2;//DP_G3_on;
		
		if(disp_buffer[4] & 0x01) final_buffer[24] |= BIT5;//DP_A1_on;
		if(disp_buffer[4] & 0x02) final_buffer[25] |= BIT5;//DP_B1_on;
		if(disp_buffer[4] & 0x04) final_buffer[26] |= BIT5;//DP_C1_on;
		if(disp_buffer[4] & 0x08) final_buffer[27] |= BIT5;//DP_D1_on;
		if(disp_buffer[4] & 0x10) final_buffer[28] |= BIT5;//DP_E1_on;
		if(disp_buffer[4] & 0x20) final_buffer[29] |= BIT5;//DP_F1_on;
		if(disp_buffer[4] & 0x40) final_buffer[30] |= BIT5;//DP_G1_on;

		if(disp_buffer[5] & 0x01) final_buffer[16] |= BIT5;//DP_A2_on;
		if(disp_buffer[5] & 0x02) final_buffer[17] |= BIT5;//DP_B2_on;
		if(disp_buffer[5] & 0x04) final_buffer[18] |= BIT5;//DP_C2_on;
		if(disp_buffer[5] & 0x08) final_buffer[19] |= BIT5;//DP_D2_on;
		if(disp_buffer[5] & 0x10) final_buffer[20] |= BIT5;//DP_E2_on;
		if(disp_buffer[5] & 0x20) final_buffer[21] |= BIT5;//DP_F2_on;
		if(disp_buffer[5] & 0x40) final_buffer[22] |= BIT5;//DP_G2_on;
		if(disp_buffer[5] & 0x80) final_buffer[23] |= BIT5;//DP_H2_on;

		if(disp_buffer[6] & 0x01) final_buffer[16] |= BIT3;//DP_A3_on;
		if(disp_buffer[6] & 0x02) final_buffer[17] |= BIT3;//DP_B3_on;
		if(disp_buffer[6] & 0x04) final_buffer[18] |= BIT3;//DP_C3_on;
		if(disp_buffer[6] & 0x08) final_buffer[19] |= BIT3;//DP_D3_on;
		if(disp_buffer[6] & 0x10) final_buffer[20] |= BIT3;//DP_E3_on;
		if(disp_buffer[6] & 0x20) final_buffer[21] |= BIT3;//DP_F3_on;
		if(disp_buffer[6] & 0x40) final_buffer[22] |= BIT3;//DP_G3_on;
	}
	
	if(lcd.Sym_TM_UNIT_C) TM_UNIT_C_on;
	if(lcd.Sym_TM_UNIT_F) TM_UNIT_F_on;
	
	if(DP_Alrm_ON[DP2]==NO_ALARM)
	{
		if(lcd.Sym_TM_MIN) TM_MIN_on;
		//if(lcd.Sym_TM_LOGO) TM_LOGO_on;
		
		if(disp_buffer[7] & 0x01) final_buffer[8] |= BIT2;//TM_A1_on;
		if(disp_buffer[7] & 0x02) final_buffer[9] |= BIT2;//TM_B1_on;
		if(disp_buffer[7] & 0x04) final_buffer[10] |= BIT2;//TM_C1_on;
		if(disp_buffer[7] & 0x08) final_buffer[11] |= BIT2;//TM_D1_on;
		if(disp_buffer[7] & 0x10) final_buffer[12] |= BIT2;//TM_E1_on;
		if(disp_buffer[7] & 0x20) final_buffer[13] |= BIT2;//TM_F1_on;
		if(disp_buffer[7] & 0x40) final_buffer[14] |= BIT2;//TM_G1_on;

		if(disp_buffer[8] & 0x01) final_buffer[0] |= BIT4;//TM_A2_on;
		if(disp_buffer[8] & 0x02) final_buffer[1] |= BIT4;//TM_B2_on;
		if(disp_buffer[8] & 0x04) final_buffer[2] |= BIT4;//TM_C2_on;
		if(disp_buffer[8] & 0x08) final_buffer[3] |= BIT4;//TM_D2_on;
		if(disp_buffer[8] & 0x10) final_buffer[4] |= BIT4;//TM_E2_on;
		if(disp_buffer[8] & 0x20) final_buffer[5] |= BIT4;//TM_F2_on;
		if(disp_buffer[8] & 0x40) final_buffer[6] |= BIT4;//TM_G2_on;
		if(disp_buffer[8] & 0x80) final_buffer[7] |= BIT4;//TM_H2_on;

		if(disp_buffer[9] & 0x01) final_buffer[8] |= BIT4;//TM_A3_on;
		if(disp_buffer[9] & 0x02) final_buffer[9] |= BIT4;//TM_B3_on;
		if(disp_buffer[9] & 0x04) final_buffer[10] |= BIT4;//TM_C3_on;
		if(disp_buffer[9] & 0x08) final_buffer[11] |= BIT4;//TM_D3_on;
		if(disp_buffer[9] & 0x10) final_buffer[12] |= BIT4;//TM_E3_on;
		if(disp_buffer[9] & 0x20) final_buffer[13] |= BIT4;//TM_F3_on;
		if(disp_buffer[9] & 0x40) final_buffer[14] |= BIT4;//TM_G3_on;
	}
	else if(DP_Alrm_ON[DP2]==LOWER_ALARM)
	{
		if(lcd.Sym_TM_MIN_ALM) TM_MIN_ALM_on;
		//if(lcd.Sym_TM_LOGO_ALM) TM_LOGO_ALM_on;
		
		if(disp_buffer[7] & 0x01) final_buffer[8] |= BIT3;//TM_A1_on;
		if(disp_buffer[7] & 0x02) final_buffer[9] |= BIT3;//TM_B1_on;
		if(disp_buffer[7] & 0x04) final_buffer[10] |= BIT3;//TM_C1_on;
		if(disp_buffer[7] & 0x08) final_buffer[11] |= BIT3;//TM_D1_on;
		if(disp_buffer[7] & 0x10) final_buffer[12] |= BIT3;//TM_E1_on;
		if(disp_buffer[7] & 0x20) final_buffer[13] |= BIT3;//TM_F1_on;
		if(disp_buffer[7] & 0x40) final_buffer[14] |= BIT3;//TM_G1_on;

		if(disp_buffer[8] & 0x01) final_buffer[0] |= BIT5;//TM_A2_on;
		if(disp_buffer[8] & 0x02) final_buffer[1] |= BIT5;//TM_B2_on;
		if(disp_buffer[8] & 0x04) final_buffer[2] |= BIT5;//TM_C2_on;
		if(disp_buffer[8] & 0x08) final_buffer[3] |= BIT5;//TM_D2_on;
		if(disp_buffer[8] & 0x10) final_buffer[4] |= BIT5;//TM_E2_on;
		if(disp_buffer[8] & 0x20) final_buffer[5] |= BIT5;//TM_F2_on;
		if(disp_buffer[8] & 0x40) final_buffer[6] |= BIT5;//TM_G2_on;
		if(disp_buffer[8] & 0x80) final_buffer[7] |= BIT5;//TM_H2_on;

		if(disp_buffer[9] & 0x01) final_buffer[8] |= BIT5;//TM_A3_on;
		if(disp_buffer[9] & 0x02) final_buffer[9] |= BIT5;//TM_B3_on;
		if(disp_buffer[9] & 0x04) final_buffer[10] |= BIT5;//TM_C3_on;
		if(disp_buffer[9] & 0x08) final_buffer[11] |= BIT5;//TM_D3_on;
		if(disp_buffer[9] & 0x10) final_buffer[12] |= BIT5;//TM_E3_on;
		if(disp_buffer[9] & 0x20) final_buffer[13] |= BIT5;//TM_F3_on;
		if(disp_buffer[9] & 0x40) final_buffer[14] |= BIT5;//TM_G3_on;
	}
	else
	{
		if(lcd.Sym_TM_MIN) 
		{
			TM_MIN_on;
			TM_MIN_ALM_on;
		}
		//if(lcd.Sym_TM_LOGO_ALM) TM_LOGO_ALM_on;
		
		if(disp_buffer[7] & 0x01) final_buffer[8] |= BIT2;//TM_A1_on;
		if(disp_buffer[7] & 0x02) final_buffer[9] |= BIT2;//TM_B1_on;
		if(disp_buffer[7] & 0x04) final_buffer[10] |= BIT2;//TM_C1_on;
		if(disp_buffer[7] & 0x08) final_buffer[11] |= BIT2;//TM_D1_on;
		if(disp_buffer[7] & 0x10) final_buffer[12] |= BIT2;//TM_E1_on;
		if(disp_buffer[7] & 0x20) final_buffer[13] |= BIT2;//TM_F1_on;
		if(disp_buffer[7] & 0x40) final_buffer[14] |= BIT2;//TM_G1_on;

		if(disp_buffer[8] & 0x01) final_buffer[0] |= BIT4;//TM_A2_on;
		if(disp_buffer[8] & 0x02) final_buffer[1] |= BIT4;//TM_B2_on;
		if(disp_buffer[8] & 0x04) final_buffer[2] |= BIT4;//TM_C2_on;
		if(disp_buffer[8] & 0x08) final_buffer[3] |= BIT4;//TM_D2_on;
		if(disp_buffer[8] & 0x10) final_buffer[4] |= BIT4;//TM_E2_on;
		if(disp_buffer[8] & 0x20) final_buffer[5] |= BIT4;//TM_F2_on;
		if(disp_buffer[8] & 0x40) final_buffer[6] |= BIT4;//TM_G2_on;
		if(disp_buffer[8] & 0x80) final_buffer[7] |= BIT4;//TM_H2_on;

		if(disp_buffer[9] & 0x01) final_buffer[8] |= BIT4;//TM_A3_on;
		if(disp_buffer[9] & 0x02) final_buffer[9] |= BIT4;//TM_B3_on;
		if(disp_buffer[9] & 0x04) final_buffer[10] |= BIT4;//TM_C3_on;
		if(disp_buffer[9] & 0x08) final_buffer[11] |= BIT4;//TM_D3_on;
		if(disp_buffer[9] & 0x10) final_buffer[12] |= BIT4;//TM_E3_on;
		if(disp_buffer[9] & 0x20) final_buffer[13] |= BIT4;//TM_F3_on;
		if(disp_buffer[9] & 0x40) final_buffer[14] |= BIT4;//TM_G3_on;
		
		if(disp_buffer[7] & 0x01) final_buffer[8] |= BIT3;//TM_A1_on;
		if(disp_buffer[7] & 0x02) final_buffer[9] |= BIT3;//TM_B1_on;
		if(disp_buffer[7] & 0x04) final_buffer[10] |= BIT3;//TM_C1_on;
		if(disp_buffer[7] & 0x08) final_buffer[11] |= BIT3;//TM_D1_on;
		if(disp_buffer[7] & 0x10) final_buffer[12] |= BIT3;//TM_E1_on;
		if(disp_buffer[7] & 0x20) final_buffer[13] |= BIT3;//TM_F1_on;
		if(disp_buffer[7] & 0x40) final_buffer[14] |= BIT3;//TM_G1_on;

		if(disp_buffer[8] & 0x01) final_buffer[0] |= BIT5;//TM_A2_on;
		if(disp_buffer[8] & 0x02) final_buffer[1] |= BIT5;//TM_B2_on;
		if(disp_buffer[8] & 0x04) final_buffer[2] |= BIT5;//TM_C2_on;
		if(disp_buffer[8] & 0x08) final_buffer[3] |= BIT5;//TM_D2_on;
		if(disp_buffer[8] & 0x10) final_buffer[4] |= BIT5;//TM_E2_on;
		if(disp_buffer[8] & 0x20) final_buffer[5] |= BIT5;//TM_F2_on;
		if(disp_buffer[8] & 0x40) final_buffer[6] |= BIT5;//TM_G2_on;
		if(disp_buffer[8] & 0x80) final_buffer[7] |= BIT5;//TM_H2_on;

		if(disp_buffer[9] & 0x01) final_buffer[8] |= BIT5;//TM_A3_on;
		if(disp_buffer[9] & 0x02) final_buffer[9] |= BIT5;//TM_B3_on;
		if(disp_buffer[9] & 0x04) final_buffer[10] |= BIT5;//TM_C3_on;
		if(disp_buffer[9] & 0x08) final_buffer[11] |= BIT5;//TM_D3_on;
		if(disp_buffer[9] & 0x10) final_buffer[12] |= BIT5;//TM_E3_on;
		if(disp_buffer[9] & 0x20) final_buffer[13] |= BIT5;//TM_F3_on;
		if(disp_buffer[9] & 0x40) final_buffer[14] |= BIT5;//TM_G3_on;
	}

	if(lcd.Sym_RH_UNIT) RH_UNIT_on;
	
	if(DP_Alrm_ON[DP3]==NO_ALARM)
	{
		if(lcd.Sym_RH_MIN) RH_MIN_on;
		//if(lcd.Sym_RH_LOGO) RH_LOGO_on;
		
		if(disp_buffer[10] & 0x01) final_buffer[0] |= BIT0;//RH_A1_on;
		if(disp_buffer[10] & 0x02) final_buffer[1] |= BIT0;//RH_B1_on;
		if(disp_buffer[10] & 0x04) final_buffer[2] |= BIT0;//RH_C1_on;
		if(disp_buffer[10] & 0x08) final_buffer[3] |= BIT0;//RH_D1_on;
		if(disp_buffer[10] & 0x10) final_buffer[4] |= BIT0;//RH_E1_on;
		if(disp_buffer[10] & 0x20) final_buffer[5] |= BIT0;//RH_F1_on;
		if(disp_buffer[10] & 0x40) final_buffer[6] |= BIT0;//RH_G1_on;

		if(disp_buffer[11] & 0x01) final_buffer[8] |= BIT0;//RH_A2_on;
		if(disp_buffer[11] & 0x02) final_buffer[9] |= BIT0;//RH_B2_on;
		if(disp_buffer[11] & 0x04) final_buffer[10] |= BIT0;//RH_C2_on;
		if(disp_buffer[11] & 0x08) final_buffer[11] |= BIT0;//RH_D2_on;
		if(disp_buffer[11] & 0x10) final_buffer[12] |= BIT0;//RH_E2_on;
		if(disp_buffer[11] & 0x20) final_buffer[13] |= BIT0;//RH_F2_on;
		if(disp_buffer[11] & 0x40) final_buffer[14] |= BIT0;//RH_G2_on;
		if(disp_buffer[11] & 0x80) final_buffer[15] |= BIT0;//RH_H2_on;

		if(disp_buffer[12] & 0x01) final_buffer[0] |= BIT2;//RH_A3_on;
		if(disp_buffer[12] & 0x02) final_buffer[1] |= BIT2;//RH_B3_on;
		if(disp_buffer[12] & 0x04) final_buffer[2] |= BIT2;//RH_C3_on;
		if(disp_buffer[12] & 0x08) final_buffer[3] |= BIT2;//RH_D3_on;
		if(disp_buffer[12] & 0x10) final_buffer[4] |= BIT2;//RH_E3_on;
		if(disp_buffer[12] & 0x20) final_buffer[5] |= BIT2;//RH_F3_on;
		if(disp_buffer[12] & 0x40) final_buffer[6] |= BIT2;//RH_G3_on;
	}
	else if(DP_Alrm_ON[DP3]==LOWER_ALARM)
	{
		if(lcd.Sym_RH_MIN_ALM) RH_MIN_ALM_on;
		//if(lcd.Sym_RH_LOGO_ALM) RH_LOGO_ALM_on;
		
		if(disp_buffer[10] & 0x01) final_buffer[0] |= BIT1;//RH_A1_on;
		if(disp_buffer[10] & 0x02) final_buffer[1] |= BIT1;//RH_B1_on;
		if(disp_buffer[10] & 0x04) final_buffer[2] |= BIT1;//RH_C1_on;
		if(disp_buffer[10] & 0x08) final_buffer[3] |= BIT1;//RH_D1_on;
		if(disp_buffer[10] & 0x10) final_buffer[4] |= BIT1;//RH_E1_on;
		if(disp_buffer[10] & 0x20) final_buffer[5] |= BIT1;//RH_F1_on;
		if(disp_buffer[10] & 0x40) final_buffer[6] |= BIT1;//RH_G1_on;

		if(disp_buffer[11] & 0x01) final_buffer[8] |= BIT1;//RH_A2_on;
		if(disp_buffer[11] & 0x02) final_buffer[9] |= BIT1;//RH_B2_on;
		if(disp_buffer[11] & 0x04) final_buffer[10] |= BIT1;//RH_C2_on;
		if(disp_buffer[11] & 0x08) final_buffer[11] |= BIT1;//RH_D2_on;
		if(disp_buffer[11] & 0x10) final_buffer[12] |= BIT1;//RH_E2_on;
		if(disp_buffer[11] & 0x20) final_buffer[13] |= BIT1;//RH_F2_on;
		if(disp_buffer[11] & 0x40) final_buffer[14] |= BIT1;//RH_G2_on;
		if(disp_buffer[11] & 0x80) final_buffer[15] |= BIT1;//RH_H2_on;

		if(disp_buffer[12] & 0x01) final_buffer[0] |= BIT3;//RH_A3_on;
		if(disp_buffer[12] & 0x02) final_buffer[1] |= BIT3;//RH_B3_on;
		if(disp_buffer[12] & 0x04) final_buffer[2] |= BIT3;//RH_C3_on;
		if(disp_buffer[12] & 0x08) final_buffer[3] |= BIT3;//RH_D3_on;
		if(disp_buffer[12] & 0x10) final_buffer[4] |= BIT3;//RH_E3_on;
		if(disp_buffer[12] & 0x20) final_buffer[5] |= BIT3;//RH_F3_on;
		if(disp_buffer[12] & 0x40) final_buffer[6] |= BIT3;//RH_G3_on;
	}	
	else
	{
		if(lcd.Sym_RH_MIN) 
		{
			RH_MIN_on;
			RH_MIN_ALM_on;
		}
		//if(lcd.Sym_RH_LOGO_ALM) RH_LOGO_ALM_on;
		
		if(disp_buffer[10] & 0x01) final_buffer[0] |= BIT0;//RH_A1_on;
		if(disp_buffer[10] & 0x02) final_buffer[1] |= BIT0;//RH_B1_on;
		if(disp_buffer[10] & 0x04) final_buffer[2] |= BIT0;//RH_C1_on;
		if(disp_buffer[10] & 0x08) final_buffer[3] |= BIT0;//RH_D1_on;
		if(disp_buffer[10] & 0x10) final_buffer[4] |= BIT0;//RH_E1_on;
		if(disp_buffer[10] & 0x20) final_buffer[5] |= BIT0;//RH_F1_on;
		if(disp_buffer[10] & 0x40) final_buffer[6] |= BIT0;//RH_G1_on;

		if(disp_buffer[11] & 0x01) final_buffer[8] |= BIT0;//RH_A2_on;
		if(disp_buffer[11] & 0x02) final_buffer[9] |= BIT0;//RH_B2_on;
		if(disp_buffer[11] & 0x04) final_buffer[10] |= BIT0;//RH_C2_on;
		if(disp_buffer[11] & 0x08) final_buffer[11] |= BIT0;//RH_D2_on;
		if(disp_buffer[11] & 0x10) final_buffer[12] |= BIT0;//RH_E2_on;
		if(disp_buffer[11] & 0x20) final_buffer[13] |= BIT0;//RH_F2_on;
		if(disp_buffer[11] & 0x40) final_buffer[14] |= BIT0;//RH_G2_on;
		if(disp_buffer[11] & 0x80) final_buffer[15] |= BIT0;//RH_H2_on;

		if(disp_buffer[12] & 0x01) final_buffer[0] |= BIT2;//RH_A3_on;
		if(disp_buffer[12] & 0x02) final_buffer[1] |= BIT2;//RH_B3_on;
		if(disp_buffer[12] & 0x04) final_buffer[2] |= BIT2;//RH_C3_on;
		if(disp_buffer[12] & 0x08) final_buffer[3] |= BIT2;//RH_D3_on;
		if(disp_buffer[12] & 0x10) final_buffer[4] |= BIT2;//RH_E3_on;
		if(disp_buffer[12] & 0x20) final_buffer[5] |= BIT2;//RH_F3_on;
		if(disp_buffer[12] & 0x40) final_buffer[6] |= BIT2;//RH_G3_on;
		
		if(disp_buffer[10] & 0x01) final_buffer[0] |= BIT1;//RH_A1_on;
		if(disp_buffer[10] & 0x02) final_buffer[1] |= BIT1;//RH_B1_on;
		if(disp_buffer[10] & 0x04) final_buffer[2] |= BIT1;//RH_C1_on;
		if(disp_buffer[10] & 0x08) final_buffer[3] |= BIT1;//RH_D1_on;
		if(disp_buffer[10] & 0x10) final_buffer[4] |= BIT1;//RH_E1_on;
		if(disp_buffer[10] & 0x20) final_buffer[5] |= BIT1;//RH_F1_on;
		if(disp_buffer[10] & 0x40) final_buffer[6] |= BIT1;//RH_G1_on;

		if(disp_buffer[11] & 0x01) final_buffer[8] |= BIT1;//RH_A2_on;
		if(disp_buffer[11] & 0x02) final_buffer[9] |= BIT1;//RH_B2_on;
		if(disp_buffer[11] & 0x04) final_buffer[10] |= BIT1;//RH_C2_on;
		if(disp_buffer[11] & 0x08) final_buffer[11] |= BIT1;//RH_D2_on;
		if(disp_buffer[11] & 0x10) final_buffer[12] |= BIT1;//RH_E2_on;
		if(disp_buffer[11] & 0x20) final_buffer[13] |= BIT1;//RH_F2_on;
		if(disp_buffer[11] & 0x40) final_buffer[14] |= BIT1;//RH_G2_on;
		if(disp_buffer[11] & 0x80) final_buffer[15] |= BIT1;//RH_H2_on;

		if(disp_buffer[12] & 0x01) final_buffer[0] |= BIT3;//RH_A3_on;
		if(disp_buffer[12] & 0x02) final_buffer[1] |= BIT3;//RH_B3_on;
		if(disp_buffer[12] & 0x04) final_buffer[2] |= BIT3;//RH_C3_on;
		if(disp_buffer[12] & 0x08) final_buffer[3] |= BIT3;//RH_D3_on;
		if(disp_buffer[12] & 0x10) final_buffer[4] |= BIT3;//RH_E3_on;
		if(disp_buffer[12] & 0x20) final_buffer[5] |= BIT3;//RH_F3_on;
		if(disp_buffer[12] & 0x40) final_buffer[6] |= BIT3;//RH_G3_on;
	}
	
	if(lcd.Sym_DOOR) DOOR_on;
	if(lcd.Sym_DOOR_SYM) DOOR_SYM_on;
	if(lcd.Sym_DOOR_ALM) DOOR_ALM_on;
	if(lcd.Sym_LOGO) LOGO_on;
	if(lcd.Sym_MIN) MIN_on;
	if(lcd.Sym_MAX) MAX_on;
	if(lcd.Sym_MEAN) MEAN_on;
	if(lcd.Sym_SET) SET_on;
	if(lcd.Sym_ID) ID_on;
	if(lcd.Sym_ACK) ACK_on;
	
	for(uint8_t i=0;i<32;i++) final_buffer[i] = changeNibbles(final_buffer[i]);
	TM1680WritePage(0x00, final_buffer, sizeof(final_buffer));	
}

void conv_value(void)
{
	for(uint8_t i=1;i<NO_DIGIT;i++) data[i] = BLANK;
	
	lcd.Sym_DOOR = 0;
	lcd.Sym_DOOR_SYM = 0;
	lcd.Sym_DOOR_ALM = 0;
	lcd.Sym_LOGO = 0;
	lcd.Sym_MIN = 0;
	lcd.Sym_MAX = 0;
	lcd.Sym_MEAN = 0;
	lcd.Sym_SET = 0;
	lcd.Sym_ID = 0;
	lcd.Sym_ACK = 0;
	lcd.Sym_RTC_AM = 0;
	lcd.Sym_RTC_PM = 0;
	lcd.Sym_RTC_BC1 = 0;
	lcd.Sym_RTC_COL = 0;

	lcd.Sym_DP_LOGO = 0;
	lcd.Sym_DP_LOGO_ALM = 0;
	lcd.Sym_DP_UNIT = 0;
	lcd.Sym_DP_MIN = 0;
	lcd.Sym_DP_MIN_ALM = 0;
	
	lcd.Sym_RH_LOGO = 0;
	lcd.Sym_RH_LOGO_ALM = 0;
	lcd.Sym_RH_UNIT = 0;
	lcd.Sym_RH_MIN = 0;
	lcd.Sym_RH_MIN_ALM = 0;
	
	lcd.Sym_TM_LOGO = 0;
	lcd.Sym_TM_LOGO_ALM = 0;
	lcd.Sym_TM_UNIT_C = 0;
	lcd.Sym_TM_UNIT_F = 0;
	lcd.Sym_TM_MIN = 0;
	lcd.Sym_TM_MIN_ALM = 0;
	
	lcd.Sym_LOGO = 1;
	
	#ifndef DISABLE_DOOR_SENSING
	lcd.Sym_DOOR_SYM = 1;
	if(bool_doorStatus==OPEN)
	{
		lcd.Sym_DOOR_ALM = 1;
	}
	else
	{
		lcd.Sym_DOOR = 1;
	}
	#endif
	
	switch(mode)
	{
		case NORMAL_MODE:
		
			if(gu16_parameterWord & ENABLE_RTC)
			{
				if(!RTCSetFlag)
				{
					if(bool_mec500_blink_flag) 
					{
						lcd.Sym_RTC_COL = 1;
						
						convert_char(rtc.minute,&data[2],2);

						if(bool_AM_PM_Flag)
						{
							lcd.Sym_RTC_AM = 1;
							
							convert_char(rtc.hour,&data[0],2);
						}
						else
						{
							lcd.Sym_RTC_PM = 1;
						
							if(rtc.hour>12)
								convert_char(rtc.hour-12,&data[0],2);
							else
								convert_char(rtc.hour,&data[0],2);
						}
						if(data[0] == 1) lcd.Sym_RTC_BC1 = 1;
					}
				}
				else
				{
					convert_char(rtc.minute,&data[2],2);
				
					if(bool_AM_PM_Flag)
					{
						lcd.Sym_RTC_AM = 1;
						
						convert_char(rtc.hour,&data[0],2);
					}
					else
					{
						lcd.Sym_RTC_PM = 1;
					
						if(rtc.hour>12)
							convert_char(rtc.hour-12,&data[0],2);
						else
							convert_char(rtc.hour,&data[0],2);
					}
					if(data[0] == 1) lcd.Sym_RTC_BC1 = 1;
				
					if(bool_Sec_blink_flag) lcd.Sym_RTC_COL = 1;
				}
			}
			
			switch(Normal_para_cnt)
			{
				case 0:
				
					if(gu16_parameterWord & ENABLE_DP1)
					{
						if(bool_DP_NC[DP1])
						{
							data[4]=E;
							data[5]=r;
							data[6]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = Dpressure[DP1];
						
							if(!DP_limit[DP1])
							{
								if(tempfloat<0.0)
								{
									tempfloat *= (-1.0);
									
									if(DP_Alrm_ON[DP1]) 
									{
										lcd.Sym_DP_MIN_ALM = 1;
									}
									else
									{
										lcd.Sym_DP_MIN = 1;
									}
								}
								//----------------------------------------------------
								if(tempfloat < 10.0)
								{
									convert_float(tempfloat,&data[5],1);
								}
								else if(tempfloat < 100.0)
								{
									convert_float(tempfloat,&data[4],1);
								}
								else
								{
									convert_float(tempfloat,&data[4],0);
								}
							}
							else if(DP_limit[DP1]==1)
							{
								data[5]=H;
								data[6]=I;
							}
							else if(DP_limit[DP1]==2)
							{
								data[5]=L;
								data[6]=0;
							}

							//----------------------------------------------------
						}
						lcd.Sym_DP_UNIT = 1;
						if(DP_Alrm_ON[DP1]) 
						{
							lcd.Sym_DP_LOGO_ALM = 1;
						}
						else
						{
							lcd.Sym_DP_LOGO = 1;
						}
					}
					
					if(gu16_parameterWord & ENABLE_DP2)
					{
						if(bool_DP_NC[DP2])
						{
							data[7]=E;
							data[8]=r;
							data[9]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = Dpressure[DP2];
						
							if(!DP_limit[DP2])
							{
								if(tempfloat<0.0)
								{
									tempfloat *= (-1.0);
									
									if(DP_Alrm_ON[DP2]) 
									{
										lcd.Sym_TM_MIN_ALM = 1;
									}
									else
									{
										lcd.Sym_TM_MIN = 1;
									}
								}
								//----------------------------------------------------
								if(tempfloat < 10.0)
								{
									convert_float(tempfloat,&data[8],1);
								}
								else if(tempfloat < 100.0)
								{
									convert_float(tempfloat,&data[7],1);
								}
								else
								{
									convert_float(tempfloat,&data[7],0);
								}
							}
							else if(DP_limit[DP2]==1)
							{
								data[8]=H;
								data[9]=I;
							}
							else if(DP_limit[DP2]==2)
							{
								data[8]=L;
								data[9]=0;
							}

							//----------------------------------------------------
						}
						lcd.Sym_DP_UNIT = 1;
						
						if(DP_Alrm_ON[DP2]) 
						{
							lcd.Sym_TM_LOGO_ALM = 1;
						}
						else
						{
							lcd.Sym_TM_LOGO = 1;
						}
					}
					
					if(gu16_parameterWord & ENABLE_DP3)
					{
						if(bool_DP_NC[DP3])
						{
							data[10]=E;
							data[11]=r;
							data[12]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = Dpressure[DP3];
						
							if(!DP_limit[DP3])
							{
								if(tempfloat<0.0)
								{
									tempfloat *= (-1.0);
									
									if(DP_Alrm_ON[DP3]) 
									{
										lcd.Sym_RH_MIN_ALM = 1;
									}
									else
									{
										lcd.Sym_RH_MIN = 1;
									}
								}
								//----------------------------------------------------
								if(tempfloat < 10.0)
								{
									convert_float(tempfloat,&data[11],1);
								}
								else if(tempfloat < 100.0)
								{
									convert_float(tempfloat,&data[10],1);
								}
								else
								{
									convert_float(tempfloat,&data[10],0);
								}
							}
							else if(DP_limit[DP3]==1)
							{
								data[11]=H;
								data[12]=I;
							}
							else if(DP_limit[DP3]==2)
							{
								data[11]=L;
								data[12]=0;
							}

							//----------------------------------------------------
						}
						lcd.Sym_DP_UNIT = 1;
						
						if(DP_Alrm_ON[DP3]) 
						{
							lcd.Sym_RH_LOGO_ALM = 1;
						}
						else
						{
							lcd.Sym_RH_LOGO = 1;
						}
					}
			
				break;
				
				case 1:
				
					lcd.Sym_ID = 1;
					convert_char(DeviceID,&data[4],3);
				
				break;
				
				case 2:
				
					data[10] = B;
					data[11] = D;
					data[12] = r;
				
					switch(UART_BaudRate)
					{
						case BAUD_1200:		convert_char(1200,&data[4],4);		break;
						case BAUD_2400:		convert_char(2400,&data[4],4);		break;
						case BAUD_4800:		convert_char(4800,&data[4],4);		break;
						case BAUD_9600:		convert_char(9600,&data[4],4);		break;
						case BAUD_14400:	convert_char(14400,&data[4],5);		break;
						case BAUD_19200:	convert_char(19200,&data[4],5);		break;
						case BAUD_28800:	convert_char(28800,&data[4],5);		break;
						case BAUD_38400:	convert_char(38400,&data[4],5);		break;
						case BAUD_57600:	convert_char(57600,&data[4],5);		break;
						case BAUD_115200:	convert_float(115200,&data[1],0);	break;
					}
				
				break;
				
				case 3:
				
					lcd.Sym_MIN = 1;
				
					if(gu16_parameterWord & ENABLE_DP1)
					{
						if(bool_DP_NC[DP1])
						{
							data[4]=E;
							data[5]=r;
							data[6]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = DP_Min[DP1];
						
							if(tempfloat<0.0)
							{
								tempfloat *= (-1.0);
								lcd.Sym_DP_MIN = 1;
							}
							//----------------------------------------------------
							if(tempfloat < 10.0)
							{
								convert_float(tempfloat,&data[5],1);
							}
							else if(tempfloat < 100.0)
							{
								convert_float(tempfloat,&data[4],1);
							}
							else
							{
								convert_float(tempfloat,&data[4],0);
							}
							//----------------------------------------------------
						}
						lcd.Sym_DP_UNIT = 1;
						lcd.Sym_DP_LOGO = 1;
					}
				
					if(gu16_parameterWord & ENABLE_DP2)
					{
						if(bool_DP_NC[DP2])
						{
							data[7]=E;
							data[8]=r;
							data[9]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = DP_Min[DP2];
						
							if(tempfloat<0.0)
							{
								tempfloat *= (-1.0);
								lcd.Sym_TM_MIN = 1;
							}
							//----------------------------------------------------
							if(tempfloat < 10.0)
							{
								convert_float(tempfloat,&data[8],1);
							}
							else if(tempfloat < 100.0)
							{
								convert_float(tempfloat,&data[7],1);
							}
							else
							{
								convert_float(tempfloat,&data[7],0);
							}
							//----------------------------------------------------
						}
					}
					
					if(gu16_parameterWord & ENABLE_DP3)
					{
						if(bool_DP_NC[DP3])
						{
							data[10]=E;
							data[11]=r;
							data[12]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = DP_Min[DP3];
						
							if(tempfloat<0.0)
							{
								tempfloat *= (-1.0);
								lcd.Sym_RH_MIN = 1;
							}
							//----------------------------------------------------
							if(tempfloat < 10.0)
							{
								convert_float(tempfloat,&data[11],1);
							}
							else if(tempfloat < 100.0)
							{
								convert_float(tempfloat,&data[10],1);
							}
							else
							{
								convert_float(tempfloat,&data[10],0);
							}
							//----------------------------------------------------
						}
					}
					
				break;
					
				case 4:
					
					lcd.Sym_MAX = 1;
					lcd.Sym_DP_UNIT = 1;
					lcd.Sym_DP_LOGO = 1;
				
					if(gu16_parameterWord & ENABLE_DP1)
					{
						if(bool_DP_NC[DP1])
						{
							data[4]=E;
							data[5]=r;
							data[6]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = DP_Max[DP1];
							
							if(tempfloat<0.0)
							{
								tempfloat *= (-1.0);
								lcd.Sym_DP_MIN = 1;
							}
							//----------------------------------------------------
							if(tempfloat < 10.0)
							{
								convert_float(tempfloat,&data[5],1);
							}
							else if(tempfloat < 100.0)
							{
								convert_float(tempfloat,&data[4],1);
							}
							else
							{
								convert_float(tempfloat,&data[4],0);
							}
							
							//----------------------------------------------------
						}
					}
					
					if(gu16_parameterWord & ENABLE_DP2)
					{
						if(bool_DP_NC[DP2])
						{
							data[7]=E;
							data[8]=r;
							data[9]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = DP_Max[DP2];
						
							if(tempfloat<0.0)
							{
								tempfloat *= (-1.0);
								lcd.Sym_TM_MIN = 1;
							}
							//----------------------------------------------------
							if(tempfloat < 10.0)
							{
								convert_float(tempfloat,&data[8],1);
							}
							else if(tempfloat < 100.0)
							{
								convert_float(tempfloat,&data[7],1);
							}
							else
							{
								convert_float(tempfloat,&data[7],0);
							}
							//----------------------------------------------------
						}
					}

					if(gu16_parameterWord & ENABLE_DP3)
					{
						if(bool_DP_NC[DP3])
						{
							data[10]=E;
							data[11]=r;
							data[12]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = DP_Max[DP3];
						
							if(tempfloat<0.0)
							{
								tempfloat *= (-1.0);
								lcd.Sym_RH_MIN = 1;
							}
							//----------------------------------------------------
							if(tempfloat < 10.0)
							{
								convert_float(tempfloat,&data[11],1);
							}
							else if(tempfloat < 100.0)
							{
								convert_float(tempfloat,&data[10],1);
							}
							else
							{
								convert_float(tempfloat,&data[10],0);
							}
							//----------------------------------------------------
						}
					}
					
				break;
					
				case 5:
					
					lcd.Sym_ACK = 1;
					convert_char(dummy1,&data[4],2);
					if(gu8_SetACKPwd==2)
					{
						convert_char(dummy,&data[10],3);
					}
					
					if(gu8_SetACKPwd)lcd.Sym_SET = 1;
					
				break;
			}
			
		break;	
		
		case DP_AUTO_CAL_MODE:
		
			data[4] = A;
			data[5] = U;
			data[6] = t;
			
			data[7] = C;
			data[8] = A;
			data[9] = L;

			lcd.Sym_DP_LOGO = 1;
			
		break;
		
		case MIN_MAX_MEAN_MODE:
			
			switch(min_max_mean_page_disp_cnt)
			{
				case 0:
			
					data[4] = P;
					data[5] = A;
					data[6] = 9;
					data[7] = E;
				
					MIN_on;
					MAX_on;
					MEAN_on;
			
				break;
			
				case 1:
				case 5:
				case 9:
				case 13:
				case 17:
				case 21:
				case 25:
				case 29:
				case 33:
				case 37:
				case 41:
				case 45:
				case 49:
				case 53:
				case 57:
			
					convert_char(dispLogInd,&data[2],2);
					
					data[4] = D;
					data[5] = t;
					
					convert_char(rtc2.day,&data[7],2);
					convert_char(rtc2.month,&data[10],2);
			
				break;
			
				case 2:
				case 6:
				case 10:
				case 14:
				case 18:
				case 22:
				case 26:
				case 30:
				case 34:
				case 38:
				case 42:
				case 46:
				case 50:
				case 54:
				case 58:
			
					MIN_on;
					memcpy(&tempfloat,&MinMaxMeanDayLogArr4Disp[4],4);
					memcpy(&tempfloat1,&MinMaxMeanDayLogArr4Disp1[4],4);
					memcpy(&tempfloat2,&MinMaxMeanDayLogArr4Disp2[4],4);
			
				break;
			
				case 3:
				case 7:
				case 11:
				case 15:
				case 19:
				case 23:
				case 27:
				case 31:
				case 35:
				case 39:
				case 43:
				case 47:
				case 51:
				case 55:
				case 59:
			
					MAX_on;
					memcpy(&tempfloat,&MinMaxMeanDayLogArr4Disp[8],4);
					memcpy(&tempfloat1,&MinMaxMeanDayLogArr4Disp1[8],4);
					memcpy(&tempfloat2,&MinMaxMeanDayLogArr4Disp2[8],4);
					
				break;
			
				case 4:
				case 8:
				case 12:
				case 16:
				case 20:
				case 24:
				case 28:
				case 32:
				case 36:
				case 40:
				case 44:
				case 48:
				case 52:
				case 56:
				case 60:
			
					MEAN_on;
					memcpy(&tempfloat,&MinMaxMeanDayLogArr4Disp[12],4);
					memcpy(&tempfloat1,&MinMaxMeanDayLogArr4Disp1[12],4);
					memcpy(&tempfloat2,&MinMaxMeanDayLogArr4Disp2[12],4);
					
				break;
			}
		
			if((min_max_mean_page_disp_cnt>0) && ((min_max_mean_page_disp_cnt-1)%4))
			{
				lcd.Sym_DP_UNIT = 1;
				lcd.Sym_DP_LOGO = 1;
				
				//DP1 ------------------------------
				if(gu16_parameterWord & ENABLE_DP1)
				{
					if(bool_noData)
					{
						data[4]=DASH;
						data[5]=DASH;
						data[6]=DASH;
					}
					else
					{
						if(tempfloat<0.0)
						{
							tempfloat *= (-1.0);
							lcd.Sym_DP_MIN = 1;
						}
					
						if(tempfloat < 10.0)
						{
							convert_float(tempfloat,&data[5],1);
						}
						else if(tempfloat < 100.0)
						{
							convert_float(tempfloat,&data[4],1);
						}
						else
						{
							convert_float(tempfloat,&data[4],0);
						}
					}
				}
			
				//DP2 ------------------------------
				if(gu16_parameterWord & ENABLE_DP2)
				{
					if(bool_noData)
					{
						data[7]=DASH;
						data[8]=DASH;
						data[9]=DASH;
					}
					else
					{
						if(tempfloat1<0.0)
						{
							tempfloat1 *= (-1.0);
							lcd.Sym_TM_MIN = 1;
						}
					
						if(tempfloat1 < 10.0)
						{
							convert_float(tempfloat1,&data[8],1);
						}
						else if(tempfloat1 < 100.0)
						{
							convert_float(tempfloat1,&data[7],1);
						}
						else
						{
							convert_float(tempfloat1,&data[7],0);
						}
					}
				}
			
				//DP3 ------------------------------
				if(gu16_parameterWord & ENABLE_DP3)
				{
					if(bool_noData)
					{
						data[10]=DASH;
						data[11]=DASH;
						data[12]=DASH;
					}
					else
					{
						if(tempfloat2<0.0)
						{
							tempfloat2 *= (-1.0);
							lcd.Sym_RH_MIN = 1;
						}
						//----------------------------------------------------
						if(tempfloat2 < 10.0)
						{
							convert_float(tempfloat2,&data[11],1);
						}
						else if(tempfloat2 < 100.0)
						{
							convert_float(tempfloat2,&data[10],1);
						}
						else
						{
							convert_float(tempfloat2,&data[10],0);
						}
						//----------------------------------------------------
					}
				}
			}
			
		break;
			
		case MEAN_HOUR_MODE:
			
			if(!mean_hr_page_disp_cnt)
			{
				data[4] = P;
				data[5] = A;
				data[6] = 9;
				data[7] = E;
			
				data[10] = M;
				data[11] = N;
			}
			else if((mean_hr_page_disp_cnt>=1) && (mean_hr_page_disp_cnt<=24))
			{
				lcd.Sym_DP_UNIT = 1;
				lcd.Sym_DP_LOGO = 1;
				
				convert_char(dispMinMaxMeanLogInd,&data[2],2);
			
				if(gu16_parameterWord & ENABLE_DP1)
				{
					if(bool_DP_NC[DP1])
					{
						data[4]=E;
						data[5]=r;
						data[6]=r;
					}
					else
					{
						if(tempfloat<0.0)
						{
							tempfloat *= (-1.0);
							lcd.Sym_DP_MIN = 1;
						}
						//----------------------------------------------------
						if(tempfloat < 10.0)
						{
							convert_float(tempfloat,&data[5],1);
						}
						else if(tempfloat < 100.0)
						{
							convert_float(tempfloat,&data[4],1);
						}
						else
						{
							convert_float(tempfloat,&data[4],0);
						}
					}
				}
				//--------------------------------------------------
				if(gu16_parameterWord & ENABLE_DP2)
				{
					if(bool_DP_NC[DP2])
					{
						data[7]=E;
						data[8]=r;
						data[9]=r;
					}
					else
					{
						if(tempfloat1<0.0)
						{
							tempfloat1 *= (-1.0);
							lcd.Sym_TM_MIN = 1;
						}
						//----------------------------------------------------
						if(tempfloat1 < 10.0)
						{
							convert_float(tempfloat1,&data[8],1);
						}
						else if(tempfloat1 < 100.0)
						{
							convert_float(tempfloat1,&data[7],1);
						}
						else
						{
							convert_float(tempfloat1,&data[7],0);
						}
					}
				}
				//--------------------------------------------------
				if(gu16_parameterWord & ENABLE_DP3)
				{
					if(bool_DP_NC[DP3])
					{
						data[10]=E;
						data[11]=r;
						data[12]=r;
					}
					else
					{
						if(tempfloat2<0.0)
						{
							tempfloat2 *= (-1.0);
							lcd.Sym_RH_MIN = 1;
						}
						//----------------------------------------------------
						if(tempfloat2 < 10.0)
						{
							convert_float(tempfloat2,&data[11],1);
						}
						else if(tempfloat2 < 100.0)
						{
							convert_float(tempfloat2,&data[10],1);
						}
						else
						{
							convert_float(tempfloat2,&data[10],0);
						}
					}
				}
			}
			
		break;
		
		case PROG_MODE:
		
			switch(prog_para_cnt)
			{
				case 0:
				
					data[4] = P;
					data[5] = r;
					data[6] = 9;
				
				break;
				
				case 1:
				
					data[4] = D;
					data[5] = V;
					data[6] = C;
				
					data[2] = 1;
					data[3] = D;
				
					convert_char(dummy,&data[7],3);
				
				break;
				
				case 2:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
					
					data[1] = 0;
					data[2] = N;
					
					data[10] = U;
					data[11] = P;
					
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 3:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					data[10] = U;
					data[11] = P;
	
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 4:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					data[10] = L;
					data[11] = 0;
				
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 5:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = N;
				
					data[10] = L;
					data[11] = 0;
				
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
					
				case 6:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
					
					data[1] = 0;
					data[2] = N;
					
					data[10] = U;
					data[11] = P;
					
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 7:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					data[10] = U;
					data[11] = P;
	
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 8:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					data[10] = L;
					data[11] = 0;
				
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 9:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = N;
				
					data[10] = L;
					data[11] = 0;
				
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
					
				case 10:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
					
					data[1] = 0;
					data[2] = N;
					
					data[10] = U;
					data[11] = P;
					
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 11:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					data[10] = U;
					data[11] = P;
	
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 12:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					data[10] = L;
					data[11] = 0;
				
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
				
				case 13:
				
					lcd.Sym_DP_LOGO = 1;
					lcd.Sym_DP_UNIT = 1;
				
					data[1] = 0;
					data[2] = N;
				
					data[10] = L;
					data[11] = 0;
				
					if(dummy<0)
					{
						lcd.Sym_TM_MIN = 1;
						convert_char(-dummy,&data[6],4);
					}
					else
					{
						convert_char(dummy,&data[6],4);
					}
				
				break;
						
				case 14:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = H;
					data[5] = r;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case 15:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = M;
					data[5] = N;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case 16:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = D;
					data[5] = t;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case 17:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = M;
					data[5] = 0;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case 18:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = Y;
					data[5] = r;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case 19:
				
					data[4] = B;
					data[5] = 2;
					data[6] = r;
				
					data[2] = 0;
					data[3] = N;
				
					convert_char(dummy,&data[7],3);
				
				break;
				
				case 20:
				
					data[4] = B;
					data[5] = 2;
					data[6] = r;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					convert_char(dummy,&data[7],3);
				
				break;
				
				case 21:
				
					data[4] = L;
					data[5] = 0;
					data[6] = 9;
				
					data[1] = t;
					data[2] = M;
					data[3] = E;
				
					convert_char(dummy,&data[7],3);
				
				break;
				
				case 22:
				
					data[1] = U;
					data[2] = r;
					data[3] = t;
				
					data[10] = B;
					data[11] = D;
					data[12] = r;
				
					switch(dummy)
					{
						case BAUD_1200:		convert_char(1200,&data[4],4);		break;
						case BAUD_2400:		convert_char(2400,&data[4],4);		break;
						case BAUD_4800:		convert_char(4800,&data[4],4);		break;
						case BAUD_9600:		convert_char(9600,&data[4],4);		break;
						case BAUD_14400:	convert_char(14400,&data[4],5);		break;
						case BAUD_19200:	convert_char(19200,&data[4],5);		break;
						case BAUD_28800:	convert_char(28800,&data[4],5);		break;
						case BAUD_38400:	convert_char(38400,&data[4],5);		break;
						case BAUD_57600:	convert_char(57600,&data[4],5);		break;
						case BAUD_115200:	convert_float(115200,&data[4],6);	break;
					}

				break;
				
				case 23:
				
					data[1] = C;
					data[2] = A;
					data[3] = L;
				
					convert_char(dummy,&data[4],3);
				
				break;
			}
			
		break;
	}
}

void SetMAC2Xbee(uint8_t *mac,uint8_t ReadSelfMac)
{
	uint8_t buffer[5]={0};
	
	PLATFORM_DelayMS(1000);
	//-------------------------
	opstr("+++");
	PLATFORM_DelayMS(1000);
	//-------------------------
	opstr("ATDH");
	SendToUART(&mac[0],8);
	opchar('\r');
	PLATFORM_DelayMS(50);
	//-------------------------
	opstr("ATDL");
	SendToUART(&mac[8],8);
	opchar('\r');
	PLATFORM_DelayMS(50);
	//-------------------------
	if(ReadSelfMac==1)
	{
		memset(gu8arr_XbeeSelfMac,'0',XBEE_MAC_SIZE);
		memset(XbeeRxBuffer,0,XBEE_RX_IND_MAX);
		XbeeRxInd=0;
		opstr("ATSH?\r");
		PLATFORM_DelayMS(50);
		memcpy(&gu8arr_XbeeSelfMac[2],&XbeeRxBuffer[0],6);
		//-------------------------
		memset(XbeeRxBuffer,0,XBEE_RX_IND_MAX);
		XbeeRxInd=0;
		opstr("ATSL?\r");
		PLATFORM_DelayMS(50);
		memcpy(&gu8arr_XbeeSelfMac[8],&XbeeRxBuffer[0],8);
		//-------------------------
		chartostr(DeviceID,&buffer[0],3);
		opstr("ATBISAP");
		SendToUART(&buffer[0],3);
		opchar('-');
		SendToUART(&gu8ar_SrNumber[8],8);
		opchar('\r');
		PLATFORM_DelayMS(50);
	}
	//-------------------------
	opstr("ATWR\r");
	PLATFORM_DelayMS(50);
	//-------------------------
	opstr("ATCN\r");
	PLATFORM_DelayMS(50);
	//-------------------------
	
	//SendToUART(gu8arr_XbeeSelfMac,XBEE_MAC_SIZE);
}

#ifdef ENABLE_KEY_LOGIC
	
void check_key(void)
{
	keybyte=0;

	if(!PARA_SELECT_KEY) keybyte |= BIT1;

	if((!keybyte)&&(!bool_keybit))
	{
		if(debounce)
		{
			debounce--;
			if(!debounce)
			{
				bool_keybit=1;
				debounce=DEBOUNCE;
			}
		}
	}	
	else if((keybyte)&&(bool_keybit))
	{
		if(debounce)
		{
			debounce--;
			if(!debounce)
			{
				bool_keybit=0;
				keyboard();
				debounce=DEBOUNCE;
			}
		}
	}
	else debounce=DEBOUNCE;

	if(UP_KEY) key_up_count=0;
	if(DN_KEY) key_dn_count=0;
}

void CheckUpDnKey(void)
{
	uint8_t i=0;
	
	if((gu16_parameterWord & ENABLE_M3LOG) && !PARA_SELECT_KEY && !PROG_ENT_KEY)
	{
		if(mode==NORMAL_MODE)
		{
			MinMaxMeanModeTimer++;
			if(MinMaxMeanModeTimer > 20)
			{
				MinMaxMeanModeTimer=0;
				
				mode=MIN_MAX_MEAN_MODE;
				min_max_mean_page_disp_cnt=0;
				dispMinMaxMeanLogInd=0;
				progTimeout=60;
				
				if(MinMaxMeanDayLogInd) dispMinMaxMeanLogInd=MinMaxMeanDayLogInd-1;
				else dispMinMaxMeanLogInd=TOTAL_MIN_MAX_MEAN_LOG-1;
			}
		}
	}
	else if(!PARA_SELECT_KEY && !UP_KEY)
	{
		if(mode==NORMAL_MODE)
		{
			MeanHrModeTimer++;
			if(MeanHrModeTimer > 20)
			{
				MeanHrModeTimer=0;
				
				mode=MEAN_HOUR_MODE;
				mean_hr_page_disp_cnt=0;
				dispMinMaxMeanLogInd=0;
				progTimeout=60;
			}
		}
	}
	else if(!PARA_SELECT_KEY && !DN_KEY)
	{
		if(mode==NORMAL_MODE)
		{
			gu8_MinMaxClearTimer++;
			if(gu8_MinMaxClearTimer > 20)
			{
				gu8_MinMaxClearTimer=0;
				
				ResetMinMax();
				
				for(i=0;i<NO_DIGIT;i++) data[i]=BLANK;
				lcd.Sym_MIN = 1;
				lcd.Sym_MAX = 1;
				data[4] = C;
				data[5] = L;
				data[6] = r;
				disp_value();
				
				PLATFORM_DelayMS(4000);
			}
		}
	}
	else if(!UP_KEY && !DN_KEY)
	{
		if(mode==DP_AUTO_CAL_MODE)
		{
			progTimeout=60;
			
			DPAutoCalTimer++;
			if(DPAutoCalTimer > 10)
			{
				DPAutoCalTimer=0;
				
				switch(autoCal_para_cnt)
				{
					case 0:
						
						DP_Cal_Value_C[DP1] = (int16_t)((RealDpressure[DP1] - DP_Cal_float_Value_F[DP1])*10.0);
						//DP_Cal_Value_C[DP1] = RealDpressure[DP1]*10.0;
						WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],2);
						DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;
						
						/*AutocalCnt = (signed short)(Dpressure[DP1] * 10.0);
						DP_Cal_Count_C[DP1] -= AutocalCnt;
						WriteEEPROMData(DP1_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP1]);
						*/
						
					break;
				
					case 1:
				
						DP_Cal_Value_C[DP2] = (int16_t)((RealDpressure[DP2] - DP_Cal_float_Value_F[DP2])*10.0);
						//DP_Cal_Value_C[DP2] = RealDpressure[DP2]*10.0;
						WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],2);
						DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
						
						/*AutocalCnt = (signed short)(Dpressure[DP2] * 10.0);
						DP_Cal_Count_C[DP2] -= AutocalCnt;
						WriteEEPROMData(DP2_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP2]);
						*/
						
					break;
					
					case 2:
				
						DP_Cal_Value_C[DP3] = (int16_t)((RealDpressure[DP3] - DP_Cal_float_Value_F[DP3])*10.0);
						//DP_Cal_Value_C[DP3] = RealDpressure[DP3]*10.0;
						WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
						DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
						
						/*AutocalCnt = (signed short)(Dpressure[DP3] * 10.0);
						DP_Cal_Count_C[DP3] -= AutocalCnt;
						WriteEEPROMData(DP3_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP3]);
						*/
						
					break;
				}
				
				for(i=0;i<NO_DIGIT;i++) data[i]=BLANK;
				data[4] = D;
				data[5] = 0;
				data[6] = N;
				disp_value();
				
				PLATFORM_DelayMS(4000);
				
				mode=NORMAL_MODE;
				Normal_para_cnt=0;
				autoCal_para_cnt=0;
				progTimeout=0;
			}
		}
		else
		{
			restoreFactoryCalibrationTimer++;
			if(restoreFactoryCalibrationTimer > 20)
			{
				restoreFactoryCalibrationTimer=0;
			
				DP_Cal_Value_C[DP1]=0;
				DP_Cal_float_Value_C[DP1] = 0.0;
				WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],2);
				
				DP_Cal_Value_C[DP2]=0;
				DP_Cal_float_Value_C[DP2] = 0.0;
				WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],2);
				
				DP_Cal_Value_C[DP3]=0;
				DP_Cal_float_Value_C[DP3] = 0.0;
				WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
				
				Normal_para_cnt=0;
				progTimeout=0;

				for(uint8_t i=0;i<NO_DIGIT;i++) data[i]=BLANK;
				data[1] = F;
				data[2] = A;
				data[3] = C;
				
				data[4] = C;
				data[5] = A;
				data[6] = L;
				disp_value();
				
				PLATFORM_DelayMS(4000);
				
			}
		}
	}
	else if(!PROG_ENT_KEY)
	{
		ProgModeTimer++;
		if(ProgModeTimer > 10)
		{
			ProgModeTimer=0;
			
			if(mode==NORMAL_MODE)
			{
				mode=PROG_MODE;
				prog_para_cnt=0;
				
				Normal_para_cnt=0;
				bool_RTCChangeOccure=0;
				bool_UARTChanged=0;
				Lastpara_cnt=0;
				progTimeout=60;
				gu8_SetACKPwd=0;
			}
			else if(mode==PROG_MODE)
			{
				mode=NORMAL_MODE;
				progTimeout=0;
				
				for(i=0;i<NO_DIGIT;i++) data[i]=BLANK;
				data[4] = N;
				data[5] = 0;
				data[6] = r;
				disp_value();
				
				PLATFORM_DelayMS(4000);
			}
		}
	}
	else if(!UP_KEY)
	{
		if(key_up_count<20)key_up_count++;
	
		if(key_up_count<10)			dummy++;
		else if(key_up_count<20)	dummy+=10;
		else						dummy+=100;
	
		//print_short(key_up_count,test,3);		opstr("\r\n");
		
		switch(mode)
		{
			case NORMAL_MODE: 
			
				progTimeout=60;
			
				if(!gu8_SetACKPwd)
				{
					Normal_para_cnt++;

					if(Normal_para_cnt==5)
					{
						if((DP_Alrm_ON[DP1]) || (DP_Alrm_ON[DP2]) || (DP_Alrm_ON[DP3]))
						{
							dummy1=0;
							dummy=0;
						}
						else
						{
							Normal_para_cnt=0;
							progTimeout=0;
							gu8_SetACKPwd=0;
						}
					}
					
					if(Normal_para_cnt>5)
					{
						Normal_para_cnt=0;
						progTimeout=0;
						gu8_SetACKPwd=0;
					}
				}
				else if(gu8_SetACKPwd==1)
				{
					//gu8_SetACKPwd=1;
					if(dummy>15)dummy=15;
					dummy1=dummy;
				}
				else
				{
					if(dummy>999)dummy=999;
				}
			
			break;
			
			case DP_AUTO_CAL_MODE:
			
				progTimeout=60;
				autoCal_para_cnt=1;
				
			break;
			
			case MIN_MAX_MEAN_MODE:
			
				progTimeout=60;
				
				min_max_mean_page_disp_cnt++;
				if(min_max_mean_page_disp_cnt>60)
				{
					min_max_mean_page_disp_cnt=1;
					dispLogInd=0;
				}
			
				if(!((min_max_mean_page_disp_cnt-1) % 4))
				{
					if(gu16_parameterWord & ENABLE_DP1) 
					{
						ReadMinMaxLog(LAST_DP1_MIN_MAX_OFFSET,dispMinMaxMeanLogInd,&MinMaxMeanDayLogArr4Disp[0],MIN_MAX_MEAN_LOG_SIZE);
						if(MinMaxMeanDayLogArr4Disp[3]==0xFF)	//If no log then set Log to Zero
						{
							memset(MinMaxMeanDayLogArr4Disp,0,MIN_MAX_MEAN_LOG_SIZE);
						}
					}
					if(gu16_parameterWord & ENABLE_DP2) 
					{
						ReadMinMaxLog(LAST_DP2_MIN_MAX_OFFSET,dispMinMaxMeanLogInd,&MinMaxMeanDayLogArr4Disp1[0],MIN_MAX_MEAN_LOG_SIZE);
						if(MinMaxMeanDayLogArr4Disp1[3]==0xFF)	//If no log then set Log to Zero
						{
							memset(MinMaxMeanDayLogArr4Disp1,0,MIN_MAX_MEAN_LOG_SIZE);
						}
					}
					if(gu16_parameterWord & ENABLE_DP3) 
					{
						ReadMinMaxLog(LAST_DP3_MIN_MAX_OFFSET,dispMinMaxMeanLogInd,&MinMaxMeanDayLogArr4Disp2[0],MIN_MAX_MEAN_LOG_SIZE);
						if(MinMaxMeanDayLogArr4Disp2[3]==0xFF)	//If no log then set Log to Zero
						{
							memset(MinMaxMeanDayLogArr4Disp2,0,MIN_MAX_MEAN_LOG_SIZE);
						}
					}	
					//--------------------------------------------------------------------------------------------
					if(gu16_parameterWord & ENABLE_DP1)
					{
						memcpy((uint8_t*)&ep1.currentEpochTime,&MinMaxMeanDayLogArr4Disp[0],4);
					}
					else
					{
						if(gu16_parameterWord & ENABLE_DP2)
						{
							memcpy((uint8_t*)&ep1.currentEpochTime,&MinMaxMeanDayLogArr4Disp1[0],4);
						}
						else
						{
							memcpy((uint8_t*)&ep1.currentEpochTime,&MinMaxMeanDayLogArr4Disp2[0],4);
						}
					}		
				
					if(!ep1.currentEpochTime) 
					{
						bool_noData=1;
						rtc2.day=0;
						rtc2.month=0;
					}
					else 
					{
						bool_noData=0;
						get_date_time(&rtc2,ep1.currentEpochTime);
					}
					
					if(dispMinMaxMeanLogInd)
					{
						dispMinMaxMeanLogInd--;
					}
					else
					{
						dispMinMaxMeanLogInd=TOTAL_MIN_MAX_MEAN_LOG-1;
					}
					
					dispLogInd++;
				}
				
			break;
			
			case MEAN_HOUR_MODE:
			
				progTimeout=60;
			
				mean_hr_page_disp_cnt++;
				if(mean_hr_page_disp_cnt>24)
				{
					mean_hr_page_disp_cnt=1;
					dispMinMaxMeanLogInd=0;
				}
		
				if(mean_hr_page_disp_cnt==1)
				{
					if(gu16_parameterWord & ENABLE_DP1)
					{
						ReadMinMaxLog(DP1_CURR_24HR_MEAN_OFFSET,0,&MeanHrLogArr4Disp[0],HOUR_MEAN_VALUE_SPACE);
					}
					if(gu16_parameterWord & ENABLE_DP2)
					{
						ReadMinMaxLog(DP2_CURR_24HR_MEAN_OFFSET,0,&MeanHrLogArr4Disp1[0],HOUR_MEAN_VALUE_SPACE);
					}
					if(gu16_parameterWord & ENABLE_DP3)
					{
						ReadMinMaxLog(DP3_CURR_24HR_MEAN_OFFSET,0,&MeanHrLogArr4Disp2[0],HOUR_MEAN_VALUE_SPACE);
					}
				}
				
				memcpy(&tempfloat,&MeanHrLogArr4Disp[dispMinMaxMeanLogInd*4],4);
				memcpy(&tempfloat1,&MeanHrLogArr4Disp1[dispMinMaxMeanLogInd*4],4);
				memcpy(&tempfloat2,&MeanHrLogArr4Disp2[dispMinMaxMeanLogInd*4],4);
				dispMinMaxMeanLogInd++;
			
			break;
			
			case PROG_MODE:
			
				if(!PARA_SELECT_KEY)
				{
					//opstr("\r\nProg Mode + Up Key + Para_Select key pressed\r\n");
					prog_para_cnt++;
					
					if((prog_para_cnt==2) && !(gu16_parameterWord & ENABLE_DP1))
					{
						prog_para_cnt=6;
					}
					
					if((prog_para_cnt==6) && !(gu16_parameterWord & ENABLE_DP2))
					{
						prog_para_cnt=10;
					}
					
					if((prog_para_cnt==10) && !(gu16_parameterWord & ENABLE_DP3))
					{
						prog_para_cnt=14;
					}
					
					if((prog_para_cnt==14) && (gu16_parameterWord & ENABLE_LOG))
					{
						prog_para_cnt=19;
					}
					
					if((prog_para_cnt==21) && !(gu16_parameterWord & ENABLE_LOG))
					{
						prog_para_cnt=22;
					}
					
					if(prog_para_cnt>23)
					{
						prog_para_cnt=1;
					}
					
					dummy=0;
					
					Lastpara_cnt=prog_para_cnt;
					//----------------------------------------------------------------------
					switch(prog_para_cnt)
					{
						case 1:		dummy = DeviceID;				break;
						case 2:		dummy = DP_Upper_Alm_ON[DP1]; 		break;
						case 3:		dummy = DP_Upper_Alm_OFF[DP1]; 		break;
						case 4:		dummy = DP_Lower_Alm_OFF[DP1];  	break;
						case 5:		dummy = DP_Lower_Alm_ON[DP1];  		break;
						case 6:		dummy = DP_Upper_Alm_ON[DP2]; 		break;
						case 7:		dummy = DP_Upper_Alm_OFF[DP2]; 		break;
						case 8:		dummy = DP_Lower_Alm_OFF[DP2];  	break;
						case 9:		dummy = DP_Lower_Alm_ON[DP2];  		break;
						case 10:	dummy = DP_Upper_Alm_ON[DP3]; 		break;
						case 11:	dummy = DP_Upper_Alm_OFF[DP3]; 		break;
						case 12:	dummy = DP_Lower_Alm_OFF[DP3];  	break;
						case 13:	dummy = DP_Lower_Alm_ON[DP3];  		break;
						case 14:
							Temp_RTC_ARR[0] = rtc.hour;
							dummy = rtc.hour;
						break;
						case 15:
							Temp_RTC_ARR[1] = rtc.minute;
							dummy = rtc.minute;
						break;
						case 16:
							Temp_RTC_ARR[2] = rtc.day;
							dummy = rtc.day;
						break;
						case 17:
							Temp_RTC_ARR[3] = rtc.month;
							dummy = rtc.month;
						break;
						case 18:
							Temp_RTC_ARR[4] = (uint8_t)rtc.year;
							dummy = rtc.year;
						break;
						case 19:	dummy = Buzzer_ON_Time;	  		break;
						case 20:	dummy = Buzzer_OFF_Time;	  	break;
						case 21:	dummy = LogInterval;			break;
						case 22:	dummy = UART_BaudRate;		  	break;
						case 23:	dummy = 0;						break;
					}
				}
				else
				{
					progTimeout=60;
					
					switch(prog_para_cnt)
					{
						case 1:		if(dummy>250)dummy=250;										break;
						case 2:		if(dummy>DEFAUT_DP1_MIN*10.0)dummy=DEFAUT_DP1_MIN*10.0;		break;
						case 3:		if(dummy>DP_Upper_Alm_ON[DP1]) dummy=(DP_Upper_Alm_ON[DP1]-1);		break;
						case 4:		if(dummy>DP_Upper_Alm_OFF[DP1])dummy=(DP_Upper_Alm_OFF[DP1]-1);		break;
						case 5:		if(dummy>DP_Lower_Alm_OFF[DP1])dummy=(DP_Lower_Alm_OFF[DP1]-1);		break;
						case 6:		if(dummy>DEFAUT_DP2_MIN*10.0)dummy=DEFAUT_DP2_MIN*10.0;		break;
						case 7:		if(dummy>DP_Upper_Alm_ON[DP2]) dummy=(DP_Upper_Alm_ON[DP2]-1);		break;
						case 8:		if(dummy>DP_Upper_Alm_OFF[DP2])dummy=(DP_Upper_Alm_OFF[DP2]-1);		break;
						case 9:		if(dummy>DP_Lower_Alm_OFF[DP2])dummy=(DP_Lower_Alm_OFF[DP2]-1);		break;
						case 10:	if(dummy>DEFAUT_DP3_MIN*10.0)dummy=DEFAUT_DP3_MIN*10.0;		break;
						case 11:	if(dummy>DP_Upper_Alm_ON[DP3]) dummy=(DP_Upper_Alm_ON[DP3]-1);		break;
						case 12:	if(dummy>DP_Upper_Alm_OFF[DP3])dummy=(DP_Upper_Alm_OFF[DP3]-1);		break;
						case 13:	if(dummy>DP_Lower_Alm_OFF[DP3])dummy=(DP_Lower_Alm_OFF[DP3]-1);		break;
						case 14: 	if(dummy>23)dummy=23; 	bool_RTCChangeOccure = 1;			break;
						case 15: 	if(dummy>59)dummy=59; 	bool_RTCChangeOccure = 1;			break;
						case 16: 	if(dummy>31)dummy=31; 	bool_RTCChangeOccure = 1;			break;
						case 17: 	if(dummy>12)dummy=12; 	bool_RTCChangeOccure = 1;			break;
						case 18: 	if(dummy>99)dummy=99;	bool_RTCChangeOccure = 1;			break;
						case 19:	if(dummy>60)dummy=60;									break;
						case 20:	if(dummy>960)dummy=960;									break;
						case 21:	if(dummy>MAX_LOG_INTERVAL)dummy=MAX_LOG_INTERVAL;		break;
						case 22:	if(dummy>9)dummy=9;		bool_UARTChanged = 1;			break;
						case 23:	if(dummy>999)dummy=999;									break;
					}
				}
			break;
		}
	}
	else if(!DN_KEY)
	{
		if(key_dn_count<20)key_dn_count++;
	
		if(key_dn_count<10)			dummy--;
		else if(key_dn_count<20)	dummy-=10;
		else						dummy-=100;
	
		switch(mode)
		{
			case NORMAL_MODE: 
			
				progTimeout=60;
				
				if(!gu8_SetACKPwd)
				{
					
				}
				else if(gu8_SetACKPwd==1)
				{
					if(dummy<1)dummy=1;
					dummy1=dummy;
				}
				else 
				{
					if(dummy<0)dummy=0;
				}
				
			break;
			
			case DP_AUTO_CAL_MODE:
			
				/*progTimeout=60;
				
				autoCal_para_cnt=1;
				
				*/
				
			break;
			
			case MIN_MAX_MEAN_MODE:
						
			break;
			
			case MEAN_HOUR_MODE:
			
			break;
			
			case PROG_MODE:

				if(!PARA_SELECT_KEY)
				{
					//opstr("\r\nProg Mode + Down Key + Para_Select key pressed\r\n");
					if(prog_para_cnt)prog_para_cnt--;
					
					if(!prog_para_cnt)prog_para_cnt=23;
					
					if(prog_para_cnt>23)
					{
						prog_para_cnt=23;
						bool_cal_mode=0;
					}
							
					if((prog_para_cnt==21) && !(gu16_parameterWord & ENABLE_LOG))
					{
						prog_para_cnt=20;
					}
					
					if((prog_para_cnt==18) && (gu16_parameterWord & ENABLE_LOG))
					{
						prog_para_cnt=13;
					}
					
					if((prog_para_cnt==13) && !(gu16_parameterWord & ENABLE_DP3))
					{
						prog_para_cnt=9;
					}
					
					if((prog_para_cnt==9) && !(gu16_parameterWord & ENABLE_DP2))
					{
						prog_para_cnt=5;
					}
					
					if((prog_para_cnt==5) && !(gu16_parameterWord & ENABLE_DP1))
					{
						prog_para_cnt=1;
					}
					
					dummy=0;
					
					Lastpara_cnt=prog_para_cnt;
					//----------------------------------------------------------------------
					switch(prog_para_cnt)
					{
						case 1:		dummy = DeviceID;				break;
						case 2:		dummy = DP_Upper_Alm_ON[DP1]; 		break;
						case 3:		dummy = DP_Upper_Alm_OFF[DP1]; 		break;
						case 4:		dummy = DP_Lower_Alm_OFF[DP1];  	break;
						case 5:		dummy = DP_Lower_Alm_ON[DP1];  		break;
						case 6:		dummy = DP_Upper_Alm_ON[DP2]; 		break;
						case 7:		dummy = DP_Upper_Alm_OFF[DP2]; 		break;
						case 8:		dummy = DP_Lower_Alm_OFF[DP2];  	break;
						case 9:		dummy = DP_Lower_Alm_ON[DP2];  		break;
						case 10:	dummy = DP_Upper_Alm_ON[DP3]; 		break;
						case 11:	dummy = DP_Upper_Alm_OFF[DP3]; 		break;
						case 12:	dummy = DP_Lower_Alm_OFF[DP3];  	break;
						case 13:	dummy = DP_Lower_Alm_ON[DP3];  		break;
						case 14:
							Temp_RTC_ARR[0] = rtc.hour;
							dummy = rtc.hour;
						break;
						case 15:
							Temp_RTC_ARR[1] = rtc.minute;
							dummy = rtc.minute;
						break;
						case 16:
							Temp_RTC_ARR[2] = rtc.day;
							dummy = rtc.day;
						break;
						case 17:
							Temp_RTC_ARR[3] = rtc.month;
							dummy = rtc.month;
						break;
						case 18:
							Temp_RTC_ARR[4] = rtc.year;
							dummy = rtc.year;
						break;
						case 19:	dummy = Buzzer_ON_Time;	  		break;
						case 20:	dummy = Buzzer_OFF_Time;	  	break;
						case 21:	dummy = LogInterval;			break;
						case 22:	dummy = UART_BaudRate;		  	break;
						case 23:	dummy = 0;						break;
					}
				}
				else
				{
					progTimeout=60;
					
					switch(prog_para_cnt)
					{
						case 1:		if(dummy<1)dummy=1;										break;
						case 2:		if(dummy<DP_Upper_Alm_OFF[DP1])dummy=(DP_Upper_Alm_OFF[DP1]+1);	break;
						case 3:		if(dummy<DP_Lower_Alm_OFF[DP1])dummy=(DP_Lower_Alm_OFF[DP1]+1);	break;
						case 4:		if(dummy<DP_Lower_Alm_ON[DP1])dummy=(DP_Lower_Alm_ON[DP1]+1);	break;
						case 5:		if(dummy<DEFAUT_DP1_MAX*10.0)dummy=DEFAUT_DP1_MAX*10.0;	break;
						case 6:		if(dummy<DP_Upper_Alm_OFF[DP2])dummy=(DP_Upper_Alm_OFF[DP2]+1);	break;
						case 7:		if(dummy<DP_Lower_Alm_OFF[DP2])dummy=(DP_Lower_Alm_OFF[DP2]+1);	break;
						case 8:		if(dummy<DP_Lower_Alm_ON[DP2])dummy=(DP_Lower_Alm_ON[DP2]+1);	break;
						case 9:		if(dummy<DEFAUT_DP2_MAX*10.0)dummy=DEFAUT_DP2_MAX*10.0;	break;
						case 10:	if(dummy<DP_Upper_Alm_OFF[DP3])dummy=(DP_Upper_Alm_OFF[DP3]+1);	break;
						case 11:	if(dummy<DP_Lower_Alm_OFF[DP3])dummy=(DP_Lower_Alm_OFF[DP3]+1);	break;
						case 12:	if(dummy<DP_Lower_Alm_ON[DP3])dummy=(DP_Lower_Alm_ON[DP3]+1);	break;
						case 13:	if(dummy<DEFAUT_DP3_MAX*10.0)dummy=DEFAUT_DP3_MAX*10.0;	break;
						case 14: 	if(dummy<0)dummy=0; 	bool_RTCChangeOccure = 1;			break;
						case 15: 	if(dummy<0)dummy=0; 	bool_RTCChangeOccure = 1;			break;
						case 16: 	if(dummy<1)dummy=1; 	bool_RTCChangeOccure = 1;			break;
						case 17: 	if(dummy<1)dummy=1; 	bool_RTCChangeOccure = 1;			break;
						case 18: 	if(dummy<0)dummy=0;		bool_RTCChangeOccure = 1;			break;
						case 19:	if(dummy<0)dummy=0;										break;
						case 20:	if(dummy<0)dummy=0;										break;
						case 21:	if(dummy<MIN_LOG_INTERVAL)dummy=MIN_LOG_INTERVAL;		break;
						case 22:	if(dummy<3)dummy=3;		bool_UARTChanged = 1;			break;
						case 23:	if(dummy<0)dummy=0;										break;
					}
				}
				
			break;
		}
	}
	else if(!PARA_SELECT_KEY)
	{
		if(mode==NORMAL_MODE)
		{
			DPAutoCalModeTimer++;
			if(DPAutoCalModeTimer > 20)
			{
				DPAutoCalModeTimer=0;
				
				mode=DP_AUTO_CAL_MODE;
				
				progTimeout=60;
			}
		}
	}
	else
	{
		restoreFactoryCalibrationTimer=0;
		DPAutoCalModeTimer=0;
		DPAutoCalTimer=0;
		ProgModeTimer=0;
		MinMaxMeanModeTimer=0;
		MeanHrModeTimer=0;
		gu8_MinMaxClearTimer=0;
	}
}
//**********************************************************************************************************************************************/
void keyboard(void)
{	
	switch(keybyte)
	{
		/*case PROG_ENT:
		
			for(uint8_t i=0;i<NO_DIGIT;i++) data[i]=BLANK;
			
			if(mode==NORMAL_MODE)
			{
				mode=PROG_MODE;
				prog_para_cnt=0;
				Normal_para_cnt=0;
				b.RTCChangeOccure=0;
				b.UARTChanged=0;
				Lastpara_cnt=0;
				progTimeout=60;
				gu8_SetACKPwd=0;
			}
			else
			{
				mode=NORMAL_MODE;
				progTimeout=0;
			}
		
		break;
		*/
		
		case PARA_SELECT:
		
			switch(mode)
			{
				case NORMAL_MODE:
				
					if(Normal_para_cnt==5)
					{
						if(!gu8_SetACKPwd)
						{
							if((DP_Alrm_ON[DP1]) || (DP_Alrm_ON[DP2]) || (DP_Alrm_ON[DP3]))
							{
								gu8_SetACKPwd=1;
								dummy1=0;
							}
						}
						else if(gu8_SetACKPwd==1)
						{
							if((DP_Alrm_ON[DP1]) || (DP_Alrm_ON[DP2]) || (DP_Alrm_ON[DP3]))
							{
								gu8_SetACKPwd=2;
								dummy=0;
							}
						}
						else
						{
							if(AckPwdInd)
							{
								//for(uint8_t i=0;i<NO_OF_ACKPWD;i++)
								{
									if(AckPwd[dummy1-1]==dummy)
									{
										AlarmAckTimer=(unsigned long)AckTimer * 60;
										
										LogReading(ALM_ACK_LOG,dummy1,AckPwd[dummy1-1]);
										FillRamBuffer(ALM_ACK_LOG,dummy1,AckPwd[dummy1-1]);
										
										dummy1=0;
									}
								}
							}
							else
							{
								if(dummy==FACT_ACK_PWD)
								{
									AlarmAckTimer=(unsigned long)AckTimer * 60;
									
									LogReading(ALM_ACK_LOG,0,FACT_ACK_PWD);
									FillRamBuffer(ALM_ACK_LOG,0,FACT_ACK_PWD);
								}
							}
							
							Normal_para_cnt=0;
							gu8_SetACKPwd=0;
						}
					}
					
				break;
				
				case PROG_MODE: 
				
					progTimeout=60;
				
					////cli();			//Global Interrupt Disable
					
					switch(Lastpara_cnt)
					{
						case 1:
							if(DeviceID != dummy)
							{
								DeviceID = dummy;
								gu8_groupID = ((DeviceID - 1)/gu8_DeviceInGroup)+1;
								WriteEEPROMData(DEVICE_ID,&DeviceID,sizeof(DeviceID));
							}
						break;
						case 2:
							if(DP_Upper_Alm_ON[DP1] != dummy)
							{
								DP_Upper_Alm_ON[DP1] = dummy;
								WriteEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],2);
							}
						break;
						case 3:
							if(DP_Upper_Alm_OFF[DP1] != dummy)
							{
								DP_Upper_Alm_OFF[DP1] = dummy;
								WriteEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],2);
							}
						break;
						case 4:
							if(DP_Lower_Alm_OFF[DP1] != dummy)
							{
								DP_Lower_Alm_OFF[DP1] = dummy;
								WriteEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],2);
							}
						break;
						case 5:
							if(DP_Lower_Alm_ON[DP1] != dummy)
							{
								DP_Lower_Alm_ON[DP1] = dummy;
								WriteEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],2);
							}
						break;	
						case 6:
							if(DP_Upper_Alm_ON[DP2] != dummy)
							{
								DP_Upper_Alm_ON[DP2] = dummy;
								WriteEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],2);
							}
						break;
						case 7:
							if(DP_Upper_Alm_OFF[DP2] != dummy)
							{
								DP_Upper_Alm_OFF[DP2] = dummy;
								WriteEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],2);
							}
						break;
						case 8:
							if(DP_Lower_Alm_OFF[DP2] != dummy)
							{
								DP_Lower_Alm_OFF[DP2] = dummy;
								WriteEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],2);
							}
						break;
						case 9:
							if(DP_Lower_Alm_ON[DP2] != dummy)
							{
								DP_Lower_Alm_ON[DP2] = dummy;
								WriteEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],2);
							}
						break;								
						case 10:
							if(DP_Upper_Alm_ON[DP3] != dummy)
							{
								DP_Upper_Alm_ON[DP3] = dummy;
								WriteEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],2);
							}
						break;
						case 11:
							if(DP_Upper_Alm_OFF[DP3] != dummy)
							{
								DP_Upper_Alm_OFF[DP3] = dummy;
								WriteEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],2);
							}
						break;
						case 12:
							if(DP_Lower_Alm_OFF[DP3] != dummy)
							{
								DP_Lower_Alm_OFF[DP3] = dummy;
								WriteEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],2);
							}
						break;
						case 13:
							if(DP_Lower_Alm_ON[DP3] != dummy)
							{
								DP_Lower_Alm_ON[DP3] = dummy;
								WriteEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],2);
							}
						break;								
						case 14:
							if(Temp_RTC_ARR[0] != dummy)
							{
								Temp_RTC_ARR[0] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case 15:
							if(Temp_RTC_ARR[1] != dummy)
							{
								Temp_RTC_ARR[1] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case 16:
							if(Temp_RTC_ARR[2] != dummy)
							{
								Temp_RTC_ARR[2] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case 17:
							if(Temp_RTC_ARR[3] != dummy)
							{
								Temp_RTC_ARR[3] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case 18:
							if(Temp_RTC_ARR[4] != dummy)
							{
								Temp_RTC_ARR[4] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case 19:
							if(Buzzer_ON_Time != dummy)
							{
								Buzzer_ON_Time = dummy;
								WriteEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,2);
							
								if(!Buzzer_ON_Time)
								{
									//b.buzzerStart=NO;
									BUZZER_OFF;
									buzzerOnTime=0;
									buzzerOffTime=0;
								}
								else
								{
									if(bool_buzzerStart==YES)
									{
										buzzerOnTime=Buzzer_ON_Time;
										buzzerOffTime=0;
										BUZZER_ON;
									}
								}
							}
						break;
						case 20:
							if(Buzzer_OFF_Time != dummy)
							{
								Buzzer_OFF_Time = dummy;
								WriteEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,2);
							}
						break;
						case 21:
							if(LogInterval != dummy)
							{
								LogInterval = dummy;
								logTimer = LogInterval;
								//FlashlogTimer=60;
								//logTimer=(unsigned long)LogInterval*60;
								WriteEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,2);
							}
						break;
						case 22:
							if(UART_BaudRate != dummy)
							{
								UART_BaudRate = dummy;
								WriteEEPROMData(UART_BAUDRATE,&UART_BaudRate,sizeof(UART_BaudRate));
							}
						break;
					}
					
					if(bool_RTCChangeOccure==1)
					{
						RTC_data[0] = 0x00;	// enable oscillator (bit 7=0)
						RTC_data[1] = HEX2BCD(Temp_RTC_ARR[1]);	// minute = 59
						RTC_data[2] = HEX2BCD(Temp_RTC_ARR[0]);	// hour = 05 ,24-hour mode(bit 6=0)
						RTC_data[3] = HEX2BCD(Temp_RTC_ARR[2]);	// date = 30
						RTC_data[4] = HEX2BCD(Temp_RTC_ARR[3]);	// month = december
						RTC_data[5] = HEX2BCD(Temp_RTC_ARR[4]);	// year = 11 or 2011
						
						Write_byte_PCF8563(RTC_TIMESEC_REG,RTC_data[0]);
						Write_byte_PCF8563(RTC_TIMEMIN_REG,RTC_data[1]);
						Write_byte_PCF8563(RTC_TIMEHOUR_REG,RTC_data[2]);
						Write_byte_PCF8563(RTC_DATE_DATE_REG,RTC_data[3]);
						Write_byte_PCF8563(RTC_DATE_MONTH_REG,RTC_data[4]);
						Write_byte_PCF8563(RTC_DATE_YEAR_REG,RTC_data[5]);
						
//						rtc2.day = Temp_RTC_ARR[2];
//						rtc2.month = Temp_RTC_ARR[3];
//						rtc2.year = Temp_RTC_ARR[4];
//						rtc2.year += 2000;	
//						rtc2.hour = Temp_RTC_ARR[0];
//						rtc2.minute = Temp_RTC_ARR[1];
//						rtc2.second = 0;
//					
//						ep.currentEpochTime = get_epoch_time(rtc2);
						
						bool_RTCChangeOccure=0;
					}
					
					if(bool_UARTChanged==1)
					{
						UART_Configure(UART_BaudRate);
						bool_UARTChanged=0;
					}
					
					//sei();			//Global Interrupt Enable
					//----------------------------------------------------------------------
					prog_para_cnt++;
					
					if((prog_para_cnt==2) && !(gu16_parameterWord & ENABLE_DP1))
					{
						prog_para_cnt=6;
					}
					
					if((prog_para_cnt==6) && !(gu16_parameterWord & ENABLE_DP2))
					{
						prog_para_cnt=10;
					}
					
					if((prog_para_cnt==10) && !(gu16_parameterWord & ENABLE_DP3))
					{
						prog_para_cnt=14;
					}
					
					if((prog_para_cnt==14) && (gu16_parameterWord & ENABLE_LOG))
					{
						prog_para_cnt=19;
					}
					
					if((prog_para_cnt==21) && !(gu16_parameterWord & ENABLE_LOG))
					{
						prog_para_cnt=22;
					}
					
					if(prog_para_cnt>23)
					{
						prog_para_cnt=1;
					}
					
					dummy=0;
					
					Lastpara_cnt=prog_para_cnt;
					//----------------------------------------------------------------------
					switch(prog_para_cnt)
					{
						case 1:		dummy = DeviceID;				break;
						case 2:		dummy = DP_Upper_Alm_ON[DP1]; 		break;
						case 3:		dummy = DP_Upper_Alm_OFF[DP1]; 		break;
						case 4:		dummy = DP_Lower_Alm_OFF[DP1];  	break;
						case 5:		dummy = DP_Lower_Alm_ON[DP1];  		break;
						case 6:		dummy = DP_Upper_Alm_ON[DP2]; 		break;
						case 7:		dummy = DP_Upper_Alm_OFF[DP2]; 		break;
						case 8:		dummy = DP_Lower_Alm_OFF[DP2];  	break;
						case 9:		dummy = DP_Lower_Alm_ON[DP2];  		break;
						case 10:	dummy = DP_Upper_Alm_ON[DP3]; 		break;
						case 11:	dummy = DP_Upper_Alm_OFF[DP3]; 		break;
						case 12:	dummy = DP_Lower_Alm_OFF[DP3];  	break;
						case 13:	dummy = DP_Lower_Alm_ON[DP3];  		break;
						case 14:
							Temp_RTC_ARR[0] = rtc.hour;
							dummy = rtc.hour;
						break;
						case 15:
							Temp_RTC_ARR[1] = rtc.minute;
							dummy = rtc.minute;
						break;
						case 16:
							Temp_RTC_ARR[2] = rtc.day;
							dummy = rtc.day;
						break;
						case 17:
							Temp_RTC_ARR[3] = rtc.month;
							dummy = rtc.month;
						break;
						case 18:
							Temp_RTC_ARR[4] = rtc.year;
							dummy = rtc.year;
						break;
						case 19:	dummy = Buzzer_ON_Time;	  		break;
						case 20:	dummy = Buzzer_OFF_Time;	  	break;
						case 21:	dummy = LogInterval;			break;
						case 22:	dummy = UART_BaudRate;		  	break;
						case 23:	dummy = 0;						break;
					}
					
				break;
			}
		break;
		default:
		break;
	}
}

#endif

void StartBuzzer(void)
{
	if(bool_buzzerStart==NO)
	{
		bool_buzzerStart=YES;
		buzzerOnTime=Buzzer_ON_Time;
		buzzerOffTime=0;
		
		if(buzzerOnTime)
		{
			BUZZER_ON;
		}
	}
}

void StopBuzzer(void)
{
	bool_buzzerStart=NO;
	BUZZER_OFF;
	buzzerOnTime=0;
	buzzerOffTime=0;
}

void SendToSlave(void)
{
	memset(&Buffer1[0],0,sizeof(Buffer1));
	
	Buffer1[0]=50;
	
	Buffer1[1]=DeviceID;
	
	Buffer1[2]=0;//BatteryPercentage;
	
	Buffer1[3]=RTCSetFlag;
	Buffer1[4]=rtc.hour;
	Buffer1[5]=rtc.minute;
	Buffer1[6]=rtc.second;
	Buffer1[7]=rtc.day;
	Buffer1[8]=rtc.month;
	Buffer1[9]=rtc.year;
	
	Buffer1[10]=0;
	if(bool_DP_NC[DP1]) 	Buffer1[10] |= DP1_FAULTY;
	if(bool_DP_NC[DP2]) 	Buffer1[10] |= DP2_FAULTY;
	if(bool_DP_NC[DP3])		Buffer1[10] |= DP3_FAULTY;
	
	memcpy(&Buffer1[11],(uint8_t*)&Dpressure[DP1],4);
	memcpy(&Buffer1[15],(uint8_t*)&Dpressure[DP2],4);
	memcpy(&Buffer1[19],(uint8_t*)&Dpressure[DP3],4);
	memcpy(&Buffer1[23],(uint8_t*)&DP_Min[DP1],4);
	memcpy(&Buffer1[27],(uint8_t*)&DP_Max[DP1],4);
	memcpy(&Buffer1[31],(uint8_t*)&DP_Min[DP2],4);
	memcpy(&Buffer1[35],(uint8_t*)&DP_Max[DP2],4);
	memcpy(&Buffer1[39],(uint8_t*)&DP_Min[DP3],4);
	memcpy(&Buffer1[43],(uint8_t*)&DP_Max[DP3],4);
	
	if(gu16_parameterWord & ENABLE_DP1)
	{
		Buffer1[47] = DP_Alrm_ON[DP1];
	}

	if(gu16_parameterWord & ENABLE_DP2)
	{
		Buffer1[48] = DP_Alrm_ON[DP2];
	}
	
	if(gu16_parameterWord & ENABLE_DP3)
	{
		Buffer1[49] = DP_Alrm_ON[DP3];
	}
	
	Buffer1[50]=find_Checksum(50,&Buffer1[0]);
	
	SendToUART(&Buffer1[0],51);
	
	/*Buffer1[0]=0xFD;
	Buffer1[1]=0x00;
	Buffer1[2]=0x01;
	
	Buffer1[3]=DeviceID;
	
	Buffer1[4]=BatteryPercentage;
	
	Buffer1[5]=RTCSetFlag;
	Buffer1[6]=rtc.hour;
	Buffer1[7]=rtc.minute;
	Buffer1[8]=rtc.second;
	Buffer1[9]=rtc.day;
	Buffer1[10]=rtc.month;
	Buffer1[11]=rtc.year;
	
	Buffer1[12]=0;
	if(bool_DP_NC[DP1]) 	Buffer1[12] |= DP1_FAULTY;
	if(bool_DP_NC[DP2]) 	Buffer1[12] |= DP2_FAULTY;
	if(bool_DP_NC[DP3])Buffer1[12] |= DP3_FAULTY;
	
	memcpy(&Buffer1[13],(uint8_t*)&Dpressure[DP1],4);
	memcpy(&Buffer1[17],(uint8_t*)&Dpressure[DP2],4);
	memcpy(&Buffer1[21],(uint8_t*)&temperatureC,4);
	memcpy(&Buffer1[25],(uint8_t*)&humidityRH,4);
	memcpy(&Buffer1[29],(uint8_t*)&DP_Min[DP1],4);
	memcpy(&Buffer1[33],(uint8_t*)&DP_Max[DP1],4);
	memcpy(&Buffer1[37],(uint8_t*)&DP_Min[DP2],4);
	memcpy(&Buffer1[41],(uint8_t*)&DP_Max[DP2],4);
	memcpy(&Buffer1[45],(uint8_t*)&TM_Min,4);
	memcpy(&Buffer1[49],(uint8_t*)&TM_Max,4);
	memcpy(&Buffer1[53],(uint8_t*)&RH_Min,4);
	memcpy(&Buffer1[57],(uint8_t*)&RH_Max,4);
	
	if(gu16_parameterWord & ENABLE_DP1)
	{
		Buffer1[61] = DP_Alrm_ON[DP1];
	}

	if(gu16_parameterWord & ENABLE_DP2)
	{
		Buffer1[62] = DP_Alrm_ON[DP2];
	}

	if(gu16_parameterWord & ENABLE_TEMP)
	{
		Buffer1[63] = TM_Alrm_ON;
		
		if(TM_Unit)
		{
			Buffer1[63] |= 0x80;
		}
	}

	if(gu16_parameterWord & ENABLE_RH)
	{
		Buffer1[64] = RH_Alrm_ON;
	}
	
	Buffer1[65]=CalCRC(&Buffer1[1],64);
	Buffer1[66]=0xFC;
	
	SendToUART(&Buffer1[0],67);
	*/
}

//------------------------------------------------------------------------------
uint8_t find_Checksum(uint16_t Count,uint8_t *msg)
{
	uint32_t Total=0;
	uint16_t i=0;
	
	for (i=0; i<Count; i++)
	{
		Total += (uint32_t)*(msg + i);
	}
	Total = Total & 0xFF;
	Total = ((~Total) + 1) & 0xFF;
	
	return (uint8_t)(Total);
}


void EraseWholeFlash(void)
{
	if(gu16_parameterWord & ENABLE_M3LOG)
	{
		MinMaxMeanDayLogInd=0;
		WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
	}
	
	//Reset Data Logging Parameter -------------------------------------------
	CurrentLogIndReadLoc = 0;
	WriteEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,2);
	
	FlashOVFByte=0;
	WriteEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
	
	CurrentLogInd = 0;
	WriteEEPROMData(CURR_LOG_IND,(uint8_t*)&CurrentLogInd,4);
	
	CurrentLog24IndReadLoc = 0;
	WriteEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,sizeof(CurrentLog24IndReadLoc));
	
	CurrentLog24Ind = 0;
	WriteEEPROMData(CURR_LOG24_IND,(uint8_t*)&CurrentLog24Ind,2);
	
	bool_DPLog[DP1]=0;
	bool_DPLog[DP2]=0;
	bool_DPLog[DP3]=0;
	
	LastDP_Alrm_ON[DP1]=0;
	WriteEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
	
	LastDP_Alrm_ON[DP2]=0;
	WriteEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
	
	LastDP_Alrm_ON[DP3]=0;
	WriteEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
	
	#ifdef ENABLE_PRINTF
	opstr("Flash Erase\r\n");
	#endif
	
	/*
	DP1_RED_ON;
	AT45D_ChipErase();
	DP1_RED_OFF;
	*/

	data[1] = BLANK;
	data[2] = BLANK;
	data[3] = BLANK;

	data[4] = P;
	data[5] = 1;
	data[6] = 5;

	data[7] = E;
	data[8] = E;
	data[9] = BLANK;
	
	data[10] = E;
	data[11] = r;
	data[12] = 5;
	
	disp_value();
	
	//Erase whole Flash
	for(a1=0;a1<64;a1++)
	{
		AT45D_SectorErase(a1);
		
		//Serve Watchdog Timer
		IWDG_ReloadCounter();
	}
	
	RAMBufferLog=0;
	memset(&RAMBuffer[0],0,sizeof(RAMBuffer));
}

void FillRamBuffer(uint8_t logtype,uint8_t userID,uint16_t password)
{
	if((gu16_parameterWord & ENABLE_RTC) && !DP_StartUpTimer && RTCSetFlag)
	{
		unsigned short i=0;
		
		RAMBufferInd = RAM_FILL_START + (RAMBufferLog * LOG_SIZE);
		i=RAMBufferInd;
	
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&ep.currentEpochTime,4);
		RAMBufferInd += 4;
	
		RAMBuffer[RAMBufferInd++]=DeviceID;
		RAMBuffer[RAMBufferInd++]=logtype;
		RAMBuffer[RAMBufferInd++]=userID;
		RAMBuffer[RAMBufferInd++]=password & 0x00FF;
		RAMBuffer[RAMBufferInd++]=password >> 8;
	
		RAMBuffer[RAMBufferInd]=0;
		if(bool_DP_NC[DP1]) 	RAMBuffer[RAMBufferInd] |= DP1_FAULTY;
		if(bool_DP_NC[DP2]) 	RAMBuffer[RAMBufferInd] |= DP2_FAULTY;
		if(bool_DP_NC[DP3])		RAMBuffer[RAMBufferInd] |= DP3_FAULTY;
		RAMBufferInd++;
	
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&Dpressure[DP1],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&Dpressure[DP2],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&Dpressure[DP3],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Min[DP1],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Max[DP1],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Min[DP2],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Max[DP2],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Min[DP3],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Max[DP3],4);			RAMBufferInd += 4;
	
		if(gu16_parameterWord & ENABLE_DP1)
		{
			if(!DP_Alrm_ON[DP1]) 
			{
				if(logtype==DP1_ALM_RESTORE_LOG)
				{
					if(LastDP_Alrm_ON[DP1]==UPPER_ALARM)		 RAMBuffer[RAMBufferInd++] = 1;
					else if(LastDP_Alrm_ON[DP1]==LOWER_ALARM) RAMBuffer[RAMBufferInd++] = 2;
					else								 RAMBuffer[RAMBufferInd++] = 0;
				}
				else
				{
					RAMBuffer[RAMBufferInd++] = 0;
				}
			}
			else
			{
				if(LastDP_Alrm_ON[DP1]==UPPER_ALARM)		 RAMBuffer[RAMBufferInd++] = 1;
				else if(LastDP_Alrm_ON[DP1]==LOWER_ALARM) RAMBuffer[RAMBufferInd++] = 2;
				else								 RAMBuffer[RAMBufferInd++] = 0;
			}
		}
		else
		{
			RAMBuffer[RAMBufferInd++] = 0;
		}
		
		if(gu16_parameterWord & ENABLE_DP2)
		{
			if(!DP_Alrm_ON[DP2])
			{
				if(logtype==DP2_ALM_RESTORE_LOG)
				{
					if(LastDP_Alrm_ON[DP2]==UPPER_ALARM)		 RAMBuffer[RAMBufferInd++] = 1;
					else if(LastDP_Alrm_ON[DP2]==LOWER_ALARM) RAMBuffer[RAMBufferInd++] = 2;
					else								 RAMBuffer[RAMBufferInd++] = 0;
				}
				else
				{
					RAMBuffer[RAMBufferInd++] = 0;
				}
			}
			else
			{
				if(LastDP_Alrm_ON[DP2]==UPPER_ALARM)		 RAMBuffer[RAMBufferInd++] = 1;
				else if(LastDP_Alrm_ON[DP2]==LOWER_ALARM) RAMBuffer[RAMBufferInd++] = 2;
				else								 RAMBuffer[RAMBufferInd++] = 0;
			}
		}
		else
		{
			RAMBuffer[RAMBufferInd++] = 0;
		}
	
		if(gu16_parameterWord & ENABLE_DP3)
		{
			if(!DP_Alrm_ON[DP3])
			{
				if(logtype==DP3_ALM_RESTORE_LOG)
				{
					if(LastDP_Alrm_ON[DP3]==UPPER_ALARM)		 RAMBuffer[RAMBufferInd++] = 1;
					else if(LastDP_Alrm_ON[DP3]==LOWER_ALARM) RAMBuffer[RAMBufferInd++] = 2;
					else								 RAMBuffer[RAMBufferInd++] = 0;
				}
				else
				{
					RAMBuffer[RAMBufferInd++] = 0;
				}
			}
			else
			{
				if(LastDP_Alrm_ON[DP3]==UPPER_ALARM)		 RAMBuffer[RAMBufferInd++] = 1;
				else if(LastDP_Alrm_ON[DP3]==LOWER_ALARM) RAMBuffer[RAMBufferInd++] = 2;
				else								 RAMBuffer[RAMBufferInd++] = 0;
			}
		}
		else
		{
			RAMBuffer[RAMBufferInd++] = 0;
		}
	
		RAMBufferLog++;
		if(RAMBufferLog > 29) RAMBufferLog=0;
		
		//Log Data to 24 Hour memory Location ------------------------------------------
		if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_LOG))
		{
			WriteLog(REGULAR_LOG_ADDR,LAST_LOG24_ADDR_OFFSET + CurrentLog24Ind,&RAMBuffer[i],LOG_SIZE);
			
			//cli();
			CurrentLog24Ind++;
			if(CurrentLog24Ind>=LAST_LOG24_ADDR)
			{
				CurrentLog24Ind=0;
				
				CurrentLog24IndReadLoc++;
				if(CurrentLog24IndReadLoc>=100)
				{
					CurrentLog24IndReadLoc=0;
				}
				WriteEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,sizeof(CurrentLog24IndReadLoc));
			}
			WriteEEPROMData((CURR_LOG24_IND+(CurrentLog24IndReadLoc*2)),(uint8_t*)&CurrentLog24Ind,2);
			//sei();
		}
	}
}

void LogReading(uint8_t logtype,uint8_t userID,uint16_t password)
{
	if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_LOG) && (gu16_parameterWord & ENABLE_RTC) && !DP_StartUpTimer && RTCSetFlag)
	{
		memset(&Buffer1[0],0,sizeof(Buffer1));
		
		memcpy(&Buffer1[0],(uint8_t*)&ep.currentEpochTime,4);
		Buffer1[4]=DeviceID;
		Buffer1[5]=logtype;
		Buffer1[6]=userID;
		Buffer1[7]=password & 0x00FF;
		Buffer1[8]=password >> 8;
	
		Buffer1[9]=0;
		if(bool_DP_NC[DP1]) 	Buffer1[9] |= DP1_FAULTY;
		if(bool_DP_NC[DP2]) 	Buffer1[9] |= DP2_FAULTY;
		if(bool_DP_NC[DP3])		Buffer1[9] |= DP3_FAULTY;
	
		memcpy(&Buffer1[10],(uint8_t*)&Dpressure[DP1],4);
		memcpy(&Buffer1[14],(uint8_t*)&Dpressure[DP2],4);
		memcpy(&Buffer1[18],(uint8_t*)&Dpressure[DP3],4);
		memcpy(&Buffer1[22],(uint8_t*)&DP_Min[DP1],4);
		memcpy(&Buffer1[26],(uint8_t*)&DP_Max[DP1],4);
		memcpy(&Buffer1[30],(uint8_t*)&DP_Min[DP2],4);
		memcpy(&Buffer1[34],(uint8_t*)&DP_Max[DP2],4);
		memcpy(&Buffer1[38],(uint8_t*)&DP_Min[DP3],4);
		memcpy(&Buffer1[42],(uint8_t*)&DP_Max[DP3],4);
	
		if(gu16_parameterWord & ENABLE_DP1)
		{
			if(!DP_Alrm_ON[DP1])
			{
				if(logtype==DP1_ALM_RESTORE_LOG)
				{
					if(LastDP_Alrm_ON[DP1]==UPPER_ALARM)		 Buffer1[46] = 1;
					else if(LastDP_Alrm_ON[DP1]==LOWER_ALARM) Buffer1[46] = 2;
					else								 Buffer1[46] = 0;
				}
				else
				{
					Buffer1[46] = 0;	
				}
			}
			else
			{
				if(LastDP_Alrm_ON[DP1]==UPPER_ALARM)		 Buffer1[46] = 1;
				else if(LastDP_Alrm_ON[DP1]==LOWER_ALARM) Buffer1[46] = 2;
				else								 Buffer1[46] = 0;
			}
		}
		else
		{
			Buffer1[46] = 0;
		}
		
		if(gu16_parameterWord & ENABLE_DP2)
		{
			if(!DP_Alrm_ON[DP2])
			{
				if(logtype==DP2_ALM_RESTORE_LOG)
				{
					if(LastDP_Alrm_ON[DP2]==UPPER_ALARM)		 Buffer1[47] = 1;
					else if(LastDP_Alrm_ON[DP2]==LOWER_ALARM) Buffer1[47] = 2;
					else								 Buffer1[47] = 0;
				}
				else
				{
					Buffer1[47] = 0;
				}
			}
			else
			{
				if(LastDP_Alrm_ON[DP2]==UPPER_ALARM)		 Buffer1[47] = 1;
				else if(LastDP_Alrm_ON[DP2]==LOWER_ALARM) Buffer1[47] = 2;
				else								 Buffer1[47] = 0;
			}
		}
		else
		{
			Buffer1[47] = 0;
		}
		
		if(gu16_parameterWord & ENABLE_DP3)
		{
			if(!DP_Alrm_ON[DP3])
			{
				if(logtype==DP3_ALM_RESTORE_LOG)
				{
					if(LastDP_Alrm_ON[DP3]==UPPER_ALARM)		 Buffer1[48] = 1;
					else if(LastDP_Alrm_ON[DP3]==LOWER_ALARM) Buffer1[48] = 2;
					else								 Buffer1[48] = 0;
				}
				else
				{
					Buffer1[48] = 0;
				}
			}
			else
			{
				if(LastDP_Alrm_ON[DP3]==UPPER_ALARM)		 Buffer1[48] = 1;
				else if(LastDP_Alrm_ON[DP3]==LOWER_ALARM) Buffer1[48] = 2;
				else								 Buffer1[48] = 0;
			}
		}
		else
		{
			Buffer1[48] = 0;
		}
	
		WriteLog(REGULAR_LOG_ADDR,CurrentLogInd,&Buffer1[0],LOG_SIZE);
	
		//cli();
	
		CurrentLogInd++;
		if(CurrentLogInd>=LAST_LOG_ADDR)
		{
			CurrentLogInd=0;
			FlashOVFByte=1;
		
			WriteEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
		
			CurrentLogIndReadLoc++;
			if(CurrentLogIndReadLoc>=100)
			{
				CurrentLogIndReadLoc=0;
			}
			WriteEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,2);
		}
		WriteEEPROMData((CURR_LOG_IND + (CurrentLogIndReadLoc*4)),(uint8_t*)&CurrentLogInd,4);

		//sei();
	}
	
	#ifdef ENABLE_BROADCAST
	
		if(bool_brodcastEnb && !bool_FlashReadCmd && !bool_Flash24ReadCmd && !bool_MinMaxMeanLogReadCmd && !bool_MeanHrLogReadCmd && !bool_RamReadCmd && !bool_RamAllReadCmd)
		{
			Buffer1[0]=0xFD;
			memcpy(&Buffer1[1],&Buffer1[4],58);
			Buffer1[59]=CalCRC(&Buffer1[1],58);
			Buffer1[60]=0xFC;
		
			SendToUART(&Buffer1[0],61);
		}
	
	#endif
}

void AutoSendDataResponse(uint8_t SrcPort)
{
	uint8_t j=0;
	
	Buffer1[0]=0xFD;
	Buffer1[1]=DeviceID;
	Buffer1[2]=PARA_WITH_ALM_READ_CMD;
	Buffer1[3]=0x00;
	if(bool_paraIdNotValid) Buffer1[3] |= INVALID_PARA;
	if(bool_DP_NC[DP1]) 		Buffer1[3] |= DP1_FAULTY;
	if(bool_DP_NC[DP2]) 		Buffer1[3] |= DP2_FAULTY;
	if(bool_DP_NC[DP3]) 		Buffer1[3] |= DP3_FAULTY;
	
	j=4;
	
	if(bool_DP_NC[DP1]) 			
	{
		Buffer1[j++] |= DP1_FAULTY;
	}
	else
	{
		Buffer1[j++]=0;
		
		//Fill DP value
		templong = Dpressure[DP1]*100;
		j += fillValue(&Buffer1[j],templong);	
	}
	
	Buffer1[j++] = 0xEE;	//Field Separator
	
	if(bool_DP_NC[DP2])
	{
		Buffer1[j++] |= DP2_FAULTY;
	}
	else
	{
		Buffer1[j++]=0;
		
		//Fill DP value
		templong = Dpressure[DP2]*100;
		j += fillValue(&Buffer1[j],templong);
	}
	
	Buffer1[j++] = 0xEE;	//Field Separator
	
	if(bool_DP_NC[DP3])
	{
		Buffer1[j++] |= DP3_FAULTY;
	}
	else
	{
		Buffer1[j++]=0;
		
		//Fill DP value
		templong = Dpressure[DP3]*100;
		j += fillValue(&Buffer1[j],templong);
	}
	
	Buffer1[j++] = 0xEE;	//Field Separator

	if(gu16_parameterWord & ENABLE_DP1)
	{
		if(!DP_Alrm_ON[DP1])
		{
			Buffer1[j++] = 0;
		}
		else
		{
			if(DP_Alrm_ON[DP1]==UPPER_ALARM) Buffer1[j++] = 1;
			else						  Buffer1[j++] = 2;
		}
	}
	else
	{
		Buffer1[j++] = 0;
	}
	
	if(gu16_parameterWord & ENABLE_DP2)
	{
		if(!DP_Alrm_ON[DP2])
		{
			Buffer1[j++] = 0;
		}
		else
		{
			if(DP_Alrm_ON[DP2]==UPPER_ALARM) Buffer1[j++] = 1;
			else						  Buffer1[j++] = 2;
		}
	}
	else
	{
		Buffer1[j++] = 0;
	}
	
	if(gu16_parameterWord & ENABLE_DP3)
	{
		if(!DP_Alrm_ON[DP3])
		{
			Buffer1[j++] = 0;
		}
		else
		{
			if(DP_Alrm_ON[DP3]==UPPER_ALARM) Buffer1[j++] = 1;
			else						  Buffer1[j++] = 2;
		}
	}
	else
	{
		Buffer1[j++] = 0;
	}
	
	Buffer1[j++] = 0xEE;	//Field Separator
	
	#ifdef DISABLE_DOOR_SENSING

		//Door Close
		Buffer1[j++] = 0x0F;
		Buffer1[j++] = 0x0F;
	
	#else
		
	if(bool_doorStatus==OPEN)
	{
		//Door Open
		Buffer1[j++] = 0x0E;
		Buffer1[j++] = 0x0E;	
	}
	else
	{
		//Door Close
		Buffer1[j++] = 0x0F;
		Buffer1[j++] = 0x0F;
	}
		
	#endif
	
	Buffer1[j++] = 0xEE;	//Field Separator
	
	memcpy(&Buffer1[j],&gu8ar_SrNumber[8],8);
	j+=8;
	
	Buffer1[j++] = 0xEE;	//Field Separator
	
	memcpy(&Buffer1[j],(uint8_t*)&ep.currentEpochTime,4);
	j+=4;
	
	Buffer1[j]=CalCRC(&Buffer1[1],j-1);
	j++;
	
	Buffer1[j++]=0xFC;
	
	SendToUART(&Buffer1[0],j);
	//SetTxmode(Buffer1,j);
}
void ServePCMsg(void)
{
	#ifdef DEBUG_RCV_CMD
		opstr("\r\nMsg OK");
	#endif
	uint8_t index=0,j=0,lu8_sendResponse=0,lu8_GroupDelay=0,m=0;
	
	bool_paraIdNotValid=0;
	
	if(gu8_Mac2ValidTimer) gu8_Mac2ValidTimer=60;
	
	if(RxBuffer[2]==PARA_WITH_ALM_READ_CMD)
	{	
		if(RxInd==6)
		{
			if((RxBuffer[3]==0x00) || (RxBuffer[3]==gu8_groupID))
			{
				lu8_sendResponse = 1;
				
				//if(gu8_groupID>1)
				//{
					//for(m=0;m<gu8_groupID;m++)
					//{
						//PLATFORM_DelayMS(100);
					//}
				//}
				
				lu8_GroupDelay = (DeviceID - 1)%gu8_DeviceInGroup;
				
				if(lu8_GroupDelay)
				{
					for(m=0;m<lu8_GroupDelay;m++)
					{
						PLATFORM_DelayMS(1);
					}
				}
			}
			else
			{
				lu8_sendResponse = 0;
			}
		}
		else
		{	
			lu8_sendResponse = 1;
		}
		
		if(lu8_sendResponse==1)
		{
			RxBuffer[0]=0xFD;
			
			RxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	RxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			RxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			RxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			RxBuffer[3] |= DP3_FAULTY;
			
			j=4;
			
			if(bool_DP_NC[DP1]) 			
			{
				RxBuffer[j++] |= DP1_FAULTY;
			}
			else
			{
				RxBuffer[j++]=0;
				
				//Fill DP value
				templong = Dpressure[DP1]*100;
				j += fillValue(&RxBuffer[j],templong);	
			}
			
			RxBuffer[j++] = 0xEE;	//Field Separator
			
			if(bool_DP_NC[DP2])
			{
				RxBuffer[j++] |= DP2_FAULTY;
			}
			else
			{
				RxBuffer[j++]=0;
				
				//Fill DP value
				templong = Dpressure[DP2]*100;
				j += fillValue(&RxBuffer[j],templong);
			}
			
			RxBuffer[j++] = 0xEE;	//Field Separator
			
			if(bool_DP_NC[DP3])
			{
				RxBuffer[j++] |= DP3_FAULTY;
			}
			else
			{
				RxBuffer[j++]=0;
				
				//Fill DP value
				templong = Dpressure[DP3]*100;
				j += fillValue(&RxBuffer[j],templong);
			}
			
			RxBuffer[j++] = 0xEE;	//Field Separator
			
			if(gu16_parameterWord & ENABLE_DP1)
			{
				if(!DP_Alrm_ON[DP1])
				{
					RxBuffer[j++] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP1]==UPPER_ALARM) RxBuffer[j++] = 1;
					else						  RxBuffer[j++] = 2;
				}
			}
			else
			{
				RxBuffer[j++] = 0;
			}
			
			if(gu16_parameterWord & ENABLE_DP2)
			{
				if(!DP_Alrm_ON[DP2])
				{
					RxBuffer[j++] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP2]==UPPER_ALARM) RxBuffer[j++] = 1;
					else						  RxBuffer[j++] = 2;
				}
			}
			else
			{
				RxBuffer[j++] = 0;
			}
			
			if(gu16_parameterWord & ENABLE_DP3)
			{
				if(!DP_Alrm_ON[DP3])
				{
					RxBuffer[j++] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP3]==UPPER_ALARM) RxBuffer[j++] = 1;
					else						  RxBuffer[j++] = 2;
				}
			}
			else
			{
				RxBuffer[j++] = 0;
			}
			
			RxBuffer[j++] = 0xEE;	//Field Separator
			
			#ifdef DISABLE_DOOR_SENSING

				//Door Close
				RxBuffer[j++] = 0x0F;
				RxBuffer[j++] = 0x0F;
			
			#else
				
			if(bool_doorStatus==OPEN)
			{
				//Door Open
				RxBuffer[j++] = 0x0E;
				RxBuffer[j++] = 0x0E;	
			}
			else
			{
				//Door Close
				RxBuffer[j++] = 0x0F;
				RxBuffer[j++] = 0x0F;
			}
				
			#endif
			
			RxBuffer[j++] = 0xEE;	//Field Separator
			
			memcpy(&RxBuffer[j],&gu8ar_SrNumber[8],8);
			j+=8;
			
			RxBuffer[j++] = 0xEE;	//Field Separator
			
			memcpy(&RxBuffer[j],(uint8_t*)&ep.currentEpochTime,4);
			j+=4;
			
			RxBuffer[j]=CalCRC(&RxBuffer[1],j-1);
			j++;
			
			RxBuffer[j++]=0xFC;
		
			SendToUART(RxBuffer,j);
			//SetTxmode(RxBuffer,j);
		}
	}
	else if(RxBuffer[2]==PARA_WRITE_CMD)
	{
		#ifdef DEBUG_RCV_CMD
			opstr("\r\nWrite Cmd");
		#endif
		
		switch(RxBuffer[3])
		{
			case ACK_PW_ID:	
			case ALM_ACK_ID:			tempshort = findValue(&RxBuffer[5],RxInd-7);	break;
			case XBEE_MAC_ADDR_ID:		break;
			case SET_DPARA_PWD_ID:		break;
			case SRNO_ID:		  		break;
			case BRDSTR_ID:		  		break;
			case BRDSTP_ID:		  		break;
			case DATETIME_ID:	  		break;
			case EXT_FLASH_ERASE_ID:	break;
			case DFLT_RTC_ID:			break;
			case DFLT_CAL_ID:			break;
			case CORR_RTC_DATA_ID:		break;
			case BIG_FONT_LED_SET_ID:	break;
			case DP1CAL_ID:
			case DP2CAL_ID:
			case DP3CAL_ID:
			
				tempshort = findValue(&RxBuffer[4],5);
				
			break;
			
			case DP_SW_FACT_ID:
			
				tempshort = findValue(&RxBuffer[6],5);
				if(RxBuffer[5]=='-') tempshort *= (-1);	
				
			break;
			
			default: 
				tempshort = findValue(&RxBuffer[4],RxInd-6);
			break;
		}
			
		#ifdef DEBUG_RCV_CMD
			opstr("\r\nValue:");
			print_float(tempshort,test,0);	
		#endif

		//cli();
		
		switch(RxBuffer[3])
		{
			case SET_DPARA_PWD_ID:
			
				us2=0;	//password
				us1 = RxBuffer[4]-'0';		us1 *= 1000;			us2 += us1;		us1 = 0;
				us1 = RxBuffer[5]-'0';		us1 *= 100;				us2 += us1;		us1 = 0;
				us1 = RxBuffer[6]-'0';		us1 *= 10;				us2 += us1;		us1 = 0;
				us1 = RxBuffer[7]-'0';								us2 += us1;		us1 = 0;
				
				if(us2 == FACTORY_PARASET_PWD)
				{
					us3=0;	//Display Parameter bit pattern
					us1 = RxBuffer[8]-'0';		us1 *= 10000;			us3 += us1;		us1 = 0;
					us1 = RxBuffer[9]-'0';		us1 *= 1000;			us3 += us1;		us1 = 0;
					us1 = RxBuffer[10]-'0';		us1 *= 100;				us3 += us1;		us1 = 0;
					us1 = RxBuffer[11]-'0';		us1 *= 10;				us3 += us1;		us1 = 0;
					us1 = RxBuffer[12]-'0';								us3 += us1;		us1 = 0;
					
					gu16_parameterWord=us3;
					WriteEEPROMData(DISP_PARA_SELECT,(uint8_t*)&gu16_parameterWord,2);

					gu8_restartTimer = 3;
				}
			
			break;
			
			case DATETIME_ID:
			
				a1 = (RxBuffer[4]-'0');
				a1 *= 10;
				a2 = (RxBuffer[5]-'0');
				a1 += a2;
				rtc2.day = a1;//HEX2BCD(a1);

				a1 = (RxBuffer[6]-'0');
				a1 *= 10;
				a2 = (RxBuffer[7]-'0');
				a1 += a2;
				rtc2.month = a1;//HEX2BCD(a1);

				a1 = (RxBuffer[8]-'0');
				a1 *= 10;
				a2 = (RxBuffer[9]-'0');
				a1 += a2;
				rtc2.year = a1;//HEX2BCD(a1);
				rtc2.year += 2000;	//Year
				
				a1 = (RxBuffer[10]-'0');
				a1 *= 10;
				a2 = (RxBuffer[11]-'0');
				a1 += a2;
				rtc2.hour = a1;//HEX2BCD(a1);

				a1 = (RxBuffer[12]-'0');
				a1 *= 10;
				a2 = (RxBuffer[13]-'0');
				a1 += a2;
				rtc2.minute = a1;//HEX2BCD(a1);

				a1 = (RxBuffer[14]-'0');
				a1 *= 10;
				a2 = (RxBuffer[15]-'0');
				a1 += a2;
				rtc2.second = a1;//HEX2BCD(a1);

				ep1.currentEpochTime = get_epoch_time(rtc2);
				
				rtc2.second = HEX2BCD(rtc2.second);
				rtc2.minute = HEX2BCD(rtc2.minute);
				rtc2.hour = HEX2BCD(rtc2.hour);
				rtc2.day = HEX2BCD(rtc2.day);
				rtc2.month = HEX2BCD(rtc2.month);
				rtc2.year -= 2000;
				rtc2.year = HEX2BCD(rtc2.year);
				
				//If SetDate is greater than current date then set it otherwise discard it
				if(ep1.currentEpochTime >= ep.currentEpochTime)
				{
					Write_byte_PCF8563(RTC_TIMESEC_REG,rtc2.second); 
					Write_byte_PCF8563(RTC_TIMEMIN_REG,rtc2.minute); 
					Write_byte_PCF8563(RTC_TIMEHOUR_REG,rtc2.hour); 
					Write_byte_PCF8563(RTC_DATE_DATE_REG,rtc2.day); 
					Write_byte_PCF8563(RTC_DATE_MONTH_REG,rtc2.month); 
					Write_byte_PCF8563(RTC_DATE_YEAR_REG,rtc2.year); 
		
					RTCSetFlag=1;
					WriteEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
				}

			break;
			case DP1UAON_ID:	
				ss1 = (tempshort/10);
				if((ss1<=1000) && (ss1>= DP_Upper_Alm_OFF[DP1]))
				{
					DP_Upper_Alm_ON[DP1]=ss1;
					WriteEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],2); 
				}
			break;
			case DP1UAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_ON[DP1]) && (ss1>= DP_Lower_Alm_OFF[DP1]))
				{
					DP_Upper_Alm_OFF[DP1]=ss1;
					WriteEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],2); 
				}
			break;
			case DP1LAON_ID:	
				ss1 = (tempshort/10);
				if((ss1<=DP_Lower_Alm_OFF[DP1]) && (ss1>= -1000))	
				{
					DP_Lower_Alm_ON[DP1]=ss1;
					WriteEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],2); 
				}
			break;
			case DP1LAOFF_ID:	
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_OFF[DP1]) && (ss1>= DP_Lower_Alm_ON[DP1]))	
				{
					DP_Lower_Alm_OFF[DP1]=ss1;
					WriteEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],2); 
				}
			break;
			
			case DP2UAON_ID:
				ss1 = (tempshort/10);
				if((ss1<=1000) && (ss1>= DP_Upper_Alm_OFF[DP2]))
				{
					DP_Upper_Alm_ON[DP2]=ss1;
					WriteEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],2);
				}
			break;
			case DP2UAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_ON[DP2]) && (ss1>= DP_Lower_Alm_OFF[DP2]))
				{
					DP_Upper_Alm_OFF[DP2]=ss1;
					WriteEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],2);
				}
			break;
			case DP2LAON_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Lower_Alm_OFF[DP2]) && (ss1>= -1000))
				{
					DP_Lower_Alm_ON[DP2]=ss1;
					WriteEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],2);
				}
			break;
			case DP2LAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_OFF[DP2]) && (ss1>= DP_Lower_Alm_ON[DP2]))
				{
					DP_Lower_Alm_OFF[DP2]=ss1;
					WriteEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],2);
				}
			break;
			
			case DP3UAON_ID:
				ss1 = (tempshort/10);
				if((ss1<=1000) && (ss1>= DP_Upper_Alm_OFF[DP3]))
				{
					DP_Upper_Alm_ON[DP3]=ss1;
					WriteEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],2);
				}
			break;
			case DP3UAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_ON[DP3]) && (ss1>= DP_Lower_Alm_OFF[DP3]))
				{
					DP_Upper_Alm_OFF[DP3]=ss1;
					WriteEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],2);
				}
			break;
			case DP3LAON_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Lower_Alm_OFF[DP3]) && (ss1>= -1000))
				{
					DP_Lower_Alm_ON[DP3]=ss1;
					WriteEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],2);
				}
			break;
			case DP3LAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_OFF[DP3]) && (ss1>= DP_Lower_Alm_ON[DP3]))
				{
					DP_Lower_Alm_OFF[DP3]=ss1;
					WriteEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],2);
				}
			break;
				
			case LOGINTVAL_ID:		
				if((tempshort>=MIN_LOG_INTERVAL) && (tempshort<=MAX_LOG_INTERVAL))
				{
					LogInterval=tempshort;
					logTimer = LogInterval;
					//FlashlogTimer=60;
					//logTimer=(unsigned long)LogInterval*60;
					WriteEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,2);	
				}
			break;
			case DVCID_ID:			
				gu8_deviceIDChangeTryTimer = 10;
				gu8_deviceIDChangeTry++;
				if(gu8_deviceIDChangeTry>1)
				{
					gu8_deviceIDChangeTry=0;
					
					DeviceID=tempshort;
					gu8_groupID = ((DeviceID - 1)/gu8_DeviceInGroup)+1;
					WriteEEPROMData(DEVICE_ID,&DeviceID,sizeof(DeviceID));
				}
				
			break;
			case BZRON_ID:		
				Buzzer_ON_Time=tempshort;
				WriteEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,2);
				if(!Buzzer_ON_Time)
				{
					//bool_buzzerStart=NO;
					BUZZER_OFF;
					buzzerOnTime=0;
					buzzerOffTime=0;
				}
				else
				{
					if(bool_buzzerStart==YES)
					{
						buzzerOnTime=Buzzer_ON_Time;
						buzzerOffTime=0;
						BUZZER_ON;
					}
				}
			break;

			case BZROFF_ID:		
				Buzzer_OFF_Time=tempshort;
				WriteEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,2);
			break;
			
			case EXT_FLASH_ERASE_ID:
			
				//Erase External Flash
				bool_EraseFlash=1;
				
			break;
			
			case DFLT_RTC_ID:
				
				//Erase External Flash
				bool_EraseFlash=1;
				
				//Set Default RTC
			
				Write_byte_PCF8563(RTC_TIMESEC_REG,0); 
				Write_byte_PCF8563(RTC_TIMEMIN_REG,0); 
				Write_byte_PCF8563(RTC_TIMEHOUR_REG,0); 
				Write_byte_PCF8563(RTC_DATE_DATE_REG,1); 
				Write_byte_PCF8563(RTC_DATE_MONTH_REG,1); 
				Write_byte_PCF8563(RTC_DATE_YEAR_REG,0); 
				
				RTCSetFlag=0;
				WriteEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
				
			break;
			
			case DFLT_CAL_ID:
			
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					switch(RxBuffer[4])
					{
						case '0':
					
							//DP_Cal_Value_F[DP1]=0;
							//WriteEEPROMData(DP1_CAL_VAL_F_ADDR,DP_Cal_Value_F[DP1]);

							DP_Cal_Value_C[DP1]=0;
							WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],2);

							//DP_Cal_float_Value_F[DP1] = 0.0;
							DP_Cal_float_Value_C[DP1] = 0.0;
						
						break;
					
						case '1':
					
							//DP_Cal_Value_F[DP2]=0;
							//WriteEEPROMData(DP2_CAL_VAL_F_ADDR,DP_Cal_Value_F[DP2]);
						
							DP_Cal_Value_C[DP2]=0;
							WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],2);
						
							//DP_Cal_float_Value_F[DP2] = 0.0;
							DP_Cal_float_Value_C[DP2] = 0.0;
					
						break;
					
						case '2':
					
							//DP_Cal_Value_F[DP3]=0;
							//WriteEEPROMData(DP3_CAL_VAL_F_ADDR,DP_Cal_Value_F[DP3]);
						
							DP_Cal_Value_C[DP3]=0;
							WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
						
							//DP_Cal_float_Value_F[DP3] = 0.0;
							DP_Cal_float_Value_C[DP3] = 0.0;
					
						break;
					}
				}
			break;

			case CAL_FPWD_ID:		
				if(tempshort==FACTORY_PASSWORD)
				{
					bool_FactoryCalibrationOn=1;
					bool_CustmerCalibrationOn=0;
					PCCalibrationTimer=60;
				}
			break;
			
			case CAL_CPWD_ID:		
				if((tempshort==CustPassword) || (tempshort==FactCustPassword) || (tempshort==989))
				{	
					bool_FactoryCalibrationOn=0;
					bool_CustmerCalibrationOn=1;
					PCCalibrationTimer=60;
				}

			break;
			case CPWD_ID:		
				if(tempshort<=999)
				{
					CustPassword=tempshort;
					WriteEEPROMData(CUSTOMER_PASSWORD,(uint8_t*)&CustPassword,2);
				}
			break;
			case FCPWD_ID:
				if(tempshort<=9999)
				{
					FactCustPassword=tempshort;
					WriteEEPROMData(FAC_CUSTOMER_PASSWORD,(uint8_t*)&FactCustPassword,2);
				}
			break;
				
			case DP_OFFSET_ID:
			
				if(bool_CustmerCalibrationOn==1)
				{
					index=RxBuffer[4]-'0';
					
					if(index<MAX_SUPPORTED_DP)
					{
						su16_dp_offset[index]=tempshort;
						f32_dp_offset[index]=(float)su16_dp_offset[index]/100.0;
						WriteEEPROMData((DP_OFFSET_ADDR+(index*2)),(uint8_t*)&su16_dp_offset[index],2);
						gu8_dp_sw_factor_add_cnt[index]=0;
						TempDpressure[index]=0;
					}
				}
				
			break;
			
			case DP_SW_FACT_ID:
			
				if(bool_CustmerCalibrationOn==1)
				{
					index=RxBuffer[4]-'0';
					
					if(index<MAX_SUPPORTED_DP)
					{	
						su16_dp_sw_factor[index]=tempshort;
						f32_dp_sw_factor[index]=(float)su16_dp_sw_factor[index]/100.0;
						WriteEEPROMData((DP_SW_FACT_ADDR+(index*2)),(uint8_t*)&su16_dp_sw_factor[index],2);
					}
				}	
				
			break;
			
			case DP_LIMIT_ID:
			
				index=RxBuffer[4]-'0';
				
				if(index<MAX_SUPPORTED_DP)
				{
					u16_dp_limit[index]=tempshort;
					f32_dp_limit[index]=(float)u16_dp_limit[index]/10.0;
					WriteEEPROMData((DP_LIMIT_ADDR+(index*2)),(uint8_t*)&u16_dp_limit[index],2);
				}
			
			break;
			case RELAY_CNTL_ID:
			
				switch(RxBuffer[4])
				{
					case '1':
						if(RxBuffer[5]=='0') RELAY1_OFF;
						else 			RELAY1_ON;
					break;
					case '2':
						if(RxBuffer[5]=='0') RELAY2_OFF;
						else 			RELAY2_ON;
					break;
				}
				
				gu8_rly_stat = 0;
				
				if(RELAY1_STAT) gu8_rly_stat |= 0x01;
				if(RELAY2_STAT) gu8_rly_stat |= 0x02;
				
				WriteEEPROMData(RELAY_STAT_ADDR,&gu8_rly_stat,sizeof(gu8_rly_stat));

			break;

			case DP1CAL_ID:
				
				if(bool_FactoryCalibrationOn==1)
				{
					//DP_Cal_Count[DP1]=tempshort;
					//DP_Cal_Count_C[DP1]=0;
					//WriteEEPROMData(DP1_CAL_CNT,DP_Cal_Count[DP1]);
					
					DP_Cal_Value_F[DP1] = RealDpressure[DP1]*10.0;
					WriteEEPROMData(DP1_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP1],2);
					DP_Cal_float_Value_F[DP1] = (float)DP_Cal_Value_F[DP1]/10.0;
					
					DP_Cal_Value_C[DP1] = 0;
					DP_Cal_float_Value_C[DP1] = 0;
					
					su16_dp_sw_factor[DP1]=0;
					f32_dp_sw_factor[DP1]=0.0;
					WriteEEPROMData(DP_SW_FACT_ADDR,(uint8_t*)&su16_dp_sw_factor[DP1],2);
					
					su16_dp_offset[DP1]=0;
					f32_dp_offset[DP1]=0.0;
					WriteEEPROMData(DP_OFFSET_ADDR,(uint8_t*)&su16_dp_offset[DP1],2);
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					//DP_Cal_Count_C[DP1]=tempshort;
					
					DP_Cal_Value_C[DP1] = (RealDpressure[DP1] - DP_Cal_float_Value_F[DP1])*10.0;
					DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;
				}
				
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;
					
					//WriteEEPROMData(DP1_CAL_CNT_C,DP_Cal_Count_C[DP1]);
					WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],2);
					
					DP_Max[DP1] = DEFAUT_DP1_MAX;
					DP_Min[DP1] = DEFAUT_DP1_MIN;
					WriteEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],4);
					WriteEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],4);
				}
				
				/*if(bool_FactoryCalibrationOn==1)
				{
					//DP_Cal_Count[DP1]=tempshort;
					//DP_Cal_Count_C[DP1]=0;
					//WriteEEPROMData(DP1_CAL_CNT,DP_Cal_Count[DP1]);
					
					DP_Cal_Value_F[DP1] = RealDpressure[DP1]*10.0;
					WriteEEPROMData(DP1_CAL_VAL_F_ADDR,DP_Cal_Value_F[DP1]);
					DP_Cal_float_Value_F[DP1] = (float)DP_Cal_Value_F[DP1]/10.0;
					
					DP_Cal_Value_C[DP1] = 0;
					DP_Cal_float_Value_C[DP1] = 0;
					
					WriteEEPROMData(DP1_CAL_DATE_ADDR,(uint8_t*)&RxBuffer[9],12);
					WriteEEPROMData(DP1_CAL_CERT_ADDR,(uint8_t*)&RxBuffer[21],15);
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					//DP_Cal_Count_C[DP1]=tempshort;
					
					DP_Cal_Value_C[DP1] = (RealDpressure[DP1] - DP_Cal_float_Value_F[DP1])*10.0;
					DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;
						
					Buffer1[0] = findValue(&RxBuffer[9],2);
					Buffer1[1] = findValue(&RxBuffer[11],2);
					Buffer1[2] = findValue(&RxBuffer[13],2);
								
					if(!Buffer1[0] && !Buffer1[1] && !Buffer1[2])
					{	
						if(!DP_UserCalDateInd[DP1]) a1=NO_OF_USER_CAL_DATE-1;
						else a1=DP_UserCalDateInd[DP1]-1;
					
						us1 = DP1_USER_CAL_DATE_ADDR + (a1 * 6);
						ReadEEPROMData(us1,(uint8_t*)&Buffer1[0],6);
						
						if(memcmp(&Buffer1[0],&RxBuffer[9],6))
						{
							us1 = DP1_USER_CAL_DATE_ADDR + (DP_UserCalDateInd[DP1] * 6);
							WriteEEPROMData(us1,(uint8_t*)&RxBuffer[9],6);
						
							DP_UserCalDateInd[DP1]++;
							if(DP_UserCalDateInd[DP1]>=NO_OF_USER_CAL_DATE) DP_UserCalDateInd[DP1]=0;
							WriteEEPROMData(DP1_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP1],sizeof(DP_UserCalDateInd[DP1]));
						}
					}
				}
				
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;
					
					//WriteEEPROMData(DP1_CAL_CNT_C,DP_Cal_Count_C[DP1]);
					WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1]);
					
					DP_Max[DP1] = DEFAUT_DP1_MAX;
					DP_Min[DP1] = DEFAUT_DP1_MIN;
					WriteEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],4);
					WriteEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],4);
				}*/
				
			break;
			
			case DP2CAL_ID:
			
				if(bool_FactoryCalibrationOn==1)
				{
					//DP_Cal_Count[DP2]=tempshort;
					//DP_Cal_Count_C[DP2]=0;
					//WriteEEPROMData(DP2_CAL_CNT,DP_Cal_Count[DP2]);
					
					DP_Cal_Value_F[DP2] = RealDpressure[DP2]*10.0;
					WriteEEPROMData(DP2_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP2],2);
					DP_Cal_float_Value_F[DP2] = (float)DP_Cal_Value_F[DP2]/10.0;
					
					DP_Cal_Value_C[DP2] = 0;
					DP_Cal_float_Value_C[DP2] = 0;
					
					su16_dp_sw_factor[DP2]=0;
					f32_dp_sw_factor[DP2]=0.0;
					WriteEEPROMData((DP_SW_FACT_ADDR+2),(uint8_t*)&su16_dp_sw_factor[DP2],2);
					
					su16_dp_offset[DP2]=0;
					f32_dp_offset[DP2]=0.0;
					WriteEEPROMData((DP_OFFSET_ADDR+2),(uint8_t*)&su16_dp_offset[DP2],2);
					
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					//DP_Cal_Count_C[DP2]=tempshort;
					
					DP_Cal_Value_C[DP2] = (RealDpressure[DP2] - DP_Cal_float_Value_F[DP2])*10.0;
					DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
				}
				
				//opstr("\r\nDP2_Cal_F:");
				//print_float(DP_Cal_float_Value_F[DP2],test,1);
				//opstr("\r\n");
				//
				//opstr("\r\nDP2_Cal_C:");
				//print_float(DP_Cal_float_Value_C[DP2],test,1);
				//opstr("\r\n");
			
				//opstr("\r\nDP2_Cal_float_Value_F:");
				//print_float(DP_Cal_float_Value_F[DP2],test,1);
				//opstr("\r\n");
				//
				//opstr("\r\nDP2_Cal_float_Value_C:");
				//print_float(DP_Cal_float_Value_C[DP2],test,1);
				//opstr("\r\n");
			
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;
					
					//WriteEEPROMData(DP2_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP2]);
					WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],2);
					
					DP_Max[DP2] = DEFAUT_DP2_MAX;
					DP_Min[DP2] = DEFAUT_DP2_MIN;
					WriteEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],4);
					WriteEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],4);
				}
				
				/*if(bool_FactoryCalibrationOn==1)
				{
					//DP_Cal_Count[DP2]=tempshort;
					//DP_Cal_Count_C[DP2]=0;
					//WriteEEPROMData(DP2_CAL_CNT,(uint8_t*)&DP_Cal_Count[DP2]);
					
					DP_Cal_Value_F[DP2] = RealDpressure[DP2]*10.0;
					WriteEEPROMData(DP2_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP2]);
					DP_Cal_float_Value_F[DP2] = (float)DP_Cal_Value_F[DP2]/10.0;
					
					DP_Cal_Value_C[DP2] = 0;
					DP_Cal_float_Value_C[DP2] = 0;
									
					WriteEEPROMData(DP2_CAL_DATE_ADDR,(uint8_t*)&RxBuffer[9],12);
					WriteEEPROMData(DP2_CAL_CERT_ADDR,(uint8_t*)&RxBuffer[21],15);
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					//DP_Cal_Count_C[DP2]=tempshort;
					
					DP_Cal_Value_C[DP2] = (RealDpressure[DP2] - DP_Cal_float_Value_F[DP2])*10.0;
					DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
					
					Buffer1[0] = findValue(&RxBuffer[9],2);
					Buffer1[1] = findValue(&RxBuffer[11],2);
					Buffer1[2] = findValue(&RxBuffer[13],2);
					
					if(!Buffer1[0] && !Buffer1[1] && !Buffer1[2])
					{
						if(!DP_UserCalDateInd[DP2]) a1=14;
						else a1=DP_UserCalDateInd[DP2]-1;
					
						us1 = DP2_USER_CAL_DATE_ADDR + (a1 * 6);
						ReadEEPROMData(us1(uint8_t*)&Buffer1[0],,6);
					
						if(memcmp(&Buffer1[0],&RxBuffer[9],6))
						{
							us1 = DP2_USER_CAL_DATE_ADDR + (DP_UserCalDateInd[DP2] * 6);
							WriteEEPROMData(us1,(uint8_t*)&RxBuffer[9],6);
						
							DP_UserCalDateInd[DP2]++;
							if(DP_UserCalDateInd[DP2]>14) DP_UserCalDateInd[DP2]=0;
							WriteEEPROMData(DP2_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP2],sizeof(DP_UserCalDateInd[DP2]));
						}
					}
				}
			
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;
					
					//WriteEEPROMData(DP2_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP2]);
					WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2]);
					
					DP_Max[DP2] = DEFAUT_DP2_MAX;
					DP_Min[DP2] = DEFAUT_DP2_MIN;
					WriteEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],4);
					WriteEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],4);
				}*/
				
			break;
			
			case DP3CAL_ID:
			
				if(bool_FactoryCalibrationOn==1)
				{
					//DP_Cal_Count[DP3]=tempshort;
					//DP_Cal_Count_C[DP3]=0;
					//WriteEEPROMData(DP3_CAL_CNT,DP_Cal_Count[DP3]);
					
					DP_Cal_Value_F[DP3] = RealDpressure[DP3]*10.0;
					WriteEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3],2);
					DP_Cal_float_Value_F[DP3] = (float)DP_Cal_Value_F[DP3]/10.0;
					
					DP_Cal_Value_C[DP3] = 0;
					DP_Cal_float_Value_C[DP3] = 0;
					
					su16_dp_sw_factor[DP3]=0;
					f32_dp_sw_factor[DP3]=0.0;
					WriteEEPROMData((DP_SW_FACT_ADDR+3),(uint8_t*)&su16_dp_sw_factor[DP3],2);
					
					su16_dp_offset[DP3]=0;
					f32_dp_offset[DP3]=0.0;
					WriteEEPROMData((DP_OFFSET_ADDR+4),(uint8_t*)&su16_dp_offset[DP3],2);
					
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					//DP_Cal_Count_C[DP3]=tempshort;
					
					DP_Cal_Value_C[DP3] = (RealDpressure[DP3] - DP_Cal_float_Value_F[DP3])*10.0;
					DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
				}
				
				//opstr("\r\nDP3_Cal_F:");
				//print_float(DP_Cal_float_Value_F[DP3],test,1);
				//opstr("\r\n");
				//
				//opstr("\r\nDP3_Cal_C:");
				//print_float(DP_Cal_float_Value_C[DP3],test,1);
				//opstr("\r\n");
			
				//opstr("\r\nDP3_Cal_float_Value_F:");
				//print_float(DP_Cal_float_Value_F[DP3],test,1);
				//opstr("\r\n");
				//
				//opstr("\r\nDP3_Cal_float_Value_C:");
				//print_float(DP_Cal_float_Value_C[DP3],test,1);
				//opstr("\r\n");
			
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;
					
					//WriteEEPROMData(DP3_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP3]);
					WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
					
					DP_Max[DP3] = DEFAUT_DP3_MAX;
					DP_Min[DP3] = DEFAUT_DP3_MIN;
					WriteEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],4);
					WriteEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],4);
				}
				
				/*if(bool_FactoryCalibrationOn==1)
				{
					//DP_Cal_Count[DP3]=tempshort;
					//DP_Cal_Count_C[DP3]=0;
					//WriteEEPROMData(DP3_CAL_CNT,(uint8_t*)&DP_Cal_Count[DP3]);
					
					DP_Cal_Value_F[DP3] = RealDpressure[DP3]*10.0;
					WriteEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3]);
					DP_Cal_float_Value_F[DP3] = (float)DP_Cal_Value_F[DP3]/10.0;
					
					DP_Cal_Value_C[DP3] = 0;
					DP_Cal_float_Value_C[DP3] = 0;
									
					WriteEEPROMData(DP3_CAL_DATE_ADDR,(uint8_t*)&RxBuffer[9],12);
					WriteEEPROMData(DP3_CAL_CERT_ADDR,(uint8_t*)&RxBuffer[21],15);
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					//DP_Cal_Count_C[DP3]=tempshort;
					
					DP_Cal_Value_C[DP3] = (RealDpressure[DP3] - DP_Cal_float_Value_F[DP3])*10.0;
					DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
					
					Buffer1[0] = findValue(&RxBuffer[9],2);
					Buffer1[1] = findValue(&RxBuffer[11],2);
					Buffer1[2] = findValue(&RxBuffer[13],2);
					
					if(!Buffer1[0] && !Buffer1[1] && !Buffer1[2])
					{
						if(!DP_UserCalDateInd[DP3]) a1=14;
						else a1=DP_UserCalDateInd[DP3]-1;
					
						us1 = DP3_USER_CAL_DATE_ADDR + (a1 * 6);
						ReadEEPROMData(us1,(uint8_t*)&Buffer1[0],6);
					
						if(memcmp(&Buffer1[0],&RxBuffer[9],6))
						{
							us1 = DP3_USER_CAL_DATE_ADDR + (DP_UserCalDateInd[DP3] * 6);
							WriteEEPROMData(us1,(uint8_t*)&RxBuffer[9],6);
						
							DP_UserCalDateInd[DP3]++;
							if(DP_UserCalDateInd[DP3]>14) DP_UserCalDateInd[DP3]=0;
							WriteEEPROMData(DP3_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP3],sizeof(DP_UserCalDateInd[DP3]));
						}
					}
				}
			
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;
					
					//WriteEEPROMData(DP3_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP3]);
					WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3]);
					
					DP_Max[DP3] = DEFAUT_DP3_MAX;
					DP_Min[DP3] = DEFAUT_DP3_MIN;
					WriteEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],4);
					WriteEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],4);
				}*/
				
			break;
			
			case UBRT_ID:
				if((tempshort>=3) && (tempshort<=9))
				{
					UART_BaudRate=tempshort;
					WriteEEPROMData(UART_BAUDRATE,&UART_BaudRate,sizeof(UART_BaudRate));
				}
			break;
			
//			case PUT_DFU_ID:
//				if(tempshort==DFU_PASSWORD)
//				{
//					lcd.Sym_DOOR = 0;
//					lcd.Sym_DOOR_SYM = 0;
//					lcd.Sym_DOOR_ALM = 0;
//					lcd.Sym_LOGO = 0;
//					lcd.Sym_MIN = 0;
//					lcd.Sym_MAX = 0;
//					lcd.Sym_MEAN = 0;
//					lcd.Sym_SET = 0;
//					lcd.Sym_ID = 0;
//					lcd.Sym_ACK = 0;
//					lcd.Sym_RTC_AM = 0;
//					lcd.Sym_RTC_PM = 0;
//					lcd.Sym_RTC_BC1 = 0;
//					lcd.Sym_RTC_COL = 0;

//					lcd.Sym_DP_LOGO = 0;
//					lcd.Sym_DP_LOGO_ALM = 0;
//					lcd.Sym_DP_UNIT = 0;
//					lcd.Sym_DP_MIN = 0;
//					lcd.Sym_DP_MIN_ALM = 0;
//					
//					lcd.Sym_RH_LOGO = 0;
//					lcd.Sym_RH_LOGO_ALM = 0;
//					lcd.Sym_RH_UNIT = 0;
//					lcd.Sym_RH_MIN = 0;
//					lcd.Sym_RH_MIN_ALM = 0;
//					
//					lcd.Sym_TM_LOGO = 0;
//					lcd.Sym_TM_LOGO_ALM = 0;
//					lcd.Sym_TM_UNIT_C = 0;
//					lcd.Sym_TM_UNIT_F = 0;
//					lcd.Sym_TM_MIN = 0;
//					lcd.Sym_TM_MIN_ALM = 0;
//					
//					for(uint8_t i=0;i<NO_DIGIT;i++) data[i]=BLANK;
//					data[4] = D;
//					data[5] = F;
//					data[6] = U;
//					
//					disp_value();
//					
//						WriteEEPROMData(DFU_NUMBER_LOGIC,(uint8_t*)&(0xABCD)); 
//					
//					gu8_restartTimer = 5;
//				}
//			break;
//			
			case MENB_ID:
				if((tempshort==0) || (tempshort==1))
				{
					gu8_masterEnable=tempshort;
					WriteEEPROMData(MASTER_ENABLE_ADDR,&gu8_masterEnable,sizeof(gu8_masterEnable));
				}
			break;
			case DOOR_SENSE_POLARITY_ID:
				if((tempshort==0) || (tempshort==1))
				{
					gu8_doorSensingPolarity=tempshort;
					WriteEEPROMData(DOOR_SENSE_POLARITY_ADDR,&gu8_doorSensingPolarity,sizeof(gu8_doorSensingPolarity));
				}
			break;
			case DOOR_SENSE_TIME_ID:
				if(tempshort<=250)
				{
					gu8_doorSensingTime=tempshort;
					WriteEEPROMData(DOOR_SENSE_TIME_ADDR,&gu8_doorSensingTime,sizeof(gu8_doorSensingTime));
				}
			break;
			case LCD_BRIGHT_CNT_ID:
				if(tempshort<=63)
				{
					gu8_LCDBrigthnessCnt=tempshort;
					TM1680WriteCommand(BRIGHTNESS | gu8_LCDBrigthnessCnt);
					WriteEEPROMData(LCD_BRIGHT_CNT_ADDR,&gu8_LCDBrigthnessCnt,sizeof(gu8_LCDBrigthnessCnt));
				}
			break;
			case AUTO_SENT_INTERVAL_ID:
				if((tempshort!=0) && (tempshort <= 240))
				{
					gu8_AutoSentInterval=tempshort;
					WriteEEPROMData(AUTO_SENT_INTERVAL_ADDR,&gu8_AutoSentInterval,sizeof(gu8_AutoSentInterval));
				}
			break;
			case DEVICES_IN_GROUP_ID:
				if((tempshort>=2) && (tempshort <= 100))
				{
					gu8_DeviceInGroup=tempshort;
					WriteEEPROMData(DEVICES_IN_GROUP_ADDR,&gu8_DeviceInGroup,sizeof(gu8_DeviceInGroup));
				}
			break;
			case XBEE_RST_INTERVAL_ID:
				if(tempshort <= 1440)
				{
					gu16_XbeeRstInterval=tempshort;
					gu32_triggerXbeeResetTimer = (unsigned long)gu16_XbeeRstInterval*60;
					WriteEEPROMData(XBEE_RST_INTERVAL_ADDR,(uint8_t*)&gu16_XbeeRstInterval,2);
				}
			break;
			case DP1_ALM_SENSE_TIME_ID:
				if(tempshort<=250)
				{
					gu8_DpAlarmSensingTime[DP1]=tempshort;
					WriteEEPROMData(DP1_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP1],sizeof(gu8_DpAlarmSensingTime[DP1]));
				}
			break;
			case DP2_ALM_SENSE_TIME_ID:
				if(tempshort<=250)
				{
					gu8_DpAlarmSensingTime[DP2]=tempshort;
					WriteEEPROMData(DP2_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP2],sizeof(gu8_DpAlarmSensingTime[DP2]));
				}
			break;
			case DP3_ALM_SENSE_TIME_ID:
				if(tempshort<=250)
				{
					gu8_DpAlarmSensingTime[DP3]=tempshort;
					WriteEEPROMData(DP3_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP3],sizeof(gu8_DpAlarmSensingTime[DP3]));
				}
			break;
			case XBEE_MAC_ADDR_ID:
				
				tempchar=RxBuffer[4]-'1';
				
				if(tempchar<NO_OF_XBEE_MAC)
				{			
					memcpy(&gu8arr_XbeeMac[tempchar][0],&RxBuffer[5],XBEE_MAC_SIZE);
					WriteEEPROMData((XBEE_MAC_ADDR+(tempchar*XBEE_MAC_SIZE)),(uint8_t*)&gu8arr_XbeeMac[tempchar][0],XBEE_MAC_SIZE);
				}
				
				if(tempchar==1)
				{
					gu8_Mac2ValidTimer = 60;
				}
				
			break;
			case ACK_TIMER_ID:
				AckTimer=tempshort;
				WriteEEPROMData(ACK_TIMER,(uint8_t*)&AckTimer,2);
			break;
			case ACK_PW_ID:
				
				if(RxBuffer[4]<=NO_OF_ACKPWD)
				{
					AckPwdInd=RxBuffer[4];
					tempchar=RxBuffer[4]-1;
					AckPwd[tempchar] = tempshort;
					
					WriteEEPROMData((ACK_PASSWORD+(2*tempchar)),(uint8_t*)&AckPwd[tempchar],2);
					WriteEEPROMData(ACK_PWD_IND,&AckPwdInd,sizeof(AckPwdInd));
				}
										
			break;
			case ALM_ACK_ID:

				if(AckPwdInd)
				{		
					a1=RxBuffer[4]-1;
									
					if(AckPwd[a1]==tempshort)
					{
						AlarmAckTimer=(unsigned long)AckTimer * 60;
							
						LogReading(ALM_ACK_LOG,RxBuffer[4],AckPwd[a1]);
						FillRamBuffer(ALM_ACK_LOG,RxBuffer[4],AckPwd[a1]);
						
						break;
					}
				}
				else
				{
					if(tempshort==FACT_ACK_PWD) 
					{
						AlarmAckTimer=(unsigned long)AckTimer * 60;	
						
						LogReading(ALM_ACK_LOG,0,FACT_ACK_PWD);
						FillRamBuffer(ALM_ACK_LOG,0,FACT_ACK_PWD);
					}
				}
			break;
			case SRNO_ID:
				memcpy(gu8ar_SrNumber,&RxBuffer[4],16);
				WriteEEPROMData(DEVICE_SR_NO,(uint8_t*)&gu8ar_SrNumber,16);
				gu32_SrNumber = ascii2hex(&gu8ar_SrNumber[8],8);
			break;	
			case BRDSTP_ID:
				bool_brodcastEnb=0;
				StartBroadcastTimer=0;
				gu8_broadcast=0;
				WriteEEPROMData(BROADCAST_ENB_ADDR,&gu8_broadcast,sizeof(gu8_broadcast));
			break;
			case BRDSTR_ID:
				bool_brodcastEnb=1;
				tempshort=findValue(&RxBuffer[4],RxInd-6);
				StartBroadcastTimer=(unsigned long)tempshort*60; 
				gu8_broadcast=1;
				WriteEEPROMData(BROADCAST_ENB_ADDR,&gu8_broadcast,sizeof(gu8_broadcast));
			break;
			default:
				bool_paraIdNotValid=1;
			break;
		}
		
		//sei();			//Global Interrupt Enable
		
		
		TxBuffer[0]=0xFD;
		TxBuffer[1]=RxBuffer[1];
		TxBuffer[2]=RxBuffer[2];
		TxBuffer[3]=0x00;
		if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
		if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
		if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
		if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
		
		for(j=4;j<RxInd;j++)TxBuffer[j]=RxBuffer[j-1];
		
		TxBuffer[RxInd-1]=CalCRC(&TxBuffer[1],RxInd-2);
		TxBuffer[RxInd]=0xFC;
		
		SetTxmode(TxBuffer,RxInd+1);
		
		if((RxBuffer[3]==UBRT_ID)||(RxBuffer[3]==UDBT_ID)||(RxBuffer[3]==UPRT_ID)||(RxBuffer[3]==USTB_ID))
		{
			UART_Configure(UART_BaudRate);
		}	
		
		if(RxBuffer[3]==XBEE_MAC_ADDR_ID)
		{
			if(gu8_Mac2ValidTimer) SetMAC2Xbee(&gu8arr_XbeeMac[1][0],0);
		}
		
	}
	else if(RxBuffer[2]==PARA_READ_CMD)
	{
		#ifdef DEBUG_RCV_CMD
			opstr("\r\nRead Cmd");
		#endif
		
		tempshort = 0;
		
		switch(RxBuffer[3])
		{
			case SEC_ID:		tempshort = rtc.second;					break;
			case MN_ID:			tempshort = rtc.minute;					break;
			case HR_ID:			tempshort = rtc.hour;					break;
			case DT_ID:			tempshort = rtc.day;					break;
			case MNTH_ID:		tempshort = rtc.month;					break;
			case YR_ID:			tempshort = (2000 + rtc.year);			break;
			case DP1UAON_ID:	tempshort = DP_Upper_Alm_ON[DP1]*10;		break;
			case DP1UAOFF_ID:	tempshort = DP_Upper_Alm_OFF[DP1]*10;		break;
			case DP1LAON_ID:	tempshort = DP_Lower_Alm_ON[DP1]*10;		break;
			case DP1LAOFF_ID:	tempshort = DP_Lower_Alm_OFF[DP1]*10;		break;
			case DP2UAON_ID:	tempshort = DP_Upper_Alm_ON[DP2]*10;		break;
			case DP2UAOFF_ID:	tempshort = DP_Upper_Alm_OFF[DP2]*10;		break;
			case DP2LAON_ID:	tempshort = DP_Lower_Alm_ON[DP2]*10;		break;
			case DP2LAOFF_ID:	tempshort = DP_Lower_Alm_OFF[DP2]*10;		break;
			case DP3UAON_ID:	tempshort = DP_Upper_Alm_ON[DP3]*10;		break;
			case DP3UAOFF_ID:	tempshort = DP_Upper_Alm_OFF[DP3]*10;		break;
			case DP3LAON_ID:	tempshort = DP_Lower_Alm_ON[DP3]*10;		break;
			case DP3LAOFF_ID:	tempshort = DP_Lower_Alm_OFF[DP3]*10;		break;
			case LOGINTVAL_ID:	tempshort = LogInterval;				break;
			case DVCID_ID:		tempshort = DeviceID;					break;
			case BZRON_ID:		tempshort = Buzzer_ON_Time;				break;
			case BZROFF_ID:		tempshort = Buzzer_OFF_Time;			break;
			case DP1MIN_ID:		tempshort = DP_Min[DP1]*100;				break;
			case DP1MAX_ID:		tempshort = DP_Max[DP1]*100;				break;
			case DP2MIN_ID:		tempshort = DP_Min[DP2]*100;				break;
			case DP2MAX_ID:		tempshort = DP_Max[DP2]*100;				break;
			case DP3MIN_ID:		tempshort = DP_Min[DP3]*100;				break;
			case DP3MAX_ID:		tempshort = DP_Max[DP3]*100;				break;
			case DP_SW_FACT_ID:
			
				index=RxBuffer[4]-'0';
				if(index<MAX_SUPPORTED_DP)
				{	
					tempshort = su16_dp_sw_factor[index];
				}
				
			break;
			case DP_LIMIT_ID:
			
				index=RxBuffer[4]-'0';
				if(index<MAX_SUPPORTED_DP)
				{
					tempshort = u16_dp_limit[index];
				}
			
			break;
			case DP1CAL_ID:		
					if(bool_FactoryCalibrationOn==1)
					{
						PCCalibrationTimer=60;
						tempshort = DP_Cal_Value_F[DP1];
					}
					else if(bool_CustmerCalibrationOn==1)
					{
						PCCalibrationTimer=60;
						tempshort = DP_Cal_Value_C[DP1];
					}		
					else
					{
						tempshort = 0;
					}		
					break;
			case DP2CAL_ID:
				if(bool_FactoryCalibrationOn==1)
				{
					PCCalibrationTimer=60;
					tempshort = DP_Cal_Value_F[DP2];
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					PCCalibrationTimer=60;
					tempshort = DP_Cal_Value_C[DP2];
				}
				else
				{
					tempshort = 0;
				}
			break;		
			case DP3CAL_ID:
				if(bool_FactoryCalibrationOn==1)
				{
					PCCalibrationTimer=60;
					tempshort = DP_Cal_Value_F[DP3];
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					PCCalibrationTimer=60;
					tempshort = DP_Cal_Value_C[DP3];
				}
				else
				{
					tempshort = 0;
				}
			break;		
			case SFVER_ID:		tempshort = SOFT_VER;					break;
			case ACK_TIMER_ID:	tempshort = AckTimer;					break;
			case CPWD_ID:		tempshort = CustPassword;				break;
			case FCPWD_ID:		tempshort = FactCustPassword;			break;
			case SET_DPARA_PWD_ID:	
				
				us2=0;	//password
				us1 = RxBuffer[4]-'0';		us1 *= 1000;			us2 += us1;		us1 = 0;
				us1 = RxBuffer[5]-'0';		us1 *= 100;				us2 += us1;		us1 = 0;
				us1 = RxBuffer[6]-'0';		us1 *= 10;				us2 += us1;		us1 = 0;
				us1 = RxBuffer[7]-'0';								us2 += us1;		us1 = 0;
				
				if(us2 == FACTORY_PARASET_PWD)
				{
					tempshort = gu16_parameterWord;			
				}
				
			break;
			
			case ACK_PW_ID:		tempshort = AckPwd[RxBuffer[4]-1];		break;
			case UBRT_ID:		tempshort = UART_BaudRate;				break;
			case MENB_ID:		tempshort = gu8_masterEnable;			break;
			case DOOR_SENSE_POLARITY_ID:	tempshort = gu8_doorSensingPolarity;	break;
			case DOOR_SENSE_TIME_ID:		tempshort = gu8_doorSensingTime;		break;
			case LCD_BRIGHT_CNT_ID:			tempshort = gu8_LCDBrigthnessCnt;		break;
			case AUTO_SENT_INTERVAL_ID:			tempshort = gu8_AutoSentInterval;		break;
			case DEVICES_IN_GROUP_ID:			tempshort = gu8_DeviceInGroup;			break;
			case XBEE_RST_INTERVAL_ID:			tempshort = gu16_XbeeRstInterval;		break;
			case DP1_ALM_SENSE_TIME_ID:			tempshort = gu8_DpAlarmSensingTime[DP1];	break;
			case DP2_ALM_SENSE_TIME_ID:			tempshort = gu8_DpAlarmSensingTime[DP2];	break;
			case DP3_ALM_SENSE_TIME_ID:			tempshort = gu8_DpAlarmSensingTime[DP3];	break;
			case SRNO_ID:												break;		
			case XBEE_SELF_MAC_ADDR_ID:									break;
			case XBEE_MAC_ADDR_ID:										break;
			case RAM_ALL_ID:											break;
			case RAM_IND_ID:											break;
			case FLASH24_IND_ID:										break;
			case MINMAXMEAN_IND_ID:										break;
			case MEAN_HR_ID:											break;
			case REALTIME_VAL_ID:										break;
			case RDLG_DT_ID:											break;
			case RDLG_CNT_ID:											break;
			case DATETIME_ID:											break;
			case CORR_RTC_DATA_ID:										break;
			case FLASH24_CUR_IND_ID:	tempshort = CurrentLog24Ind;	break;
			default:			bool_paraIdNotValid=1;						break;
		}
			
		if(RxBuffer[3]==DATETIME_ID)
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			chartostr(rtc.day,&TxBuffer[5],2);
			chartostr(rtc.month,&TxBuffer[7],2);
			chartostr(rtc.year,&TxBuffer[9],2);
			chartostr(rtc.hour,&TxBuffer[11],2);
			chartostr(rtc.minute,&TxBuffer[13],2);
			chartostr(rtc.second,&TxBuffer[15],2);
			
			TxBuffer[17]=CalCRC(&TxBuffer[1],16);
			TxBuffer[18]=0xFC;
			
			SetTxmode(TxBuffer,19);
		}
		else if(RxBuffer[3]==ACK_PW_ID)
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			
			tempchar = fillValue(&TxBuffer[5],tempshort);
			
			TxBuffer[5+tempchar]=CalCRC(&TxBuffer[1],4+tempchar);	
			TxBuffer[6+tempchar]=0xFC;
			
			SetTxmode(TxBuffer,7+tempchar);
		}
		else if(RxBuffer[3]==SRNO_ID)
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			
			memcpy(&TxBuffer[5],gu8ar_SrNumber,16);
			
			TxBuffer[21]=CalCRC(&TxBuffer[1],20);	
			TxBuffer[22]=0xFC;
			
			SetTxmode(TxBuffer,23);
		}
		else if(RxBuffer[3]==XBEE_MAC_ADDR_ID)
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			TxBuffer[5]=RxBuffer[4];
			
			tempchar = RxBuffer[4]-'1';
			
			memcpy(&TxBuffer[6],&gu8arr_XbeeMac[tempchar][0],XBEE_MAC_SIZE);
			
			TxBuffer[22]=CalCRC(&TxBuffer[1],21);	
			TxBuffer[23]=0xFC;
			
			SetTxmode(TxBuffer,24);
		}
		else if(RxBuffer[3]==XBEE_SELF_MAC_ADDR_ID)
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			
			memcpy(&TxBuffer[5],gu8arr_XbeeSelfMac,XBEE_MAC_SIZE);
			
			TxBuffer[21]=CalCRC(&TxBuffer[1],20);	
			TxBuffer[22]=0xFC;
			
			SetTxmode(TxBuffer,23);
		}
		else if(RxBuffer[3]==RAM_ALL_ID)
		{
			RAMBuffer[0]=0xFD;
			RAMBuffer[1]=RxBuffer[1];
			RAMBuffer[2]=RxBuffer[2];
			RAMBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	RAMBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			RAMBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			RAMBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 		RAMBuffer[3] |= DP3_FAULTY;
			RAMBuffer[4]=RxBuffer[3];

			//Send CRC and 0xFC
			RAMBuffer[1505]=CalCRC(&RAMBuffer[1],1504);	
			RAMBuffer[1506]=0xFC;
			
			SetTxmode(RAMBuffer,1507);
		}
		else if(RxBuffer[3]==RAM_IND_ID)
		{
			flash24_StartInd = RxBuffer[4];
			flash24_EndInd = RxBuffer[5];
						
			bool_RamReadCmd=1;
			//gu16_logtransfer=flash24_StartInd;
			bool_logtransferStart=0;
			
			if(RAMBufferLog)
			{
				flash24_StartInd=RAMBufferLog-1;
			}
			else
			{
				flash24_StartInd=29;
			}
			
			NoOf24Log=flash24_EndInd;
				
			/*
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(b.DP_NC) 			TxBuffer[3] |= DP_FAULTY;
			if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			TxBuffer[5]=RxBuffer[4];

			tempshort = (RxBuffer[4] * LOG_SIZE) + RAM_FILL_START;
			memcpy(&TxBuffer[6],&RAMBuffer[tempshort],LOG_SIZE);
			
			TxBuffer[56]=CalCRC(&TxBuffer[1],55);
			TxBuffer[57]=0xFC;
			
			SendToUART(&TxBuffer[0],58);
			*/
		}
		else if(RxBuffer[3]==FLASH24_IND_ID)
		{
			flash24_StartInd=0;
			us1 = RxBuffer[4]-'0';		us1 *= 1000;			flash24_StartInd += us1;		us1 = 0;
			us1 = RxBuffer[5]-'0';		us1 *= 100;				flash24_StartInd += us1;		us1 = 0;
			us1 = RxBuffer[6]-'0';		us1 *= 10;				flash24_StartInd += us1;		us1 = 0;
			us1 = RxBuffer[7]-'0';								flash24_StartInd += us1;		us1 = 0;
			
			flash24_EndInd=0;
			us1 = RxBuffer[8]-'0';		us1 *= 1000;			flash24_EndInd += us1;		us1 = 0;
			us1 = RxBuffer[9]-'0';		us1 *= 100;				flash24_EndInd += us1;		us1 = 0;
			us1 = RxBuffer[10]-'0';		us1 *= 10;				flash24_EndInd += us1;		us1 = 0;
			us1 = RxBuffer[11]-'0';								flash24_EndInd += us1;		us1 = 0;
			
			bool_Flash24ReadCmd=1;
			//gu16_logtransfer=flash24_StartInd;
			bool_logtransferStart=0;	
			
			if(CurrentLog24Ind)
			{
				flash24_StartInd=CurrentLog24Ind-1;	
			}
			else
			{
				flash24_StartInd=LAST_LOG24_ADDR-1;
			}
			
			NoOf24Log=flash24_EndInd;		
		}
		else if((gu16_parameterWord & ENABLE_M3LOG) && (RxBuffer[3]==MINMAXMEAN_IND_ID))
		{
			flash24_StartInd=0;
			
			MinMaxMeanReadParaType=RxBuffer[4];

			flash24_EndInd=0;
			us1 = RxBuffer[5]-'0';		us1 *= 10;				flash24_EndInd += us1;		us1 = 0;
			us1 = RxBuffer[6]-'0';								flash24_EndInd += us1;		us1 = 0;
			
			bool_MinMaxMeanLogReadCmd=1;
			bool_logtransferStart=0;
			
			if(MinMaxMeanDayLogInd)
			{
				flash24_StartInd=MinMaxMeanDayLogInd-1;
			}
			else
			{
				flash24_StartInd=TOTAL_MIN_MAX_MEAN_LOG-1;
			}
			
			NoOf24Log=flash24_EndInd;
		}
		else if((gu16_parameterWord & ENABLE_M3LOG) && (RxBuffer[3]==MEAN_HR_ID))
		{
			flash24_StartInd=0;
			
			MinMaxMeanReadParaType=RxBuffer[4];

			bool_MeanHrLogReadCmd=1;
			bool_logtransferStart=0;
			
			flash24_StartInd=0;
			
			NoOf24Log=24;
		}
		else if(RxBuffer[3]==REALTIME_VAL_ID)
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			
			memcpy(&TxBuffer[5],(uint8_t*)&ep.currentEpochTime,4);
			
			TxBuffer[9]=0;
			if(bool_DP_NC[DP1]) 	TxBuffer[9] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 	TxBuffer[9] |= DP2_FAULTY;
			if(bool_DP_NC[DP3])TxBuffer[9] |= DP3_FAULTY;
			
			memcpy(&TxBuffer[10],(uint8_t*)&Dpressure[DP1],4);
			memcpy(&TxBuffer[14],(uint8_t*)&Dpressure[DP2],4);
			memcpy(&TxBuffer[18],(uint8_t*)&Dpressure[DP3],4);
			memcpy(&TxBuffer[22],(uint8_t*)&DP_Min[DP1],4);
			memcpy(&TxBuffer[26],(uint8_t*)&DP_Max[DP1],4);
			memcpy(&TxBuffer[30],(uint8_t*)&DP_Min[DP2],4);
			memcpy(&TxBuffer[34],(uint8_t*)&DP_Max[DP2],4);
			memcpy(&TxBuffer[38],(uint8_t*)&DP_Min[DP3],4);
			memcpy(&TxBuffer[42],(uint8_t*)&DP_Max[DP3],4);
			
			if(gu16_parameterWord & ENABLE_DP1)
			{
				if(!DP_Alrm_ON[DP1])
				{
					TxBuffer[43] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP1]==UPPER_ALARM) TxBuffer[43] = 1;
					else						  TxBuffer[43] = 2;
				}
			}
			else
			{
				TxBuffer[43] = 0;
			}
			
			if(gu16_parameterWord & ENABLE_DP2)
			{
				if(!DP_Alrm_ON[DP2])
				{
					TxBuffer[44] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP2]==UPPER_ALARM) TxBuffer[44] = 1;
					else						  TxBuffer[44] = 2;
				}
			}
			else
			{
				TxBuffer[44] = 0;
			}
			
			if(gu16_parameterWord & ENABLE_DP3)
			{
				if(!DP_Alrm_ON[DP3])
				{
					TxBuffer[45] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP3]==UPPER_ALARM) TxBuffer[45] = 1;
					else						  TxBuffer[45] = 2;
				}
			}
			else
			{
				TxBuffer[45] = 0;
			}

			TxBuffer[46]=CalCRC(&TxBuffer[1],45);
			TxBuffer[47]=0xFC;
			
			SetTxmode(TxBuffer,48);
		}
		else if(RxBuffer[3]==RDLG_DT_ID)
		{
			//cli();			//Global Interrupt Disable

			rtc1.second = RxBuffer[9];		//Second
			rtc1.minute = RxBuffer[8];		//Minute
			rtc1.hour	= RxBuffer[7];		//Hour
			rtc1.day	= RxBuffer[4];		//Date
			rtc1.month	= RxBuffer[5];		//Month
			rtc1.year	= RxBuffer[6] + 2000;	//Year
			
			StartEpochTime = get_epoch_time(rtc1);
			
			rtc1.second = RxBuffer[15];		//Second
			rtc1.minute = RxBuffer[14];		//Minute
			rtc1.hour	= RxBuffer[13];		//Hour
			rtc1.day	= RxBuffer[10];		//Date
			rtc1.month	= RxBuffer[11];		//Month
			rtc1.year	= RxBuffer[12] + 2000;	//Year
			
			EndEpochTime = get_epoch_time(rtc1);

			#ifdef DEBUG_RCV_CMD
			
				print_Hex(StartEpochTime >> 24);
				print_Hex(StartEpochTime >> 16);
				print_Hex(StartEpochTime >> 8);
				print_Hex(StartEpochTime);
				opchar(0,' ');
				print_Hex(EndEpochTime >> 24);
				print_Hex(EndEpochTime >> 16);
				print_Hex(EndEpochTime >> 8);
				print_Hex(EndEpochTime);
				opstr("\r\n");
					
				opstr("\r\nRec Epoch:");
				print_short(StartEpochTime,test,10);		opstr("  ");
				print_short(EndEpochTime,test,10);		opstr("\r\n");
				
				print_Hex(StartEpochTime >> 24);
				print_Hex(StartEpochTime >> 16);
				print_Hex(StartEpochTime >> 8);
				print_Hex(StartEpochTime);
				opchar(0,' ');
				print_Hex(EndEpochTime >> 24);
				print_Hex(EndEpochTime >> 16);
				print_Hex(EndEpochTime >> 8);
				print_Hex(EndEpochTime);
				opstr("\r\n");
			#endif

			if(FlashOVFByte || CurrentLogInd)
			{
				if(!FlashOVFByte)
				{
					InitLogInd=0;
					LastLogInd=CurrentLogInd-1;
				}
				else
				{
					InitLogInd=CurrentLogInd;

					if(!CurrentLogInd)
					{
						LastLogInd = LAST_LOG_ADDR-1;
					}
					else
					{
						LastLogInd = CurrentLogInd-1;
					}
				}	
				
				#ifdef DEBUG_RCV_CMD
					opstr("Log Ind:");
					print_float(InitLogInd,test,0);		opstr("  ");
					print_float(LastLogInd,test,0);		opstr("\r\n");
				#endif

				ReadLog(InitLogInd,(uint8_t*)&StartEpoch,4);
				ReadLog(LastLogInd,(uint8_t*)&EndEpoch,4);
				
				#ifdef DEBUG_RCV_CMD
					opstr("Log Epoch:");
					print_short(StartEpoch,test,10);		opstr("  ");
					print_short(EndEpoch,test,10);		opstr("\r\n");
				#endif

				if((StartEpochTime < StartEpoch) && (EndEpochTime < StartEpoch))
				{
					TotalLog = 0;
					#ifdef DEBUG_RCV_CMD
						opstr("Both < StartEpoch\r\n");
					#endif
				}
				else if((StartEpochTime > EndEpoch) && (EndEpochTime > EndEpoch))
				{
					TotalLog = 0;
					
					#ifdef DEBUG_RCV_CMD
						opstr("Both > EndEpoch\r\n");
					#endif
				}
				else
				{	
					if(StartEpochTime <= StartEpoch) 
					{
						StartLogInd = InitLogInd;
						
						#ifdef DEBUG_RCV_CMD
							opstr("Send ST Epoch <= log ST Epoch\r\n");
						#endif
					}
					else
					{
						//if(EndEpoch > StartEpoch)
						{
							StartLogInd = FindLogIndex(StartEpochTime,InitLogInd,LastLogInd);
						}
						
						#ifdef DEBUG_RCV_CMD
							opstr("Send ST Epoch > log ST Epoch\r\n");	
						#endif
					}
					
					#ifdef DEBUG_RCV_CMD
						opstr("StartEpochLogIndex:");
						print_float(StartLogInd,test,0);		opstr("\r\n");
					#endif
					
					if(EndEpochTime >= EndEpoch)
					{
						EndLogInd = LastLogInd;
						#ifdef DEBUG_RCV_CMD
							opstr("Send ED Epoch >= log ED Epoch\r\n");
						#endif
					}
					else
					{
						//if(EndEpoch > StartEpoch)
						{
							EndLogInd = FindLogIndex(EndEpochTime,InitLogInd,LastLogInd);
						}
						
						#ifdef DEBUG_RCV_CMD
							opstr("Send ED Epoch < log ED Epoch\r\n");
						#endif
					}
					
					#ifdef DEBUG_RCV_CMD
						opstr("EndEpochLogIndex:");
						print_float(EndLogInd,test,0);		opstr("\r\n");
					#endif
					
					if((StartLogInd==0xFFFFFFFF) || (EndLogInd==0xFFFFFFFF))
					{
						TotalLog = 0;
					}
					else if(StartLogInd==EndLogInd)
					{
						TotalLog = 1;
					}
					else
					{
						if(StartLogInd < EndLogInd)
						{
							TotalLog = EndLogInd-StartLogInd;
						}
						else
						{
							TotalLog = (LAST_LOG_ADDR - StartLogInd) + EndLogInd;
						}
					}
				}
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
				TxBuffer[4]=0xA9;
				memcpy(&TxBuffer[5],(uint8_t*)&TotalLog,4);
				TxBuffer[9]=CalCRC(&TxBuffer[1],8);
				TxBuffer[10]=0xFC;
					
				SendToUART(&TxBuffer[0],11);
				
				if(TotalLog)
				{
					templong=StartLogInd;
					bool_FlashReadCmd=1;
				}
				else
				{
					templong=0;
					bool_FlashReadCmd=0;
				}
				bool_logtransferStart=0;
			}
			else
			{
				TotalLog = 0;
				templong=0;
				bool_FlashReadCmd=0;
				bool_logtransferStart=0;
			}
			
			#ifdef DEBUG_RCV_CMD
				opstr("Total Log:");
				print_short(TotalLog,test,10);		opstr("\r\n");
			#endif
			
			//templong=StartLogInd;

			/*while(TotalLog)
			{
				ReadLog(templong,&TxBuffer[0],LOG_SIZE);
				SendToUART(&TxBuffer[0],LOG_SIZE);	opstr("\r\n");
				templong++;
				if(templong>=LAST_LOG_ADDR)
				{
					templong=0;
				}
				TotalLog--;
			}*/
			
			//sei();
		}
		else if((RxBuffer[3]==DP1CAL_ID) || (RxBuffer[3]==DP2CAL_ID) || (RxBuffer[3]==DP3CAL_ID))
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			
			a1=0;
			if(tempshort<0)
			{
				a1=1;
				tempshort *= (-1);
			}
			chartostr(tempshort,&TxBuffer[5],5);	
			if(a1==1)TxBuffer[5] = '-';
			
			if(bool_FactoryCalibrationOn)
			{
				switch(RxBuffer[3])
				{
					case DP1CAL_ID:			us1=DP1_CAL_DATE_ADDR;	us2=DP1_CAL_CERT_ADDR;			break;
					case DP2CAL_ID:			us1=DP2_CAL_DATE_ADDR;	us2=DP2_CAL_CERT_ADDR;			break;
					case DP3CAL_ID:			us1=DP3_CAL_DATE_ADDR;	us2=DP3_CAL_CERT_ADDR;			break;
				}
				
				ReadEEPROMData(us1,(uint8_t*)&TxBuffer[10],12);
				ReadEEPROMData(us2,(uint8_t*)&TxBuffer[22],15);
				
				TxBuffer[37]=CalCRC(&TxBuffer[1],36);
				TxBuffer[38]=0xFC;
				
				SetTxmode(TxBuffer,39);
			}
			else if(bool_CustmerCalibrationOn)
			{
				switch(RxBuffer[3])
				{
					case DP1CAL_ID:			us1=DP1_USER_CAL_DATE_ADDR;				break;
					case DP2CAL_ID:			us1=DP2_USER_CAL_DATE_ADDR;				break;
					case DP3CAL_ID:			us1=DP3_USER_CAL_DATE_ADDR;				break;
				}
				
				ReadEEPROMData(us1,(uint8_t*)&TxBuffer[10],60);
				
				TxBuffer[70]=CalCRC(&TxBuffer[1],69);
				TxBuffer[71]=0xFC;
				
				SetTxmode(TxBuffer,72);
			}
		}
		else
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			TxBuffer[4]=RxBuffer[3];
			tempchar = fillValue(&TxBuffer[5],tempshort);
			
			TxBuffer[5+tempchar]=CalCRC(&TxBuffer[1],4+tempchar);	
			TxBuffer[6+tempchar]=0xFC;
			
			SetTxmode(TxBuffer,7+tempchar);
		}
	}
	else
	{
		//If Invalid Command received
		TxBuffer[0]=0xFD;
		TxBuffer[1]=RxBuffer[1];
		TxBuffer[2]=RxBuffer[2];
		TxBuffer[3]=INVALID_CMD;
		TxBuffer[4]=CalCRC(&TxBuffer[1],3);	
		TxBuffer[5]=0xFC;
		
		SetTxmode(TxBuffer,6);
	}

	bool_msgRcvOK=0;
	RxInd=0;
}

void SetTxmode(uint8_t *buffer,uint16_t bytes)
{
	RS485_TX_ENB;
	SendToUART(buffer,bytes);
	RS485_RX_ENB;
}

uint32_t FindLogIndex(uint32_t EpochTime,uint32_t InitLogInd,uint32_t LastLogInd)
{		
	uint32_t LastLogInd1=LastLogInd;

	if(FlashOVFByte)
	{
		LastLogInd = LAST_LOG_ADDR-1;
	}
	
	while(LastLogInd > InitLogInd)
	{	
		MidLogInd=(InitLogInd + LastLogInd)/2;

		ReadLog(MidLogInd,(uint8_t*)&MidEpoch,4);
		
		#ifdef DEBUG_RCV_CMD
			opstr("Mid Index + Epoch:");
			print_short(MidLogInd,test,10);		opstr("    ");
			print_short(MidEpoch,test,10);		opstr("\r\n");
		#endif	
		
		if(EpochTime < MidEpoch)
		{
			ReadLog(MidLogInd-1,(uint8_t*)&MidEpoch1,4);
			
			#ifdef DEBUG_RCV_CMD
				opstr("EpochTime < MidEpoch\r\n");
				opstr("Mid1 Epoch:");
				print_short(MidEpoch1,test,10);		opstr("\r\n");
			#endif
			
			if((EpochTime >= MidEpoch1) && (EpochTime <= MidEpoch))
			{
				return MidLogInd;
			}
			else
			{
				LastLogInd = MidLogInd-1;
			}
		}
		else if(EpochTime > MidEpoch)
		{
			ReadLog(MidLogInd+1,(uint8_t*)&MidEpoch1,4);
			
			#ifdef DEBUG_RCV_CMD
				opstr("EpochTime > MidEpoch\r\n");
				opstr("Mid1 Epoch:");
				print_short(MidEpoch1,test,10);		opstr("\r\n");
			#endif
			
			if((EpochTime >= MidEpoch) && (EpochTime <= MidEpoch1))
			{
				return (MidLogInd+1);
			}
			else
			{
				InitLogInd = MidLogInd+1;
			}
		}
		else
		{
			#ifdef DEBUG_RCV_CMD
				opstr("EpochTime == MidEpoch\r\n");
			#endif

			return MidLogInd;
		}
	}
	
	if(FlashOVFByte)
	{
		InitLogInd = 0;
		LastLogInd = LastLogInd1;
		
		#ifdef DEBUG_RCV_CMD
			opstr("\r\nMemory Overwrite\r\n");
		#endif

		while(LastLogInd >= InitLogInd)
		{	
			MidLogInd=(InitLogInd + LastLogInd)/2;

			ReadLog(MidLogInd,(uint8_t*)&MidEpoch,4);
			
			#ifdef DEBUG_RCV_CMD
				opstr("Mid Index + Epoch:");
				print_short(MidLogInd,test,10);		opstr("    ");
				print_short(MidEpoch,test,10);		opstr("\r\n");
			#endif

			if(EpochTime < MidEpoch)
			{
				ReadLog(MidLogInd-1,(uint8_t*)&MidEpoch1,4);
				
				#ifdef DEBUG_RCV_CMD
					opstr("EpochTime < MidEpoch\r\n");
					opstr("Mid1 Epoch:");
					print_short(MidEpoch1,test,10);		opstr("\r\n");
				#endif

				if((EpochTime >= MidEpoch1) && (EpochTime <= MidEpoch))
				{
					return MidLogInd;
				}
				else
				{
					LastLogInd = MidLogInd-1;
				}
			}
			else if(EpochTime > MidEpoch)
			{
				ReadLog(MidLogInd+1,(uint8_t*)&MidEpoch1,4);
				
				#ifdef DEBUG_RCV_CMD
					opstr("EpochTime > MidEpoch\r\n");
					opstr("Mid1 Epoch:");
					print_short(MidEpoch1,test,10);		opstr("\r\n");
				#endif

				if((EpochTime >= MidEpoch) && (EpochTime <= MidEpoch1))
				{
					return (MidLogInd+1);
				}
				else
				{
					InitLogInd = MidLogInd+1;
				}
			}
			else
			{
				#ifdef DEBUG_RCV_CMD
					opstr("EpochTime == MidEpoch\r\n");
				#endif
				
				return MidLogInd;
			}
		}
	}
	
	return 0xFFFFFFFF;

	/*
	while(LastLogInd >= InitLogInd)
	{	
		MidLogInd=(InitLogInd + LastLogInd)/2;

		ReadLog(MidLogInd,(uint8_t*)&MidEpoch,4);
		
		opstr("Mid Index + Epoch:");
		print_short(MidLogInd,test,10);		opstr("    ");
		print_short(MidEpoch,test,10);		opstr("\r\n");
		
		if(EpochTime < MidEpoch)
		{
			opstr("EpochTime < MidEpoch\r\n");
			
			ReadLog(MidLogInd-1,(uint8_t*)&MidEpoch1,4);
			
			opstr("Mid1 Epoch:");
			print_short(MidEpoch1,test,10);		opstr("\r\n");
			
			if((EpochTime >= MidEpoch1) && (EpochTime <= MidEpoch))
			{
				return MidLogInd;
			}
			else
			{
				LastLogInd = MidLogInd-1;
			}
		}
		else if(EpochTime > MidEpoch)
		{
			opstr("EpochTime > MidEpoch\r\n");
			
			ReadLog(MidLogInd+1,(uint8_t*)&MidEpoch1,4);
			
			opstr("Mid1 Epoch:");
			print_short(MidEpoch1,test,10);		opstr("\r\n");
			
			//if(MidEpoch1 < EpochTime)
			
			if((EpochTime >= MidEpoch) && (EpochTime <= MidEpoch1))
			{
				return (MidLogInd+1);
			}
			else
			{
				InitLogInd = MidLogInd+1;
			}
		}
		else
		{
			opstr("EpochTime == MidEpoch\r\n");
			
			return MidLogInd;
		}
	}
	*/
}

int16_t findValue(uint8_t *ptr,uint8_t NoOfDigit)
{
	uint16_t Value=0,value1=1;
	uint8_t minus=0;
	
	if(*ptr=='-') 
	{
		ptr += (NoOfDigit-1);
		NoOfDigit--;
		minus=1;
	}
	else
	{
		ptr += (NoOfDigit-1);
	}
	
	
	while(NoOfDigit)
	{
		//*ptr -= '0';
		Value += ((unsigned short)(*ptr - '0') * value1);
		
		ptr--;
		NoOfDigit--;
		
		value1 *= 10;
	}
	
	if(minus) Value *= (-1);
	
	return Value;
}

uint8_t fillValue(uint8_t *ptr,long value)
{
	uint8_t x,k,minus=0;
	unsigned long temppow,tempshort;
	
	x=1;
	temppow=10;
	
	
	if(value<0)
	{
		value *= (-1);
		*ptr = '-';
		ptr++;
		minus=1;
	}
	
	tempshort=value;
	
	while(tempshort>=temppow) //to find out digit before decimal point
	{
		temppow*=10;
		x++;
	}
	ptr+=x;

	for(k=x;k>0;k--)  // to convert value before dp into BCD
	{
		ptr--;
		*ptr=(tempshort%10)+0x30;  
		tempshort/=10;
	}
	
	if(minus) x++;
	
	return x;
}

uint8_t CalCRC(uint8_t *ptr,uint16_t NoOfByte)
{
	unsigned long Total=0x00000055;
	unsigned short i=0;
	
	for (i=0;i<NoOfByte;i++)
    {
        Total += (unsigned long)*(ptr + i);
    }
	
	if(Total > 0x0000007F) Total &= 0x0000007F;

	return ((uint8_t)Total);
}

void ReadDiffPressure(uint8_t SensNo, uint32_t pressure)
{
	RealDpressure[SensNo] = pressure;
	
	float f32_temp=0;
	f32_temp = RealDpressure[SensNo];
	f32_temp -= DP_Cal_float_Value_F[SensNo];
	f32_temp -= DP_Cal_float_Value_C[SensNo];
	
	Dpressure[SensNo] = f32_temp;
	
	Dpressure[SensNo] = Kalman_Update(&Kalman[SensNo], Dpressure[SensNo]);
	
	if((Dpressure[SensNo]<1.0) && (Dpressure[SensNo]>-1.0))
	{
		Dpressure[SensNo] = 0;
	}
	
	//Add Zero offset -----------------------------------------------------------
	Dpressure[SensNo] += f32_dp_offset[SensNo];
	
	//Add manipulation offset ---------------------------------------------------
	if(bool_dp_sw_factor_add[SensNo])
	{
		if(Dpressure[SensNo]>1.5)
		{
			if(gu8_dp_sw_factor_add_cnt[SensNo]<DP_SW_FACT_DIVISION)
			{
				TempDpressure[SensNo] += (f32_dp_sw_factor[SensNo]/DP_SW_FACT_DIVISION);
				gu8_dp_sw_factor_add_cnt[SensNo]++;
				LastDpressure[SensNo] = Dpressure[SensNo];
			}
		}
		else
		{
			if(gu8_dp_sw_factor_add_cnt[SensNo])
			{
				TempDpressure[SensNo] -= (f32_dp_sw_factor[SensNo]/DP_SW_FACT_DIVISION);
				gu8_dp_sw_factor_add_cnt[SensNo]--;
				LastDpressure[SensNo] = Dpressure[SensNo];
			}
		}
		
		bool_dp_sw_factor_add[SensNo] = 0;
	}
	Dpressure[SensNo] += TempDpressure[SensNo];
	//------------------------------------------------------------------
	if(Dpressure[SensNo] > f32_dp_limit[SensNo]) 
	{
		Dpressure[SensNo] = f32_dp_limit[SensNo];
		DP_limit[SensNo] = 1;
	}
	else if(Dpressure[SensNo] < -f32_dp_limit[SensNo]) 
	{
		Dpressure[SensNo] = -f32_dp_limit[SensNo];
		DP_limit[SensNo] = 2;
	}
	else
	{
		DP_limit[SensNo] = 0;
	}
	//------------------------------------------------------------------
	if(!DP_StartUpTimer)
	{
		//Find DP1 Min/Max
		if(Dpressure[SensNo] > DP_Max[SensNo])
		{
			DP_Max[SensNo] = Dpressure[SensNo];
			WriteEEPROMData(DP1_MAXIMUM+(SensNo*4),(uint8_t*)&DP_Max[SensNo],4);
		}
					
		if(Dpressure[SensNo] < DP_Min[SensNo])
		{
			DP_Min[SensNo] = Dpressure[SensNo];
			WriteEEPROMData(DP1_MINIMUM+(SensNo*4),(uint8_t*)&DP_Min[SensNo],4);
		}
		
		if(gu16_parameterWord & ENABLE_ALERT)
		{			
			//Check Alarm Limit for DP ------------------------------------------------------------------------
			if(Dpressure[SensNo] > (float)DP_Upper_Alm_ON[SensNo]/10.0)
			{
				if(gu8_DpAlarmSensingTimer[SensNo]<=gu8_DpAlarmSensingTime[SensNo])
				{
					gu8_DpAlarmSensingTimer[SensNo]++;
				}
				else
				{
					DP_Alrm_ON[SensNo]=UPPER_ALARM;
				
					if(!bool_DPLog[SensNo])
					{
						LastDP_Alrm_ON[SensNo]=DP_Alrm_ON[SensNo];
						WriteEEPROMData(LAST_DP1_ALRM_STAT+SensNo,&LastDP_Alrm_ON[SensNo],sizeof(LastDP_Alrm_ON[SensNo]));
					
						FillRamBuffer(DP1_ALM_OCCURE_LOG,0,0xFFFF);
					
						bool_autoSendResponse = true;

						bool_DPLog[SensNo]=1;
					}
					else
					{
						if(LastDP_Alrm_ON[SensNo]!=DP_Alrm_ON[SensNo])
						{
							FillRamBuffer(DP1_ALM_RESTORE_LOG,0,0xFFFF);
						
							LastDP_Alrm_ON[SensNo]=NO_ALARM;
							WriteEEPROMData(LAST_DP1_ALRM_STAT+SensNo,&LastDP_Alrm_ON[SensNo],sizeof(LastDP_Alrm_ON[SensNo]));
						
							bool_DPLog[SensNo]=0;
						}
					}
				}
			}
			else if(Dpressure[SensNo] < (float)DP_Lower_Alm_ON[SensNo]/10.0)
			{
				if(gu8_DpAlarmSensingTimer[SensNo]<=gu8_DpAlarmSensingTime[SensNo])
				{
					gu8_DpAlarmSensingTimer[SensNo]++;
				}
				else
				{
					DP_Alrm_ON[SensNo]=LOWER_ALARM;
					
					if(!bool_DPLog[SensNo])
					{
						LastDP_Alrm_ON[SensNo]=DP_Alrm_ON[SensNo];
						WriteEEPROMData(LAST_DP1_ALRM_STAT+SensNo,&LastDP_Alrm_ON[SensNo],sizeof(LastDP_Alrm_ON[SensNo]));
						
						FillRamBuffer(DP1_ALM_OCCURE_LOG,0,0xFFFF);

						bool_autoSendResponse = true;
						
						bool_DPLog[SensNo]=1;
					}
					else
					{
						if(LastDP_Alrm_ON[SensNo]!=DP_Alrm_ON[SensNo])
						{
							FillRamBuffer(DP1_ALM_RESTORE_LOG,0,0xFFFF);
							
							LastDP_Alrm_ON[SensNo]=NO_ALARM;
							WriteEEPROMData(LAST_DP1_ALRM_STAT+SensNo,&LastDP_Alrm_ON[SensNo],sizeof(LastDP_Alrm_ON[SensNo]));
							
							bool_DPLog[SensNo]=0;
						}
					}
				}
			}
			else if((Dpressure[SensNo] < (float)DP_Upper_Alm_OFF[SensNo]/10.0) && (Dpressure[SensNo] > (float)DP_Lower_Alm_OFF[SensNo]/10.0))
			{
				DP_Alrm_ON[SensNo]=NO_ALARM;
				
				gu8_DpAlarmSensingTimer[SensNo]=0;
				
				if(bool_DPLog[SensNo]==1)
				{
					FillRamBuffer(DP1_ALM_RESTORE_LOG,0,0xFFFF);
					
					bool_DPLog[SensNo]=0;
					
					LastDP_Alrm_ON[SensNo]=DP_Alrm_ON[SensNo];
					WriteEEPROMData(LAST_DP1_ALRM_STAT+SensNo,&LastDP_Alrm_ON[SensNo],sizeof(LastDP_Alrm_ON[SensNo]));
				}
			}
		}
	}
}

/**@brief Convert 8 digit ascii serial# into hex format
 */
uint32_t ascii2hex(uint8_t *data, uint8_t NoOfdigit)
{
    uint8_t i,val;
    uint32_t num=0;

    for(i=0; i<NoOfdigit; i++)
    {
        val = (*data - 0x30);
        //num = num + (uint32_t)((double)val * pow(10,((NoOfdigit-1)-i)));
		num = (num*10) + val;
        data++;
    }

    return num;
}

void SecondTick(void)
{
	//Serve Watchdog Timer
	IWDG_ReloadCounter();
	
	Check_RTC();
	
	if(gu8_masterEnable==1)
	{
		SendToSlave();
	}
	
	if(gu16_DPAutoCalTimer10Sec[DP1])
	{
		gu16_DPAutoCalTimer10Sec[DP1]--;
		if(!gu16_DPAutoCalTimer10Sec[DP1])
		{
			if(gu8_DPAutoCalDoorCnt[DP1] >= 5)
			{
				if(bool_doorStatus==OPEN)
				{
					gu16_DPAutoCalTimer5Min[DP1] = 300;
				}
			}
			else
			{
				gu8_DPAutoCalDoorCnt[DP1] = 0;
			}
		}
	}
	
	if(gu16_DPAutoCalTimer5Min[DP1])
	{
		gu16_DPAutoCalTimer5Min[DP1]--;
		if(!gu16_DPAutoCalTimer5Min[DP1])
		{
			//Auto cal DP1
			DP_Cal_Value_C[DP1] = (RealDpressure[DP1] - DP_Cal_float_Value_F[DP1])*10.0;
			DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;
			WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],2);
		}
	}
	
	//-----------------------------------------------------------------------------------------------------------
	if(gu16_DPAutoCalTimer10Sec[DP2])
	{
		gu16_DPAutoCalTimer10Sec[DP2]--;
		if(!gu16_DPAutoCalTimer10Sec[DP2])
		{
			if(gu8_DPAutoCalDoorCnt[DP2] >= 5)
			{
				if(bool_doorStatus==OPEN)
				{
					gu16_DPAutoCalTimer5Min[DP2] = 300;
				}
			}
			else
			{
				gu8_DPAutoCalDoorCnt[DP2] = 0;
			}
		}
	}
	
	if(gu16_DPAutoCalTimer5Min[DP2])
	{
		gu16_DPAutoCalTimer5Min[DP2]--;
		if(!gu16_DPAutoCalTimer5Min[DP2])
		{
			//Auto cal DP2
			DP_Cal_Value_C[DP2] = (RealDpressure[DP2] - DP_Cal_float_Value_F[DP2])*10.0;
			DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
			WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],2);
		}
	}
	//-----------------------------------------------------------------------------------------------------------
	if(gu16_DPAutoCalTimer10Sec[DP3])
	{
		gu16_DPAutoCalTimer10Sec[DP3]--;
		if(!gu16_DPAutoCalTimer10Sec[DP3])
		{
			if(gu8_DPAutoCalDoorCnt[DP3] >= 5)
			{
				if(bool_doorStatus==OPEN)
				{
					gu16_DPAutoCalTimer5Min[DP3] = 300;
				}
			}
			else
			{
				gu8_DPAutoCalDoorCnt[DP3] = 0;
			}
		}
	}
	
	if(gu16_DPAutoCalTimer5Min[DP3])
	{
		gu16_DPAutoCalTimer5Min[DP3]--;
		if(!gu16_DPAutoCalTimer5Min[DP3])
		{
			//Auto cal DP3
			DP_Cal_Value_C[DP3] = (RealDpressure[DP3] - DP_Cal_float_Value_F[DP3])*10.0;
			DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
			WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
		}
	}
	//-----------------------------------------------------------------------------------------------------------
	
	if(gu8_deviceIDChangeTryTimer)
	{
		gu8_deviceIDChangeTryTimer--;
		if(!gu8_deviceIDChangeTryTimer)
		{
			gu8_deviceIDChangeTry=0;
		}
	}
	
	if(gu8_Mac2ValidTimer)
	{
		gu8_Mac2ValidTimer--;
		if(!gu8_Mac2ValidTimer)
		{
			SetMAC2Xbee(&gu8arr_XbeeMac[0][0],0);
		}
	}
	
	if(gu32_triggerXbeeResetTimer)
	{
		gu32_triggerXbeeResetTimer--;
		if(!gu32_triggerXbeeResetTimer)
		{
			gu32_triggerXbeeResetTimer = (unsigned long)gu16_XbeeRstInterval*60;

			bool_triggerXbeeReset = true;
		}
	}
	
	if(gu8_broadcast)
	{
		if(gu8_AutoSentTimer)
		{
			gu8_AutoSentTimer--;
			if(!gu8_AutoSentTimer)
			{
				bool_autoSendResponse = true;

				gu8_AutoSentTimer=gu8_AutoSentInterval;
			}
		}
		
		if(gu8_AutoSentTimeout)
		{
			gu8_AutoSentTimeout--;
			if(!gu8_AutoSentTimeout)
			{
				bool_autoSendResponse = true;
				
				gu8_AutoSentTimer=gu8_AutoSentInterval;
			}
		}
	}
	
	//--------------------------------------------
	if(bool_buzzerStart==YES)
	{
		if(buzzerOnTime)
		{
			buzzerOnTime--;
			if(!buzzerOnTime)
			{
				buzzerOffTime=Buzzer_OFF_Time;
				
				if(buzzerOffTime)
				{
					BUZZER_OFF;	
				}
				else
				{
					buzzerOnTime=Buzzer_ON_Time;
				}
			}
		}
		else if(buzzerOffTime)
		{
			buzzerOffTime--;
			if(!buzzerOffTime)
			{
				buzzerOnTime=Buzzer_ON_Time;
				
				if(buzzerOnTime)
				{
					BUZZER_ON;	
				}
				else
				{
					buzzerOffTime=Buzzer_OFF_Time;	
				}
			}
		}
	}
	if(DP_StartUpTimer)DP_StartUpTimer--;
	
	if(gu8_restartTimer)
	{
		gu8_restartTimer--;
		if(!gu8_restartTimer)
		{
			NVIC_SystemReset();	
		}
	}
	
	//--------------------------------------------
	if(progTimeout)
	{
		progTimeout--;
		if(!progTimeout)
		{
			mode=NORMAL_MODE;
			Normal_para_cnt=0;
			autoCal_para_cnt=0;
			gu8_SetACKPwd=0;
			
			restoreFactoryCalibrationTimer=0;
			DPAutoCalModeTimer=0;
			DPAutoCalTimer=0;
			ProgModeTimer=0;
			MinMaxMeanModeTimer=0;
			MeanHrModeTimer=0;
			dispMinMaxMeanLogInd=0;
			gu8_MinMaxClearTimer=0;
			
			for(uint8_t i=0;i<NO_DIGIT;i++) data[i]=BLANK;
			data[4] = N;
			data[5] = 0;
			data[6] = r;
			disp_value();
			
			PLATFORM_DelayMS(4000);
		}
	}
	//--------------------------------------------
	if(PCCalibrationTimer)
	{
		PCCalibrationTimer--;
		if(!PCCalibrationTimer)
		{
			bool_FactoryCalibrationOn=0;
			bool_CustmerCalibrationOn=0;
		}
	}
	//--------------------------------------------
	if(StartBroadcastTimer)
	{
		StartBroadcastTimer--;
		if(!StartBroadcastTimer)
		{
			bool_brodcastEnb=0;
		}
	}
	//--------------------------------------------
	if(AlarmAckTimer)
	{
		AlarmAckTimer--;
		if(!AlarmAckTimer)
		{
			bool_DPLog[DP1]=0;
			bool_DPLog[DP2]=0;
			bool_DPLog[DP3]=0;
		}
	}

	#ifndef DISABLE_DOOR_SENSING
	if((!DOOR_SENSE && gu8_doorSensingPolarity) || (DOOR_SENSE && !gu8_doorSensingPolarity))
	{
		if(bool_doorStatus==CLOSE)
		{
			bool_doorStatus=OPEN;
			bool_autoSendResponse = true;
			//opstr("DoorOpen\n");
			//StartBuzzer();
			if(!gu16_DPAutoCalTimer5Min[DP1])
			{
				if(!gu16_DPAutoCalTimer10Sec[DP1]) 
				{
					gu16_DPAutoCalTimer10Sec[DP1] = 5;
					gu8_DPAutoCalDoorCnt[DP1] = 1;
				}
				else
				{
					gu8_DPAutoCalDoorCnt[DP1]++;
				}
			}
				
			if(!gu16_DPAutoCalTimer5Min[DP2])
			{
				if(!gu16_DPAutoCalTimer10Sec[DP2]) 
				{
					gu16_DPAutoCalTimer10Sec[DP2] = 5;
					gu8_DPAutoCalDoorCnt[DP2] = 1;
				}
				else
				{
					gu8_DPAutoCalDoorCnt[DP2]++;
				}
			}
			
			if(!gu16_DPAutoCalTimer5Min[DP3])
			{
				if(!gu16_DPAutoCalTimer10Sec[DP3]) 
				{
					gu16_DPAutoCalTimer10Sec[DP3] = 5;
					gu8_DPAutoCalDoorCnt[DP3] = 1;
				}
				else
				{
					gu8_DPAutoCalDoorCnt[DP3]++;
				}
			}
		}
	}
	else
	{
		if(bool_doorStatus==OPEN)
		{
			bool_doorStatus=CLOSE;
			bool_autoSendResponse = true;
			gu16_DPAutoCalTimer5Min[DP1] = 0;
			gu16_DPAutoCalTimer5Min[DP2] = 0;
			gu16_DPAutoCalTimer5Min[DP3] = 0;
			//opstr("DoorClose\n");
		}
	}	
	#endif
	//====================================================
	if(AlarmAckTimer)
	{
		StopBuzzer();
	}
	
	if(gu16_parameterWord & ENABLE_ALERT)
	{
		if((DP_Alrm_ON[DP1]!=NO_ALARM)||(DP_Alrm_ON[DP2]!=NO_ALARM)||(DP_Alrm_ON[DP3]!=NO_ALARM)||(bool_doorStatus==OPEN))
		{	
			if(bool_buzzeralert==0)
			{	
				gu8_doorSensingTimer++;
				if(gu8_doorSensingTimer>=gu8_doorSensingTime)
				{
					gu8_doorSensingTimer=0;
					StartBuzzer();
					bool_buzzeralert=1;
				}	
			}
		}
		else
		{
			if(bool_buzzeralert==1)
			{
				StopBuzzer();
				gu8_doorSensingTimer=0;
				bool_buzzeralert=0;
			}	
		}
	}
	
	//====================================================
	if(bool_EraseFlash)
	{
		EraseWholeFlash();		
		last_sec=Read_byte_PCF8563(RTC_TIMESEC_REG);
		last_min=Read_byte_PCF8563(RTC_TIMEMIN_REG);	
		bool_EraseFlash=0;
	}
}

void chartostr(unsigned short val,uint8_t *data1,uint8_t no_of_digit)
{
	data1+=(no_of_digit-1);
	
	while(no_of_digit)
	{
		*data1	= (val%10)+0x30;
		val/=10;
		data1--;
		no_of_digit--;
	}
}
	
void convert_char(unsigned short val,uint8_t* data1,uint8_t no_of_digit)
{
	data1+=(no_of_digit-1);
		
	while(no_of_digit)
	{
		*data1=(val%10);
		val/=10;
		data1--;
		no_of_digit--;
	}
}

void convert_long(unsigned long val,uint8_t* data1,uint8_t no_of_digit)
{
	data1+=(no_of_digit-1);
		
	while(no_of_digit)
	{
		*data1=(val%10);
		val/=10;
		data1--;
		no_of_digit--;
	}
}

void convert_float(float value,uint8_t* data1,uint8_t bytes_after_dp)
{
	uint8_t x,k;
	unsigned long temppow,lu32_templong;
	float tempdoub;
		
	x=1;
	temppow=10;
	tempdoub=value;
		
	//----------------------------------------------------------
	if(tempdoub<0.0)
	{
		tempdoub*=(-1.0);
		*data1=DASH;		//means '-' sign
		data1++;
	}
	//----------------------------------------------------------
		
	lu32_templong=tempdoub;
	tempdoub-=lu32_templong;
		
	while(lu32_templong>=temppow) //to find out digit before decimal point
	{
		temppow*=10;
		x++;
	}
	data1+=x;

	for(k=x;k>0;k--)  // to convert value before dp into BCD
	{
		data1--;
		*data1=lu32_templong%10;
		lu32_templong/=10;
	}

	if(bytes_after_dp)  // to convert value after dp into BCD
	{
		data1+=x;
		data1--;
		*data1 += 10;
		data1++;
		for(k=0;k<bytes_after_dp;k++)
		{
			tempdoub*=10;
			x=tempdoub;
			tempdoub-=x;
			*data1=x;
			data1++;
		}
	}
}

void ResetMinMax(void)
{
	if(gu16_parameterWord & ENABLE_DP1)
	{
		DP_Max[DP1] = DEFAUT_DP1_MAX;
		DP_Min[DP1] = DEFAUT_DP1_MIN;
		
		WriteEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],4);
		WriteEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],4);
	}
	
	if(gu16_parameterWord & ENABLE_DP2)
	{
		DP_Max[DP2] = DEFAUT_DP2_MAX;
		DP_Min[DP2] = DEFAUT_DP2_MIN;
	
		WriteEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],4);
		WriteEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],4);
	}
	
	if(gu16_parameterWord & ENABLE_DP3)
	{
		DP_Max[DP3] = DEFAUT_DP3_MAX;
		DP_Min[DP3] = DEFAUT_DP3_MIN;
	
		WriteEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],4);
		WriteEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],4);
	}
}

void opstr(char *str)
{
	RS485_TX_ENB;
	
	PLATFORM_DelayUS(40);
	
	while(*str != '\0')
	{		
		UART_SendData(UART1, *str);
		while (RESET == UART_GetITStatus(UART1, UART_IT_TXIEN)){};
        UART_ClearITPendingBit(UART1, UART_IT_TXIEN);
	
		str++;
	}

	RS485_RX_ENB;
}

void SendToUART(uint8_t *str,uint16_t NoOfBytes)
{
	RS485_TX_ENB;
		
	PLATFORM_DelayUS(40);
			
	while(NoOfBytes)
	{
		UART_SendData(UART1, *str);
		while (RESET == UART_GetITStatus(UART1, UART_IT_TXIEN)){};
        UART_ClearITPendingBit(UART1, UART_IT_TXIEN);
	
		str++;
		NoOfBytes--;
	}

	RS485_RX_ENB;
}

void opchar(uint8_t str)
{
	RS485_TX_ENB;
	
	PLATFORM_DelayUS(40);

	UART_SendData(UART1, str);
	while (RESET == UART_GetITStatus(UART1, UART_IT_TXIEN)){};
	UART_ClearITPendingBit(UART1, UART_IT_TXIEN);
	
	RS485_RX_ENB;
}

void print_Hex(uint8_t portNo,uint8_t str)
{
	uint8_t cnt=0;
	
	cnt=str >> 4;
	
	if(cnt > 9)
	{
		cnt -= 10;
		opchar(cnt + 'A');
	}
	else
	{
		opchar(cnt + '0');
	}
	
	cnt=str & 0x0F;
	
	if(cnt > 9)
	{
		cnt -= 10;
		opchar(cnt + 'A');
	}
	else
	{
		opchar(cnt + '0');
	}
}

void print_float(uint8_t portNo,float val,char *data1,uint8_t bytes_after_dp)
{
	uint8_t x,k=0;
	unsigned long temppow,lu32_templong;
	float tempdoub;
	
	x=1;
	temppow=10;
	tempdoub=val;
	
	//----------------------------------------------------------
	if(tempdoub<0.0) 
	{
		tempdoub*=(-1.0);
		*data1='-';		//means '-' sign
		data1++;
	}
	//----------------------------------------------------------
	
	lu32_templong=tempdoub;
	tempdoub-=lu32_templong;
	
	while(lu32_templong>=temppow) //to findout digit before decimal point
	{
		temppow*=10;
		x++;
	}
	data1+=x;

	for(k=x;k>0;k--)  // to convert value before dp into BCD
	{
		data1--;
		*data1=(lu32_templong%10)+0x30;  
		lu32_templong/=10;
	}

	if(bytes_after_dp)  // to convert value after dp into BCD
	{
		data1+=x;
		*data1 = '.';
		data1++;
		for(k=0;k<bytes_after_dp;k++)
		{
			tempdoub*=10;
			x=tempdoub;
			tempdoub-=x;
			*data1=x+0x30;
			data1++;
		}
	}	
	else
	{
		data1+=x;
		//data1++;
	}
	
	*data1=0;
	
	opstr(data1);
}

void print_short(long val,char *data1,uint8_t no_of_digit)
{	
	if(val<0)
	{
		val *= (-1);
		*data1 = '-';
		data1++;
	}
	
	data1+=(no_of_digit-1);
	
	data1++;
	*data1=0;
	data1--;
	
	while(no_of_digit)
	{
		*data1=(val%10)+0x30;
		val/=10;
		data1--;
		no_of_digit--;
	}
	
	opstr(data1);
}

void whileTask(void)
{
	if(bool_resetDevice) 	
	{
		while(1);
	}	
					
	if(bool_autoSendResponse)
	{
		AutoSendDataResponse(1);
		bool_autoSendResponse = false;
	}
	
	if(bool_triggerXbeeReset)
	{
		PLATFORM_DelayMS(25);
		XBEE_RST_LOW;
		PLATFORM_DelayMS(1);
		XBEE_RST_HIGH;
		PLATFORM_DelayMS(25);
		
		bool_triggerXbeeReset=false;
	}
	
	// Check for any RS485 Command ================================================
	if(bool_msgRcvOK)
	{
		crcVal=CalCRC(&RxBuffer1[1],RxInd-3);
					
		#ifdef DEBUG_RCV_CMD
		opstr(0,"\r\nCRC:");
		print_Hex(0,crcVal);
		#endif
		
		gu8_rxMode=0;
		RxTimeout=0;
		if(RxBuffer1[RxInd-2]==crcVal)
		{
			if(!bool_FlashReadCmd && !bool_Flash24ReadCmd && !bool_MinMaxMeanLogReadCmd && !bool_MeanHrLogReadCmd && !bool_RamReadCmd && !bool_RamAllReadCmd)
			{
				for(uint8_t m=0;m<RxInd;m++) RxBuffer[m]=RxBuffer1[m];
				RxBuffer[1]=DeviceID;
				ServePCMsg();
			}
			else
			{
				RxInd=0;
			}
		}
		else
		{
			RxInd=0;
		}
	}
	else
	{
		if(bool_FlashReadCmd)
		{
			if(TotalLog && !bool_logtransferStart)
			{
				//cli();
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				TxBuffer[4]=RxBuffer[3];
				//TxBuffer[5]=RxBuffer[4];
				//TxBuffer[6]=RxBuffer[5];
				
				ReadLog(templong,&TxBuffer[5],LOG_SIZE);
				
				TxBuffer[68]=CalCRC(&TxBuffer[1],67);
				TxBuffer[69]=0xFC;
				
				templong++;
				if(templong>=LAST_LOG_ADDR)
				{
					templong=0;
				}
				
				bool_logtransferStart=1;
				
				SetTxmode(TxBuffer,70);
				//sei();
								
				TotalLog--;
				if(!TotalLog)
				{
					bool_FlashReadCmd=0;
				}
			}
		}
		else if(bool_Flash24ReadCmd)
		{
			if(NoOf24Log && !bool_logtransferStart)
			{
				//cli();
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
				TxBuffer[4]=RxBuffer[3];
				TxBuffer[5]=flash24_StartInd>>8;
				TxBuffer[6]=flash24_StartInd;
				
				ReadLog(LAST_LOG24_ADDR_OFFSET + flash24_StartInd,&TxBuffer[7],LOG_SIZE);
				
				TxBuffer[70]=CalCRC(&TxBuffer[1],69);
				TxBuffer[71]=0xFC;
				
				if(flash24_StartInd)
				{
					flash24_StartInd--;	
				}
				else
				{
					flash24_StartInd=LAST_LOG24_ADDR-1;
				}
				
				bool_logtransferStart=1;
				
				SetTxmode(TxBuffer,72);
				
				//sei();
				
				/*gu16_logtransfer++;
				if(gu16_logtransfer>flash24_EndInd)
				{
					bool_Flash24ReadCmd=0;
				}
				*/
				
				NoOf24Log--;
				if(!NoOf24Log)
				{
					bool_Flash24ReadCmd=0;
				}
			}
		}
		else if((gu16_parameterWord & ENABLE_M3LOG) && (bool_MinMaxMeanLogReadCmd))
		{
			if(NoOf24Log && !bool_logtransferStart)
			{
				//cli();
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				TxBuffer[4]=RxBuffer[3];
				TxBuffer[5]=MinMaxMeanReadParaType;
				TxBuffer[6]=flash24_StartInd;
				
				switch(MinMaxMeanReadParaType)
				{
					case '0':		ul1=LAST_DP1_MIN_MAX_OFFSET;		break;
					case '1':		ul1=LAST_DP2_MIN_MAX_OFFSET;		break;
					case '2':		ul1=LAST_DP3_MIN_MAX_OFFSET;		break;
				}
				ReadMinMaxLog(ul1,flash24_StartInd,&TxBuffer[7],MIN_MAX_MEAN_LOG_SIZE);
				if(TxBuffer[10]==0xFF)	//If no log then set Log to Zero
				{
					memset(&TxBuffer[7],0,MIN_MAX_MEAN_LOG_SIZE);
				}
				
				TxBuffer[23]=CalCRC(&TxBuffer[1],22);
				TxBuffer[24]=0xFC;
				
				if(flash24_StartInd)
				{
					flash24_StartInd--;	
				}
				else
				{
					flash24_StartInd=TOTAL_MIN_MAX_MEAN_LOG-1;
				}
				
				bool_logtransferStart=1;
				
				SetTxmode(TxBuffer,25);
				
				//sei();
						
				NoOf24Log--;
				if(!NoOf24Log)
				{
					bool_MinMaxMeanLogReadCmd=0;
				}
			}
		}
		else if((gu16_parameterWord & ENABLE_M3LOG) && (bool_MeanHrLogReadCmd))
		{
			if(NoOf24Log && !bool_logtransferStart)
			{
				//cli();
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				TxBuffer[4]=RxBuffer[3];
				TxBuffer[5]=MinMaxMeanReadParaType;
				TxBuffer[6]=flash24_StartInd;
				
				switch(MinMaxMeanReadParaType)
				{
					case '0':		ul1=DP1_CURR_24HR_MEAN_OFFSET;		break;
					case '1':		ul1=DP2_CURR_24HR_MEAN_OFFSET;		break;
					case '2':		ul1=DP3_CURR_24HR_MEAN_OFFSET;		break;
				}
				ReadMinMaxLog(ul1,flash24_StartInd,&TxBuffer[7],4);
				
				TxBuffer[11]=CalCRC(&TxBuffer[1],10);
				TxBuffer[12]=0xFC;
				
				flash24_StartInd++;
				
				bool_logtransferStart=1;
				
				SetTxmode(TxBuffer,13);
				
				//sei();
				
				NoOf24Log--;
				if(!NoOf24Log)
				{
					bool_MeanHrLogReadCmd=0;
				}
			}
		}
		else if(bool_RamReadCmd)
		{
			if(NoOf24Log && !bool_logtransferStart)
			{
				//cli();
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 		TxBuffer[3] |= DP3_FAULTY;
				TxBuffer[4]=RxBuffer[3];
				TxBuffer[5]=RxBuffer[4];

				tempshort = (flash24_StartInd * LOG_SIZE) + RAM_FILL_START;
				memcpy(&TxBuffer[6],&RAMBuffer[tempshort],LOG_SIZE);
				
				TxBuffer[69]=CalCRC(&TxBuffer[1],68);
				TxBuffer[70]=0xFC;

				if(flash24_StartInd)
				{
					flash24_StartInd--;
				}
				else
				{
					flash24_StartInd=29;
				}
				
				bool_logtransferStart=1;
				
				SetTxmode(TxBuffer,71);
				//sei();

				/*gu16_logtransfer++;
				if(gu16_logtransfer>flash24_EndInd)
				{
					bool_RamReadCmd=0;
				}
				*/
				NoOf24Log--;
				if(!NoOf24Log)
				{
					bool_RamReadCmd=0;
				}
			}
		}
		else if(bool_RamAllReadCmd)
		{
			//cli();
			SendToUART(&RAMBuffer[0],1507);
			
			bool_RamAllReadCmd=0;
			//sei();
		}
	}
	
	#ifdef ENABLE_KEY_LOGIC
	check_key();		//Check Keyboard
	if(bool_mec500_blink_flag1)
	{
		CheckUpDnKey();		//Check UP and Down key
		bool_mec500_blink_flag1=0;
	}
	#endif
		
	if(bool_msec50_flag)
	{
		if(RxTimeout)
		{
			RxTimeout--;
			if(!RxTimeout)
			{
				gu8_rxMode=0;
				RxTimeout=0;
				RxInd=0;
			}
		}

		//Read Differential Pressure -----------------------------------------
		if(gu16_parameterWord & ENABLE_DP1) 
		{
			if(StageDP[DP1]==0)
			{
				TriggerConvSM9543(DP1);
				StageDP[DP1]=1;
			}
			else
			{
				if(!ReadXGZP6891D(DP1, &RealDpressure[DP1]))
				{
					ReadDiffPressure(DP1, RealDpressure[DP1]);
				}
				else
				{
					bool_DP_NC[DP1]=1;		
					Dpressure[DP1]=0.0;
					DP_Alrm_ON[DP1]=NO_ALARM;
					gu8_DpAlarmSensingTimer[DP1]=0;
				}
				
				StageDP[DP1]=0;
			}
		}
		else
		{
			bool_DP_NC[DP1]=1;
			
			Dpressure[DP1]=0.0;

			DP_Max[DP1]=0.0;
			DP_Min[DP1]=0.0;

			DP_Alrm_ON[DP1]=NO_ALARM;
			
			DP_StartUpTimer=0;
		}
		//-------------------------------------------------------------
		if(gu16_parameterWord & ENABLE_DP2) 
		{
			if(StageDP[DP2]==0)
			{
				TriggerConvSM9543(DP2);
				StageDP[DP2]=1;
			}
			else
			{
				if(!ReadXGZP6891D(DP2, &RealDpressure[DP2]))
				{
					ReadDiffPressure(DP2, RealDpressure[DP2]);
				}
				else
				{
					bool_DP_NC[DP2]=1;		
					Dpressure[DP2]=0.0;
					DP_Alrm_ON[DP2]=NO_ALARM;
					gu8_DpAlarmSensingTimer[DP2]=0;
				}
				
				StageDP[DP2]=0;
			}
		}
		else
		{
			bool_DP_NC[DP2]=1;
			
			Dpressure[DP2]=0.0;

			DP_Max[DP2]=0.0;
			DP_Min[DP2]=0.0;

			DP_Alrm_ON[DP2]=NO_ALARM;
			
			DP_StartUpTimer=0;
		}
		//-------------------------------------------------------------
		if(gu16_parameterWord & ENABLE_DP3) 
		{
			if(StageDP[DP3]==0)
			{
				TriggerConvSM9543(DP3);
				StageDP[DP3]=1;
			}
			else
			{
				if(!ReadXGZP6891D(DP3, &RealDpressure[DP3]))
				{
					ReadDiffPressure(DP3, RealDpressure[DP3]);
				}
				else
				{
					bool_DP_NC[DP3]=1;		
					Dpressure[DP3]=0.0;
					DP_Alrm_ON[DP3]=NO_ALARM;
					gu8_DpAlarmSensingTimer[DP3]=0;
				}
				
				StageDP[DP3]=0;
			}
		}
		else
		{
			bool_DP_NC[DP3]=1;
			
			Dpressure[DP3]=0.0;

			DP_Max[DP3]=0.0;
			DP_Min[DP3]=0.0;

			DP_Alrm_ON[DP3]=NO_ALARM;
			
			DP_StartUpTimer=0;
		}
		//-------------------------------------------------------------
		bool_msec50_flag=0;
	}

	conv_value();
	disp_value();
} 


// Initialize the Kalman filter
void Kalman_Init(KalmanFilter *kf, float qr, float rs, float initial_estimate) 
{
	kf->Q1 = qr;          // Process noise covariance
	kf->R1 = rs;          // Measurement noise covariance
	kf->X1 = initial_estimate;  // Initial estimate
	kf->P1 = 1.0;        // Initial estimate covariance
}

// Update the Kalman filter with a new measurement
float Kalman_Update(KalmanFilter *kf, float measurement) 
{
	// Prediction update
	kf->P1 += kf->Q1;

	// Measurement update
	kf->K1 = kf->P1 / (kf->P1 + kf->R1);
	kf->X1 += kf->K1 * (measurement - kf->X1);
	kf->P1 *= (1 - kf->K1);

	return kf->X1;
}

void boot_data(void)
{
	uint8_t FirstTimeCheck=0;
	
	ReadEEPROMData(FIRST_BOOT_CHECK,&FirstTimeCheck,1);
	if(FirstTimeCheck != 0xBB)
	{
		FirstTimeCheck=0xBB;
		WriteEEPROMData(FIRST_BOOT_CHECK,&FirstTimeCheck,sizeof(FirstTimeCheck)); 
		
		gu16_parameterWord=PARAMETER_WORD;
		WriteEEPROMData(DISP_PARA_SELECT,(uint8_t*)&gu16_parameterWord,2);
		
		//gu8_DPAutoCalFlag=0;
		//WriteEEPROMData(DP_AUTO_CAL_FLAG,&gu8_DPAutoCalFlag,sizeof(gu8_DPAutoCalFlag));
		
		gu8_masterEnable=0;
		WriteEEPROMData(MASTER_ENABLE_ADDR,&gu8_masterEnable,sizeof(gu8_masterEnable));
		
		memset(gu8ar_SrNumber,'0',sizeof(gu8ar_SrNumber));
		WriteEEPROMData(DEVICE_SR_NO,(uint8_t*)&gu8ar_SrNumber[0],sizeof(gu8ar_SrNumber));
		gu32_SrNumber = ascii2hex(&gu8ar_SrNumber[8],8);

		MinMaxMeanDayLogInd=0;
		WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
		
		if(gu16_parameterWord & ENABLE_DATAFLASH)
		{
			//Clear all Hour mean value
			memset(&RAMBuffer[0],0,sizeof(RAMBuffer));
			WriteLog(DP1_CURR_24HR_MEAN_OFFSET,0,&RAMBuffer[0],HOUR_MEAN_VALUE_SPACE);
			WriteLog(DP2_CURR_24HR_MEAN_OFFSET,0,&RAMBuffer[0],HOUR_MEAN_VALUE_SPACE);
			WriteLog(DP3_CURR_24HR_MEAN_OFFSET,0,&RAMBuffer[0],HOUR_MEAN_VALUE_SPACE);
		
			//Clear all Min Max Mean Logs
			WriteLog(LAST_DP1_MIN_MAX_OFFSET,0,&RAMBuffer[0],MIN_MAX_MEAN_LOG_SPACE);
			WriteLog(LAST_DP2_MIN_MAX_OFFSET,0,&RAMBuffer[0],MIN_MAX_MEAN_LOG_SPACE);
			WriteLog(LAST_DP3_MIN_MAX_OFFSET,0,&RAMBuffer[0],MIN_MAX_MEAN_LOG_SPACE);
		}
		
		DP_UserCalDateInd[DP1]=0;
		WriteEEPROMData(DP1_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP1],sizeof(DP_UserCalDateInd[DP1]));
		
		DP_UserCalDateInd[DP2]=0;
		WriteEEPROMData(DP2_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP2],sizeof(DP_UserCalDateInd[DP2]));
		
		DP_UserCalDateInd[DP3]=0;
		WriteEEPROMData(DP3_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP3],sizeof(DP_UserCalDateInd[DP3]));
		
		RTCSetFlag=0;
		WriteEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
		
		if(gu16_parameterWord & ENABLE_DP1)
		{
			//DPressure1 Parameter -----------------------------------------------------
			DP_Upper_Alm_ON[DP1]=DEFAULT_DP1_UPPER_ALM_ON;
			WriteEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],2);
		
			DP_Upper_Alm_OFF[DP1]=DEFAULT_DP1_UPPER_ALM_OFF;
			WriteEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],2);
		
			DP_Lower_Alm_ON[DP1]=DEFAULT_DP1_LOWER_ALM_ON;
			WriteEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],2);
		
			DP_Lower_Alm_OFF[DP1]=DEFAULT_DP1_LOWER_ALM_OFF;
			WriteEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],2);
		
			DP_Cal_Value_F[DP1]=0;
			WriteEEPROMData(DP1_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP1],2);
			
			DP_Cal_Value_C[DP1]=0;
			WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],2);
			
			DP_Cal_float_Value_F[DP1] = 0.0;
			DP_Cal_float_Value_C[DP1] = 0.0;			
				
			//DP_Cal_Count[DP1]=0;
			//WriteEEPROMData(DP1_CAL_CNT,(uint8_t*)&DP_Cal_Count[DP1]);
		//
			//DP_Cal_Count_C[DP1]=0;
			//WriteEEPROMData(DP1_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP1]);
			
			LastDP_Alrm_ON[DP1]=0;
			WriteEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
		}

		if(gu16_parameterWord & ENABLE_DP2)
		{
			//DPressure2 Parameter -----------------------------------------------------
			DP_Upper_Alm_ON[DP2]=DEFAULT_DP2_UPPER_ALM_ON;
			WriteEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],2);
		
			DP_Upper_Alm_OFF[DP2]=DEFAULT_DP2_UPPER_ALM_OFF;
			WriteEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],2);
		
			DP_Lower_Alm_ON[DP2]=DEFAULT_DP2_LOWER_ALM_ON;
			WriteEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],2);
		
			DP_Lower_Alm_OFF[DP2]=DEFAULT_DP2_LOWER_ALM_OFF;
			WriteEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],2);
		
			DP_Cal_Value_F[DP2]=0;
			WriteEEPROMData(DP2_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP2],2);
			
			DP_Cal_Value_C[DP2]=0;
			WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],2);
			
			DP_Cal_float_Value_F[DP2] = 0.0;
			DP_Cal_float_Value_C[DP2] = 0.0;
			
			//DP_Cal_Count[DP2]=0;
			//WriteEEPROMData(DP2_CAL_CNT,(uint8_t*)&DP_Cal_Count[DP2],2);
		//
			//DP_Cal_Count_C[DP2]=0;
			//WriteEEPROMData(DP2_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP2],2);
			
			LastDP_Alrm_ON[DP2]=0;
			WriteEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
		}
		
		if(gu16_parameterWord & ENABLE_DP3)
		{
			//DPressure2 Parameter -----------------------------------------------------
			DP_Upper_Alm_ON[DP3]=DEFAULT_DP3_UPPER_ALM_ON;
			WriteEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],2);
		
			DP_Upper_Alm_OFF[DP3]=DEFAULT_DP3_UPPER_ALM_OFF;
			WriteEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],2);
		
			DP_Lower_Alm_ON[DP3]=DEFAULT_DP3_LOWER_ALM_ON;
			WriteEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],2);
		
			DP_Lower_Alm_OFF[DP3]=DEFAULT_DP3_LOWER_ALM_OFF;
			WriteEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],2);
		
			DP_Cal_Value_F[DP3]=0;
			WriteEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3],2);
			
			DP_Cal_Value_C[DP3]=0;
			WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
			
			DP_Cal_float_Value_F[DP3] = 0.0;
			DP_Cal_float_Value_C[DP3] = 0.0;
			
			//DP_Cal_Count[DP3]=0;
			//WriteEEPROMData(DP3_CAL_CNT,DP_Cal_Count[DP3]);
		//
			//DP_Cal_Count_C[DP3]=0;
			//WriteEEPROMData(DP3_CAL_CNT_C,DP_Cal_Count_C[DP3]);
			
			LastDP_Alrm_ON[DP3]=0;
			WriteEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
		}
		
		gu8_broadcast = 0;
		WriteEEPROMData(BROADCAST_ENB_ADDR,&gu8_broadcast,sizeof(gu8_broadcast));
		
		gu8_rly_stat = 0;
		WriteEEPROMData(RELAY_STAT_ADDR,&gu8_rly_stat,sizeof(gu8_rly_stat));
		
		for(uint8_t i=0; i<MAX_SUPPORTED_DP; i++)
		{
			su16_dp_sw_factor[i]=0;
			f32_dp_sw_factor[i]=0.0;
			WriteEEPROMData((DP_SW_FACT_ADDR+(i*2)),(uint8_t*)&su16_dp_sw_factor[i],2);
			
			su16_dp_offset[i]=0;
			f32_dp_offset[i]=0.0;
			WriteEEPROMData((DP_OFFSET_ADDR+(i*2)),(uint8_t*)&su16_dp_offset[i],2);
			
			u16_dp_limit[i]=2500;
			f32_dp_limit[i]=250.0;
			WriteEEPROMData((DP_LIMIT_ADDR+(i*2)),(uint8_t*)&u16_dp_limit[i],2);
		}

		ResetMinMax();	
		
		//RS485 Parameter -------------------------------------------
		DeviceID=DEFAULT_DEVICE_ID;
		WriteEEPROMData(DEVICE_ID,&DeviceID,sizeof(DeviceID));
		
		//Buzzer Parameter -------------------------------------------
		Buzzer_ON_Time=DEFAULT_BUZZER_ON_TIME;
		WriteEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,2);
		
		Buzzer_OFF_Time=DEFAULT_BUZZER_OFF_TIME;
		WriteEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,2);
		
		if(gu16_parameterWord & ENABLE_LOG)
		{
			//Data Logging Parameter -------------------------------------------
			LogInterval=DEFAULT_LOG_INTERVAL;
			WriteEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,2);
		}
		
		//UART Parameter -------------------------------------------
		UART_BaudRate=DEFAULT_UART_BAUDRATE;	//57600
		WriteEEPROMData(UART_BAUDRATE,&UART_BaudRate,sizeof(UART_BaudRate));
		
		//Customer Password -------------------------------------------
		CustPassword=DEFAULT_CUSTOMER_PWD;
		WriteEEPROMData(CUSTOMER_PASSWORD,(uint8_t*)&CustPassword,2);
		
		//Factory Customer Password -------------------------------------------
		FactCustPassword=DEFAULT_FACTORY_PWD;
		WriteEEPROMData(FAC_CUSTOMER_PASSWORD,(uint8_t*)&FactCustPassword,2);
		
		//Acknowledge Parameter -------------------------------------------
		AckTimer=1;
		WriteEEPROMData(ACK_TIMER,(uint8_t*)&AckTimer,2);
		
		AckPwdInd=0;
		WriteEEPROMData(ACK_PWD_IND,&AckPwdInd,sizeof(AckPwdInd));
		
		for(uint8_t i=0;i<NO_OF_ACKPWD;i++)
		{
			AckPwd[i]=0;
			WriteEEPROMData((ACK_PASSWORD+(i*2)),(uint8_t*)&AckPwd[i],2);
		}
		
	
		//Reset Data Logging Parameter -------------------------------------------
		CurrentLogIndReadLoc = 0;
		WriteEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,2);
		
		FlashOVFByte=0;
		WriteEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
		
		CurrentLogInd = 0;
		WriteEEPROMData(CURR_LOG_IND,(uint8_t*)&CurrentLogInd,4);
		
		CurrentLog24IndReadLoc = 0;
		WriteEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,sizeof(CurrentLog24IndReadLoc));
		
		CurrentLog24Ind = 0;
		WriteEEPROMData(CURR_LOG24_IND,(uint8_t*)&CurrentLog24Ind,2);
		
		bool_DPLog[DP1]=0;
		bool_DPLog[DP2]=0;
		bool_DPLog[DP3]=0;
		
		LastDP_Alrm_ON[DP1]=0;
		WriteEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
		
		LastDP_Alrm_ON[DP2]=0;
		WriteEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
		
		LastDP_Alrm_ON[DP3]=0;
		WriteEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
		
		gu8_doorSensingPolarity=1;
		WriteEEPROMData(DOOR_SENSE_POLARITY_ADDR,&gu8_doorSensingPolarity,sizeof(gu8_doorSensingPolarity));
		
		gu8_doorSensingTime=60;
		WriteEEPROMData(DOOR_SENSE_TIME_ADDR,&gu8_doorSensingTime,sizeof(gu8_doorSensingTime));
		
		gu8_DpAlarmSensingTime[DP1]=5;
		WriteEEPROMData(DP1_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP1],sizeof(gu8_DpAlarmSensingTime[DP1]));
		
		gu8_DpAlarmSensingTime[DP2]=5;
		WriteEEPROMData(DP2_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP2],sizeof(gu8_DpAlarmSensingTime[DP2]));
		
		gu8_DpAlarmSensingTime[DP3]=5;
		WriteEEPROMData(DP3_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP3],sizeof(gu8_DpAlarmSensingTime[DP3]));
		
		gu8_LCDBrigthnessCnt=DEFAULT_LCD_BRIGHTNESS;
		WriteEEPROMData(LCD_BRIGHT_CNT_ADDR,&gu8_LCDBrigthnessCnt,sizeof(gu8_LCDBrigthnessCnt));
		
		gu8_AutoSentInterval=DEFAULT_AUTO_SENT_INTERVAL;
		WriteEEPROMData(AUTO_SENT_INTERVAL_ADDR,&gu8_AutoSentInterval,sizeof(gu8_AutoSentInterval));
		
		gu8_DeviceInGroup=DEFAULT_DEVICES_IN_GROUP;
		WriteEEPROMData(DEVICES_IN_GROUP_ADDR,&gu8_DeviceInGroup,sizeof(gu8_DeviceInGroup));
		
		gu16_XbeeRstInterval=DEFAULT_XBEE_RST_INTERVAL;
		WriteEEPROMData(XBEE_RST_INTERVAL_ADDR,(uint8_t*)&gu16_XbeeRstInterval,2);
		
		memcpy(&gu8arr_XbeeMac[0][0],"000000000000FFFF",XBEE_MAC_SIZE);
		WriteEEPROMData(XBEE_MAC_ADDR,(uint8_t*)&gu8arr_XbeeMac[0][0],XBEE_MAC_SIZE);
		for(uint8_t i=1;i<NO_OF_XBEE_MAC;i++)
		{
			memset(&gu8arr_XbeeMac[i][0],'0',XBEE_MAC_SIZE);
			WriteEEPROMData((XBEE_MAC_ADDR+(i*XBEE_MAC_SIZE)),(uint8_t*)&gu8arr_XbeeMac[i][0],XBEE_MAC_SIZE);
		}
		
		//EraseWholeFlash();
	}
	else
	{	
		ReadEEPROMData(DOOR_SENSE_POLARITY_ADDR,&gu8_doorSensingPolarity,1);
		if(gu8_doorSensingPolarity > 1)
		{
			gu8_doorSensingPolarity=0;
			WriteEEPROMData(DOOR_SENSE_POLARITY_ADDR,&gu8_doorSensingPolarity,sizeof(gu8_doorSensingPolarity));
		}
		
		ReadEEPROMData(LCD_BRIGHT_CNT_ADDR,&gu8_LCDBrigthnessCnt,1);
		if(gu8_LCDBrigthnessCnt > 15)
		{
			gu8_LCDBrigthnessCnt=DEFAULT_LCD_BRIGHTNESS;
			WriteEEPROMData(LCD_BRIGHT_CNT_ADDR,&gu8_LCDBrigthnessCnt,sizeof(gu8_LCDBrigthnessCnt));
		}
		
		ReadEEPROMData(AUTO_SENT_INTERVAL_ADDR,&gu8_AutoSentInterval,1);
		if((!gu8_AutoSentInterval) || (gu8_AutoSentInterval > 240))
		{
			gu8_AutoSentInterval=DEFAULT_AUTO_SENT_INTERVAL;
			WriteEEPROMData(AUTO_SENT_INTERVAL_ADDR,&gu8_AutoSentInterval,sizeof(gu8_AutoSentInterval));
		}
		
		ReadEEPROMData(DEVICES_IN_GROUP_ADDR,&gu8_DeviceInGroup,1);
		if((gu8_DeviceInGroup < 2) || (gu8_DeviceInGroup > 100))
		{
			gu8_DeviceInGroup=DEFAULT_DEVICES_IN_GROUP;
			WriteEEPROMData(DEVICES_IN_GROUP_ADDR,&gu8_DeviceInGroup,sizeof(gu8_DeviceInGroup));
		}
		
		ReadEEPROMData(XBEE_RST_INTERVAL_ADDR,(uint8_t*)&gu16_XbeeRstInterval,2);
		if(gu16_XbeeRstInterval > 1440)
		{
			gu16_XbeeRstInterval=DEFAULT_XBEE_RST_INTERVAL;
			WriteEEPROMData(XBEE_RST_INTERVAL_ADDR,(uint8_t*)&gu16_XbeeRstInterval,2);
		}
		
		ReadEEPROMData(DEVICE_SR_NO,&gu8ar_SrNumber[0],sizeof(gu8ar_SrNumber));
		gu32_SrNumber = ascii2hex(&gu8ar_SrNumber[8],8);
		
		ReadEEPROMData(DOOR_SENSE_TIME_ADDR,&gu8_doorSensingTime,1);
		if(gu8_doorSensingTime > 250)
		{
			gu8_doorSensingTime=60;
			WriteEEPROMData(DOOR_SENSE_TIME_ADDR,&gu8_doorSensingTime,sizeof(gu8_doorSensingTime));
		}
		
		ReadEEPROMData(DP1_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP1],1);
		if(gu8_DpAlarmSensingTime[DP1] > 250)
		{
			gu8_DpAlarmSensingTime[DP1]=5;
			WriteEEPROMData(DP1_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP1],sizeof(gu8_DpAlarmSensingTime[DP1]));
		}
		
		ReadEEPROMData(DP2_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP2],1);
		if(gu8_DpAlarmSensingTime[DP2] > 250)
		{
			gu8_DpAlarmSensingTime[DP2]=5;
			WriteEEPROMData(DP2_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP2],sizeof(gu8_DpAlarmSensingTime[DP2]));
		}
		
		ReadEEPROMData(DP3_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP3],1);
		if(gu8_DpAlarmSensingTime[DP3] > 250)
		{
			gu8_DpAlarmSensingTime[DP3]=5;
			WriteEEPROMData(DP3_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP3],sizeof(gu8_DpAlarmSensingTime[DP3]));
		}
			
		ReadEEPROMData(MASTER_ENABLE_ADDR,&gu8_masterEnable,1);
		if(gu8_masterEnable > 1)
		{
			gu8_masterEnable=0;
			WriteEEPROMData(MASTER_ENABLE_ADDR,&gu8_masterEnable,sizeof(gu8_masterEnable));
		}
		
		/*ReadEEPROMData(DP_AUTO_CAL_FLAG,&gu8_DPAutoCalFlag,1);
		if(gu8_DPAutoCalFlag>1)
		{
			gu8_DPAutoCalFlag=1;
			WriteEEPROMData(DP_AUTO_CAL_FLAG,&gu8_DPAutoCalFlag,sizeof(gu8_DPAutoCalFlag));
		}*/
		
		ReadEEPROMData(DISP_PARA_SELECT,(uint8_t*)&gu16_parameterWord,2);
		//gu16_parameterWord = PARAMETER_WORD;
		
		if(gu16_parameterWord & ENABLE_M3LOG)
		{
			ReadEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,1);
			if(MinMaxMeanDayLogInd>=TOTAL_MIN_MAX_MEAN_LOG)
			{
				MinMaxMeanDayLogInd=0;
				WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
			}
		}

		ReadEEPROMData(DP1_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP1],1);
		if(DP_UserCalDateInd[DP1]>15)
		{
			DP_UserCalDateInd[DP1]=0;
			WriteEEPROMData(DP1_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP1],sizeof(DP_UserCalDateInd[DP1]));
		}
		
		ReadEEPROMData(DP2_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP2],1);
		if(DP_UserCalDateInd[DP2]>15)
		{
			DP_UserCalDateInd[DP2]=0;
			WriteEEPROMData(DP2_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP2],sizeof(DP_UserCalDateInd[DP2]));
		}
		
		ReadEEPROMData(DP3_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP3],1);
		if(DP_UserCalDateInd[DP3]>15)
		{
			DP_UserCalDateInd[DP3]=0;
			WriteEEPROMData(DP3_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP3],sizeof(DP_UserCalDateInd[DP3]));
		}
		
		ReadEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,1);
		if(RTCSetFlag>1)
		{
			RTCSetFlag=0;
			WriteEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
		}

		if(gu16_parameterWord & ENABLE_DP1)
		{
			//DPressure1 Parameter -----------------------------------------------------
			ReadEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],2);
			if((DP_Upper_Alm_ON[DP1]<(DEFAUT_DP1_MAX*10)) || (DP_Upper_Alm_ON[DP1]>(DEFAUT_DP1_MIN*10)))
			{
				DP_Upper_Alm_ON[DP1]=DEFAULT_DP1_UPPER_ALM_ON;
				WriteEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],2);
			}
		
			ReadEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],2);
			if((DP_Upper_Alm_OFF[DP1]<(DEFAUT_DP1_MAX*10)) || (DP_Upper_Alm_OFF[DP1]>(DEFAUT_DP1_MIN*10)))
			{
				DP_Upper_Alm_OFF[DP1]=DEFAULT_DP1_UPPER_ALM_OFF;
				WriteEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],2);
			}
		
			ReadEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],2);
			if((DP_Lower_Alm_ON[DP1]<(DEFAUT_DP1_MAX*10)) || (DP_Lower_Alm_ON[DP1]>(DEFAUT_DP1_MIN*10)))
			{
				DP_Lower_Alm_ON[DP1]=DEFAULT_DP1_LOWER_ALM_ON;
				WriteEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],2);
			}
		
			ReadEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],2);
			if((DP_Lower_Alm_OFF[DP1]<(DEFAUT_DP1_MAX*10)) || (DP_Lower_Alm_OFF[DP1]>(DEFAUT_DP1_MIN*10)))
			{
				DP_Lower_Alm_OFF[DP1]=DEFAULT_DP1_LOWER_ALM_OFF;
				WriteEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],2);
			}
		
			ReadEEPROMData(DP1_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP1],2);
			//if((DP_Cal_Value_F[DP1]<(DEFAUT_DP1_MAX*10.0)) || (DP_Cal_Value_F[DP1]>(DEFAUT_DP1_MIN*10.0)))
			//{
				//DP_Cal_Value_F[DP1]=0;
				//WriteEEPROMData(DP1_CAL_VAL_F_ADDR,DP_Cal_Value_F[DP1]);
			//}
			DP_Cal_float_Value_F[DP1] = (float)DP_Cal_Value_F[DP1]/10.0;
			
			ReadEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],2);
			//if((DP_Cal_Value_C[DP1]<(DEFAUT_DP1_MAX*10.0)) || (DP_Cal_Value_C[DP1]>(DEFAUT_DP1_MIN*10.0)))
			//{
				//DP_Cal_Value_C[DP1]=0;
				//WriteEEPROMData(DP1_CAL_VAL_C_ADDR,DP_Cal_Value_C[DP1]);
			//}
			DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;
			
			//ReadEEPROMData(DP1_CAL_CNT,(uint8_t*)&DP_Cal_Count[DP1],2);
			//if((DP_Cal_Count[DP1]<-500) || (DP_Cal_Count[DP1]>500))
			//{
				//DP_Cal_Count[DP1]=0;
				//WriteEEPROMData(DP1_CAL_CNT,DP_Cal_Count[DP1]);
			//}
		//
			//ReadEEPROMData(DP1_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP1],2);
			//if((DP_Cal_Count_C[DP1]<-500) || (DP_Cal_Count_C[DP1]>500))
			//{
				//DP_Cal_Count_C[DP1]=0;
				//WriteEEPROMData(DP1_CAL_CNT_C,DP_Cal_Count_C[DP1]);
			//}
		
			ReadEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],4);
			if(DP_Max[DP1]<DEFAUT_DP1_MAX)
			{
				DP_Max[DP1] = DEFAUT_DP1_MAX;
				WriteEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],4);
			}
		
			ReadEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],4);
			if(DP_Min[DP1]>DEFAUT_DP1_MIN)
			{
				DP_Min[DP1] = DEFAUT_DP1_MIN;
				WriteEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],4);
			}
			
			ReadEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],1);
			if(LastDP_Alrm_ON[DP1]>2)
			{
				LastDP_Alrm_ON[DP1]=0;
				WriteEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
			}
		}
		
		if(gu16_parameterWord & ENABLE_DP2)
		{
			//DPressure2 Parameter -----------------------------------------------------
			ReadEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],2);
			if((DP_Upper_Alm_ON[DP2]<(DEFAUT_DP2_MAX*10)) || (DP_Upper_Alm_ON[DP2]>(DEFAUT_DP2_MIN*10)))
			{
				DP_Upper_Alm_ON[DP2]=DEFAULT_DP2_UPPER_ALM_ON;
				WriteEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],2);
			}
		
			ReadEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],2);
			if((DP_Upper_Alm_OFF[DP2]<(DEFAUT_DP2_MAX*10)) || (DP_Upper_Alm_OFF[DP2]>(DEFAUT_DP2_MIN*10)))
			{
				DP_Upper_Alm_OFF[DP2]=DEFAULT_DP2_UPPER_ALM_OFF;
				WriteEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],2);
			}
		
			ReadEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],2);
			if((DP_Lower_Alm_ON[DP2]<(DEFAUT_DP2_MAX*10)) || (DP_Lower_Alm_ON[DP2]>(DEFAUT_DP2_MIN*10)))
			{
				DP_Lower_Alm_ON[DP2]=DEFAULT_DP2_LOWER_ALM_ON;
				WriteEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],2);
			}
		
			ReadEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],2);
			if((DP_Lower_Alm_OFF[DP2]<(DEFAUT_DP2_MAX*10)) || (DP_Lower_Alm_OFF[DP2]>(DEFAUT_DP2_MIN*10)))
			{
				DP_Lower_Alm_OFF[DP2]=DEFAULT_DP2_LOWER_ALM_OFF;
				WriteEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],2);
			}
			
			ReadEEPROMData(DP2_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP2],2);
			//if((DP_Cal_Value_F[DP2]<(DEFAUT_DP2_MAX*10.0)) || (DP_Cal_Value_F[DP2]>(DEFAUT_DP2_MIN*10.0)))
			//{
				//DP_Cal_Value_F[DP2]=0;
				//WriteEEPROMData(DP2_CAL_VAL_F_ADDR,DP_Cal_Value_F[DP2]);
			//}
			DP_Cal_float_Value_F[DP2] = (float)DP_Cal_Value_F[DP2]/10.0;
			
			ReadEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],2);
			//if((DP_Cal_Value_C[DP2]<(DEFAUT_DP2_MAX*10.0)) || (DP_Cal_Value_C[DP2]>(DEFAUT_DP2_MIN*10.0)))
			//{
				//DP_Cal_Value_C[DP2]=0;
				//WriteEEPROMData(DP2_CAL_VAL_C_ADDR,DP_Cal_Value_C[DP2]);
			//}
			DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
			
			//ReadEEPROMData(DP2_CAL_CNT,(uint8_t*)&DP_Cal_Count[DP2],2);
			//if((DP_Cal_Count[DP2]<-500) || (DP_Cal_Count[DP2]>500))
			//{
				//DP_Cal_Count[DP2]=0;
				//WriteEEPROMData(DP2_CAL_CNT,DP_Cal_Count[DP2]);
			//}
		//
			//ReadEEPROMData(DP2_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP2],2);
			//if((DP_Cal_Count_C[DP2]<-500) || (DP_Cal_Count_C[DP2]>500))
			//{
				//DP_Cal_Count_C[DP2]=0;
				//WriteEEPROMData(DP2_CAL_CNT_C,DP_Cal_Count_C[DP2]);
			//}
		
			ReadEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],4);
			if(DP_Max[DP2]<DEFAUT_DP2_MAX)
			{
				DP_Max[DP2] = DEFAUT_DP2_MAX;
				WriteEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],4);
			}
		
			ReadEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],4);
			if(DP_Min[DP2]>DEFAUT_DP2_MIN)
			{
				DP_Min[DP2] = DEFAUT_DP2_MIN;
				WriteEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],4);
			}
			
			ReadEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],1);
			if(LastDP_Alrm_ON[DP2]>2)
			{
				LastDP_Alrm_ON[DP2]=0;
				WriteEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
			}
		}
		
		if(gu16_parameterWord & ENABLE_DP3)
		{
			//DPressure2 Parameter -----------------------------------------------------
			ReadEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],2);
			if((DP_Upper_Alm_ON[DP3]<(DEFAUT_DP3_MAX*10)) || (DP_Upper_Alm_ON[DP3]>(DEFAUT_DP3_MIN*10)))
			{
				DP_Upper_Alm_ON[DP3]=DEFAULT_DP3_UPPER_ALM_ON;
				WriteEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],2);
			}
		
			ReadEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],2);
			if((DP_Upper_Alm_OFF[DP3]<(DEFAUT_DP3_MAX*10)) || (DP_Upper_Alm_OFF[DP3]>(DEFAUT_DP3_MIN*10)))
			{
				DP_Upper_Alm_OFF[DP3]=DEFAULT_DP3_UPPER_ALM_OFF;
				WriteEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],2);
			}
		
			ReadEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],2);
			if((DP_Lower_Alm_ON[DP3]<(DEFAUT_DP3_MAX*10)) || (DP_Lower_Alm_ON[DP3]>(DEFAUT_DP3_MIN*10)))
			{
				DP_Lower_Alm_ON[DP3]=DEFAULT_DP3_LOWER_ALM_ON;
				WriteEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],2);
			}
		
			ReadEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],2);
			if((DP_Lower_Alm_OFF[DP3]<(DEFAUT_DP3_MAX*10)) || (DP_Lower_Alm_OFF[DP3]>(DEFAUT_DP3_MIN*10)))
			{
				DP_Lower_Alm_OFF[DP3]=DEFAULT_DP3_LOWER_ALM_OFF;
				WriteEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],2);
			}
			
			ReadEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3],2);
			//if((DP_Cal_Value_F[DP3]<(DEFAUT_DP3_MAX*10.0)) || (DP_Cal_Value_F[DP3]>(DEFAUT_DP3_MIN*10.0)))
			//{
				//DP_Cal_Value_F[DP3]=0;
				//WriteEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3],2);
			//}
			DP_Cal_float_Value_F[DP3] = (float)DP_Cal_Value_F[DP3]/10.0;
			
			ReadEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
			//if((DP_Cal_Value_C[DP3]<(DEFAUT_DP3_MAX*10.0)) || (DP_Cal_Value_C[DP3]>(DEFAUT_DP3_MIN*10.0)))
			//{
				//DP_Cal_Value_C[DP3]=0;
				//WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
			//}
			DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
			
			//ReadEEPROMData(DP3_CAL_CNT,(uint8_t*)&DP_Cal_Count[DP3],2);
			//if((DP_Cal_Count[DP3]<-500) || (DP_Cal_Count[DP3]>500))
			//{
				//DP_Cal_Count[DP3]=0;
				//WriteEEPROMData(DP3_CAL_CNT,(uint8_t*)&DP_Cal_Count[DP3],2);
			//}
		//
			//ReadEEPROMData(DP3_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP3],2);
			//if((DP_Cal_Count_C[DP3]<-500) || (DP_Cal_Count_C[DP3]>500))
			//{
				//DP_Cal_Count_C[DP3]=0;
				//WriteEEPROMData(DP3_CAL_CNT_C,(uint8_t*)&DP_Cal_Count_C[DP3],2);
			//}
		
			ReadEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],4);
			if(DP_Max[DP3]<DEFAUT_DP3_MAX)
			{
				DP_Max[DP3] = DEFAUT_DP3_MAX;
				WriteEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],4);
			}
		
			ReadEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],4);
			if(DP_Min[DP3]>DEFAUT_DP3_MIN)
			{
				DP_Min[DP3] = DEFAUT_DP3_MIN;
				WriteEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],4);
			}
			
			ReadEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],1);
			if(LastDP_Alrm_ON[DP3]>2)
			{
				LastDP_Alrm_ON[DP3]=0;
				WriteEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
			}
		}
		
		ReadEEPROMData(RELAY_STAT_ADDR,&gu8_rly_stat,1);
		
		ReadEEPROMData(BROADCAST_ENB_ADDR,&gu8_broadcast,1);
		if(gu8_broadcast>1)
		{	
			gu8_broadcast=0;
			WriteEEPROMData(BROADCAST_ENB_ADDR,&gu8_broadcast,sizeof(gu8_broadcast));
		}
		
		
		for(uint8_t i=0; i<MAX_SUPPORTED_DP; i++)
		{
			ReadEEPROMData((DP_SW_FACT_ADDR+(i*2)),(uint8_t*)&su16_dp_sw_factor[i],2);
			if((su16_dp_sw_factor[i]<-10000) && (su16_dp_sw_factor[i]>10000))
			{	
				su16_dp_sw_factor[i]=0;
				WriteEEPROMData((DP_SW_FACT_ADDR+(i*2)),(uint8_t*)&su16_dp_sw_factor[i],2);
			}
			f32_dp_sw_factor[i]=(float)su16_dp_sw_factor[i]/100.0;
			
			ReadEEPROMData((DP_OFFSET_ADDR+(i*2)),(uint8_t*)&su16_dp_offset[i],2);
			if((su16_dp_offset[i]<-10000) || (su16_dp_offset[i]>10000))
			{
				su16_dp_offset[i]=0;
				WriteEEPROMData((DP_OFFSET_ADDR+(i*2)),(uint8_t*)&su16_dp_offset[i],2);
			}
			f32_dp_offset[i]=(float)su16_dp_offset[i]/100.0;
			
			ReadEEPROMData((DP_LIMIT_ADDR+(i*2)),(uint8_t*)&u16_dp_limit[i],2);
			if((u16_dp_limit[i]<500) || (u16_dp_limit[i]>9990))
			{
				u16_dp_limit[i]=2500;
				WriteEEPROMData((DP_LIMIT_ADDR+(i*2)),(uint8_t*)&u16_dp_limit[i],2);
			}
			f32_dp_limit[i]=(float)u16_dp_limit[i]/10.0;
		}
		
		//RS485 Parameter -------------------------------------------
		ReadEEPROMData(DEVICE_ID,&DeviceID,1);
		if(DeviceID>250)
		{
			DeviceID=DEFAULT_DEVICE_ID;
			WriteEEPROMData(DEVICE_ID,&DeviceID,sizeof(DeviceID));
		}
		
		//Buzzer Parameter -------------------------------------------
		ReadEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,2);
		if(Buzzer_ON_Time>60)
		{
			Buzzer_ON_Time=0;
			WriteEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,2);
		}
		
		ReadEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,2);
		if(Buzzer_OFF_Time>960)
		{
			Buzzer_OFF_Time=0;
			WriteEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,2);
		}
		
		if(gu16_parameterWord & ENABLE_LOG)
		{
			//Data Logging Parameter -------------------------------------------
			ReadEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,2);
			if((LogInterval<MIN_LOG_INTERVAL) || (LogInterval>MAX_LOG_INTERVAL))
			{
				LogInterval=DEFAULT_LOG_INTERVAL;
				WriteEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,2);
			}
		}
		
		//UART Parameter -------------------------------------------
		ReadEEPROMData(UART_BAUDRATE,&UART_BaudRate,1);
		if((UART_BaudRate<3) || (UART_BaudRate>9))
		{
			UART_BaudRate=DEFAULT_UART_BAUDRATE;
			WriteEEPROMData(UART_BAUDRATE,&UART_BaudRate,sizeof(UART_BaudRate));
		}
		
		//Customer Password -------------------------------------------
		ReadEEPROMData(CUSTOMER_PASSWORD,(uint8_t*)&CustPassword,2);
		if(CustPassword>999)
		{
			CustPassword=DEFAULT_CUSTOMER_PWD;
			WriteEEPROMData(CUSTOMER_PASSWORD,(uint8_t*)&CustPassword,2);
		}
		
		//Factory Customer Password -------------------------------------------
		ReadEEPROMData(FAC_CUSTOMER_PASSWORD,(uint8_t*)&FactCustPassword,2);
		if(FactCustPassword>9999)
		{
			FactCustPassword=DEFAULT_FACTORY_PWD;
			WriteEEPROMData(FAC_CUSTOMER_PASSWORD,(uint8_t*)&FactCustPassword,2);
		}
		
		//Acknowledge Parameter -------------------------------------------
		ReadEEPROMData(ACK_TIMER,(uint8_t*)&AckTimer,2);
		if(AckTimer>1440)
		{
			AckTimer=0;
			WriteEEPROMData(ACK_TIMER,(uint8_t*)&AckTimer,2);
		}
		
		ReadEEPROMData(ACK_PWD_IND,&AckPwdInd,1);
		if(AckPwdInd>NO_OF_ACKPWD)
		{
			AckPwdInd=0;
			WriteEEPROMData(ACK_PWD_IND,&AckPwdInd,sizeof(AckPwdInd));
		}
		
		for(uint8_t i=0;i<NO_OF_ACKPWD;i++)
		{
			ReadEEPROMData((ACK_PASSWORD+(i*2)),(uint8_t*)&AckPwd[i],2);
			if(AckPwd[i]>999)
			{
				AckPwd[i]=0;
				WriteEEPROMData((ACK_PASSWORD+(i*2)),(uint8_t*)&AckPwd[i],2);
			}
		}
		
		for(uint8_t i=0;i<NO_OF_XBEE_MAC;i++)
		{
			ReadEEPROMData((XBEE_MAC_ADDR+(i*XBEE_MAC_SIZE)),&gu8arr_XbeeMac[i][0],XBEE_MAC_SIZE);
			
			if(gu8arr_XbeeMac[i][0]==0xFF)
			{
				memset(&gu8arr_XbeeMac[i][0],'0',XBEE_MAC_SIZE);
				WriteEEPROMData((XBEE_MAC_ADDR+(i*XBEE_MAC_SIZE)),(uint8_t*)&gu8arr_XbeeMac[i][0],XBEE_MAC_SIZE);
			}
		}
		
		//Data Logging Parameter -------------------------------------------	
		
		ReadEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,2);
		if(CurrentLogIndReadLoc>=100)
		{
			CurrentLogIndReadLoc=0;
			WriteEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,2);
		}
		
		ReadEEPROMData(FLSH_OVF_IND,&FlashOVFByte,1);
		if(FlashOVFByte>1)
		{
			FlashOVFByte=0;
			WriteEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
		}
		
		ReadEEPROMData((CURR_LOG_IND+(CurrentLogIndReadLoc*4)),(uint8_t*)&CurrentLogInd,4);
		if(CurrentLogInd>=LAST_LOG_ADDR)
		{
			CurrentLogInd = 0;
			WriteEEPROMData((CURR_LOG_IND+(CurrentLogIndReadLoc*4)),(uint8_t*)&CurrentLogInd,4);
		}
		
		ReadEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,1);
		if(CurrentLog24IndReadLoc>=100)
		{
			CurrentLog24IndReadLoc = 0;
			WriteEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,sizeof(CurrentLog24IndReadLoc));
		}
		
		ReadEEPROMData((CURR_LOG24_IND+(CurrentLog24IndReadLoc*2)),(uint8_t*)&CurrentLog24Ind,2);
		if(CurrentLog24Ind>=LAST_LOG24_ADDR)
		{
			CurrentLog24Ind = 0;
			WriteEEPROMData((CURR_LOG24_IND+(CurrentLog24IndReadLoc*2)),(uint8_t*)&CurrentLog24Ind,2);
		}
	}
	
	if(gu8_rly_stat & 0x01) RELAY1_ON;	else 	RELAY1_OFF;	
	if(gu8_rly_stat & 0x02) RELAY2_ON;	else 	RELAY2_OFF;
}

void Init_variables(void)
{
	uint8_t i=0;
	
	for(i=0;i<NO_DIGIT;i++)
	{	
		disp_buffer[i]=0;
		data[i]=BLANK;
	}
	
	memset(&Buffer1[0],0,sizeof(Buffer1));
	memset(&TxBuffer[0],0,sizeof(TxBuffer));
	memset(&RxBuffer[0],0,sizeof(RxBuffer));
	memset(&RAMBuffer[0],0,sizeof(RAMBuffer));
	
	bool_autoSendResponse = false;
	bool_triggerXbeeReset = false;
	gu32_triggerXbeeResetTimer = (unsigned long)gu16_XbeeRstInterval*60;
	
	//gu8_AutoSentTimer=gu8_AutoSentInterval;
	gu8_groupID = ((DeviceID - 1)/gu8_DeviceInGroup)+1;
	
	logTimer = LogInterval;
	bool_brodcastEnb=0;	
	gu16_logtransfer=0;
	bool_logtransferStart=0;
	DP_StartUpTimer=5;
	bool_resetDevice=0;
	
	RS485_RX_ENB;
	BUZZER_OFF;
	XBEE_RST_HIGH;
	
	#ifndef DISABLE_DOOR_SENSING
	if((!DOOR_SENSE && gu8_doorSensingPolarity) || (DOOR_SENSE && !gu8_doorSensingPolarity))
	{
		bool_doorStatus=OPEN;
	}
	else
	{
		bool_doorStatus=CLOSE;
	}	
	#endif
	
//	SetMAC2Xbee(&gu8arr_XbeeMac[0][0],1);

	Kalman_Init(&Kalman[0], 0.01, 0.1, 0.0);  // Initialize with default values
	Kalman_Init(&Kalman[1], 0.01, 0.1, 0.0);  // Initialize with default values
	Kalman_Init(&Kalman[2], 0.01, 0.1, 0.0);  // Initialize with default values
	
	//-------------------------------------------------------
	//POWER ON LOG
	//-------------------------------------------------------
	us1=CurrentLog24Ind;
	LogReading(POWER_UP_LOG,0,0xFFFF);
	FillRamBuffer(POWER_UP_LOG,0,0xFFFF);
	
	//-------------------------------------------------------
	//Check MinMax Day change occur
	//-------------------------------------------------------
	if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_LOG) && (gu16_parameterWord & ENABLE_RTC))
	{
		if(RTCSetFlag && ((!FlashOVFByte && CurrentLogInd) || (FlashOVFByte && !CurrentLogInd)))
		{
			bool_resetMinMax=0;
	
			rtc1.second = 0;				//Second
			rtc1.minute = 0;				//Minute
			rtc1.hour = 0;					//Hour
			rtc1.day = rtc.day;				//Date
			rtc1.month = rtc.month;			//Month
			rtc1.year = rtc.year + 2000;	//Year
	
			ep.currentEpochTime = get_epoch_time(rtc1);

			if(us1)
			{
				us1--;
			}
			else
			{
				us1=LAST_LOG24_ADDR-1;
			}
	
			ReadLog(LAST_LOG24_ADDR_OFFSET + us1,(unsigned char*)&ep1.currentEpochTime,4);
	
			#ifdef ENABLE_PRINTF
	
				opstr(0,"\r\nLast 24Hr Log Index:");
				print_float(0,us1,test,0);		
				opstr(0,"\r\n");
	
				opstr(0,"\r\nDay Epoch:");
				print_Hex(0,ep.cept[3]);
				print_Hex(0,ep.cept[2]);
				print_Hex(0,ep.cept[1]);
				print_Hex(0,ep.cept[0]);
				opstr(0,"\r\n");
	
				opstr(0,"\r\nLast Epoch:");
				print_Hex(0,ep1.cept[3]);
				print_Hex(0,ep1.cept[2]);
				print_Hex(0,ep1.cept[1]);
				print_Hex(0,ep1.cept[0]);
				opstr(0,"\r\n");
	
			#endif
		
			if(ep1.currentEpochTime==0xFFFFFFFF)
			{
				bool_resetMinMax=1;
			}
			else
			{
				if(ep1.currentEpochTime < ep.currentEpochTime)
				{
					bool_resetMinMax=1;
				
					if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_M3LOG))
					{
						memcpy(&MinMaxMeanDayLogArr[0],(unsigned char*)&ep1.currentEpochTime,4);
				
						//Find DP1 Mean Value from last 24 Hour and Store it ---------------------------------------------
						if(gu16_parameterWord & ENABLE_DP1)
						{
							ReadMinMaxLog(DP1_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							DP_Mean[DP1]=0;
							a2=0;
							for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
							{
								memcpy((unsigned char*)&tempfloat1,&Buffer1[a2],4);
								a2 += 4;
								DP_Mean[DP1] += tempfloat1;
							}
							DP_Mean[DP1] /= TOTAL_MEAN_HOUR;
					
							memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&DP_Min[DP1],4);
							memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&DP_Max[DP1],4);
							memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&DP_Mean[DP1],4);
							WriteLog(LAST_DP1_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
						}
				
						//Find DP2 Mean Value from last 24 Hour and Store it ---------------------------------------------
						if(gu16_parameterWord & ENABLE_DP2)
						{
							ReadMinMaxLog(DP2_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							DP_Mean[DP2]=0;
							a2=0;
							for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
							{
								memcpy((unsigned char*)&tempfloat1,&Buffer1[a2],4);
								a2 += 4;
								DP_Mean[DP2] += tempfloat1;
							}
							DP_Mean[DP2] /= TOTAL_MEAN_HOUR;

							memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&DP_Min[DP2],4);
							memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&DP_Max[DP2],4);
							memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&DP_Mean[DP2],4);
							WriteLog(LAST_DP2_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
						}
				
						//Find DP3 Mean Value from last 24 Hour and Store it ---------------------------------------------
						if(gu16_parameterWord & ENABLE_DP3)
						{
							ReadMinMaxLog(DP3_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							DP_Mean[DP3]=0;
							a2=0;
							for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
							{
								memcpy((unsigned char*)&tempfloat1,&Buffer1[a2],4);
								a2 += 4;
								DP_Mean[DP3] += tempfloat1;
							}
							DP_Mean[DP3] /= TOTAL_MEAN_HOUR;

							memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&DP_Min[DP3],4);
							memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&DP_Max[DP3],4);
							memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&DP_Mean[DP3],4);
							WriteLog(LAST_DP3_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
						}
				
						//Clear all Hour mean value for next day
						memset(Buffer1,0,100);
						if(gu16_parameterWord & ENABLE_DP1)
						{
							WriteLog(DP1_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							HourDP_Mean[DP1]=0;
							HrDPSampleInd[DP1]=0;
						}
						if(gu16_parameterWord & ENABLE_DP2)
						{
							WriteLog(DP2_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							HourDP_Mean[DP2]=0;
							HrDPSampleInd[DP2]=0;
						}
						if(gu16_parameterWord & ENABLE_DP3)
						{
							WriteLog(DP3_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							HourDP_Mean[DP3]=0;
							HrDPSampleInd[DP3]=0;
						}
				
						MinMaxMeanDayLogInd++;
						if(MinMaxMeanDayLogInd>=TOTAL_MIN_MAX_MEAN_LOG) MinMaxMeanDayLogInd=0;
						WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
					}
				}
			
				ep.currentEpochTime = ep1.currentEpochTime;
	
				if(gu16_parameterWord & ENABLE_DP1)
				{
					//Serve Watchdog Timer
					IWDG_ReloadCounter();
			
					if(LastDP_Alrm_ON[DP1])
					{
						LogReading(DP1_ALM_RESTORE_LOG,0,0xFFFF);
						FillRamBuffer(DP1_ALM_RESTORE_LOG,0,0xFFFF);
				
						LastDP_Alrm_ON[DP1]=NO_ALARM;
						WriteEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
					}
				}
		
				if(gu16_parameterWord & ENABLE_DP2)
				{
					//Serve Watchdog Timer
					IWDG_ReloadCounter();
			
					if(LastDP_Alrm_ON[DP2])
					{
						LogReading(DP2_ALM_RESTORE_LOG,0,0xFFFF);
						FillRamBuffer(DP2_ALM_RESTORE_LOG,0,0xFFFF);
				
						LastDP_Alrm_ON[DP2]=NO_ALARM;
						WriteEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
					}
				}
		
				if(gu16_parameterWord & ENABLE_DP3)
				{
					//Serve Watchdog Timer
					IWDG_ReloadCounter();
			
					if(LastDP_Alrm_ON[DP3])
					{
						LogReading(DP3_ALM_RESTORE_LOG,0,0xFFFF);
						FillRamBuffer(DP3_ALM_RESTORE_LOG,0,0xFFFF);
				
						LastDP_Alrm_ON[DP3]=NO_ALARM;
						WriteEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
					}
				}
			}
	
			if(bool_resetMinMax)
			{
				ResetMinMax();
				#ifdef ENABLE_PRINTF
				opstr(0,"DayChange Occure.Min Max Reset\r\n");
				#endif
				bool_resetMinMax=0;
			}
		}
	}
	
	logTimer = LogInterval;
	bool_brodcastEnb=0;	
	logtransfer=0;
	bool_logtransferStart=0;
	DP_StartUpTimer=5;
	bool_resetDevice=0;
	
	RS485_RX_ENB;
	BUZZER_OFF;
	XBEE_RST_HIGH;
}

/***********************************************************************************************************************
  * @brief  This function is main entrance
  * @note   main
  * @param  none
  * @retval none
  *********************************************************************************************************************/
int main(void)
{
    PLATFORM_Init();
    GPIO_Configure();
	//-------------------------------------------------------
	//Initialize SPI for AT45DB321D
	//-------------------------------------------------------
	
	SPI_Configure();
	AT45D_Init();
	AT45D_set_page_size_to_pwr_of_two();
	boot_data();	//Boot Data from Dataflash

	//-------------------------------------------------------
	//Initialize UART
	//-------------------------------------------------------
	UART_Configure(UART_BaudRate);
	printf("Powered ON\n");

	//-------------------------------------------------------
	//Initialize I2C for DP1/DP2/DP3/DISPLAY/RTC
	//-------------------------------------------------------
	I2C1_Init();
	I2C2_Init();
	I2C3_Init();
	Init_PCF8563();
	InitLEDController();
	
	//-------------------------------------------------------
	//Initialize Timer and Variables
	//-------------------------------------------------------	
	TIM1_Configure();
	Init_variables();
	
	IWDG_Configure(1250);	//Initialize WDT
	
    while (1)
    {
		//1 Second Tick ====================================================
		if(bool_sec_flag)
		{
			SecondTick();
			bool_sec_flag=0;
		}
	
		whileTask();
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

