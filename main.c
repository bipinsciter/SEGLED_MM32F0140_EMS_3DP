/***********************************************************************************************************************
    @file    main.c
    @author  SAP
    @date    22-August-2026
    @brief   THIS FILE PROVIDES ALL THE SYSTEM FUNCTIONS.
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
#include "pressure_sensor.h"	//selects XGZP6891D or WF200DP via PRESSURE_SENSOR_PART
#include "SHT25.h"
//#include "DS1307.h"
#include "PCF8563.h"
#include "dataflash.h"		//selects AT45DB321D or XM25QH128A via DATAFLASH_PART
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
nt16 sRH;                    //variable for raw humidity ticks
nt16 sT;                     //variable for raw temperature ticks

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

unsigned long ydhms_diff (signed long int year1, signed long int yday1, signed int hour1, signed int min1, signed int sec1, signed int year0, signed int yday0, signed int hour0, signed int min0, signed int sec0)
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

//RTC integrity -------------------------------------------------------------------
//Called whenever the clock is found untrustworthy.  Clears RTCSetFlag so that NO
//logging path resumes until the time is explicitly set again, and latches the fault
//in the reserved CORRUPT_RTC_IND_ADDR byte so it survives a reboot.
static void InvalidateRTC(void)
{
	bool_rtcValid = 0;

	if(RTCSetFlag)
	{
		RTCSetFlag = 0;
		WriteEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
	}
}

void Check_RTC(void)
{
//	//---------------------------------------------------------------		
//	bool_Sec_blink_flag ^= 1;

//	ep.currentEpochTime++;
//	get_date_time(&rtc,ep.currentEpochTime);
	
//	if(rtc.hour>=12) 
//	{
//		bool_AM_PM_Flag=0;
//	}
//	else                    
//	{
//		bool_AM_PM_Flag=1;
//	}

//	//---------------------------------------------------------------	
	
	uint8_t rtcHwFault = 0;
	
	RTC_data[0]=Read_byte_PCF8563(RTC_TIMESEC_REG);
	current_sec=RTC_data[0];
	
	//Clock integrity, re-tested every second.  VL = backup supply was lost,
	//STOP = oscillator halted.  Either way the timestamp is meaningless.
	//Tested on the RAW seconds byte - the VL bit is masked off further down.
	if((RTC_data[0] & PCF8563_VL_BIT) || (Read_byte_PCF8563(RTC_CNTRL1_ADDR) & PCF8563_STOP_BIT))
	{
		rtcHwFault = 1;
		InvalidateRTC();
	}
	
	if(last_sec != current_sec)
	{
		last_sec = current_sec;
		
		Read_PCF8563(RTC_TIMEMIN_REG,&RTC_data[1],6);
		
		bool_Sec_blink_flag ^= 1;

		RTC_data[0] &= 0x7F;
		RTC_data[1] &= 0x7F;
		RTC_data[2] &= 0x3F;
		RTC_data[3] &= 0x3F;
		RTC_data[5] &= 0x1F;
		
		rtc.second = BCD2HEX(RTC_data[0]);		//Second
		rtc.minute = BCD2HEX(RTC_data[1]);		//Minute
		rtc.hour = BCD2HEX(RTC_data[2]);		//Hour
		rtc.day = BCD2HEX(RTC_data[3]);			//Date
		rtc.month = BCD2HEX(RTC_data[5]);		//Month
		rtc.year = BCD2HEX(RTC_data[6]);		//Year
		
		//The register masks above still allow impossible values (hour up to 45,
		//month up to 19), so validate the decoded fields before trusting them.
		if((rtc.second>59) || (rtc.minute>59) || (rtc.hour>23) ||
		   (rtc.day<1) || (rtc.day>31) || (rtc.month<1) || (rtc.month>12))
		{
			InvalidateRTC();
		}
		else if(!rtcHwFault)
		{
			//Integrity bits clear AND fields sane: the timestamp is usable
			bool_rtcValid = 1;
		}
		
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
			
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)

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

			#else

			if(gu16_parameterWord & ENABLE_TEMP)
			{
				if(!TM_Unit)
				{
					HourTM_Mean += temperatureC;
				}
				else
				{
					HourTM_Mean += temperatureF;
				}
				HrTMSampleInd++;
			}
		
			if(gu16_parameterWord & ENABLE_RH)
			{
				HourRH_Mean += humidityRH;
				HrRHSampleInd++;
			}

			#endif
		}
	}
	
	//---------------------------------------------------------------
	if(last_hr != current_hr)
	{
		//last_hr indexes TOTAL_MEAN_HOUR slots - a corrupt RTC hour must not address past them.
		//RTCSetFlag/bool_rtcValid: never log against an unset or untrustworthy clock.
		#if BUILD_MEAN24_LOG
		if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_M3LOG)
			&& RTCSetFlag && bool_rtcValid && (last_hr < TOTAL_MEAN_HOUR))
		{
			if(gu16_parameterWord & ENABLE_DP1)
			{
				HourDP_Mean[DP1] /= HrDPSampleInd[DP1];
				WriteLog(DP1_CURR_24HR_MEAN_OFFSET,last_hr,(uint8_t*)&HourDP_Mean[DP1],4);
				HourDP_Mean[DP1]=0.0;
				HrDPSampleInd[DP1]=0;
			}
			
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)

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

			#else

			if(gu16_parameterWord & ENABLE_TEMP)
			{
				HourTM_Mean /= HrTMSampleInd;
				WriteLog(TM_CURR_24HR_MEAN_OFFSET,last_hr,(uint8_t*)&HourTM_Mean,4);
				HourTM_Mean=0.0;
				HrTMSampleInd=0;
			}
		
			if(gu16_parameterWord & ENABLE_RH)
			{
				HourRH_Mean /= HrRHSampleInd;
				WriteLog(RH_CURR_24HR_MEAN_OFFSET,last_hr,(uint8_t*)&HourRH_Mean,4);
				HourRH_Mean=0.0;
				HrRHSampleInd=0;
			}
			
			#endif
		}
		#endif	// BUILD_MEAN24_LOG
		
		last_hr = current_hr;
	}
			
	if((ep.currentEpochTime % 86400) < 5)
	{
		if(!bool_resetMinMax)
		{
			//RTCSetFlag/bool_rtcValid: never log against an unset or untrustworthy clock
			#if (BUILD_MINMAX_LOG || BUILD_MEAN24_LOG)
			if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_M3LOG)
				&& RTCSetFlag && bool_rtcValid)
			{
				#if BUILD_MINMAX_LOG
				//Store Last Day Epoch with less than 2 minutes
				ep1.currentEpochTime = ep.currentEpochTime - 120;
			
				memcpy(&MinMaxMeanDayLogArr[0],(uint8_t*)&ep1.currentEpochTime,4);
								
				//Find DP1 Mean Value from last 24 Hour and Store it ---------------------------------------------
				if(gu16_parameterWord & ENABLE_DP1)
				{
					#if BUILD_MEAN24_LOG
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
					#else
					//24 h mean log not built - the daily record keeps min and max, mean is zero
					DP_Mean[DP1] = 0;
					#endif
			
					memcpy(&MinMaxMeanDayLogArr[4],(uint8_t*)&DP_Min[DP1],4);
					memcpy(&MinMaxMeanDayLogArr[8],(uint8_t*)&DP_Max[DP1],4);
					memcpy(&MinMaxMeanDayLogArr[12],(uint8_t*)&DP_Mean[DP1],4);
					WriteLog(LAST_DP1_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
				}
				
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)

				//Find DP2 Mean Value from last 24 Hour and Store it ---------------------------------------------
				if(gu16_parameterWord & ENABLE_DP2)
				{
					#if BUILD_MEAN24_LOG
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
					#else
					//24 h mean log not built - the daily record keeps min and max, mean is zero
					DP_Mean[DP2] = 0;
					#endif
			
					memcpy(&MinMaxMeanDayLogArr[4],(uint8_t*)&DP_Min[DP2],4);
					memcpy(&MinMaxMeanDayLogArr[8],(uint8_t*)&DP_Max[DP2],4);
					memcpy(&MinMaxMeanDayLogArr[12],(uint8_t*)&DP_Mean[DP2],4);
					WriteLog(LAST_DP2_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
				}
			
				//Find DP3 Mean Value from last 24 Hour and Store it ---------------------------------------------
				if(gu16_parameterWord & ENABLE_DP3)
				{
					#if BUILD_MEAN24_LOG
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
					#else
					//24 h mean log not built - the daily record keeps min and max, mean is zero
					DP_Mean[DP3] = 0;
					#endif
				
					memcpy(&MinMaxMeanDayLogArr[4],(uint8_t*)&DP_Min[DP3],4);
					memcpy(&MinMaxMeanDayLogArr[8],(uint8_t*)&DP_Max[DP3],4);
					memcpy(&MinMaxMeanDayLogArr[12],(uint8_t*)&DP_Mean[DP3],4);
					WriteLog(LAST_DP3_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
				}

				#else

				//Find TM Mean Value from last 24 Hour and Store it ---------------------------------------------
				if(gu16_parameterWord & ENABLE_TEMP)
				{
					#if BUILD_MEAN24_LOG
					ReadMinMaxLog(TM_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					TM_Mean=0;
					a2=0;
					for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
					{
						memcpy((unsigned char*)&tempfloat1,&Buffer1[a2],4);
						a2 += 4;
						TM_Mean += tempfloat1;
					}
					TM_Mean /= TOTAL_MEAN_HOUR;
					#else
					//24 h mean log not built - the daily record keeps min and max, mean is zero
					TM_Mean = 0;
					#endif
				
					memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&TM_Min,4);
					memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&TM_Max,4);
					memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&TM_Mean,4);
					WriteLog(LAST_TM_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
				}
				
				//Find RH Mean Value from last 24 Hour and Store it ---------------------------------------------
				if(gu16_parameterWord & ENABLE_RH)
				{
					#if BUILD_MEAN24_LOG
					ReadMinMaxLog(RH_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					RH_Mean=0;
					a2=0;
					for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
					{
						memcpy((unsigned char*)&tempfloat1,&Buffer1[a2],4);
						a2 += 4;
						RH_Mean += tempfloat1;
					}
					RH_Mean /= TOTAL_MEAN_HOUR;
					#else
					//24 h mean log not built - the daily record keeps min and max, mean is zero
					RH_Mean = 0;
					#endif
			
					memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&RH_Min,4);
					memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&RH_Max,4);
					memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&RH_Mean,4);
					WriteLog(LAST_RH_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
				}
				
				#endif
			
				#endif	// BUILD_MINMAX_LOG

				//Clear all Hour mean value for next day
				#if BUILD_MEAN24_LOG
				memset(Buffer1,0,100);
				if(gu16_parameterWord & ENABLE_DP1)
				{
					WriteLog(DP1_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					HourDP_Mean[DP1]=0;
					HrDPSampleInd[DP1]=0;
				}
				
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)

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
				
				#else

				if(gu16_parameterWord & ENABLE_TEMP)
				{
					WriteLog(TM_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					HourTM_Mean=0;
					HrTMSampleInd=0;
				}
				if(gu16_parameterWord & ENABLE_RH)
				{
					WriteLog(RH_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
					HourRH_Mean=0;
					HrRHSampleInd=0;
				}
				
				#endif
			
				#endif	// BUILD_MEAN24_LOG

				#if BUILD_MINMAX_LOG
				MinMaxMeanDayLogInd++;
				if(MinMaxMeanDayLogInd>=TOTAL_MIN_MAX_MEAN_LOG) MinMaxMeanDayLogInd=0;
				WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
				#endif	// BUILD_MINMAX_LOG
			}
			#endif	// (BUILD_MINMAX_LOG || BUILD_MEAN24_LOG)
			
			//ResetMinMax() is NOT part of the archive - the displayed min/max still has to
			//roll over at midnight whether or not anything is being logged.
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

	if(gu8_IsLCDDisable)
	{
		TM1680WriteCommand(SYS_DISABLE);
		TM1680WriteCommand(LED_OFF);
	}
	else
	{
		TM1680WriteCommand(SYS_ENABLE);
		TM1680WriteCommand(LED_ON);
		TM1680Brighness(gu8_LCDBrigthnessCnt);
	}
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
	
	data[5] = FW_MAJOR+10;
	data[6] = FW_MINOR;
						
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
	PLATFORM_DelayMS(1000);
}

//The digits show HI / LO the instant a reading is clamped to the sensor's range limit
//(f32_dp_limit), but the alarm state itself is deliberately filtered by
//gu8_DpAlarmSensingTime - so the panel used to show HI or LO in the normal colour for
//the whole length of that filter, contradicting itself.
//
//An off-scale reading is not the kind of transient the filter exists to reject (the
//sensor is pegged), so it drives the alarm colour straight away.  The real alarm state
//- logging, relay, buzzer and the protocol - still respects the sensing time and is
//untouched by this.
//Hold the displayed DP value until the reading moves by more than DP_DISP_HYSTERESIS.
//Sensor noise of a tenth of a Pa no longer makes the last digit flicker, while the
//alarm comparison keeps using the raw Dpressure[] and so responds just as quickly.
static float DpDisplayValue(uint8_t SensNo)
{
	static float shown[MAX_SUPPORTED_DP]={0};
	static uint8_t primed[MAX_SUPPORTED_DP]={0};
	float value;

	if(SensNo >= MAX_SUPPORTED_DP)	return 0.0;

	value = Dpressure[SensNo];

	if(!primed[SensNo] ||
	   (value >= (shown[SensNo] + DP_DISP_HYSTERESIS)) ||
	   (value <= (shown[SensNo] - DP_DISP_HYSTERESIS)))
	{
		shown[SensNo]  = value;
		primed[SensNo] = 1;
	}

	return shown[SensNo];
}

static uint8_t DpDisplayAlarm(uint8_t SensNo)
{
	if(DP_Alrm_ON[SensNo] != NO_ALARM)	return DP_Alrm_ON[SensNo];
	if(DP_limit[SensNo] == 1)			return UPPER_ALARM;	//clamped at +full scale
	if(DP_limit[SensNo] == 2)			return LOWER_ALARM;	//clamped at -full scale

	return NO_ALARM;
}

//What disp_value() should DRAW for a channel.
//
//A real alarm renders steadily, exactly as before.  An early warning renders THE
//SAME WAY - same bit plane, same alarm symbols - but only on alternate 500 ms
//phases of bool_mec500_blink_flag, which the TIM1 ISR already toggles.  On the
//other phase it falls back to the normal rendering, so the reading itself stays
//on screen and legible the whole time and it is the alarm STYLING that flashes.
//
//To blank the digits outright on the off phase instead, return a state that draws
//nothing rather than NO_ALARM here - but note the value then disappears for half
//of every second, which is hard to read on a segment display.
static uint8_t AlarmDisplayState(uint8_t alarm,uint8_t nearAlarm)
{
	if(alarm != NO_ALARM)						return alarm;
	if(nearAlarm != NO_ALARM && gu8_nearBlinkOn)	return nearAlarm;

	return NO_ALARM;
}

static uint8_t DpDisplayState(uint8_t SensNo)
{
	return AlarmDisplayState(DpDisplayAlarm(SensNo),gu8_DP_NearAlrm[SensNo]);
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
	
	if(DpDisplayState(DP1)==NO_ALARM)
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
	else if(DpDisplayState(DP1)==LOWER_ALARM)
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
	
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	
	if(DpDisplayState(DP2)==NO_ALARM)
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
	else if(DpDisplayState(DP2)==LOWER_ALARM)
	{
		if(lcd.Sym_TM_MIN_ALM) TM_MIN_ALM_on;
		
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
	
	if(DpDisplayState(DP3)==NO_ALARM)
	{
		if(lcd.Sym_RH_MIN) RH_MIN_on;
		
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
	else if(DpDisplayState(DP3)==LOWER_ALARM)
	{
		if(lcd.Sym_RH_MIN_ALM) RH_MIN_ALM_on;
		
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
	
	#else
	
	if(lcd.Sym_TM_UNIT_C) TM_UNIT_C_on;
	if(lcd.Sym_TM_UNIT_F) TM_UNIT_F_on;
	
	if(AlarmDisplayState(TM_Alrm_ON,gu8_TM_NearAlrm)==NO_ALARM)
	{
		if(lcd.Sym_TM_MIN) TM_MIN_on;
		if(lcd.Sym_TM_LOGO) TM_LOGO_on;
		
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
	else if(AlarmDisplayState(TM_Alrm_ON,gu8_TM_NearAlrm)==UPPER_ALARM)
	{
		if(lcd.Sym_TM_MIN_ALM) TM_MIN_ALM_on;
		if(lcd.Sym_TM_LOGO_ALM) TM_LOGO_ALM_on;
		
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
		if(lcd.Sym_TM_LOGO_ALM) TM_LOGO_ALM_on;
		
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
	
	if(AlarmDisplayState(RH_Alrm_ON,gu8_RH_NearAlrm)==NO_ALARM)
	{
		if(lcd.Sym_RH_MIN) RH_MIN_on;
		if(lcd.Sym_RH_LOGO) RH_LOGO_on;
		
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
	else if(AlarmDisplayState(RH_Alrm_ON,gu8_RH_NearAlrm)==UPPER_ALARM)
	{
		if(lcd.Sym_RH_MIN_ALM) RH_MIN_ALM_on;
		if(lcd.Sym_RH_LOGO_ALM) RH_LOGO_ALM_on;
		
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
		if(lcd.Sym_RH_LOGO_ALM) RH_LOGO_ALM_on;
		
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
	#endif
	
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
	
	//Steady on normally; blinks for LOGO_ACK_BLINK_MS after each valid message
	//served by ServePCMsg(), so UART activity is visible from the front.
	lcd.Sym_LOGO = gu16_logoAckBlinkTimer ? bool_mec500_blink_flag : 1;
	
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
				if(!RTCSetFlag || !bool_rtcValid)	//unset OR untrustworthy clock blinks the time
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
							tempfloat = DpDisplayValue(DP1);
						
							if(!DP_limit[DP1])
							{
								if(tempfloat<0.0)
								{
									tempfloat *= (-1.0);
									
									if(DpDisplayAlarm(DP1)) 
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
						if(DpDisplayAlarm(DP1)) 
						{
							lcd.Sym_DP_LOGO_ALM = 1;
						}
						else
						{
							lcd.Sym_DP_LOGO = 1;
						}
					}
					
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					
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
							tempfloat = DpDisplayValue(DP2);
						
							if(!DP_limit[DP2])
							{
								if(tempfloat<0.0)
								{
									tempfloat *= (-1.0);
									
									if(DpDisplayAlarm(DP2)) 
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
						
						if(DpDisplayAlarm(DP2)) 
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
							tempfloat = DpDisplayValue(DP3);
						
							if(!DP_limit[DP3])
							{
								if(tempfloat<0.0)
								{
									tempfloat *= (-1.0);
									
									if(DpDisplayAlarm(DP3)) 
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
						
						if(DpDisplayAlarm(DP3)) 
						{
							lcd.Sym_RH_LOGO_ALM = 1;
						}
						else
						{
							lcd.Sym_RH_LOGO = 1;
						}
					}
					
					#else
					
					if(gu16_parameterWord & ENABLE_TEMP)
					{
						if(bool_RH_TEMP_NC)
						{
							data[7]=E;
							data[8]=r;
							data[9]=r;
						}
						else
						{
							//----------------------------------------------------
							if(!TM_Unit)
							{
								tempfloat = temperatureC;
							}
							else
							{
								tempfloat = temperatureF;
							}
						
							if(tempfloat<0.0)
							{
								tempfloat *= (-1.0);
								
								if(TM_Alrm_ON) 
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
								convert_float(99.9,&data[7],0);
							}
							//----------------------------------------------------
						}
						if(!TM_Unit)
						{
							lcd.Sym_TM_UNIT_C = 1;
						}
						else
						{
							lcd.Sym_TM_UNIT_F = 1;
						}
						
						if(TM_Alrm_ON) 
						{
							lcd.Sym_TM_LOGO_ALM = 1;
						}
						else
						{
							lcd.Sym_TM_LOGO = 1;
						}
					}
					
					if(gu16_parameterWord & ENABLE_RH)
					{
						if(bool_RH_TEMP_NC)
						{
							data[10]=E;
							data[11]=r;
							data[12]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = humidityRH;
						
							if(tempfloat<0.0)
							{
								tempfloat *= (-1.0);
								if(RH_Alrm_ON) 
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
								convert_float(99.9,&data[10],0);
							}
							//----------------------------------------------------
						}
						
						if(RH_Alrm_ON) 
						{
							lcd.Sym_RH_LOGO_ALM = 1;
						}
						else
						{
							lcd.Sym_RH_LOGO = 1;
						}
						lcd.Sym_RH_UNIT = 1;
					}
					
					#endif
			
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
					
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					
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
					
					#else
				
					if(gu16_parameterWord & ENABLE_TEMP)
					{
						if(bool_RH_TEMP_NC)
						{
							data[7]=E;
							data[8]=r;
							data[9]=r;
						}
						else
						{
							//----------------------------------------------------
							if(!TM_Unit)
							{
								tempfloat = TM_Min;
								lcd.Sym_TM_UNIT_C = 1;
							}
							else
							{
								tempfloat = (TM_Min * 1.8) + 32.0;
								lcd.Sym_TM_UNIT_F = 1;
							}
						
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
								convert_float(99.9,&data[7],0);
							}
							//----------------------------------------------------
							if(TM_Alrm_ON) 
							{
								lcd.Sym_TM_LOGO_ALM = 1;
							}
							else
							{
								lcd.Sym_TM_LOGO = 1;
							}
						}
					}
					
					if(gu16_parameterWord & ENABLE_RH)
					{
						if(bool_RH_TEMP_NC)
						{
							data[10]=E;
							data[11]=r;
							data[12]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = RH_Min;
							
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
								convert_float(99.9,&data[10],0);
							}
							//----------------------------------------------------
						}
		
						lcd.Sym_RH_LOGO = 1;
						lcd.Sym_RH_UNIT = 1;
					}
					
					#endif
					
				break;
					
				case 4:
					
					lcd.Sym_MAX = 1;
					
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
						
						lcd.Sym_DP_UNIT = 1;
						lcd.Sym_DP_LOGO = 1;
					}
					
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)

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
					
					#else
					
					if(gu16_parameterWord & ENABLE_TEMP)
					{
						if(bool_RH_TEMP_NC)
						{
							data[7]=E;
							data[8]=r;
							data[9]=r;
						}
						else
						{
							//----------------------------------------------------
							if(!TM_Unit)
							{
								tempfloat = TM_Max;
							}
							else
							{
								tempfloat = (TM_Max * 1.8) + 32.0;
							}
							
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
								convert_float(99.9,&data[7],0);
							}
							//----------------------------------------------------
						}
						if(!TM_Unit)
						{
							lcd.Sym_TM_UNIT_C = 1;
						}
						else
						{
							lcd.Sym_TM_UNIT_F = 1;
						}

						if(TM_Alrm_ON) 
						{
							lcd.Sym_TM_LOGO_ALM = 1;
						}
						else
						{
							lcd.Sym_TM_LOGO = 1;
						}
					}

					if(gu16_parameterWord & ENABLE_RH)
					{
						if(bool_RH_TEMP_NC)
						{
							data[10]=E;
							data[11]=r;
							data[12]=r;
						}
						else
						{
							//----------------------------------------------------
							tempfloat = RH_Max;
							
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
								convert_float(99.9,&data[10],0);
							}
							//----------------------------------------------------
						}
						
						lcd.Sym_RH_LOGO = 1;
						lcd.Sym_RH_UNIT = 1;
					}
					
					#endif
					
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
		
		#if BUILD_MINMAX_LOG
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
			
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				
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
				
				#else
				
				//Temperature ------------------------------
				if(gu16_parameterWord & ENABLE_TEMP)
				{
					if(bool_noData)
					{
						data[7]=DASH;
						data[8]=DASH;
						data[9]=DASH;
					}
					else
					{
						if(TM_Unit)
						{
							tempfloat1 = (tempfloat1 * 1.8) + 32.0;
						}
					
						if(tempfloat1<0.0)
						{
							tempfloat1 *= (-1.0);
						}
						//----------------------------------------------------
						if(tempfloat1 < 10.0)
						{
							convert_float(tempfloat1,&data[8],1);
						}
						else if(tempfloat < 100.0)
						{
							convert_float(tempfloat1,&data[7],1);
						}
						else
						{
							convert_float(tempfloat1,&data[7],0);
						}
					}
					//----------------------------------------------------				
					if(!TM_Unit)
					{
						lcd.Sym_TM_UNIT_C = 1;
					}
					else
					{
						lcd.Sym_TM_UNIT_F = 1;
					}
						
					lcd.Sym_TM_LOGO = 1;
				}
				
				// RH -----------------------------
				if(gu16_parameterWord & ENABLE_RH)
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
					lcd.Sym_RH_LOGO = 1;
					lcd.Sym_RH_UNIT = 1;
				}

				#endif
			}
			
		break;
			
		#endif	// BUILD_MINMAX_LOG
		#if BUILD_MEAN24_LOG
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
				
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				
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
				#else
				//--------------------------------------------------
				if(gu16_parameterWord & ENABLE_TEMP)
				{
					if(bool_RH_TEMP_NC)
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
						//----------------------------------------------------
					}
					if(!TM_Unit)
					{
						lcd.Sym_TM_UNIT_C = 1;
					}
					else
					{
						lcd.Sym_TM_UNIT_F = 1;
					}
						
					lcd.Sym_TM_LOGO = 1;
				}
				//--------------------------------------------------
				if(gu16_parameterWord & ENABLE_RH)
				{
					if(bool_RH_TEMP_NC)
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
					lcd.Sym_RH_LOGO = 1;
					lcd.Sym_RH_UNIT = 1;
				}
				#endif
			}
			
		break;
		
		#endif	// BUILD_MEAN24_LOG
		case PROG_MODE:
		
			switch(emProgpara)
			{
				case PROG_PAGE_DISP:
				
					data[4] = P;
					data[5] = r;
					data[6] = 9;
				
				break;
				
				case DEVICE_ID_DISP:
				
					data[4] = D;
					data[5] = V;
					data[6] = C;
				
					data[2] = 1;
					data[3] = D;
				
					convert_char(dummy,&data[7],3);
				
				break;
				
				case DP1_ALM_UP_ON_DISP:
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				case DP2_ALM_UP_ON_DISP:
				case DP3_ALM_UP_ON_DISP:
				#endif
				
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
					
					data[8] += 10;
				
				break;
				
				case DP1_ALM_UP_OFF_DISP:
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				case DP2_ALM_UP_OFF_DISP:
				case DP3_ALM_UP_OFF_DISP:
				#endif
				
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
					
					data[8] += 10;
				
				break;
				
				case DP1_ALM_LO_OFF_DISP:
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				case DP2_ALM_LO_OFF_DISP:
				case DP3_ALM_LO_OFF_DISP:
				#endif
					
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
					
					data[8] += 10;
				
				break;
				
				case DP1_ALM_LO_ON_DISP:
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				case DP2_ALM_LO_ON_DISP:
				case DP3_ALM_LO_ON_DISP:
				#endif
					
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
					
					data[8] += 10;
				
				break;
					
				#if (DEVICE_MODE==DP1_TEMP_RH_MODE)

				case TM_ALM_UP_ON_DISP:
					
					if(!TM_Unit)
					{
						lcd.Sym_TM_UNIT_C = 1;
					}
					else
					{
						lcd.Sym_TM_UNIT_F = 1;
					}
						
					lcd.Sym_TM_LOGO = 1;
				
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
					
					data[8] += 10;
				
				break;
				
				case TM_ALM_UP_OFF_DISP:
				
					if(!TM_Unit)
					{
						lcd.Sym_TM_UNIT_C = 1;
					}
					else
					{
						lcd.Sym_TM_UNIT_F = 1;
					}
						
					lcd.Sym_TM_LOGO = 1;
				
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
					
					data[8] += 10;
				
				break;
				
				case TM_ALM_LO_OFF_DISP:
					
					if(!TM_Unit)
					{
						lcd.Sym_TM_UNIT_C = 1;
					}
					else
					{
						lcd.Sym_TM_UNIT_F = 1;
					}
						
					lcd.Sym_TM_LOGO = 1;
				
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
					
					data[8] += 10;
				
				break;
				
				case TM_ALM_LO_ON_DISP:
				
					if(!TM_Unit)
					{
						lcd.Sym_TM_UNIT_C = 1;
					}
					else
					{
						lcd.Sym_TM_UNIT_F = 1;
					}
						
					lcd.Sym_TM_LOGO = 1;
				
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
					
					data[8] += 10;
				
				break;
				
				case TM_UNIT_DISP:
				
					if(!dummy)
					{
						lcd.Sym_TM_UNIT_C = 1;
					}
					else
					{
						lcd.Sym_TM_UNIT_F = 1;
					}
						
					lcd.Sym_TM_LOGO = 1;
				
					data[7] = U;
					data[8] = N;
					data[9] = t;
				
				break;
				
				case RH_ALM_UP_ON_DISP:

					lcd.Sym_RH_LOGO = 1;
					lcd.Sym_RH_UNIT = 1;
					
					data[1] = 0;
					data[2] = N;
					
					data[10] = U;
					data[11] = P;

					convert_char(dummy,&data[6],4);
				
					data[8] += 10;
						
				break;
				
				case RH_ALM_UP_OFF_DISP:
				
					lcd.Sym_RH_LOGO = 1;
					lcd.Sym_RH_UNIT = 1;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					data[10] = U;
					data[11] = P;
				
					convert_char(dummy,&data[6],4);
				
					data[8] += 10;
				
				break;
				
				case RH_ALM_LO_OFF_DISP:

					lcd.Sym_RH_LOGO = 1;
					lcd.Sym_RH_UNIT = 1;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					data[10] = L;
					data[11] = 0;

					convert_char(dummy,&data[6],4);
				
					data[8] += 10;
				
				break;
				
				case RH_ALM_LO_ON_DISP:

					lcd.Sym_RH_LOGO = 1;
					lcd.Sym_RH_UNIT = 1;
				
					data[1] = 0;
					data[2] = N;
				
					data[10] = L;
					data[11] = 0;

					convert_char(dummy,&data[6],4);
				
					data[8] += 10;
				
				break;	
				
				#endif
				
				case RTC_HR_DISP:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = H;
					data[5] = r;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case RTC_MN_DISP:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = M;
					data[5] = N;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case RTC_DT_DISP:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = D;
					data[5] = t;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case RTC_MH_DISP:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = M;
					data[5] = 0;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case RTC_YR_DISP:
				
					data[1] = r;
					data[2] = t;
					data[3] = C;
				
					data[4] = Y;
					data[5] = r;
				
					convert_char(dummy,&data[7],2);
				
				break;
				
				case BUZ_ON_DISP:
				
					data[4] = B;
					data[5] = U;
					data[6] = r;
				
					data[2] = 0;
					data[3] = N;
				
					convert_char(dummy,&data[7],3);
				
				break;
				
				case BUZ_OFF_DISP:
				
					data[4] = B;
					data[5] = U;
					data[6] = r;
				
					data[1] = 0;
					data[2] = F;
					data[3] = F;
				
					convert_char(dummy,&data[7],3);
				
				break;
				
				case UART_BDT_DISP:
				
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
						case BAUD_115200:	convert_float(115200,&data[4],0);	break;
					}

				break;
					
				case CAL_DISP:
				
					data[1] = C;
					data[2] = A;
					data[3] = L;
				
					convert_char(dummy,&data[4],3);
				
				break;
			}
			
		break;
	}
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

//Auto-repeat ladder for the UP / DOWN keys.  cnt is the number of consecutive
//50 ms ticks the key has been held; the return value is how much to step this tick
//(0 = no step).  Previously CheckUpDnKey() only ran every 500 ms, which capped the
//repeat rate at 2 steps/second and made the +1 / +10 / +100 ladder feel jumpy.
//TRUE when UP/DOWN is navigating pages or parameters rather than editing a value.
//In PROG_MODE the PARA_SELECT chord selects the parameter; without it UP/DOWN edits
//the value.  In NORMAL_MODE the keys cycle display pages unless an ACK password is
//being entered.
static uint8_t KeyInNavMode(void)
{
	#if BUILD_MINMAX_LOG
	if(mode==MIN_MAX_MEAN_MODE)									return 1;
	#endif
	#if BUILD_MEAN24_LOG
	if(mode==MEAN_HOUR_MODE)									return 1;
	#endif
	if((mode==NORMAL_MODE) && !gu8_SetACKPwd)					return 1;
	if((mode==PROG_MODE) && !PARA_SELECT_KEY)					return 1;

	return 0;
}

//Navigation repeat: one step on the initial press, then the original slow cadence.
static uint8_t KeyNavStep(uint8_t cnt)
{
	if(cnt == 1)				return 1;		//one step on the initial press
	if(cnt <= KEY_NAV_DELAY)	return 0;		//hold-off

	return ((cnt % KEY_NAV_REPEAT) == 0) ? 1 : 0;
}

static int16_t KeyRepeatStep(uint8_t cnt)
{
	if(cnt == 1)					return 1;					//first press - step immediately
	if(cnt <= KEY_REPEAT_DELAY)		return 0;					//hold-off: a tap gives one step
	if(cnt <= KEY_REPEAT_MEDIUM)	return (cnt & 1) ? 1 : 0;	//10 steps/s - fine adjust
	if(cnt <= KEY_REPEAT_FAST)		return 1;					//20 steps/s

	return 10;													//coarse - long hold
}

void CheckUpDnKey(void)
{
	uint8_t i=0;
	
	if((gu16_parameterWord & ENABLE_M3LOG) && !PARA_SELECT_KEY && !PROG_ENT_KEY)
	{
		//Gate the BODY, not the branch: an #if around the `if` would leave the
		//`else if` below it dangling.
		#if BUILD_MINMAX_LOG
		if(mode==NORMAL_MODE)
		{
			MinMaxMeanModeTimer++;
			if(MinMaxMeanModeTimer > KEY_HOLD_10SEC)
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
		#endif	// BUILD_MINMAX_LOG
	}
	else if(!PARA_SELECT_KEY && !UP_KEY)
	{
		#if BUILD_MEAN24_LOG
		if(mode==NORMAL_MODE)
		{
			MeanHrModeTimer++;
			if(MeanHrModeTimer > KEY_HOLD_10SEC)
			{
				MeanHrModeTimer=0;
				
				mode=MEAN_HOUR_MODE;
				mean_hr_page_disp_cnt=0;
				dispMinMaxMeanLogInd=0;
				progTimeout=60;
			}
		}
		#endif	// BUILD_MEAN24_LOG
	}
	else if(!PARA_SELECT_KEY && !DN_KEY)
	{
		if(mode==NORMAL_MODE)
		{
			gu8_MinMaxClearTimer++;
			if(gu8_MinMaxClearTimer > KEY_HOLD_10SEC)
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
			if(DPAutoCalTimer > KEY_HOLD_5SEC)
			{
				DPAutoCalTimer=0;
						
				DP_Cal_Value_C[DP1] = (int16_t)((RealDpressure[DP1] - DP_Cal_float_Value_F[DP1])*10.0);
				WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],sizeof(DP_Cal_Value_C[DP1]));
				DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;

				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				DP_Cal_Value_C[DP2] = (int16_t)((RealDpressure[DP2] - DP_Cal_float_Value_F[DP2])*10.0);
				//DP_Cal_Value_C[DP2] = RealDpressure[DP2]*10.0;
				WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],sizeof(DP_Cal_Value_C[DP2]));
				DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
		
				DP_Cal_Value_C[DP3] = (int16_t)((RealDpressure[DP3] - DP_Cal_float_Value_F[DP3])*10.0);
				//DP_Cal_Value_C[DP3] = RealDpressure[DP3]*10.0;
				WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],sizeof(DP_Cal_Value_C[DP3]));
				DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
				#endif
				
//				switch(autoCal_para_cnt)
//				{
//					case 0:
//						
//						DP_Cal_Value_C[DP1] = (int16_t)((RealDpressure[DP1] - DP_Cal_float_Value_F[DP1])*10.0);
//						//DP_Cal_Value_C[DP1] = RealDpressure[DP1]*10.0;
//						WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],sizeof(DP_Cal_Value_C[DP1]));
//						DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;
//						
//					break;
//				
//					case 1:
//				
//						DP_Cal_Value_C[DP2] = (int16_t)((RealDpressure[DP2] - DP_Cal_float_Value_F[DP2])*10.0);
//						//DP_Cal_Value_C[DP2] = RealDpressure[DP2]*10.0;
//						WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],sizeof(DP_Cal_Value_C[DP2]));
//						DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
//						
//					break;
//					
//					case 2:
//				
//						DP_Cal_Value_C[DP3] = (int16_t)((RealDpressure[DP3] - DP_Cal_float_Value_F[DP3])*10.0);
//						//DP_Cal_Value_C[DP3] = RealDpressure[DP3]*10.0;
//						WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],sizeof(DP_Cal_Value_C[DP3]));
//						DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
//
//					break;
//				}
				
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
			if(restoreFactoryCalibrationTimer > KEY_HOLD_10SEC)
			{
				restoreFactoryCalibrationTimer=0;
			
				DP_Cal_Value_C[DP1]=0;
				DP_Cal_float_Value_C[DP1] = 0.0;
				WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],sizeof(DP_Cal_Value_C[DP1]));
				
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				DP_Cal_Value_C[DP2]=0;
				DP_Cal_float_Value_C[DP2] = 0.0;
				WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],sizeof(DP_Cal_Value_C[DP2]));
				
				DP_Cal_Value_C[DP3]=0;
				DP_Cal_float_Value_C[DP3] = 0.0;
				WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],sizeof(DP_Cal_Value_C[DP3]));
				#else
				TM_Cal_Value_C=0;
				TM_Cal_float_Value_C = 0.0;
				WriteEEPROMData(TM_CAL_VAL_C_ADDR,(uint8_t*)&TM_Cal_Value_C,sizeof(TM_Cal_Value_C));
				
				RH_Cal_Value_C=0;
				RH_Cal_float_Value_C = 0.0;
				WriteEEPROMData(RH_CAL_VAL_C_ADDR,(uint8_t*)&RH_Cal_Value_C,sizeof(RH_Cal_Value_C));
				#endif

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
				emProgpara=PROG_PAGE_DISP;
				
				Normal_para_cnt=0;
				bool_RTCChangeOccure=0;
				bool_UARTChanged=0;
				emLastProgpara=PROG_PAGE_DISP;
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
		if(key_up_count<KEY_COUNT_MAX)	key_up_count++;

		//Navigation steps once per press and then repeats slowly; only value editing
		//below uses the fast ladder.  Nothing follows this if/else chain, so returning
		//early simply skips this tick.
		if(KeyInNavMode() && !KeyNavStep(key_up_count))	return;

		dummy += KeyRepeatStep(key_up_count);
	
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
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						if((DP_Alrm_ON[DP1]) || (DP_Alrm_ON[DP2]) || (DP_Alrm_ON[DP3]))
						#else
						if((DP_Alrm_ON[DP1]) || (TM_Alrm_ON) || (RH_Alrm_ON))
						#endif
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
			
			#if BUILD_MINMAX_LOG
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
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
					#else
					if(gu16_parameterWord & ENABLE_TEMP) 
					{
						ReadMinMaxLog(LAST_TM_MIN_MAX_OFFSET,dispMinMaxMeanLogInd,&MinMaxMeanDayLogArr4Disp1[0],MIN_MAX_MEAN_LOG_SIZE);
						if(MinMaxMeanDayLogArr4Disp1[3]==0xFF)	//If no log then set Log to Zero
						{
							memset(MinMaxMeanDayLogArr4Disp1,0,MIN_MAX_MEAN_LOG_SIZE);
						}
					}
					if(gu16_parameterWord & ENABLE_RH) 
					{
						ReadMinMaxLog(LAST_RH_MIN_MAX_OFFSET,dispMinMaxMeanLogInd,&MinMaxMeanDayLogArr4Disp2[0],MIN_MAX_MEAN_LOG_SIZE);
						if(MinMaxMeanDayLogArr4Disp2[3]==0xFF)	//If no log then set Log to Zero
						{
							memset(MinMaxMeanDayLogArr4Disp2,0,MIN_MAX_MEAN_LOG_SIZE);
						}
					}
					#endif
					//--------------------------------------------------------------------------------------------
					if(gu16_parameterWord & ENABLE_DP1)
					{
						memcpy((uint8_t*)&ep1.currentEpochTime,&MinMaxMeanDayLogArr4Disp[0],4);
					}
					else
					{
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						if(gu16_parameterWord & ENABLE_DP2)
						{
							memcpy((uint8_t*)&ep1.currentEpochTime,&MinMaxMeanDayLogArr4Disp1[0],4);
						}
						else
						{
							memcpy((uint8_t*)&ep1.currentEpochTime,&MinMaxMeanDayLogArr4Disp2[0],4);
						}
						#else
						if(gu16_parameterWord & ENABLE_TEMP)
						{
							memcpy((uint8_t*)&ep1.currentEpochTime,&MinMaxMeanDayLogArr4Disp1[0],4);
						}
						else
						{
							memcpy((uint8_t*)&ep1.currentEpochTime,&MinMaxMeanDayLogArr4Disp2[0],4);
						}
						#endif
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
			
			#endif	// BUILD_MINMAX_LOG
			#if BUILD_MEAN24_LOG
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
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					if(gu16_parameterWord & ENABLE_DP2)
					{
						ReadMinMaxLog(DP2_CURR_24HR_MEAN_OFFSET,0,&MeanHrLogArr4Disp1[0],HOUR_MEAN_VALUE_SPACE);
					}
					if(gu16_parameterWord & ENABLE_DP3)
					{
						ReadMinMaxLog(DP3_CURR_24HR_MEAN_OFFSET,0,&MeanHrLogArr4Disp2[0],HOUR_MEAN_VALUE_SPACE);
					}
					#else
					if(gu16_parameterWord & ENABLE_TEMP)
					{
						ReadMinMaxLog(TM_CURR_24HR_MEAN_OFFSET,0,&MeanHrLogArr4Disp1[0],HOUR_MEAN_VALUE_SPACE);
					}
					if(gu16_parameterWord & ENABLE_RH)
					{
						ReadMinMaxLog(RH_CURR_24HR_MEAN_OFFSET,0,&MeanHrLogArr4Disp2[0],HOUR_MEAN_VALUE_SPACE);
					}
					#endif
				}
				
				memcpy(&tempfloat,&MeanHrLogArr4Disp[dispMinMaxMeanLogInd*4],4);
				memcpy(&tempfloat1,&MeanHrLogArr4Disp1[dispMinMaxMeanLogInd*4],4);
				#if (DEVICE_MODE==DP1_TEMP_RH_MODE)
				if(TM_Unit)
				{
					tempfloat1 = (tempfloat1 * 1.8) + 32.0;
				}
				#endif
				memcpy(&tempfloat2,&MeanHrLogArr4Disp2[dispMinMaxMeanLogInd*4],4);
				dispMinMaxMeanLogInd++;
			
			break;
			
			#endif	// BUILD_MEAN24_LOG
			case PROG_MODE:
			
				if(!PARA_SELECT_KEY)
				{
					//opstr("\r\nProg Mode + Up Key + Para_Select key pressed\r\n");
					emProgpara++;
					
					if((emProgpara==DP1_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_DP1))
					{
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						emProgpara=DP2_ALM_UP_ON_DISP;
						#else
						emProgpara=TM_ALM_UP_ON_DISP;
						#endif
					}
					
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					if((emProgpara==DP2_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_DP2))
					{
						emProgpara=DP3_ALM_UP_ON_DISP;
					}
					if((emProgpara==DP3_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_DP3))
					{
						emProgpara=RTC_HR_DISP;
					}
					#else
					if((emProgpara==TM_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_TEMP))
					{
						emProgpara=RH_ALM_UP_ON_DISP;
					}
					if((emProgpara==RH_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_RH))
					{
						emProgpara=RTC_HR_DISP;
					}
					#endif
					
					if(emProgpara>CAL_DISP)
					{
						emProgpara=DEVICE_ID_DISP;
					}
					
					dummy=0;
					
					emLastProgpara=emProgpara;
					//----------------------------------------------------------------------
					switch(emProgpara)
					{
						case DEVICE_ID_DISP:			dummy = DeviceID;					break;
						case DP1_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP1]; 		break;
						case DP1_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP1]; 		break;
						case DP1_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP1];  	break;
						case DP1_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP1];  		break;
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						case DP2_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP2]; 		break;
						case DP2_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP2]; 		break;
						case DP2_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP2];  	break;
						case DP2_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP2];  		break;
						case DP3_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP3]; 		break;
						case DP3_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP3]; 		break;
						case DP3_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP3];  	break;
						case DP3_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP3];  		break;
						#else
						case TM_ALM_UP_ON_DISP:			dummy = TM_Upper_Alm_ON;  		break;
						case TM_ALM_UP_OFF_DISP:		dummy = TM_Upper_Alm_OFF; 		break;
						case TM_ALM_LO_OFF_DISP:		dummy = TM_Lower_Alm_OFF; 		break;
						case TM_ALM_LO_ON_DISP:			dummy = TM_Lower_Alm_ON;  		break;
						case TM_UNIT_DISP:				dummy = TM_Unit;				break;
						case RH_ALM_UP_ON_DISP:			dummy = RH_Upper_Alm_ON;  		break;
						case RH_ALM_UP_OFF_DISP:		dummy = RH_Upper_Alm_OFF;  		break;
						case RH_ALM_LO_OFF_DISP:		dummy = RH_Lower_Alm_OFF;  		break;
						case RH_ALM_LO_ON_DISP:			dummy = RH_Lower_Alm_ON;  		break;
						#endif
						
						case RTC_HR_DISP:
							Temp_RTC_ARR[0] = rtc.hour;
							dummy = rtc.hour;
						break;
						case RTC_MN_DISP:
							Temp_RTC_ARR[1] = rtc.minute;
							dummy = rtc.minute;
						break;
						case RTC_DT_DISP:
							Temp_RTC_ARR[2] = rtc.day;
							dummy = rtc.day;
						break;
						case RTC_MH_DISP:
							Temp_RTC_ARR[3] = rtc.month;
							dummy = rtc.month;
						break;
						case RTC_YR_DISP:
							Temp_RTC_ARR[4] = (uint8_t)rtc.year;
							dummy = rtc.year;
						break;
						case BUZ_ON_DISP:	dummy = Buzzer_ON_Time;	  		break;
						case BUZ_OFF_DISP:	dummy = Buzzer_OFF_Time;	  	break;
						case UART_BDT_DISP:	dummy = UART_BaudRate;		  	break;
						case CAL_DISP:	dummy = 0;							break;
						default: break;
					}
				}
				else
				{
					progTimeout=60;
					
					switch(emProgpara)
					{
						case DEVICE_ID_DISP:		if(dummy>250)dummy=250;										break;
						case DP1_ALM_UP_ON_DISP:		if(dummy>DP_ALM_LIMIT_MIN*10.0)dummy=DP_ALM_LIMIT_MIN*10.0;		break;
						case DP1_ALM_UP_OFF_DISP:		if(dummy>DP_Upper_Alm_ON[DP1]) dummy=(DP_Upper_Alm_ON[DP1]-1);		break;
						case DP1_ALM_LO_OFF_DISP:		if(dummy>DP_Upper_Alm_OFF[DP1])dummy=(DP_Upper_Alm_OFF[DP1]-1);		break;
						case DP1_ALM_LO_ON_DISP:		if(dummy>DP_Lower_Alm_OFF[DP1])dummy=(DP_Lower_Alm_OFF[DP1]-1);		break;
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						case DP2_ALM_UP_ON_DISP:		if(dummy>DP_ALM_LIMIT_MIN*10.0)dummy=DP_ALM_LIMIT_MIN*10.0;		break;
						case DP2_ALM_UP_OFF_DISP:		if(dummy>DP_Upper_Alm_ON[DP2]) dummy=(DP_Upper_Alm_ON[DP2]-1);		break;
						case DP2_ALM_LO_OFF_DISP:		if(dummy>DP_Upper_Alm_OFF[DP2])dummy=(DP_Upper_Alm_OFF[DP2]-1);		break;
						case DP2_ALM_LO_ON_DISP:		if(dummy>DP_Lower_Alm_OFF[DP2])dummy=(DP_Lower_Alm_OFF[DP2]-1);		break;
						case DP3_ALM_UP_ON_DISP:	if(dummy>DP_ALM_LIMIT_MIN*10.0)dummy=DP_ALM_LIMIT_MIN*10.0;		break;
						case DP3_ALM_UP_OFF_DISP:	if(dummy>DP_Upper_Alm_ON[DP3]) dummy=(DP_Upper_Alm_ON[DP3]-1);		break;
						case DP3_ALM_LO_OFF_DISP:	if(dummy>DP_Upper_Alm_OFF[DP3])dummy=(DP_Upper_Alm_OFF[DP3]-1);		break;
						case DP3_ALM_LO_ON_DISP:	if(dummy>DP_Lower_Alm_OFF[DP3])dummy=(DP_Lower_Alm_OFF[DP3]-1);		break;
						#else
						case TM_ALM_UP_ON_DISP:
							if(!TM_Unit)
							{
								if(dummy>DEFAUT_TEMP_C_MIN*10.0)dummy=DEFAUT_TEMP_C_MIN*10.0;
							}
							else
							{
								if(dummy>DEFAUT_TEMP_F_MIN*10.0)dummy=DEFAUT_TEMP_F_MIN*10.0;
							}
						break;
						case TM_ALM_UP_OFF_DISP:		if(dummy>TM_Upper_Alm_ON)dummy=(TM_Upper_Alm_ON-1);		break;
						case TM_ALM_LO_OFF_DISP:	if(dummy>TM_Upper_Alm_OFF)dummy=(TM_Upper_Alm_OFF-1);	break;
						case TM_ALM_LO_ON_DISP:	if(dummy>TM_Lower_Alm_OFF)dummy=(TM_Lower_Alm_OFF-1);		break;
						case TM_UNIT_DISP:	dummy=1;														break;
						case RH_ALM_UP_ON_DISP:	if(dummy>DEFAUT_RH_MIN*10.0)dummy=DEFAUT_RH_MIN*10.0;		break;
						case RH_ALM_UP_OFF_DISP:	if(dummy>RH_Upper_Alm_ON)dummy=(RH_Upper_Alm_ON-1);		break;
						case RH_ALM_LO_OFF_DISP:	if(dummy>RH_Upper_Alm_OFF)dummy=(RH_Upper_Alm_OFF-1);	break;
						case RH_ALM_LO_ON_DISP:	if(dummy>RH_Lower_Alm_OFF)dummy=(RH_Lower_Alm_OFF-1);		break;
						#endif
						case RTC_HR_DISP: 	if(dummy>23)dummy=23; 	bool_RTCChangeOccure = 1;			break;
						case RTC_MN_DISP: 	if(dummy>59)dummy=59; 	bool_RTCChangeOccure = 1;			break;
						case RTC_DT_DISP: 	if(dummy>31)dummy=31; 	bool_RTCChangeOccure = 1;			break;
						case RTC_MH_DISP: 	if(dummy>12)dummy=12; 	bool_RTCChangeOccure = 1;			break;
						case RTC_YR_DISP: 	if(dummy>99)dummy=99;	bool_RTCChangeOccure = 1;			break;
						case BUZ_ON_DISP:	if(dummy>60)dummy=60;										break;
						case BUZ_OFF_DISP:	if(dummy>960)dummy=960;										break;
						case UART_BDT_DISP:	if(dummy>9)dummy=9;		bool_UARTChanged = 1;				break;
						case CAL_DISP:		if(dummy>999)dummy=999;										break;
						default: break;
					}
				}
			break;
		}
	}
	else if(!DN_KEY)
	{
		if(key_dn_count<KEY_COUNT_MAX)	key_dn_count++;

		//Navigation steps once per press and then repeats slowly; only value editing
		//below uses the fast ladder.  Nothing follows this if/else chain, so returning
		//early simply skips this tick.
		if(KeyInNavMode() && !KeyNavStep(key_dn_count))	return;

		dummy -= KeyRepeatStep(key_dn_count);
	
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
			
				/*
				progTimeout=60;
				autoCal_para_cnt=1;
				*/
				
			break;
			
			#if BUILD_MINMAX_LOG
			case MIN_MAX_MEAN_MODE:
						
			break;
			
			#endif	// BUILD_MINMAX_LOG
			#if BUILD_MEAN24_LOG
			case MEAN_HOUR_MODE:
			
			break;
			
			#endif	// BUILD_MEAN24_LOG
			case PROG_MODE:

				if(!PARA_SELECT_KEY)
				{
					//opstr("\r\nProg Mode + Down Key + Para_Select key pressed\r\n");
					if(emProgpara)emProgpara--;
					
					if(emProgpara==PROG_PAGE_DISP)emProgpara=CAL_DISP;
					
					if(emProgpara>CAL_DISP)
					{
						emProgpara=CAL_DISP;
						bool_cal_mode=0;
					}
					
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					if((emProgpara==DP3_ALM_LO_ON_DISP) && !(gu16_parameterWord & ENABLE_DP3))
					{
						emProgpara=DP2_ALM_LO_ON_DISP;
					}
					
					if((emProgpara==DP2_ALM_LO_ON_DISP) && !(gu16_parameterWord & ENABLE_DP2))
					{
						emProgpara=DP1_ALM_LO_ON_DISP;
					}
					#else
					if((emProgpara==RH_ALM_LO_ON_DISP) && !(gu16_parameterWord & ENABLE_RH))
					{
						emProgpara=TM_UNIT_DISP;
					}
					
					if((emProgpara==TM_UNIT_DISP) && !(gu16_parameterWord & ENABLE_TEMP))
					{
						emProgpara=DP1_ALM_LO_ON_DISP;
					}
					#endif
					
					if((emProgpara==DP1_ALM_LO_ON_DISP) && !(gu16_parameterWord & ENABLE_DP1))
					{
						emProgpara=DEVICE_ID_DISP;
					}
					
					dummy=0;
					
					emLastProgpara=emProgpara;
					//----------------------------------------------------------------------
					switch(emProgpara)
					{
						case DEVICE_ID_DISP:			dummy = DeviceID;					break;
						case DP1_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP1]; 		break;
						case DP1_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP1]; 		break;
						case DP1_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP1];  	break;
						case DP1_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP1];  		break;
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						case DP2_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP2]; 		break;
						case DP2_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP2]; 		break;
						case DP2_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP2];  	break;
						case DP2_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP2];  		break;
						case DP3_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP3]; 		break;
						case DP3_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP3]; 		break;
						case DP3_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP3];  	break;
						case DP3_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP3];  		break;
						#else
						case TM_ALM_UP_ON_DISP:			dummy = TM_Upper_Alm_ON;  		break;
						case TM_ALM_UP_OFF_DISP:		dummy = TM_Upper_Alm_OFF; 		break;
						case TM_ALM_LO_OFF_DISP:		dummy = TM_Lower_Alm_OFF; 		break;
						case TM_ALM_LO_ON_DISP:			dummy = TM_Lower_Alm_ON;  		break;
						case TM_UNIT_DISP:				dummy = TM_Unit;				break;
						case RH_ALM_UP_ON_DISP:			dummy = RH_Upper_Alm_ON;  		break;
						case RH_ALM_UP_OFF_DISP:		dummy = RH_Upper_Alm_OFF;  		break;
						case RH_ALM_LO_OFF_DISP:		dummy = RH_Lower_Alm_OFF;  		break;
						case RH_ALM_LO_ON_DISP:			dummy = RH_Lower_Alm_ON;  		break;
						#endif
						
						case RTC_HR_DISP:
							Temp_RTC_ARR[0] = rtc.hour;
							dummy = rtc.hour;
						break;
						case RTC_MN_DISP:
							Temp_RTC_ARR[1] = rtc.minute;
							dummy = rtc.minute;
						break;
						case RTC_DT_DISP:
							Temp_RTC_ARR[2] = rtc.day;
							dummy = rtc.day;
						break;
						case RTC_MH_DISP:
							Temp_RTC_ARR[3] = rtc.month;
							dummy = rtc.month;
						break;
						case RTC_YR_DISP:
							Temp_RTC_ARR[4] = (uint8_t)rtc.year;
							dummy = rtc.year;
						break;
						case BUZ_ON_DISP:	dummy = Buzzer_ON_Time;	  		break;
						case BUZ_OFF_DISP:	dummy = Buzzer_OFF_Time;	  	break;
						case UART_BDT_DISP:	dummy = UART_BaudRate;		  	break;
						case CAL_DISP:	dummy = 0;							break;
						default: break;
					}
				}
				else
				{
					progTimeout=60;
					
					switch(emProgpara)
					{
						case DEVICE_ID_DISP:		if(dummy<1)dummy=1;										break;
						case DP1_ALM_UP_ON_DISP:		if(dummy<DP_Upper_Alm_OFF[DP1])dummy=(DP_Upper_Alm_OFF[DP1]+1);	break;
						case DP1_ALM_UP_OFF_DISP:		if(dummy<DP_Lower_Alm_OFF[DP1])dummy=(DP_Lower_Alm_OFF[DP1]+1);	break;
						case DP1_ALM_LO_OFF_DISP:		if(dummy<DP_Lower_Alm_ON[DP1])dummy=(DP_Lower_Alm_ON[DP1]+1);	break;
						case DP1_ALM_LO_ON_DISP:		if(dummy<DP_ALM_LIMIT_MAX*10.0)dummy=DP_ALM_LIMIT_MAX*10.0;	break;
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						case DP2_ALM_UP_ON_DISP:		if(dummy<DP_Upper_Alm_OFF[DP2])dummy=(DP_Upper_Alm_OFF[DP2]+1);	break;
						case DP2_ALM_UP_OFF_DISP:		if(dummy<DP_Lower_Alm_OFF[DP2])dummy=(DP_Lower_Alm_OFF[DP2]+1);	break;
						case DP2_ALM_LO_OFF_DISP:		if(dummy<DP_Lower_Alm_ON[DP2])dummy=(DP_Lower_Alm_ON[DP2]+1);	break;
						case DP2_ALM_LO_ON_DISP:		if(dummy<DP_ALM_LIMIT_MAX*10.0)dummy=DP_ALM_LIMIT_MAX*10.0;	break;
						case DP3_ALM_UP_ON_DISP:	if(dummy<DP_Upper_Alm_OFF[DP3])dummy=(DP_Upper_Alm_OFF[DP3]+1);	break;
						case DP3_ALM_UP_OFF_DISP:	if(dummy<DP_Lower_Alm_OFF[DP3])dummy=(DP_Lower_Alm_OFF[DP3]+1);	break;
						case DP3_ALM_LO_OFF_DISP:	if(dummy<DP_Lower_Alm_ON[DP3])dummy=(DP_Lower_Alm_ON[DP3]+1);	break;
						case DP3_ALM_LO_ON_DISP:	if(dummy<DP_ALM_LIMIT_MAX*10.0)dummy=DP_ALM_LIMIT_MAX*10.0;	break;
						#else
						case TM_ALM_UP_ON_DISP:	if(dummy<TM_Upper_Alm_OFF)dummy=TM_Upper_Alm_OFF;		break;
						case TM_ALM_UP_OFF_DISP:	if(dummy<TM_Lower_Alm_OFF)dummy=TM_Lower_Alm_OFF;		break;
						case TM_ALM_LO_OFF_DISP:	if(dummy<TM_Lower_Alm_ON)dummy=TM_Lower_Alm_ON;			break;
						case TM_ALM_LO_ON_DISP:	if(dummy<DEFAUT_TEMP_C_MAX*10.0)dummy=DEFAUT_TEMP_C_MAX*10.0;		break;
						case TM_UNIT_DISP:	dummy=0;												break;
						case RH_ALM_UP_ON_DISP:	if(dummy<RH_Upper_Alm_OFF)dummy=RH_Upper_Alm_OFF;		break;	
						case RH_ALM_UP_OFF_DISP:	if(dummy<RH_Lower_Alm_OFF)dummy=RH_Lower_Alm_OFF;		break;	
						case RH_ALM_LO_OFF_DISP:	if(dummy<RH_Lower_Alm_ON)dummy=RH_Lower_Alm_ON;			break;
						case RH_ALM_LO_ON_DISP:	if(dummy<0)dummy=DEFAUT_RH_MAX*10.0;					break;	
						#endif
						case RTC_HR_DISP: 	if(dummy<0)dummy=0; 	bool_RTCChangeOccure = 1;			break;
						case RTC_MN_DISP: 	if(dummy<0)dummy=0; 	bool_RTCChangeOccure = 1;			break;
						case RTC_DT_DISP: 	if(dummy<1)dummy=1; 	bool_RTCChangeOccure = 1;			break;
						case RTC_MH_DISP: 	if(dummy<1)dummy=1; 	bool_RTCChangeOccure = 1;			break;
						case RTC_YR_DISP: 	if(dummy<0)dummy=0;		bool_RTCChangeOccure = 1;			break;
						case BUZ_ON_DISP:	if(dummy<0)dummy=0;										break;
						case BUZ_OFF_DISP:	if(dummy<0)dummy=0;										break;
						case UART_BDT_DISP:	if(dummy<3)dummy=3;		bool_UARTChanged = 1;			break;
						case CAL_DISP:	if(dummy<0)dummy=0;										break;
						default: break;
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
			if(DPAutoCalModeTimer > KEY_HOLD_10SEC)
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
				emProgpara=PROG_PAGE_DISP;
				Normal_para_cnt=0;
				b.RTCChangeOccure=0;
				b.UARTChanged=0;
				emLastProgpara=PROG_PAGE_DISP;
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
							#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
							if((DP_Alrm_ON[DP1]) || (DP_Alrm_ON[DP2]) || (DP_Alrm_ON[DP3]))
							#else
							if((DP_Alrm_ON[DP1]) || (TM_Alrm_ON) || (RH_Alrm_ON))
							#endif							
							{
								gu8_SetACKPwd=1;
								dummy1=0;
							}
						}
						else if(gu8_SetACKPwd==1)
						{
							#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
							if((DP_Alrm_ON[DP1]) || (DP_Alrm_ON[DP2]) || (DP_Alrm_ON[DP3]))
							#else
							if((DP_Alrm_ON[DP1]) || (TM_Alrm_ON) || (RH_Alrm_ON))
							#endif
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
					
					switch(emLastProgpara)
					{
						case DEVICE_ID_DISP:
							if(DeviceID != dummy)
							{
								DeviceID = dummy;
								gu8_groupID = ((DeviceID - 1)/gu8_DeviceInGroup)+1;
								WriteEEPROMData(DEVICE_ID,&DeviceID,sizeof(DeviceID));
							}
						break;
						case DP1_ALM_UP_ON_DISP:
							if(DP_Upper_Alm_ON[DP1] != dummy)
							{
								DP_Upper_Alm_ON[DP1] = dummy;
								WriteEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],sizeof(DP_Upper_Alm_ON[DP1]));
							}
						break;
						case DP1_ALM_UP_OFF_DISP:
							if(DP_Upper_Alm_OFF[DP1] != dummy)
							{
								DP_Upper_Alm_OFF[DP1] = dummy;
								WriteEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],sizeof(DP_Upper_Alm_OFF[DP1]));
							}
						break;
						case DP1_ALM_LO_OFF_DISP:
							if(DP_Lower_Alm_OFF[DP1] != dummy)
							{
								DP_Lower_Alm_OFF[DP1] = dummy;
								WriteEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],sizeof(DP_Lower_Alm_OFF[DP1]));
							}
						break;
						case DP1_ALM_LO_ON_DISP:
							if(DP_Lower_Alm_ON[DP1] != dummy)
							{
								DP_Lower_Alm_ON[DP1] = dummy;
								WriteEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],sizeof(DP_Lower_Alm_ON[DP1]));
							}
						break;	
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						case DP2_ALM_UP_ON_DISP:
							if(DP_Upper_Alm_ON[DP2] != dummy)
							{
								DP_Upper_Alm_ON[DP2] = dummy;
								WriteEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],sizeof(DP_Upper_Alm_ON[DP2]));
							}
						break;
						case DP2_ALM_UP_OFF_DISP:
							if(DP_Upper_Alm_OFF[DP2] != dummy)
							{
								DP_Upper_Alm_OFF[DP2] = dummy;
								WriteEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],sizeof(DP_Upper_Alm_OFF[DP2]));
							}
						break;
						case DP2_ALM_LO_OFF_DISP:
							if(DP_Lower_Alm_OFF[DP2] != dummy)
							{
								DP_Lower_Alm_OFF[DP2] = dummy;
								WriteEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],sizeof(DP_Lower_Alm_OFF[DP2]));
							}
						break;
						case DP2_ALM_LO_ON_DISP:
							if(DP_Lower_Alm_ON[DP2] != dummy)
							{
								DP_Lower_Alm_ON[DP2] = dummy;
								WriteEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],sizeof(DP_Lower_Alm_ON[DP2]));
							}
						break;								
						case DP3_ALM_UP_ON_DISP:
							if(DP_Upper_Alm_ON[DP3] != dummy)
							{
								DP_Upper_Alm_ON[DP3] = dummy;
								WriteEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],sizeof(DP_Upper_Alm_ON[DP3]));
							}
						break;
						case DP3_ALM_UP_OFF_DISP:
							if(DP_Upper_Alm_OFF[DP3] != dummy)
							{
								DP_Upper_Alm_OFF[DP3] = dummy;
								WriteEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],sizeof(DP_Upper_Alm_OFF[DP3]));
							}
						break;
						case DP3_ALM_LO_OFF_DISP:
							if(DP_Lower_Alm_OFF[DP3] != dummy)
							{
								DP_Lower_Alm_OFF[DP3] = dummy;
								WriteEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],sizeof(DP_Lower_Alm_OFF[DP3]));
							}
						break;
						case DP3_ALM_LO_ON_DISP:
							if(DP_Lower_Alm_ON[DP3] != dummy)
							{
								DP_Lower_Alm_ON[DP3] = dummy;
								WriteEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],sizeof(DP_Lower_Alm_ON[DP3]));
							}
						break;
						#else
						case TM_ALM_UP_ON_DISP:
							if(TM_Upper_Alm_ON != dummy)
							{
								TM_Upper_Alm_ON = dummy;
								WriteEEPROMData(TEMP_UP_ALM_ON,(uint8_t*)&TM_Upper_Alm_ON,sizeof(TM_Upper_Alm_ON));
							}
						break;
						case TM_ALM_UP_OFF_DISP:
							if(TM_Upper_Alm_OFF != dummy)
							{
								TM_Upper_Alm_OFF = dummy;
								WriteEEPROMData(TEMP_UP_ALM_OFF,(uint8_t*)&TM_Upper_Alm_OFF,sizeof(TM_Upper_Alm_OFF));
							}
						break;
						case TM_ALM_LO_OFF_DISP:
							if(TM_Lower_Alm_OFF != dummy)
							{
								TM_Lower_Alm_OFF = dummy;
								WriteEEPROMData(TEMP_LO_ALM_OFF,(uint8_t*)&TM_Lower_Alm_OFF,sizeof(TM_Lower_Alm_OFF));
							}
						break;
						case TM_ALM_LO_ON_DISP:
							if(TM_Lower_Alm_ON != dummy)
							{
								TM_Lower_Alm_ON = dummy;
								WriteEEPROMData(TEMP_LO_ALM_ON,(uint8_t*)&TM_Lower_Alm_ON,sizeof(TM_Lower_Alm_ON));
							}
						break;
						case TM_UNIT_DISP:
							if(TM_Unit != dummy)
							{
								TM_Unit = dummy;
								TMUnitChange();
							}
						break;
						case RH_ALM_UP_ON_DISP:
							if(RH_Upper_Alm_ON != dummy)
							{
								RH_Upper_Alm_ON = dummy;
								WriteEEPROMData(RH_UP_ALM_ON,(uint8_t*)&RH_Upper_Alm_ON,sizeof(RH_Upper_Alm_ON));
							}
						break;
						case RH_ALM_UP_OFF_DISP:
							if(RH_Upper_Alm_OFF != dummy)
							{
								RH_Upper_Alm_OFF = dummy;
								WriteEEPROMData(RH_UP_ALM_OFF,(uint8_t*)&RH_Upper_Alm_OFF,sizeof(RH_Upper_Alm_OFF));
							}
						break;
						case RH_ALM_LO_OFF_DISP:
							if(RH_Lower_Alm_OFF != dummy)
							{
								RH_Lower_Alm_OFF = dummy;
								WriteEEPROMData(RH_LO_ALM_OFF,(uint8_t*)&RH_Lower_Alm_OFF,sizeof(RH_Lower_Alm_OFF));
							}
						break;
						case RH_ALM_LO_ON_DISP:
							if(RH_Lower_Alm_ON != dummy)
							{
								RH_Lower_Alm_ON = dummy;
								WriteEEPROMData(RH_LO_ALM_ON,(uint8_t*)&RH_Lower_Alm_ON,sizeof(RH_Lower_Alm_ON));
							}
						break;
						#endif
						case RTC_HR_DISP:
							if(Temp_RTC_ARR[0] != dummy)
							{
								Temp_RTC_ARR[0] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case RTC_MN_DISP:
							if(Temp_RTC_ARR[1] != dummy)
							{
								Temp_RTC_ARR[1] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case RTC_DT_DISP:
							if(Temp_RTC_ARR[2] != dummy)
							{
								Temp_RTC_ARR[2] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case RTC_MH_DISP:
							if(Temp_RTC_ARR[3] != dummy)
							{
								Temp_RTC_ARR[3] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case RTC_YR_DISP:
							if(Temp_RTC_ARR[4] != dummy)
							{
								Temp_RTC_ARR[4] = dummy;
								bool_RTCChangeOccure=1;
							}
						break;
						case BUZ_ON_DISP:
							if(Buzzer_ON_Time != dummy)
							{
								Buzzer_ON_Time = dummy;
								WriteEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,sizeof(Buzzer_ON_Time));
							
								if(!Buzzer_ON_Time)
								{
									//b.buzzerStart=NO;
									BUZZER_OFF;
									buzzerOnTime=0;
									buzzerOffTime=0;
								}
								else
								{
									if((bool_buzzerStart==YES) && BUZZER_ENABLED())
									{
										buzzerOnTime=BUZZER_ON_PERIOD();
										buzzerOffTime=0;
										BUZZER_ON;
									}
								}
							}
						break;
						case BUZ_OFF_DISP:
							if(Buzzer_OFF_Time != dummy)
							{
								Buzzer_OFF_Time = dummy;
								WriteEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,sizeof(Buzzer_OFF_Time));
							}
						break;
						case UART_BDT_DISP:
							if(UART_BaudRate != dummy)
							{
								UART_BaudRate = dummy;
								WriteEEPROMData(UART_BAUDRATE,&UART_BaudRate,sizeof(UART_BaudRate));
							}
						break;
						default: break;
					}
					
					if(bool_RTCChangeOccure==1)
					{
						RTC_data[0] = 0x00;	// enable oscillator (bit 7=0)
						RTC_data[1] = HEX2BCD(Temp_RTC_ARR[1]);	// minute = 59
						RTC_data[2] = HEX2BCD(Temp_RTC_ARR[0]);	// hour = 05 ,24-hour mode(bit 6=0)
						RTC_data[3] = HEX2BCD(Temp_RTC_ARR[2]);	// date = 30
						RTC_data[4] = HEX2BCD(Temp_RTC_ARR[3]);	// month = december
						RTC_data[5] = HEX2BCD(Temp_RTC_ARR[4]);	// year = 11 or 2011
						
						Write_byte_PCF8563(RTC_CNTRL1_ADDR,0);	//clear STOP: oscillator must run
						Write_byte_PCF8563(RTC_TIMESEC_REG,RTC_data[0]);
						Write_byte_PCF8563(RTC_TIMEMIN_REG,RTC_data[1]);
						Write_byte_PCF8563(RTC_TIMEHOUR_REG,RTC_data[2]);
						Write_byte_PCF8563(RTC_DATE_DATE_REG,RTC_data[3]);
						Write_byte_PCF8563(RTC_DATE_MONTH_REG,RTC_data[4]);
						Write_byte_PCF8563(RTC_DATE_YEAR_REG,RTC_data[5]);
				
						rtc2.day = Temp_RTC_ARR[2];
						rtc2.month = Temp_RTC_ARR[3];
						rtc2.year = Temp_RTC_ARR[4];
						rtc2.year += 2000;	
						rtc2.hour = Temp_RTC_ARR[0];
						rtc2.minute = Temp_RTC_ARR[1];
						rtc2.second = 0;
					
						ep1.currentEpochTime = get_epoch_time(rtc2);
						
						//If SetDate is greater than current date then set it otherwise discard it
						if(ep1.currentEpochTime >= ep.currentEpochTime)
						{		
							RTCSetFlag=1;
							WriteEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
						}
						
						bool_RTCChangeOccure=0;
					}
					
					if(bool_UARTChanged==1)
					{
						UART_Configure(UART_BaudRate);
						bool_UARTChanged=0;
					}
					
					//sei();			//Global Interrupt Enable
					//----------------------------------------------------------------------
					emProgpara++;
					
					if((emProgpara==DP1_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_DP1))
					{
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						emProgpara=DP2_ALM_UP_ON_DISP;
						#else
						emProgpara=TM_ALM_UP_ON_DISP;
						#endif
					}
					
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					if((emProgpara==DP2_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_DP2))
					{
						emProgpara=DP3_ALM_UP_ON_DISP;
					}
					if((emProgpara==DP3_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_DP3))
					{
						emProgpara=RTC_HR_DISP;
					}
					#else
					if((emProgpara==TM_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_TEMP))
					{
						emProgpara=RH_ALM_UP_ON_DISP;
					}
					if((emProgpara==RH_ALM_UP_ON_DISP) && !(gu16_parameterWord & ENABLE_RH))
					{
						emProgpara=RTC_HR_DISP;
					}
					#endif
					
					if(emProgpara>CAL_DISP)
					{
						emProgpara=DEVICE_ID_DISP;
					}
					
					dummy=0;
					
					emLastProgpara=emProgpara;
					//----------------------------------------------------------------------
					switch(emProgpara)
					{
						case DEVICE_ID_DISP:			dummy = DeviceID;					break;
						case DP1_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP1]; 		break;
						case DP1_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP1]; 		break;
						case DP1_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP1];  	break;
						case DP1_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP1];  		break;
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						case DP2_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP2]; 		break;
						case DP2_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP2]; 		break;
						case DP2_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP2];  	break;
						case DP2_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP2];  		break;
						case DP3_ALM_UP_ON_DISP:		dummy = DP_Upper_Alm_ON[DP3]; 		break;
						case DP3_ALM_UP_OFF_DISP:		dummy = DP_Upper_Alm_OFF[DP3]; 		break;
						case DP3_ALM_LO_OFF_DISP:		dummy = DP_Lower_Alm_OFF[DP3];  	break;
						case DP3_ALM_LO_ON_DISP:		dummy = DP_Lower_Alm_ON[DP3];  		break;
						#else
						case TM_ALM_UP_ON_DISP:			dummy = TM_Upper_Alm_ON;  		break;
						case TM_ALM_UP_OFF_DISP:		dummy = TM_Upper_Alm_OFF; 		break;
						case TM_ALM_LO_OFF_DISP:		dummy = TM_Lower_Alm_OFF; 		break;
						case TM_ALM_LO_ON_DISP:			dummy = TM_Lower_Alm_ON;  		break;
						case TM_UNIT_DISP:				dummy = TM_Unit;				break;
						case RH_ALM_UP_ON_DISP:			dummy = RH_Upper_Alm_ON;  		break;
						case RH_ALM_UP_OFF_DISP:		dummy = RH_Upper_Alm_OFF;  		break;
						case RH_ALM_LO_OFF_DISP:		dummy = RH_Lower_Alm_OFF;  		break;
						case RH_ALM_LO_ON_DISP:			dummy = RH_Lower_Alm_ON;  		break;
						#endif
						
						case RTC_HR_DISP:
							Temp_RTC_ARR[0] = rtc.hour;
							dummy = rtc.hour;
						break;
						case RTC_MN_DISP:
							Temp_RTC_ARR[1] = rtc.minute;
							dummy = rtc.minute;
						break;
						case RTC_DT_DISP:
							Temp_RTC_ARR[2] = rtc.day;
							dummy = rtc.day;
						break;
						case RTC_MH_DISP:
							Temp_RTC_ARR[3] = rtc.month;
							dummy = rtc.month;
						break;
						case RTC_YR_DISP:
							Temp_RTC_ARR[4] = (uint8_t)rtc.year;
							dummy = rtc.year;
						break;
						case BUZ_ON_DISP:	dummy = Buzzer_ON_Time;	  		break;
						case BUZ_OFF_DISP:	dummy = Buzzer_OFF_Time;	  	break;
						case UART_BDT_DISP:	dummy = UART_BaudRate;		  	break;
						case CAL_DISP:	dummy = 0;							break;
						default: break;
					}
					
				break;
			}
		break;
		default:
		break;
	}
}

#endif

//Start the buzzer on the cadence belonging to `source`.  The state machine in
//SecondTick() reloads through BUZZER_ON_PERIOD()/BUZZER_OFF_PERIOD(), which read
//gu8_buzzerSource - so setting it here is all that picks the pattern.
void StartBuzzerFor(uint8_t source)
{
	//Switched off by the user - nothing to start.
	if(!BUZZER_ENABLED())	return;
	
	if(bool_buzzerStart==NO)
	{
		//Set everything up BEFORE arming bool_buzzerStart: BuzzerTick() runs from the
		//TIM1 ISR and keys off that flag, so arming last means it can never observe a
		//half-built state.
		gu8_buzzerSource=source;
		gu8_buzzerPulsesLeft=BUZZER_PULSES();
		
		buzzerOnTime=BUZZER_ON_PERIOD();
		buzzerOffTime=0;
		
		if(buzzerOnTime)
		{
			BUZZER_ON;
		}
		
		bool_buzzerStart=YES;
	}
}

//A real alarm keeps the user-configured Buzzer_ON_Time/Buzzer_OFF_Time cadence.
void StartBuzzer(void)
{
	StartBuzzerFor(BUZZER_SRC_ALARM);
}

//One 50 ms step of the near-alarm DISPLAY flash.  Same pattern as the chirp and
//built from the same constants, so the two stay in lockstep.
//
//Generated here rather than read off the buzzer on purpose: the sounder can be
//acknowledged or switched off entirely (Buzzer_ON_Time / Buzzer_OFF_Time = 0),
//and the visual warning has to survive both.
void NearBlinkTick(void)
{
	if(!gu8_nearAlrmActive)
	{
		//Nothing near - park the pattern so the next one starts from its first flash.
		gu8_nearBlinkOn=0;
		gu8_nearBlinkPulses=0;
		gu16_nearBlinkTimer=0;
		return;
	}
	
	if(gu16_nearBlinkTimer)
	{
		gu16_nearBlinkTimer--;
		if(gu16_nearBlinkTimer)	return;		//current phase still running
	}
	
	if(gu8_nearBlinkOn)
	{
		gu8_nearBlinkOn=0;
		
		if(gu8_nearBlinkPulses > 1)
		{
			//More flashes in this burst - short gap only.
			gu8_nearBlinkPulses--;
			gu16_nearBlinkTimer=BUZZER_MS_TO_TICKS(NEAR_BLINK_GAP_MS);
		}
		else
		{
			//Burst done - reload the count, then size the tail to match it.
			gu8_nearBlinkPulses=NEAR_PULSES();
			gu16_nearBlinkTimer=BUZZER_MS_TO_TICKS(NEAR_BLINK_TAIL_MS(gu8_nearBlinkPulses));
		}
	}
	else
	{
		gu8_nearBlinkOn=1;
		gu16_nearBlinkTimer=BUZZER_MS_TO_TICKS(NEAR_BLINK_ON_MS);
	}
}

//One 50 ms step of the buzzer cadence.  Called from the TIM1 ISR rather than
//whileTask(): a pass there takes far longer than a tick because it runs
//disp_value() every time -
//32 bytes over bit-banged I2C, about 85 ms.  Timing the buzzer off that counted
//loop passes rather than time and stretched a 20 s period to 34 s.
//
//Only the phase timing lives here.  The decision (which alarm wants the buzzer)
//and start/stop stay in main, so this touches nothing that is not already
//byte- or halfword-atomic on a Cortex-M0.
void BuzzerTick(void)
{
	if(bool_buzzerStart!=YES)	return;
	
	//Switched off underneath us - the setting can change while a beep is running.
	if(!BUZZER_ENABLED())
	{
		bool_buzzerStart=NO;
		BUZZER_OFF;
		buzzerOnTime=0;
		buzzerOffTime=0;
		return;
	}
	
	if(buzzerOnTime)
	{
		buzzerOnTime--;
		if(!buzzerOnTime)
		{
			BUZZER_OFF;
			
			if(gu8_buzzerPulsesLeft > 1)
			{
				//More chirps to come in this burst - short gap only.
				gu8_buzzerPulsesLeft--;
				buzzerOffTime=BUZZER_GAP_PERIOD();
			}
			else
			{
				//Burst finished.  Reload the count first so a change of escalation is
				//picked up, then size the tail to match it.
				gu8_buzzerPulsesLeft=BUZZER_PULSES();
				buzzerOffTime=BUZZER_OFF_PERIOD();
			}
		}
	}
	else if(buzzerOffTime)
	{
		buzzerOffTime--;
		if(!buzzerOffTime)
		{
			buzzerOnTime=BUZZER_ON_PERIOD();
			
			BUZZER_ON;
		}
	}
}

void StopBuzzer(void)
{
	//Disarm FIRST so the ISR stops stepping before the counters are cleared.
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
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	if(bool_DP_NC[DP2]) 	Buffer1[10] |= DP2_FAULTY;
	if(bool_DP_NC[DP3])		Buffer1[10] |= DP3_FAULTY;
	#else
	if(bool_RH_TEMP_NC)		Buffer1[10] |= RH_TEMP_FAULTY;
	#endif
	
	memcpy(&Buffer1[11],(uint8_t*)&Dpressure[DP1],4);
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	memcpy(&Buffer1[15],(uint8_t*)&Dpressure[DP2],4);
	memcpy(&Buffer1[19],(uint8_t*)&Dpressure[DP3],4);
	#else
	if(!TM_Unit) 
	{
		memcpy(&Buffer1[15],(unsigned char*)&temperatureC,4);
	}
	else 
	{
		memcpy(&Buffer1[15],(unsigned char*)&temperatureF,4);
	}
	memcpy(&Buffer1[19],(unsigned char*)&humidityRH,4);
	#endif
	
	memcpy(&Buffer1[23],(uint8_t*)&DP_Min[DP1],4);
	memcpy(&Buffer1[27],(uint8_t*)&DP_Max[DP1],4);
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	memcpy(&Buffer1[31],(uint8_t*)&DP_Min[DP2],4);
	memcpy(&Buffer1[35],(uint8_t*)&DP_Max[DP2],4);
	memcpy(&Buffer1[39],(uint8_t*)&DP_Min[DP3],4);
	memcpy(&Buffer1[43],(uint8_t*)&DP_Max[DP3],4);
	#else
	memcpy(&Buffer1[31],(unsigned char*)&TM_Min,4);
	memcpy(&Buffer1[35],(unsigned char*)&TM_Max,4);
	memcpy(&Buffer1[39],(unsigned char*)&RH_Min,4);
	memcpy(&Buffer1[43],(unsigned char*)&RH_Max,4);
	#endif
	
	
	if(gu16_parameterWord & ENABLE_DP1)
	{
		Buffer1[47] = DP_Alrm_ON[DP1];
	}
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	if(gu16_parameterWord & ENABLE_DP2)
	{
		Buffer1[48] = DP_Alrm_ON[DP2];
	}
	if(gu16_parameterWord & ENABLE_DP3)
	{
		Buffer1[49] = DP_Alrm_ON[DP3];
	}
	#else
	if(gu16_parameterWord & ENABLE_TEMP)
	{
		Buffer1[48] = TM_Alrm_ON;
		
		if(TM_Unit)
		{
			Buffer1[48] |= 0x80;
		}
	}
	if(gu16_parameterWord & ENABLE_RH)
	{
		Buffer1[49] = RH_Alrm_ON;
	}
	#endif
	
	Buffer1[50]=find_Checksum(50,&Buffer1[0]);
	
	SendToUART(&Buffer1[0],51);
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


//=========================================================================================
// Log-pointer persistence
//
// The regular- and 24-hour-log write pointers are saved after EVERY log record.
//
//   AT45DB321D - the DataFlash rewrites bytes in place, so the pointer is written straight
//                back to its slot.  The 100-slot arrays only advance on a ring wrap.
//
//   XM25QH128A - NOR cannot rewrite a byte without erasing its whole 4 KB sector, so
//                rewriting one slot per record would erase that sector ~2880 times a day
//                at 1 log/min and exhaust its ~100k endurance in about five weeks.
//                Instead each save APPENDS to the next slot of a dedicated sector, so it
//                lands in erased space and costs a single page program.  The sector is
//                erased only when its slots run out - once per 1024 (regular) or 2048
//                (24-hour) records, i.e. roughly one erase every 17 / 34 hours.
//                The live slot is never stored: at boot the sector is scanned for the
//                last programmed slot, which holds the newest pointer.
//=========================================================================================

void SaveCurrentLogInd(void)
{
	#if !BUILD_REGULAR_LOG
	//regular log not built - no pointer to keep
	#elif (DATAFLASH_PART == DATAFLASH_XM25QH128A)
	XM25_PtrSave(XM25_PTR_LOG,CurrentLogInd);
	#else
	WriteEEPROMData((CURR_LOG_IND + (CurrentLogIndReadLoc*4)),(uint8_t*)&CurrentLogInd,sizeof(CurrentLogInd));
	#endif
}

void SaveCurrentLog24Ind(void)
{
	#if !BUILD_LOG24_LOG
	//24 hour log not built - no pointer to keep
	#elif (DATAFLASH_PART == DATAFLASH_XM25QH128A)
	XM25_PtrSave(XM25_PTR_LOG24,CurrentLog24Ind);
	#else
	WriteEEPROMData((CURR_LOG24_IND+(CurrentLog24IndReadLoc*2)),(uint8_t*)&CurrentLog24Ind,sizeof(CurrentLog24Ind));
	#endif
}

//Wipe the stored pointer and restart from the first slot
void ResetCurrentLogInd(void)
{
	#if BUILD_REGULAR_LOG
	CurrentLogInd = 0;

	#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)
	XM25_PtrReset(XM25_PTR_LOG,CurrentLogInd);
	#else
	CurrentLogIndReadLoc = 0;
	WriteEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,sizeof(CurrentLogIndReadLoc));
	SaveCurrentLogInd();
	#endif
	#endif	// BUILD_REGULAR_LOG
}

void ResetCurrentLog24Ind(void)
{
	#if BUILD_LOG24_LOG
	CurrentLog24Ind = 0;

	#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)
	XM25_PtrReset(XM25_PTR_LOG24,CurrentLog24Ind);
	#else
	CurrentLog24IndReadLoc = 0;
	WriteEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,sizeof(CurrentLog24IndReadLoc));
	SaveCurrentLog24Ind();
	#endif
	#endif	// BUILD_LOG24_LOG
}

//Recover the newest pointer at boot
void LoadCurrentLogInd(void)
{
#if BUILD_REGULAR_LOG
	#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)
	CurrentLogInd = (uint32_t)XM25_PtrLoad(XM25_PTR_LOG);
	#else
	ReadEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,sizeof(CurrentLogIndReadLoc));
	if(CurrentLogIndReadLoc>=100)
	{
		CurrentLogIndReadLoc=0;
		WriteEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,sizeof(CurrentLogIndReadLoc));
	}
	ReadEEPROMData((CURR_LOG_IND+(CurrentLogIndReadLoc*4)),(uint8_t*)&CurrentLogInd,sizeof(CurrentLogInd));
	#endif

	if(CurrentLogInd>=TOTAL_REGULAR_LOG)
	{
		CurrentLogInd = 0;
		SaveCurrentLogInd();
	}
#else
	//regular log not built - nothing to recover
#endif	// BUILD_REGULAR_LOG
}

void LoadCurrentLog24Ind(void)
{
#if BUILD_LOG24_LOG
	#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)
	CurrentLog24Ind = (uint16_t)XM25_PtrLoad(XM25_PTR_LOG24);
	#else
	ReadEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,sizeof(CurrentLog24IndReadLoc));
	if(CurrentLog24IndReadLoc>=100)
	{
		CurrentLog24IndReadLoc = 0;
		WriteEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,sizeof(CurrentLog24IndReadLoc));
	}
	ReadEEPROMData((CURR_LOG24_IND+(CurrentLog24IndReadLoc*2)),(uint8_t*)&CurrentLog24Ind,sizeof(CurrentLog24Ind));
	#endif

	if(CurrentLog24Ind>=LAST_LOG24_ADDR)
	{
		CurrentLog24Ind = 0;
		SaveCurrentLog24Ind();
	}
#else
	//24 hour log not built - nothing to recover
#endif	// BUILD_LOG24_LOG
}

void EraseWholeFlash(void)
{
	#if BUILD_MINMAX_LOG
	if(gu16_parameterWord & ENABLE_M3LOG)
	{
		MinMaxMeanDayLogInd=0;
		WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
	}
	#endif
	
	//Reset Data Logging Parameter -------------------------------------------
	FlashOVFByte=0;
	WriteEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
	
	ResetCurrentLogInd();
	ResetCurrentLog24Ind();
	
	bool_DPLog[DP1]=0;
	LastDP_Alrm_ON[DP1]=0;
	WriteEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
	
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	bool_DPLog[DP2]=0;
	bool_DPLog[DP3]=0;
	LastDP_Alrm_ON[DP2]=0;
	WriteEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
	LastDP_Alrm_ON[DP3]=0;
	WriteEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
	#else
	bool_TMLog=0;
	bool_RHLog=0;
	LastTM_Alrm_ON=0;
	WriteEEPROMData(LAST_TM_ALRM_STAT,&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
	LastRH_Alrm_ON=0;
	WriteEEPROMData(LAST_RH_ALRM_STAT,&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
	#endif
	
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
	{
		uint32_t eraseUnit;

		for(eraseUnit=0; eraseUnit<DF_ERASE_UNITS; eraseUnit++)
		{
			DF_EraseUnit(eraseUnit);

			//Serve Watchdog Timer
			IWDG_ReloadCounter();
		}
	}
	
	//The erase above blanked the real-time store as well - re-open it and commit
	//the values EraseWholeFlash() has just reset, so a power cut before the next
	//flush cannot resurrect the old pointers.
	DF_RtReset();
	IWDG_ReloadCounter();
	
	#if BUILD_RAM_BUFFER
	RAMBufferLog=0;
	memset(&RAMBuffer[0],0,sizeof(RAMBuffer));
	#endif
}

void FillRamBuffer(uint8_t logtype,uint8_t userID,uint16_t password)
{
//The record it builds feeds the RAM reads and the 24 hour ring; with neither built
//there is nothing to fill.
#if (BUILD_RAM_BUFFER || BUILD_LOG24_LOG)
	if((gu16_parameterWord & ENABLE_RTC) && !DP_StartUpTimer && RTCSetFlag && bool_rtcValid	
		#if (DEVICE_MODE==DP1_TEMP_RH_MODE)
		&& !TMRH_StartUpTimer	
		#endif
	)
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
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		if(bool_DP_NC[DP2]) 	RAMBuffer[RAMBufferInd] |= DP2_FAULTY;
		if(bool_DP_NC[DP3])		RAMBuffer[RAMBufferInd] |= DP3_FAULTY;
		#else
		if(bool_RH_TEMP_NC)		RAMBuffer[RAMBufferInd] |= RH_TEMP_FAULTY;
		#endif
		RAMBufferInd++;
	
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&Dpressure[DP1],4);		RAMBufferInd += 4;
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&Dpressure[DP2],4);		RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&Dpressure[DP3],4);		RAMBufferInd += 4;
		#else
		if(!TM_Unit) 
		{
			memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&temperatureC,4);			RAMBufferInd += 4;
		}
		else
		{
			memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&temperatureF,4);			RAMBufferInd += 4;
		}
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&humidityRH,4);			RAMBufferInd += 4;
		
		#endif
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Min[DP1],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Max[DP1],4);			RAMBufferInd += 4;
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Min[DP2],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Max[DP2],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Min[DP3],4);			RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&DP_Max[DP3],4);			RAMBufferInd += 4;
		#else
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&TM_Min,4);				RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&TM_Max,4);				RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&RH_Min,4);				RAMBufferInd += 4;
		memcpy(&RAMBuffer[RAMBufferInd],(uint8_t*)&RH_Max,4);				RAMBufferInd += 4;
		#endif
		
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
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
		#else
		if(gu16_parameterWord & ENABLE_TEMP)
		{
			if(!TM_Alrm_ON) 
			{
				if(logtype==TM_ALM_RESTORE_LOG)
				{
					if(LastTM_Alrm_ON==UPPER_ALARM)		 RAMBuffer[RAMBufferInd] = 1;
					else if(LastTM_Alrm_ON==LOWER_ALARM) RAMBuffer[RAMBufferInd] = 2;
					else								 RAMBuffer[RAMBufferInd] = 0;
				}
				else
				{
					RAMBuffer[RAMBufferInd] = 0;
				}
			}
			else
			{
				if(LastTM_Alrm_ON==UPPER_ALARM)		 RAMBuffer[RAMBufferInd] = 1;
				else if(LastTM_Alrm_ON==LOWER_ALARM) RAMBuffer[RAMBufferInd] = 2;
				else								 RAMBuffer[RAMBufferInd] = 0;
			}
		
			if(TM_Unit)
			{
				RAMBuffer[RAMBufferInd] |= 0x80;
			}
		
			RAMBufferInd++;
		}
		else
		{
			RAMBuffer[RAMBufferInd++] = 0;
		}
	
		if(gu16_parameterWord & ENABLE_RH)
		{
			if(!RH_Alrm_ON) 
			{
				if(logtype==RH_ALM_RESTORE_LOG)
				{
					if(LastRH_Alrm_ON==UPPER_ALARM)		 RAMBuffer[RAMBufferInd++] = 1;
					else if(LastRH_Alrm_ON==LOWER_ALARM) RAMBuffer[RAMBufferInd++] = 2;
					else								 RAMBuffer[RAMBufferInd++] = 0;
				}
				else
				{
					RAMBuffer[RAMBufferInd++] = 0;
				}
			}
			else
			{
				if(LastRH_Alrm_ON==UPPER_ALARM)		 RAMBuffer[RAMBufferInd++] = 1;
				else if(LastRH_Alrm_ON==LOWER_ALARM) RAMBuffer[RAMBufferInd++] = 2;
				else								 RAMBuffer[RAMBufferInd++] = 0;
			}
		}
		else
		{
			RAMBuffer[RAMBufferInd++] = 0;
		}
		#endif
	
		RAMBufferLog++;
		if(RAMBufferLog >= RAM_LOG_SLOTS) RAMBufferLog=0;
		
		//Log Data to 24 Hour memory Location ------------------------------------------
		//Only the flash write is gated: RAMBuffer itself still backs the RAM_ALL_ID /
		//RAM_IND_ID reads, which do not depend on the 24 hour ring.
		#if BUILD_LOG24_LOG
		if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_LOG))
		{
			WriteLog(LAST_LOG24_ADDR_OFFSET,CurrentLog24Ind,&RAMBuffer[i],LOG_SIZE);
			
			//cli();
			CurrentLog24Ind++;
			if(CurrentLog24Ind>=LAST_LOG24_ADDR)
			{
				CurrentLog24Ind=0;
				
				#if (DATAFLASH_PART == DATAFLASH_AT45DB321D)
				CurrentLog24IndReadLoc++;
				if(CurrentLog24IndReadLoc>=100)
				{
					CurrentLog24IndReadLoc=0;
				}
				WriteEEPROMData(CURR_LOG24_IND_RDLC,&CurrentLog24IndReadLoc,sizeof(CurrentLog24IndReadLoc));
				#endif
			}
			SaveCurrentLog24Ind();
			//sei();
		}
		#endif	// BUILD_LOG24_LOG
	}
#else
	(void)logtype;	(void)userID;	(void)password;
#endif	// (BUILD_RAM_BUFFER || BUILD_LOG24_LOG)
}

void LogReading(uint8_t logtype,uint8_t userID,uint16_t password)
{
#if BUILD_REGULAR_LOG
	if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_LOG) && (gu16_parameterWord & ENABLE_RTC) && !DP_StartUpTimer && RTCSetFlag && bool_rtcValid
		#if (DEVICE_MODE==DP1_TEMP_RH_MODE)
		&& !TMRH_StartUpTimer
		#endif
	)
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
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		if(bool_DP_NC[DP2]) 	Buffer1[9] |= DP2_FAULTY;
		if(bool_DP_NC[DP3])		Buffer1[9] |= DP3_FAULTY;
		#else
		if(bool_RH_TEMP_NC)		Buffer1[9] |= RH_TEMP_FAULTY;
		#endif
		
		memcpy(&Buffer1[10],(uint8_t*)&Dpressure[DP1],4);
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		memcpy(&Buffer1[14],(uint8_t*)&Dpressure[DP2],4);
		memcpy(&Buffer1[18],(uint8_t*)&Dpressure[DP3],4);
		#else
		if(!TM_Unit)
		{
			memcpy(&Buffer1[14],(unsigned char*)&temperatureC,4);
		}
		else
		{
			memcpy(&Buffer1[14],(unsigned char*)&temperatureF,4);
		}
		memcpy(&Buffer1[18],(unsigned char*)&humidityRH,4);
		#endif
		memcpy(&Buffer1[22],(uint8_t*)&DP_Min[DP1],4);
		memcpy(&Buffer1[26],(uint8_t*)&DP_Max[DP1],4);
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		memcpy(&Buffer1[30],(uint8_t*)&DP_Min[DP2],4);
		memcpy(&Buffer1[34],(uint8_t*)&DP_Max[DP2],4);
		memcpy(&Buffer1[38],(uint8_t*)&DP_Min[DP3],4);
		memcpy(&Buffer1[42],(uint8_t*)&DP_Max[DP3],4);
		#else
		memcpy(&Buffer1[30],(unsigned char*)&TM_Min,4);
		memcpy(&Buffer1[34],(unsigned char*)&TM_Max,4);
		memcpy(&Buffer1[38],(unsigned char*)&RH_Min,4);
		memcpy(&Buffer1[42],(unsigned char*)&RH_Max,4);
		#endif
		
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
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
		#else
		if(gu16_parameterWord & ENABLE_TEMP)
		{
			if(!TM_Alrm_ON)
			{
				if(logtype==TM_ALM_RESTORE_LOG)
				{
					if(LastTM_Alrm_ON==UPPER_ALARM)		 Buffer1[47] = 1;
					else if(LastTM_Alrm_ON==LOWER_ALARM) Buffer1[47] = 2;
					else								 Buffer1[47] = 0;
				}
				else
				{
					Buffer1[47] = 0;
				}
			}
			else
			{
				if(LastTM_Alrm_ON==UPPER_ALARM)		 Buffer1[47] = 1;
				else if(LastTM_Alrm_ON==LOWER_ALARM) Buffer1[47] = 2;
				else								 Buffer1[47] = 0;
			}
			
			if(TM_Unit)
			{
				Buffer1[47] |= 0x80;
			}
		}
		else
		{
			Buffer1[47] = 0;
		}
	
		if(gu16_parameterWord & ENABLE_RH)
		{
			if(!RH_Alrm_ON)
			{
				if(logtype==RH_ALM_RESTORE_LOG)
				{
					if(LastRH_Alrm_ON==UPPER_ALARM)		 Buffer1[48] = 1;
					else if(LastRH_Alrm_ON==LOWER_ALARM) Buffer1[48] = 2;
					else							     Buffer1[48] = 0;
				}
				else
				{
					Buffer1[48] = 0;
				}
			}
			else
			{
				if(LastRH_Alrm_ON==UPPER_ALARM)		 Buffer1[48] = 1;
				else if(LastRH_Alrm_ON==LOWER_ALARM) Buffer1[48] = 2;
				else							     Buffer1[48] = 0;
			}
		}
		else
		{
			Buffer1[48] = 0;
		}
		#endif
		WriteLog(REGULAR_LOG_ADDR,CurrentLogInd,&Buffer1[0],LOG_SIZE);
	
		//cli();
	
		CurrentLogInd++;
		if(CurrentLogInd>=TOTAL_REGULAR_LOG)
		{
			CurrentLogInd=0;
			FlashOVFByte=1;
		
			WriteEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
		
			#if (DATAFLASH_PART == DATAFLASH_AT45DB321D)
			CurrentLogIndReadLoc++;
			if(CurrentLogIndReadLoc>=100)
			{
				CurrentLogIndReadLoc=0;
			}
			WriteEEPROMData(CURR_LOG_IND_RDLC,(uint8_t*)&CurrentLogIndReadLoc,sizeof(CurrentLogIndReadLoc));
			#endif
		}
		SaveCurrentLogInd();

		//sei();
	}
#else
	//regular log not built
	(void)logtype;
	(void)userID;
	(void)password;
#endif	// BUILD_REGULAR_LOG
}

void AutoSendDataResponse(uint8_t SrcPort)
{
	uint8_t j=0;
	
	Buffer1[0]=0xFD;
	Buffer1[1]=DeviceID;
	Buffer1[2]=PARA_WITH_ALM_READ_CMD;
	
	Buffer1[3]=0x00;
	if(bool_paraIdNotValid) 	Buffer1[3] |= INVALID_PARA;
	if(bool_DP_NC[DP1]) 		Buffer1[3] |= DP1_FAULTY;
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	if(bool_DP_NC[DP2]) 		Buffer1[3] |= DP2_FAULTY;
	if(bool_DP_NC[DP3]) 		Buffer1[3] |= DP3_FAULTY;
	#else
	if(bool_RH_TEMP_NC) 		Buffer1[3] |= RH_TEMP_FAULTY;
	#endif
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
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
	#else
	if(bool_RH_TEMP_NC) 			
	{
		Buffer1[j++] |= RH_TEMP_FAULTY;
	}
	else
	{
		Buffer1[j++]=0;
		
		//Fill Temp value
		if(!TM_Unit)
		{
			tempshort = temperatureC*100;
		}
		else
		{
			tempshort = temperatureF*100;
		}
		
		j += fillValue(&Buffer1[j],tempshort);
	}
	
	Buffer1[j++] = 0xEE;	//Field Separator
	
	if(bool_RH_TEMP_NC) 			
	{
		Buffer1[j++] |= RH_TEMP_FAULTY;
	}
	else
	{
		Buffer1[j++]=0;
		
		//Fill Temp value
		tempshort = humidityRH*100;
		j += fillValue(&Buffer1[j],tempshort);
	}
	
	Buffer1[j++] = 0xEE;	//Field Separator
	#endif
	
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
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
	#else
	if(gu16_parameterWord & ENABLE_TEMP)
	{
		if(!TM_Alrm_ON)
		{
			Buffer1[j++] = 0;
		}
		else
		{
			if(TM_Alrm_ON==UPPER_ALARM) Buffer1[j++] = 1;
			else						  Buffer1[j++] = 2;
		}
	}
	else
	{
		Buffer1[j++] = 0;
	}
	
	if(gu16_parameterWord & ENABLE_RH)
	{
		if(!RH_Alrm_ON)
		{
			Buffer1[j++] = 0;
		}
		else
		{
			if(RH_Alrm_ON==UPPER_ALARM) Buffer1[j++] = 1;
			else						  Buffer1[j++] = 2;
		}
	}
	else
	{
		Buffer1[j++] = 0;
	}
	#endif
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
}

void ServePCMsg(void)
{
	#ifdef DEBUG_RCV_CMD
		opstr("\r\nMsg OK");
	#endif
	uint8_t index=0,j=0,lu8_sendResponse=0,lu8_GroupDelay=0,m=0;
	
	if(gu8_IsCOMDisable)
	{
		if((RxBuffer[2]==PARA_WRITE_CMD) && (RxBuffer[3]==COM_CONTROL_ID))
		{
				
		}
		else
		{
			//The receive state was already cleared by the caller; clearing it here
			//would discard a frame that has arrived since.
			return;
		}
	}
	
	//Past the COM-disable gate, so this message is genuinely being served: flash
	//the logo to acknowledge it.  Retriggering restarts the full hold-off, so
	//back-to-back traffic keeps it blinking rather than stuttering.
	gu16_logoAckBlinkTimer = LOGO_ACK_BLINK_TICKS;
	
	bool_paraIdNotValid=0;
	
	if(RxBuffer[2]==PARA_WITH_ALM_READ_CMD)
	{	
		if(RxLen==6)
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
			if(bool_paraIdNotValid) 		RxBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			RxBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			RxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			RxBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			RxBuffer[3] |= RH_TEMP_FAULTY;
			#endif
			
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
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
			#else
			
			if(bool_RH_TEMP_NC) 			
			{
				RxBuffer[j++] |= RH_TEMP_FAULTY;
			}
			else
			{
				RxBuffer[j++]=0;
				
				//Fill Temp value
				if(!TM_Unit)
				{
					tempshort = temperatureC*100;
				}
				else
				{
					tempshort = temperatureF*100;
				}
				
				j += fillValue(&RxBuffer[j],tempshort);
			}
			
			RxBuffer[j++] = 0xEE;	//Field Separator
			
			if(bool_RH_TEMP_NC) 			
			{
				RxBuffer[j++] |= RH_TEMP_FAULTY;
			}
			else
			{
				RxBuffer[j++]=0;
				
				//Fill Temp value
				tempshort = humidityRH*100;
				j += fillValue(&RxBuffer[j],tempshort);
			}
			
			RxBuffer[j++] = 0xEE;	//Field Separator
			#endif
			
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
			
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
			#else
			if(gu16_parameterWord & ENABLE_TEMP)
			{
				if(!TM_Alrm_ON)
				{
					RxBuffer[j++] = 0;
				}
				else
				{
					if(TM_Alrm_ON==UPPER_ALARM) RxBuffer[j++] = 1;
					else						  RxBuffer[j++] = 2;
				}
			}
			else
			{
				RxBuffer[j++] = 0;
			}
			
			if(gu16_parameterWord & ENABLE_RH)
			{
				if(!RH_Alrm_ON)
				{
					RxBuffer[j++] = 0;
				}
				else
				{
					if(RH_Alrm_ON==UPPER_ALARM) RxBuffer[j++] = 1;
					else						  RxBuffer[j++] = 2;
				}
			}
			else
			{
				RxBuffer[j++] = 0;
			}
			#endif
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
	else if(RxBuffer[2]==READ_DP1_VALUE)
	{	
		RxBuffer[0]=0xFD;
		
		j=3;
		
		RxBuffer[j]=0x00;
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
		
		RxBuffer[j]=CalCRC(&RxBuffer[1],j-1);
		j++;
		
		RxBuffer[j++]=0xFC;
	
		SendToUART(RxBuffer,j);
	}
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	else if(RxBuffer[2]==READ_DP2_VALUE)
	{	
		RxBuffer[0]=0xFD;
		
		j=3;
		
		RxBuffer[j]=0x00;
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
		
		RxBuffer[j]=CalCRC(&RxBuffer[1],j-1);
		j++;
		
		RxBuffer[j++]=0xFC;
	
		SendToUART(RxBuffer,j);
	}
	else if(RxBuffer[2]==READ_DP3_VALUE)
	{	
		RxBuffer[0]=0xFD;
		
		j=3;
		
		RxBuffer[j]=0x00;
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
		
		RxBuffer[j]=CalCRC(&RxBuffer[1],j-1);
		j++;
		
		RxBuffer[j++]=0xFC;
	
		SendToUART(RxBuffer,j);
	}
	#else
	else if(RxBuffer[2]==READ_TEMP_VALUE)
	{	
		RxBuffer[0]=0xFD;
		
		j=3;
		
		RxBuffer[j]=0x00;
		if(bool_RH_TEMP_NC) 			
		{
			RxBuffer[j++] |= RH_TEMP_FAULTY;
		}
		else
		{
			RxBuffer[j++]=0;
			
			//Fill Temp value
			if(!TM_Unit)
			{
				tempshort = temperatureC*100;
			}
			else
			{
				tempshort = temperatureF*100;
			}
			
			j += fillValue(&RxBuffer[j],tempshort);
		}

		RxBuffer[j++] = 0xEE;	//Field Separator

		if(gu16_parameterWord & ENABLE_TEMP)
		{
			if(!TM_Alrm_ON)
			{
				RxBuffer[j++] = 0;
			}
			else
			{
				if(TM_Alrm_ON==UPPER_ALARM) RxBuffer[j++] = 1;
				else						  RxBuffer[j++] = 2;
			}
		}
		else
		{
			RxBuffer[j++] = 0;
		}

		RxBuffer[j]=CalCRC(&RxBuffer[1],j-1);
		j++;
		
		RxBuffer[j++]=0xFC;
	
		SendToUART(RxBuffer,j);
	}
	else if(RxBuffer[2]==READ_HUMIDITY_VALUE)
	{	
		RxBuffer[0]=0xFD;
		
		j=3;
		
		RxBuffer[j]=0x00;	
		if(bool_RH_TEMP_NC) 			
		{
			RxBuffer[j++] |= RH_TEMP_FAULTY;
		}
		else
		{
			RxBuffer[j++]=0;
			
			//Fill Temp value
			tempshort = humidityRH*100;
			j += fillValue(&RxBuffer[j],tempshort);
		}

		RxBuffer[j++] = 0xEE;	//Field Separator

		if(gu16_parameterWord & ENABLE_RH)
		{
			if(!RH_Alrm_ON)
			{
				RxBuffer[j++] = 0;
			}
			else
			{
				if(RH_Alrm_ON==UPPER_ALARM) RxBuffer[j++] = 1;
				else						  RxBuffer[j++] = 2;
			}
		}
		else
		{
			RxBuffer[j++] = 0;
		}
		
		RxBuffer[j]=CalCRC(&RxBuffer[1],j-1);
		j++;
		
		RxBuffer[j++]=0xFC;
	
		SendToUART(RxBuffer,j);
	}
	#endif
	else if(RxBuffer[2]==PARA_WRITE_CMD)
	{
		#ifdef DEBUG_RCV_CMD
			opstr("\r\nWrite Cmd");
		#endif
		
		switch(RxBuffer[3])
		{
			case ACK_PW_ID:	
			case ALM_ACK_ID:			tempshort = findValue(&RxBuffer[5],RxLen-7);	break;
			case SET_DPARA_PWD_ID:		break;
			case SRNO_ID:		  		break;
			case BRDSTR_ID:		  		break;
			case BRDSTP_ID:		  		break;
			case DATETIME_ID:	  		break;
			case EXT_FLASH_ERASE_ID:	break;
			case DFLT_RTC_ID:			break;
			case DFLT_CAL_ID:			break;
			case DP1CAL_ID:
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)	
			case DP2CAL_ID:
			case DP3CAL_ID:
			#else	
			case TMCAL_ID:
			case RHCAL_ID:
			#endif	
				tempshort = findValue(&RxBuffer[4],5);
				
			break;
			
			case DP_OFFSET_ID:
			case DP_SW_FACT_ID:
			
				tempshort = findValue(&RxBuffer[6],5);
				if(RxBuffer[5]=='-') tempshort *= (-1);	
				
			break;
			
			case DP_LIMIT_ID:
			
				tempshort = findValue(&RxBuffer[5],5);
			
			break;
			case DP_SLOT_OFFSET_ID:
			
				//Two index bytes ahead of the value: channel then slot
				tempshort = findValue(&RxBuffer[6],5);
			
			break;
			
			default: 
				tempshort = findValue(&RxBuffer[4],RxLen-6);
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
					WriteEEPROMData(DISP_PARA_SELECT,(uint8_t*)&gu16_parameterWord,sizeof(gu16_parameterWord));

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
					Write_byte_PCF8563(RTC_CNTRL1_ADDR,0);	//clear STOP: oscillator must run
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
				if((ss1<=(DP_ALM_LIMIT_MIN*10)) && (ss1>= DP_Upper_Alm_OFF[DP1]))
				{
					DP_Upper_Alm_ON[DP1]=ss1;
					WriteEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],sizeof(DP_Upper_Alm_ON[DP1])); 
				}
			break;
			case DP1UAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_ON[DP1]) && (ss1>= DP_Lower_Alm_OFF[DP1]))
				{
					DP_Upper_Alm_OFF[DP1]=ss1;
					WriteEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],sizeof(DP_Upper_Alm_OFF[DP1])); 
				}
			break;
			case DP1LAON_ID:	
				ss1 = (tempshort/10);
				if((ss1<=DP_Lower_Alm_OFF[DP1]) && (ss1>= (DP_ALM_LIMIT_MAX*10)))	
				{
					DP_Lower_Alm_ON[DP1]=ss1;
					WriteEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],sizeof(DP_Lower_Alm_ON[DP1])); 
				}
			break;
			case DP1LAOFF_ID:	
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_OFF[DP1]) && (ss1>= DP_Lower_Alm_ON[DP1]))	
				{
					DP_Lower_Alm_OFF[DP1]=ss1;
					WriteEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],sizeof(DP_Lower_Alm_OFF[DP1])); 
				}
			break;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			case DP2UAON_ID:
				ss1 = (tempshort/10);
				if((ss1<=(DP_ALM_LIMIT_MIN*10)) && (ss1>= DP_Upper_Alm_OFF[DP2]))
				{
					DP_Upper_Alm_ON[DP2]=ss1;
					WriteEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],sizeof(DP_Upper_Alm_ON[DP2]));
				}
			break;
			case DP2UAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_ON[DP2]) && (ss1>= DP_Lower_Alm_OFF[DP2]))
				{
					DP_Upper_Alm_OFF[DP2]=ss1;
					WriteEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],sizeof(DP_Upper_Alm_OFF[DP2]));
				}
			break;
			case DP2LAON_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Lower_Alm_OFF[DP2]) && (ss1>= (DP_ALM_LIMIT_MAX*10)))
				{
					DP_Lower_Alm_ON[DP2]=ss1;
					WriteEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],sizeof(DP_Lower_Alm_ON[DP2]));
				}
			break;
			case DP2LAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_OFF[DP2]) && (ss1>= DP_Lower_Alm_ON[DP2]))
				{
					DP_Lower_Alm_OFF[DP2]=ss1;
					WriteEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],sizeof(DP_Lower_Alm_OFF[DP2]));
				}
			break;
			
			case DP3UAON_ID:
				ss1 = (tempshort/10);
				if((ss1<=(DP_ALM_LIMIT_MIN*10)) && (ss1>= DP_Upper_Alm_OFF[DP3]))
				{
					DP_Upper_Alm_ON[DP3]=ss1;
					WriteEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],sizeof(DP_Upper_Alm_ON[DP3]));
				}
			break;
			case DP3UAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_ON[DP3]) && (ss1>= DP_Lower_Alm_OFF[DP3]))
				{
					DP_Upper_Alm_OFF[DP3]=ss1;
					WriteEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],sizeof(DP_Upper_Alm_OFF[DP3]));
				}
			break;
			case DP3LAON_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Lower_Alm_OFF[DP3]) && (ss1>= (DP_ALM_LIMIT_MAX*10)))
				{
					DP_Lower_Alm_ON[DP3]=ss1;
					WriteEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],sizeof(DP_Lower_Alm_ON[DP3]));
				}
			break;
			case DP3LAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=DP_Upper_Alm_OFF[DP3]) && (ss1>= DP_Lower_Alm_ON[DP3]))
				{
					DP_Lower_Alm_OFF[DP3]=ss1;
					WriteEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],sizeof(DP_Lower_Alm_OFF[DP3]));
				}
			break;
			#else
			case TMUAON_ID:	
				ss1 = (tempshort/10);
				if(!TM_Unit)
				{
					if((ss1<=1250) && (ss1>= TM_Upper_Alm_OFF))
					{
						TM_Upper_Alm_ON=ss1;
						WriteEEPROMData(TEMP_UP_ALM_ON,(uint8_t*)&TM_Upper_Alm_ON,sizeof(TM_Upper_Alm_ON));
					}
				}
				else
				{
					if((ss1<=2570) && (ss1>= TM_Upper_Alm_OFF))
					{
						TM_Upper_Alm_ON=ss1;
						WriteEEPROMData(TEMP_UP_ALM_ON,(uint8_t*)&TM_Upper_Alm_ON,sizeof(TM_Upper_Alm_ON));
					}
				}
			break;
			case TMUAOFF_ID:
				ss1 = (tempshort/10);
				if((ss1<=TM_Upper_Alm_ON) && (ss1>= TM_Lower_Alm_OFF))		
				{
					TM_Upper_Alm_OFF=ss1;
					WriteEEPROMData(TEMP_UP_ALM_OFF,(uint8_t*)&TM_Upper_Alm_OFF,sizeof(TM_Upper_Alm_OFF));
				}
			break;
			case TMLAON_ID:		
				ss1 = (tempshort/10);
				if((ss1<=TM_Lower_Alm_OFF) && (ss1>= -400))
				{
					TM_Lower_Alm_ON=ss1;
					WriteEEPROMData(TEMP_LO_ALM_ON,(uint8_t*)&TM_Lower_Alm_ON,sizeof(TM_Lower_Alm_ON));
				}
			break;
			case TMLAOFF_ID:	
				ss1 = (tempshort/10);
				if((ss1<=TM_Upper_Alm_OFF) && (ss1>= TM_Lower_Alm_ON))	
				{
					TM_Lower_Alm_OFF=ss1;
					WriteEEPROMData(TEMP_LO_ALM_OFF,(uint8_t*)&TM_Lower_Alm_OFF,sizeof(TM_Lower_Alm_OFF));
				}
			break;
			case RHUAON_ID:		
				ss1 = (tempshort/10);
				if((ss1<=1000) && (ss1>= RH_Upper_Alm_OFF))
				{
					RH_Upper_Alm_ON=ss1;
					WriteEEPROMData(RH_UP_ALM_ON,(uint8_t*)&RH_Upper_Alm_ON,sizeof(RH_Upper_Alm_ON));
				}
			break;
			case RHUAOFF_ID:	
				ss1 = (tempshort/10);	
				if((ss1<=RH_Upper_Alm_ON) && (ss1>= RH_Lower_Alm_OFF))
				{
					RH_Upper_Alm_OFF=ss1;
					WriteEEPROMData(RH_UP_ALM_OFF,(uint8_t*)&RH_Upper_Alm_OFF,sizeof(RH_Upper_Alm_OFF));
				}
			break;
			case RHLAON_ID:		
				ss1 = (tempshort/10);
				if((ss1<=RH_Lower_Alm_OFF) && (ss1>= 0))
				{
					RH_Lower_Alm_ON=ss1;
					WriteEEPROMData(RH_LO_ALM_ON,(uint8_t*)&RH_Lower_Alm_ON,sizeof(RH_Lower_Alm_ON));
				}
			break;
			case RHLAOFF_ID:	
				ss1 = (tempshort/10);
				if((ss1<=RH_Upper_Alm_OFF) && (ss1>= RH_Lower_Alm_ON))
				{
					RH_Lower_Alm_OFF=ss1;
					WriteEEPROMData(RH_LO_ALM_OFF,(uint8_t*)&RH_Lower_Alm_OFF,sizeof(RH_Lower_Alm_OFF));
				}
			break;
			#endif
				
			case LOGINTVAL_ID:		
				if((tempshort>=MIN_LOG_INTERVAL) && (tempshort<=MAX_LOG_INTERVAL))
				{
					LogInterval=tempshort;
					logTimer = LogInterval;
					//FlashlogTimer=60;
					//logTimer=(unsigned long)LogInterval*60;
					WriteEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,sizeof(LogInterval));	
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
				WriteEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,sizeof(Buzzer_ON_Time));
				if(!Buzzer_ON_Time)
				{
					//bool_buzzerStart=NO;
					BUZZER_OFF;
					buzzerOnTime=0;
					buzzerOffTime=0;
				}
				else
				{
					if((bool_buzzerStart==YES) && BUZZER_ENABLED())
					{
						buzzerOnTime=BUZZER_ON_PERIOD();
						buzzerOffTime=0;
						BUZZER_ON;
					}
				}
			break;

			case BZROFF_ID:		
				Buzzer_OFF_Time=tempshort;
				WriteEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,sizeof(Buzzer_OFF_Time));
			break;
			
			case EXT_FLASH_ERASE_ID:
			
				//Erase External Flash
				bool_EraseFlash=1;
				
			break;
			
			case DFLT_RTC_ID:
				
				//Erase External Flash
				bool_EraseFlash=1;
				
				//Set Default RTC
			
				Write_byte_PCF8563(RTC_CNTRL1_ADDR,0);	//clear STOP: oscillator must run
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
							
							DP_Cal_Value_C[DP1]=0;
							WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],sizeof(DP_Cal_Value_C[DP1]));

							DP_Cal_float_Value_C[DP1] = 0.0;
						
						break;
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						case '1':

							DP_Cal_Value_C[DP2]=0;
							WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],sizeof(DP_Cal_Value_C[DP2]));

							DP_Cal_float_Value_C[DP2] = 0.0;
					
						break;
						
						case '2':

							DP_Cal_Value_C[DP3]=0;
							WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],sizeof(DP_Cal_Value_C[DP3]));

							DP_Cal_float_Value_C[DP3] = 0.0;
					
						break;
						
						#else
						case '1':

							TM_Cal_Value_C=0;
							WriteEEPROMData(TM_CAL_VAL_C_ADDR,(uint8_t*)&TM_Cal_Value_C,sizeof(TM_Cal_Value_C));

							TM_Cal_float_Value_C = 0.0;
					
						break;
						
						case '2':

							RH_Cal_Value_C=0;
							WriteEEPROMData(RH_CAL_VAL_C_ADDR,(uint8_t*)&RH_Cal_Value_C,sizeof(RH_Cal_Value_C));

							RH_Cal_float_Value_C = 0.0;
					
						break;
						
						#endif
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
					WriteEEPROMData(CUSTOMER_PASSWORD,(uint8_t*)&CustPassword,sizeof(CustPassword));
				}
			break;
			case FCPWD_ID:
				if(tempshort<=9999)
				{
					FactCustPassword=tempshort;
					WriteEEPROMData(FAC_CUSTOMER_PASSWORD,(uint8_t*)&FactCustPassword,sizeof(FactCustPassword));
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
						WriteEEPROMData((DP_OFFSET_ADDR+(index*2)),(uint8_t*)&su16_dp_offset[index],sizeof(su16_dp_offset[index]));
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
						WriteEEPROMData((DP_SW_FACT_ADDR+(index*2)),(uint8_t*)&su16_dp_sw_factor[index],sizeof(su16_dp_sw_factor[index]));
						gu8_dp_sw_factor_add_cnt[index]=0;
						TempDpressure[index]=0;
					}
				}	
				
			break;
			
			case DP_LIMIT_ID:
			
				index=RxBuffer[4]-'0';
				
				if(index<MAX_SUPPORTED_DP)
				{
					u16_dp_limit[index]=tempshort;
					f32_dp_limit[index]=(float)u16_dp_limit[index]/10.0;
					WriteEEPROMData((DP_LIMIT_ADDR+(index*2)),(uint8_t*)&u16_dp_limit[index],sizeof(u16_dp_limit[index]));
				}
			
			break;
			case DP_SLOT_OFFSET_ID:
			
				{
					uint8_t slot;
					
					index = RxBuffer[4]-'0';
					slot  = RxBuffer[5]-'0';
					
					//Both indices and the value are range checked: a bad frame must not
					//write past the 18-entry region and into whatever follows it.
					if((index<MAX_SUPPORTED_DP) && (slot<DP_RANGE_SLOTS)
						&& (tempshort >= -DP_SLOT_OFFSET_LIMIT) && (tempshort <= DP_SLOT_OFFSET_LIMIT))
					{
						DpRangeSlotOffset[index][slot]=tempshort;
						WriteEEPROMData(DP_SLOT_OFFSET_ADDR + (((index*DP_RANGE_SLOTS)+slot)*2),
							(uint8_t*)&DpRangeSlotOffset[index][slot],sizeof(DpRangeSlotOffset[index][slot]));
					}
					else
					{
						bool_paraIdNotValid=1;
					}
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
					DP_Cal_Value_F[DP1] = RealDpressure[DP1]*10.0;
					WriteEEPROMData(DP1_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP1],sizeof(DP_Cal_Value_F[DP1]));
					DP_Cal_float_Value_F[DP1] = (float)DP_Cal_Value_F[DP1]/10.0;
					
					DP_Cal_Value_C[DP1] = 0;
					DP_Cal_float_Value_C[DP1] = 0;
					
					su16_dp_sw_factor[DP1]=0;
					f32_dp_sw_factor[DP1]=0.0;
					WriteEEPROMData(DP_SW_FACT_ADDR,(uint8_t*)&su16_dp_sw_factor[DP1],sizeof(su16_dp_sw_factor[DP1]));
					
					su16_dp_offset[DP1]=0;
					f32_dp_offset[DP1]=0.0;
					WriteEEPROMData(DP_OFFSET_ADDR,(uint8_t*)&su16_dp_offset[DP1],sizeof(su16_dp_offset[DP1]));
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					DP_Cal_Value_C[DP1] = (RealDpressure[DP1] - DP_Cal_float_Value_F[DP1])*10.0;
					DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;
				}
				
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;

					WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],sizeof(DP_Cal_Value_C[DP1]));
					
					DP_Max[DP1] = DEFAUT_DP1_MAX;
					DP_Min[DP1] = DEFAUT_DP1_MIN;
					WriteEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],sizeof(DP_Max[DP1]));
					WriteEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],sizeof(DP_Min[DP1]));
				}
				
			break;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			case DP2CAL_ID:
			
				if(bool_FactoryCalibrationOn==1)
				{
					DP_Cal_Value_F[DP2] = RealDpressure[DP2]*10.0;
					WriteEEPROMData(DP2_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP2],sizeof(DP_Cal_Value_F[DP2]));
					DP_Cal_float_Value_F[DP2] = (float)DP_Cal_Value_F[DP2]/10.0;
					
					DP_Cal_Value_C[DP2] = 0;
					DP_Cal_float_Value_C[DP2] = 0;
					
					su16_dp_sw_factor[DP2]=0;
					f32_dp_sw_factor[DP2]=0.0;
					WriteEEPROMData((DP_SW_FACT_ADDR+2),(uint8_t*)&su16_dp_sw_factor[DP2],sizeof(su16_dp_sw_factor[DP2]));
					
					su16_dp_offset[DP2]=0;
					f32_dp_offset[DP2]=0.0;
					WriteEEPROMData((DP_OFFSET_ADDR+2),(uint8_t*)&su16_dp_offset[DP2],sizeof(su16_dp_offset[DP2]));
					
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					DP_Cal_Value_C[DP2] = (RealDpressure[DP2] - DP_Cal_float_Value_F[DP2])*10.0;
					DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
				}
			
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;

					WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],sizeof(DP_Cal_Value_C[DP2]));
					
					DP_Max[DP2] = DEFAUT_DP2_MAX;
					DP_Min[DP2] = DEFAUT_DP2_MIN;
					WriteEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],sizeof(DP_Max[DP2]));
					WriteEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],sizeof(DP_Min[DP2]));
				}
				
			break;
			
			case DP3CAL_ID:
			
				if(bool_FactoryCalibrationOn==1)
				{
					DP_Cal_Value_F[DP3] = RealDpressure[DP3]*10.0;
					WriteEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3],sizeof(DP_Cal_Value_F[DP3]));
					DP_Cal_float_Value_F[DP3] = (float)DP_Cal_Value_F[DP3]/10.0;
					
					DP_Cal_Value_C[DP3] = 0;
					DP_Cal_float_Value_C[DP3] = 0;
					
					su16_dp_sw_factor[DP3]=0;
					f32_dp_sw_factor[DP3]=0.0;
					WriteEEPROMData((DP_SW_FACT_ADDR+4),(uint8_t*)&su16_dp_sw_factor[DP3],sizeof(su16_dp_sw_factor[DP3]));
					
					su16_dp_offset[DP3]=0;
					f32_dp_offset[DP3]=0.0;
					WriteEEPROMData((DP_OFFSET_ADDR+4),(uint8_t*)&su16_dp_offset[DP3],sizeof(su16_dp_offset[DP3]));
					
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					DP_Cal_Value_C[DP3] = (RealDpressure[DP3] - DP_Cal_float_Value_F[DP3])*10.0;
					DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
				}
			
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;

					WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],sizeof(DP_Cal_Value_C[DP3]));
					
					DP_Max[DP3] = DEFAUT_DP3_MAX;
					DP_Min[DP3] = DEFAUT_DP3_MIN;
					WriteEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],sizeof(DP_Max[DP3]));
					WriteEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],sizeof(DP_Min[DP3]));
				}

			break;
			#else
			case TMCAL_ID:
				
				if(bool_FactoryCalibrationOn==1)
				{
					if(!TM_Unit)
					{
						ss1 = RealtemperatureC*10.0;
						TM_Cal_Value_F = ss1 - tempshort;
						WriteEEPROMData(TM_CAL_VAL_F_ADDR,(uint8_t*)&TM_Cal_Value_F,sizeof(TM_Cal_Value_F));
						TM_Cal_float_Value_F = (float)TM_Cal_Value_F/10.0;
					}
					else
					{
						//The reference arrives in tenths of a degree F, so its difference from the
						//reading is a SPAN in F.  The correction is subtracted from temperatureC, so
						//it has to be stored in tenths of a degree C: divide by 1.8, and leave the 32
						//degree offset alone - that belongs to absolute temperatures, not to the
						//difference between two of them.
						{
							float f32_span;
							
							ss1 = RealtemperatureF*10.0;
							f32_span = (float)(ss1 - tempshort) / 1.8f;
							TM_Cal_Value_F = (int16_t)(f32_span + ((f32_span >= 0.0f) ? 0.5f : -0.5f));
							WriteEEPROMData(TM_CAL_VAL_F_ADDR,(uint8_t*)&TM_Cal_Value_F,sizeof(TM_Cal_Value_F));
							
							TM_Cal_float_Value_F = (float)TM_Cal_Value_F/10.0;
						}
					}
										
					TM_Cal_Value_C = 0;
					TM_Cal_float_Value_C = 0;
					WriteEEPROMData(TM_CAL_VAL_C_ADDR,(uint8_t*)&TM_Cal_Value_C,sizeof(TM_Cal_Value_C));
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					//TM_Cal_Count_C=tempshort;
					
					if(!TM_Unit)
					{
						ss1 = (RealtemperatureC - TM_Cal_float_Value_F)*10.0;
						TM_Cal_Value_C = ss1 - tempshort;
						WriteEEPROMData(TM_CAL_VAL_C_ADDR,(uint8_t*)&TM_Cal_Value_C,sizeof(TM_Cal_Value_C));
						TM_Cal_float_Value_C = (float)TM_Cal_Value_C/10.0;
					}
					else
					{
						//TM_Cal_float_Value_F is a correction in C, so it cannot be taken off
						//RealtemperatureF, which is in F.  Remove the factory correction in C, convert
						//that reading to F, and only then difference it against the reference - a span,
						//so it scales by 1.8 with no offset.
						{
							float f32_span;
							
							ss1 = (((RealtemperatureC - TM_Cal_float_Value_F) * 1.8) + 32.0) * 10.0;
							f32_span = (float)(ss1 - tempshort) / 1.8f;
							TM_Cal_Value_C = (int16_t)(f32_span + ((f32_span >= 0.0f) ? 0.5f : -0.5f));
							WriteEEPROMData(TM_CAL_VAL_C_ADDR,(uint8_t*)&TM_Cal_Value_C,sizeof(TM_Cal_Value_C));
							
							TM_Cal_float_Value_C = (float)TM_Cal_Value_C/10.0;
						}
					}
				}
				
				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;
	
					TM_Max = DEFAUT_TEMP_C_MAX;
					TM_Min = DEFAUT_TEMP_C_MIN;
					WriteEEPROMData(TEMP_MAXIMUM,(uint8_t*)&TM_Max,sizeof(TM_Max));
					WriteEEPROMData(TEMP_MINIMUM,(uint8_t*)&TM_Min,sizeof(TM_Min));
				}

			break;
			case RHCAL_ID:
				
				if(bool_FactoryCalibrationOn==1)
				{
					ss1 = RealhumidityRH*10.0;
					RH_Cal_Value_F = ss1 - tempshort;
					WriteEEPROMData(RH_CAL_VAL_F_ADDR,(uint8_t*)&RH_Cal_Value_F,sizeof(RH_Cal_Value_F));

					RH_Cal_float_Value_F = (float)RH_Cal_Value_F/10.0;
				
					RH_Cal_Value_C = 0;
					RH_Cal_float_Value_C = 0;
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					ss1 = (RealhumidityRH - RH_Cal_float_Value_F)*10.0;
					RH_Cal_Value_C = ss1 - tempshort;
					RH_Cal_float_Value_C = (float)RH_Cal_Value_C/10.0;
				}

				if((bool_FactoryCalibrationOn==1) || (bool_CustmerCalibrationOn==1))
				{
					PCCalibrationTimer=60;
					
					//WriteEEPROMData(RH_CAL_VAL_C_ADDR,(uint8_t*)&RH_Cal_Value_C,2);
					
					RH_Max = DEFAUT_RH_MAX;
					RH_Min = DEFAUT_RH_MIN;
					WriteEEPROMData(RH_MAXIMUM,(uint8_t*)&RH_Max,sizeof(RH_Max));
					WriteEEPROMData(RH_MINIMUM,(uint8_t*)&RH_Min,sizeof(RH_Min));
				}
				
			break;
			case TMUNT_ID:
				if((tempshort==0) || (tempshort==1))
				{
					if(TM_Unit!=tempshort)
					{
						TM_Unit=tempshort;
						TMUnitChange();
					}
				}
			break;
			#endif
				
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
				if(tempshort<15)
				{
					gu8_LCDBrigthnessCnt=tempshort;
					TM1680Brighness(gu8_LCDBrigthnessCnt);
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
			case LCD_CONTROL_ID:
				if((tempshort==0) || (tempshort==1))
				{
					gu8_IsLCDDisable=tempshort;
					WriteEEPROMData(LCD_CONTROL_ADDR,&gu8_IsLCDDisable,sizeof(gu8_IsLCDDisable));
					
					if(gu8_IsLCDDisable)
					{
						TM1680WriteCommand(SYS_DISABLE);
						TM1680WriteCommand(LED_OFF);
					}
					else
					{
						TM1680WriteCommand(SYS_ENABLE);
						TM1680WriteCommand(LED_ON);
						TM1680Brighness(gu8_LCDBrigthnessCnt);
					}
				}
			break;
			case COM_CONTROL_ID:
				if((tempshort==0) || (tempshort==1))
				{
					gu8_IsCOMDisable=tempshort;
					WriteEEPROMData(COM_CONTROL_ADDR,&gu8_IsCOMDisable,sizeof(gu8_IsCOMDisable));
				}
			break;
			case XBEE_RST_INTERVAL_ID:
				if(tempshort <= 1440)
				{
					gu16_XbeeRstInterval=tempshort;
					gu32_triggerXbeeResetTimer = (unsigned long)gu16_XbeeRstInterval*60;
					WriteEEPROMData(XBEE_RST_INTERVAL_ADDR,(uint8_t*)&gu16_XbeeRstInterval,sizeof(gu16_XbeeRstInterval));
				}
			break;
			case DP1_ALM_SENSE_TIME_ID:
				if(tempshort<=250)
				{
					gu8_DpAlarmSensingTime[DP1]=tempshort;
					WriteEEPROMData(DP1_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP1],sizeof(gu8_DpAlarmSensingTime[DP1]));
				}
			break;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
			#endif
				
			case ACK_TIMER_ID:
				AckTimer=tempshort;
				WriteEEPROMData(ACK_TIMER,(uint8_t*)&AckTimer,sizeof(AckTimer));
			break;
			case ACK_PW_ID:
				
				if((RxBuffer[4]) && (RxBuffer[4]<=NO_OF_ACKPWD))
				{
					AckPwdInd=RxBuffer[4];
					tempchar=RxBuffer[4]-1;
					AckPwd[tempchar] = tempshort;
					
					WriteEEPROMData((ACK_PASSWORD+(2*tempchar)),(uint8_t*)&AckPwd[tempchar],sizeof(AckPwd[tempchar]));
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
				WriteEEPROMData(DEVICE_SR_NO,(uint8_t*)&gu8ar_SrNumber,sizeof(gu8ar_SrNumber));
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
				tempshort=findValue(&RxBuffer[4],RxLen-6);
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
		if(bool_paraIdNotValid) 		TxBuffer[3] |= INVALID_PARA;
		if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
		if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
		if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
		#else
		if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
		#endif
		for(j=4;j<RxLen;j++)TxBuffer[j]=RxBuffer[j-1];
		
		TxBuffer[RxLen-1]=CalCRC(&TxBuffer[1],RxLen-2);
		TxBuffer[RxLen]=0xFC;
		
		SetTxmode(TxBuffer,RxLen+1);
		
		if(RxBuffer[3]==UBRT_ID)
		{
			UART_Configure(UART_BaudRate);
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
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			case DP2UAON_ID:	tempshort = DP_Upper_Alm_ON[DP2]*10;		break;
			case DP2UAOFF_ID:	tempshort = DP_Upper_Alm_OFF[DP2]*10;		break;
			case DP2LAON_ID:	tempshort = DP_Lower_Alm_ON[DP2]*10;		break;
			case DP2LAOFF_ID:	tempshort = DP_Lower_Alm_OFF[DP2]*10;		break;
			case DP3UAON_ID:	tempshort = DP_Upper_Alm_ON[DP3]*10;		break;
			case DP3UAOFF_ID:	tempshort = DP_Upper_Alm_OFF[DP3]*10;		break;
			case DP3LAON_ID:	tempshort = DP_Lower_Alm_ON[DP3]*10;		break;
			case DP3LAOFF_ID:	tempshort = DP_Lower_Alm_OFF[DP3]*10;		break;
			#else
			case TMUAON_ID:		tempshort = TM_Upper_Alm_ON*10;			break;
			case TMUAOFF_ID:	tempshort = TM_Upper_Alm_OFF*10;		break;
			case TMLAON_ID:		tempshort = TM_Lower_Alm_ON*10;			break;
			case TMLAOFF_ID:	tempshort = TM_Lower_Alm_OFF*10;		break;
			case RHUAON_ID:		tempshort = RH_Upper_Alm_ON*10;			break;
			case RHUAOFF_ID:	tempshort = RH_Upper_Alm_OFF*10;		break;
			case RHLAON_ID:		tempshort = RH_Lower_Alm_ON*10;			break;
			case RHLAOFF_ID:	tempshort = RH_Lower_Alm_OFF*10;		break;
			#endif
			
			case LOGINTVAL_ID:	tempshort = LogInterval;				break;
			case DVCID_ID:		tempshort = DeviceID;					break;
			case BZRON_ID:		tempshort = Buzzer_ON_Time;				break;
			case BZROFF_ID:		tempshort = Buzzer_OFF_Time;			break;
			case DP1MIN_ID:		tempshort = DP_Min[DP1]*100;				break;
			case DP1MAX_ID:		tempshort = DP_Max[DP1]*100;				break;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			case DP2MIN_ID:		tempshort = DP_Min[DP2]*100;				break;
			case DP2MAX_ID:		tempshort = DP_Max[DP2]*100;				break;
			case DP3MIN_ID:		tempshort = DP_Min[DP3]*100;				break;
			case DP3MAX_ID:		tempshort = DP_Max[DP3]*100;				break;
			#else
			case TMMIN_ID:
					if(!TM_Unit)
					{
						tempfloat = TM_Min;
					}
					else
					{
						tempfloat = (TM_Min * 1.8) + 32.0;
					}		
					tempshort = tempfloat*100;					
				break;
			case TMMAX_ID:		
					if(!TM_Unit)
					{
						tempfloat = TM_Max;
					}
					else
					{
						tempfloat = (TM_Max * 1.8) + 32.0;
					}
					tempshort = tempfloat*100;				
				break;
			case RHMIN_ID:		tempshort = RH_Min*100;					break;
			case RHMAX_ID:		tempshort = RH_Max*100;					break;
			case TMUNT_ID:		tempshort = TM_Unit;					break;
			#endif		
			case DP_OFFSET_ID:
				
				index=RxBuffer[4]-'0';
				if(index<MAX_SUPPORTED_DP)
				{	
					tempshort = su16_dp_offset[index];
				}
				
			break;
				
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
			case DP_SLOT_OFFSET_ID:
			
				{
					uint8_t slot;
					
					index = RxBuffer[4]-'0';
					slot  = RxBuffer[5]-'0';
					
					if((index<MAX_SUPPORTED_DP) && (slot<DP_RANGE_SLOTS))
					{
						tempshort = DpRangeSlotOffset[index][slot];
					}
					else
					{
						bool_paraIdNotValid=1;
					}
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
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
			#else
			case TMCAL_ID:
				if(bool_FactoryCalibrationOn==1)
				{
					PCCalibrationTimer=60;
					tempshort = TM_Cal_Value_F;
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					PCCalibrationTimer=60;
					tempshort = TM_Cal_Value_C;
				}
				else
				{
					tempshort = 0;
				}
			break;
			case RHCAL_ID:
				if(bool_FactoryCalibrationOn==1)
				{
					PCCalibrationTimer=60;
					tempshort = RH_Cal_Value_F;
				}
				else if(bool_CustmerCalibrationOn==1)
				{
					PCCalibrationTimer=60;
					tempshort = RH_Cal_Value_C;
				}
				else
				{
					tempshort = 0;
				}
			break;	
			#endif
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
			
			case ACK_PW_ID:		
				if((RxBuffer[4]) && (RxBuffer[4]<=NO_OF_ACKPWD)) 
				{
					tempshort = AckPwd[RxBuffer[4]-1];	
				}					
			break;
			case UBRT_ID:		tempshort = UART_BaudRate;				break;
			case MENB_ID:		tempshort = gu8_masterEnable;			break;
			case DOOR_SENSE_POLARITY_ID:	tempshort = gu8_doorSensingPolarity;	break;
			case DOOR_SENSE_TIME_ID:		tempshort = gu8_doorSensingTime;		break;
			case LCD_BRIGHT_CNT_ID:			tempshort = gu8_LCDBrigthnessCnt;		break;
			case AUTO_SENT_INTERVAL_ID:			tempshort = gu8_AutoSentInterval;		break;
			case DEVICES_IN_GROUP_ID:			tempshort = gu8_DeviceInGroup;			break;
			case LCD_CONTROL_ID:			tempshort = gu8_IsLCDDisable;			break;
			case COM_CONTROL_ID:			tempshort = gu8_IsCOMDisable;			break;
			case XBEE_RST_INTERVAL_ID:			tempshort = gu16_XbeeRstInterval;		break;
			case DP1_ALM_SENSE_TIME_ID:			tempshort = gu8_DpAlarmSensingTime[DP1];	break;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			case DP2_ALM_SENSE_TIME_ID:			tempshort = gu8_DpAlarmSensingTime[DP2];	break;
			case DP3_ALM_SENSE_TIME_ID:			tempshort = gu8_DpAlarmSensingTime[DP3];	break;
			#endif
			case SRNO_ID:												break;		
			#if BUILD_RAM_BUFFER
			case RAM_ALL_ID:											break;
			case RAM_IND_ID:											break;
			#endif
			#if BUILD_LOG24_LOG
			case FLASH24_IND_ID:										break;
			#endif
			#if BUILD_MINMAX_LOG
			case MINMAXMEAN_IND_ID:										break;
			#endif
			#if BUILD_MEAN24_LOG
			case MEAN_HR_ID:											break;
			#endif
			case REALTIME_VAL_ID:										break;
			#if BUILD_REGULAR_LOG
			case RDLG_DT_ID:											break;
			case RDLG_CNT_ID:											break;
			#endif
			case DATETIME_ID:											break;
			#if BUILD_LOG24_LOG
			case FLASH24_CUR_IND_ID:	tempshort = CurrentLog24Ind;	break;
			#endif
			default:			bool_paraIdNotValid=1;						break;
		}
			
		if(RxBuffer[3]==DATETIME_ID)
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 		TxBuffer[3] |= INVALID_PARA;
			if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
			#endif
			
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
			if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
			#endif
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
			if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
			#endif
			TxBuffer[4]=RxBuffer[3];
			
			memcpy(&TxBuffer[5],gu8ar_SrNumber,16);
			
			TxBuffer[21]=CalCRC(&TxBuffer[1],20);	
			TxBuffer[22]=0xFC;
			
			SetTxmode(TxBuffer,23);
		}
		#if BUILD_RAM_BUFFER
		else if(RxBuffer[3]==RAM_ALL_ID)
		{
			RAMBuffer[0]=0xFD;
			RAMBuffer[1]=RxBuffer[1];
			RAMBuffer[2]=RxBuffer[2];
			RAMBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	RAMBuffer[3] |= INVALID_PARA;
			if(bool_DP_NC[DP1]) 			RAMBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			RAMBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			RAMBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			RAMBuffer[3] |= RH_TEMP_FAULTY;
			#endif
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
		#endif	// BUILD_RAM_BUFFER
		#if BUILD_LOG24_LOG
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
		#endif	// BUILD_LOG24_LOG
		#if BUILD_MINMAX_LOG
		else if((gu16_parameterWord & ENABLE_M3LOG) && (RxBuffer[3]==MINMAXMEAN_IND_ID))
		{
			flash24_StartInd=0;
			
			MinMaxMeanReadParaType=RxBuffer[4];

			flash24_EndInd=0;
			us1 = RxBuffer[5]-'0';		us1 *= 10;				flash24_EndInd += us1;		us1 = 0;
			us1 = RxBuffer[6]-'0';								flash24_EndInd += us1;		us1 = 0;
			
			//Reject an out-of-range channel rather than reading from a stale ul1
			if((MinMaxMeanReadParaType>='0') && (MinMaxMeanReadParaType<='2'))
			{
				bool_MinMaxMeanLogReadCmd=1;
			}
			else
			{
				bool_paraIdNotValid=1;
			}
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
		#endif	// BUILD_MINMAX_LOG
		#if BUILD_MEAN24_LOG
		else if((gu16_parameterWord & ENABLE_M3LOG) && (RxBuffer[3]==MEAN_HR_ID))
		{
			flash24_StartInd=0;
			
			MinMaxMeanReadParaType=RxBuffer[4];

			//Reject an out-of-range channel rather than reading from a stale ul1
			if((MinMaxMeanReadParaType>='0') && (MinMaxMeanReadParaType<='2'))
			{
				bool_MeanHrLogReadCmd=1;
			}
			else
			{
				bool_paraIdNotValid=1;
			}
			bool_logtransferStart=0;
			
			flash24_StartInd=0;
			
			NoOf24Log=24;
		}
		#endif	// BUILD_MEAN24_LOG
		#if BUILD_REGULAR_LOG
		else if(RxBuffer[3]==RDLG_CNT_ID)
		{
			//How many records the regular log holds.  This used to be an empty case,
			//so the command answered 0 however many were stored - and answered rather
			//than raising INVALID_PARA, which made it look implemented.
			//
			//CurrentLogInd is a uint32_t reaching 60000, so it cannot go through the
			//int16_t tempshort the generic reply is built from; past 32767 it would
			//come back negative.  fillValue() takes a long, so the reply is built here.
			//
			//It is the write index, which is the record count until the ring wraps -
			//the same meaning FLASH24_CUR_IND_ID carries for the 24 hour ring.
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid)	TxBuffer[3] |= INVALID_PARA;
			if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
			if(bool_DP_NC[DP1])			TxBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2])			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3])			TxBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC)			TxBuffer[3] |= RH_TEMP_FAULTY;
			#endif
			TxBuffer[4]=RxBuffer[3];
			
			tempchar = fillValue(&TxBuffer[5],(long)CurrentLogInd);
			
			TxBuffer[5+tempchar]=CalCRC(&TxBuffer[1],4+tempchar);
			TxBuffer[6+tempchar]=0xFC;
			
			SetTxmode(TxBuffer,7+tempchar);
		}
		#endif	// BUILD_REGULAR_LOG
		else if(RxBuffer[3]==REALTIME_VAL_ID)
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
			#endif
			TxBuffer[4]=RxBuffer[3];
			
			memcpy(&TxBuffer[5],(uint8_t*)&ep.currentEpochTime,4);
			
			TxBuffer[9]=0;
			if(bool_DP_NC[DP1]) 	TxBuffer[9] |= DP1_FAULTY;
			
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			TxBuffer[9] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[9] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			TxBuffer[9] |= RH_TEMP_FAULTY;
			#endif
			
			memcpy(&TxBuffer[10],(uint8_t*)&Dpressure[DP1],4);
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			memcpy(&TxBuffer[14],(uint8_t*)&Dpressure[DP2],4);
			memcpy(&TxBuffer[18],(uint8_t*)&Dpressure[DP3],4);
			#else
			if(!TM_Unit)
			{
				memcpy(&TxBuffer[14],(uint8_t*)&temperatureC,4);
			}
			else
			{
				memcpy(&TxBuffer[14],(uint8_t*)&temperatureF,4);
			}
			memcpy(&TxBuffer[18],(uint8_t*)&humidityRH,4);
			#endif
			memcpy(&TxBuffer[22],(uint8_t*)&DP_Min[DP1],4);
			memcpy(&TxBuffer[26],(uint8_t*)&DP_Max[DP1],4);
			
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			memcpy(&TxBuffer[30],(uint8_t*)&DP_Min[DP2],4);
			memcpy(&TxBuffer[34],(uint8_t*)&DP_Max[DP2],4);
			memcpy(&TxBuffer[38],(uint8_t*)&DP_Min[DP3],4);
			memcpy(&TxBuffer[42],(uint8_t*)&DP_Max[DP3],4);
			#else
			memcpy(&TxBuffer[30],(uint8_t*)&TM_Min,4);
			memcpy(&TxBuffer[34],(uint8_t*)&TM_Max,4);
			memcpy(&TxBuffer[38],(uint8_t*)&RH_Min,4);
			memcpy(&TxBuffer[42],(uint8_t*)&RH_Max,4);
			#endif
			
			if(gu16_parameterWord & ENABLE_DP1)
			{
				if(!DP_Alrm_ON[DP1])
				{
					TxBuffer[46] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP1]==UPPER_ALARM) TxBuffer[46] = 1;
					else						  TxBuffer[46] = 2;
				}
			}
			else
			{
				TxBuffer[46] = 0;
			}
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(gu16_parameterWord & ENABLE_DP2)
			{
				if(!DP_Alrm_ON[DP2])
				{
					TxBuffer[47] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP2]==UPPER_ALARM) TxBuffer[47] = 1;
					else						  TxBuffer[47] = 2;
				}
			}
			else
			{
				TxBuffer[47] = 0;
			}
			
			if(gu16_parameterWord & ENABLE_DP3)
			{
				if(!DP_Alrm_ON[DP3])
				{
					TxBuffer[48] = 0;
				}
				else
				{
					if(DP_Alrm_ON[DP3]==UPPER_ALARM) TxBuffer[48] = 1;
					else						  TxBuffer[48] = 2;
				}
			}
			else
			{
				TxBuffer[48] = 0;
			}
			#else
			if(gu16_parameterWord & ENABLE_TEMP)
			{
				if(!TM_Alrm_ON)
				{
					TxBuffer[47] = 0;
				}
				else
				{
					if(TM_Alrm_ON==UPPER_ALARM) TxBuffer[47] = 1;
					else						  TxBuffer[47] = 2;
				}
			}
			else
			{
				TxBuffer[47] = 0;
			}

			
			if(gu16_parameterWord & ENABLE_RH)
			{
				if(!RH_Alrm_ON)
				{
					TxBuffer[48] = 0;
				}
				else
				{
					if(RH_Alrm_ON==UPPER_ALARM) TxBuffer[48] = 1;
					else						  TxBuffer[48] = 2;
				}
			}
			else
			{
				TxBuffer[48] = 0;
			}
			#endif
			
			TxBuffer[49]=CalCRC(&TxBuffer[1],48);
			TxBuffer[50]=0xFC;
			
			SetTxmode(TxBuffer,51);
		}
		#if BUILD_REGULAR_LOG
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
						LastLogInd = TOTAL_REGULAR_LOG-1;
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
							TotalLog = (TOTAL_REGULAR_LOG - StartLogInd) + EndLogInd;
						}
					}
				}
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				#else
				if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
				#endif
				
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
				if(templong>=TOTAL_REGULAR_LOG)
				{
					templong=0;
				}
				TotalLog--;
			}*/
			
			//sei();
		}
		#endif	// BUILD_REGULAR_LOG
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		else if((RxBuffer[3]==DP1CAL_ID) || (RxBuffer[3]==DP2CAL_ID) || (RxBuffer[3]==DP3CAL_ID))
		#else
		else if((RxBuffer[3]==DP1CAL_ID) || (RxBuffer[3]==TMCAL_ID) || (RxBuffer[3]==RHCAL_ID))
		#endif
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
			if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
			#endif
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
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					case DP2CAL_ID:			us1=DP2_CAL_DATE_ADDR;	us2=DP2_CAL_CERT_ADDR;			break;
					case DP3CAL_ID:			us1=DP3_CAL_DATE_ADDR;	us2=DP3_CAL_CERT_ADDR;			break;
					#else
					case TMCAL_ID:			us1=TM_CAL_DATE_ADDR;	us2=TM_CAL_CERT_ADDR;			break;
					case RHCAL_ID:			us1=RH_CAL_DATE_ADDR;	us2=RH_CAL_CERT_ADDR;			break;
					#endif
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
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					case DP2CAL_ID:			us1=DP2_USER_CAL_DATE_ADDR;				break;
					case DP3CAL_ID:			us1=DP3_USER_CAL_DATE_ADDR;				break;
					#else
					case TMCAL_ID:			us1=TM_USER_CAL_DATE_ADDR;				break;
					case RHCAL_ID:			us1=RH_USER_CAL_DATE_ADDR;				break;
					#endif
				}
				
				ReadEEPROMData(us1,(uint8_t*)&TxBuffer[10],60);
				
				TxBuffer[70]=CalCRC(&TxBuffer[1],69);
				TxBuffer[71]=0xFC;
				
				SetTxmode(TxBuffer,72);
			}
			else
			{
				//Calibration is locked, so there is no date history to attach - but the
				//host still has to get an answer.  Without this the branch fell through
				//having sent nothing at all, leaving the host to time out.  The read
				//switch has already put 0 in tempshort for the locked case.
				TxBuffer[10]=CalCRC(&TxBuffer[1],9);
				TxBuffer[11]=0xFC;
				
				SetTxmode(TxBuffer,12);
			}
		}
		else
		{
			TxBuffer[0]=0xFD;
			TxBuffer[1]=RxBuffer[1];
			TxBuffer[2]=RxBuffer[2];
			TxBuffer[3]=0x00;
			if(bool_paraIdNotValid) 		TxBuffer[3] |= INVALID_PARA;
			if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
			if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
			if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
			#else
			if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
			#endif
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

	//The receiver was re-armed before this message was served, so there is nothing
	//to clear here - and clearing it would throw away whatever arrived meanwhile.
}

void SetTxmode(uint8_t *buffer,uint16_t bytes)
{
	RS485_TX_ENB;
	SendToUART(buffer,bytes);
	RS485_RX_ENB;
}

uint32_t FindLogIndex(uint32_t EpochTime,uint32_t InitLogInd,uint32_t LastLogInd)
{		
#if BUILD_REGULAR_LOG
	uint32_t LastLogInd1=LastLogInd;

	if(FlashOVFByte)
	{
		LastLogInd = TOTAL_REGULAR_LOG-1;
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
			if(!MidLogInd)	return InitLogInd;	//no earlier record: oldest is the match
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
				if(!MidLogInd)	return InitLogInd;	//no earlier record: oldest is the match
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
			
			if(!MidLogInd)	return InitLogInd;	//no earlier record: oldest is the match
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
#else
	//Regular log not built - there is no ring to search.  Returning the caller's
	//end index makes the range empty, so a download request answers with no records
	//rather than walking a region that does not exist.
	(void)EpochTime;
	(void)InitLogInd;
	return LastLogInd;
#endif	// BUILD_REGULAR_LOG
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

void ReadDiffPressure(uint8_t SensNo)
{
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
	
	//Piecewise offset: bucket the reading by MAGNITUDE and add that slot's trim.
	//Both the ladder and the offsets are in tenths of a Pa - see DP_SLOT_n_LIMIT.
	//
	//The working value is SIGNED: differential pressure goes negative, and in an
	//unsigned type -3.0 Pa would wrap to 65506, match no slot, and convert back as
	//+6550.6 Pa.  Rounding is applied on the way in so the round trip through
	//tenths does not bias every reading toward zero by up to 0.1 Pa.
	{
		int16_t ls16_Dpressure;
		int16_t ls16_Magnitude;
		
		ls16_Dpressure = (int16_t)((Dpressure[SensNo] * 10.0f) + ((Dpressure[SensNo] >= 0.0f) ? 0.5f : -0.5f));
		ls16_Magnitude = (ls16_Dpressure < 0) ? -ls16_Dpressure : ls16_Dpressure;
		
		if(ls16_Magnitude < DP_SLOT_1_LIMIT)		ls16_Dpressure += DpRangeSlotOffset[SensNo][0];
		else if(ls16_Magnitude < DP_SLOT_2_LIMIT)	ls16_Dpressure += DpRangeSlotOffset[SensNo][1];
		else if(ls16_Magnitude < DP_SLOT_3_LIMIT)	ls16_Dpressure += DpRangeSlotOffset[SensNo][2];
		else if(ls16_Magnitude < DP_SLOT_4_LIMIT)	ls16_Dpressure += DpRangeSlotOffset[SensNo][3];
		else if(ls16_Magnitude < DP_SLOT_5_LIMIT)	ls16_Dpressure += DpRangeSlotOffset[SensNo][4];
		else if(ls16_Magnitude < DP_SLOT_6_LIMIT)	ls16_Dpressure += DpRangeSlotOffset[SensNo][5];
		
		Dpressure[SensNo] = (float)ls16_Dpressure / 10.0f;
	}
	//------------------------------------------------------------------
	
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
		//
		//Only from a reading that is actually a reading.  When the value is clamped at
		//+/-f32_dp_limit, DP_limit says so, the live display shows HI/LO instead of a
		//number and DpDisplayAlarm() raises an alarm - it is a saturation flag, not a
		//measurement.  Latching it here pinned the extremes at +/-f32_dp_limit for good,
		//because a minimum only ever moves down and a maximum only ever up: one glitch
		//and they could never recover.
		if(!DP_limit[SensNo])
		{
			//A recorded extreme that lies outside the configured clamp can never be
			//superseded once the clamp is narrowed: a minimum only moves down, a maximum
			//only moves up, and the clamp stops any reading from reaching it.  The pair
			//would sit at values this device can no longer produce for good.  The boot
			//check does not catch it - that one validates against the sensor rating, not
			//against the clamp the user set.
			//
			//Restart from the present reading, not from the seed: the seed is +/-the
			//sensor rating, which is itself outside a narrowed clamp and would trigger
			//this again on every sample.
			if((DP_Min[SensNo] < -f32_dp_limit[SensNo]) || (DP_Min[SensNo] > f32_dp_limit[SensNo]))
			{
				DP_Min[SensNo] = Dpressure[SensNo];
				WriteEEPROMData(DP1_MINIMUM+(SensNo*4),(uint8_t*)&DP_Min[SensNo],sizeof(DP_Min[SensNo]));
			}
			
			if((DP_Max[SensNo] < -f32_dp_limit[SensNo]) || (DP_Max[SensNo] > f32_dp_limit[SensNo]))
			{
				DP_Max[SensNo] = Dpressure[SensNo];
				WriteEEPROMData(DP1_MAXIMUM+(SensNo*4),(uint8_t*)&DP_Max[SensNo],sizeof(DP_Max[SensNo]));
			}
			
			if(Dpressure[SensNo] > DP_Max[SensNo])
			{
				DP_Max[SensNo] = Dpressure[SensNo];
				WriteEEPROMData(DP1_MAXIMUM+(SensNo*4),(uint8_t*)&DP_Max[SensNo],sizeof(DP_Max[SensNo]));
			}

			if(Dpressure[SensNo] < DP_Min[SensNo])
			{
				DP_Min[SensNo] = Dpressure[SensNo];
				WriteEEPROMData(DP1_MINIMUM+(SensNo*4),(uint8_t*)&DP_Min[SensNo],sizeof(DP_Min[SensNo]));
			}
		}
		
		if(gu16_parameterWord & ENABLE_ALERT)
		{			
			//Near-alarm band: raised while the pressure is within ALARM_NEAR_THRESHOLD_DP of a
			//setpoint on the APPROACH side, and cleared otherwise - including once the
			//alarm itself trips, since past the setpoint it is no longer 'nearly' there.
			//Evaluated every sample, ahead of the trip chain below, so it never depends
			//on which branch of that chain runs.
			{
				float nearHi = (float)DP_Upper_Alm_ON[SensNo]/10.0;
				float nearLo = (float)DP_Lower_Alm_ON[SensNo]/10.0;
			
				if((Dpressure[SensNo] < nearHi) && (Dpressure[SensNo] >= (nearHi - ALARM_NEAR_THRESHOLD_DP)))
				{
					gu8_DP_NearAlrm[SensNo] = UPPER_ALARM;
				}
				else if((Dpressure[SensNo] > nearLo) && (Dpressure[SensNo] <= (nearLo + ALARM_NEAR_THRESHOLD_DP)))
				{
					gu8_DP_NearAlrm[SensNo] = LOWER_ALARM;
				}
				else
				{
					gu8_DP_NearAlrm[SensNo] = NO_ALARM;
				}
			}
			
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
	static uint16_t rtFlushTimer=0;
	
	//Serve Watchdog Timer
	IWDG_ReloadCounter();
	
	//Commit the real-time parameters (min/max, alarm state, log pointers).  They are
	//held in RAM and written as one record on this timer instead of one flash write
	//per new extreme - see the real-time store in Interface/XM25QH128A.c.  Nothing
	//happens when none of them has changed, or on a part that rewrites in place.
	if(++rtFlushTimer >= RT_FLUSH_INTERVAL_SEC)
	{
		rtFlushTimer=0;
		DF_RtFlush();
	}
	
	Check_RTC();
	
	if(gu8_masterEnable==1)
	{
		SendToSlave();
	}
	
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	for(uint8_t i=0; i<MAX_SUPPORTED_DP; i++)
	#else
	for(uint8_t i=0; i<1; i++)
	#endif
	{
		if(gu16_DPAutoCalTimer10Sec[i])
		{
			gu16_DPAutoCalTimer10Sec[i]--;
			if(!gu16_DPAutoCalTimer10Sec[i])
			{
				if(gu8_DPAutoCalDoorCnt[i] >= 5)
				{
					if(bool_doorStatus==OPEN)
					{
						gu16_DPAutoCalTimer5Min[i] = 300;
					}
				}
				else
				{
					gu8_DPAutoCalDoorCnt[i] = 0;
				}
			}
		}
		
		if(gu16_DPAutoCalTimer5Min[i])
		{
			gu16_DPAutoCalTimer5Min[i]--;
			if(!gu16_DPAutoCalTimer5Min[i])
			{
				//Auto cal DP1
				DP_Cal_Value_C[i] = (RealDpressure[i] - DP_Cal_float_Value_F[i])*10.0;
				DP_Cal_float_Value_C[i] = (float)DP_Cal_Value_C[i]/10.0;
				WriteEEPROMData(DP1_CAL_VAL_C_ADDR+(i*2),(uint8_t*)&DP_Cal_Value_C[i],sizeof(DP_Cal_Value_C[i]));
			}
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
	
	if(DP_StartUpTimer)DP_StartUpTimer--;
	#if (DEVICE_MODE==DP1_TEMP_RH_MODE)
	if(TMRH_StartUpTimer)TMRH_StartUpTimer--;
	#endif
	
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
//	if(StartBroadcastTimer)
//	{
//		StartBroadcastTimer--;
//		if(!StartBroadcastTimer)
//		{
//			bool_brodcastEnb=0;
//		}
//	}
	//--------------------------------------------
	if(AlarmAckTimer)
	{
		AlarmAckTimer--;
		if(!AlarmAckTimer)
		{
			bool_DPLog[DP1]=0;
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			bool_DPLog[DP2]=0;
			bool_DPLog[DP3]=0;
			#else
			bool_TMLog=0;
			bool_RHLog=0;
			#endif
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
			
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			for(uint8_t i=0; i<MAX_SUPPORTED_DP; i++)
			#else
			for(uint8_t i=0; i<1; i++)
			#endif
			{
				if(!gu16_DPAutoCalTimer5Min[i])
				{
					if(!gu16_DPAutoCalTimer10Sec[i]) 
					{
						gu16_DPAutoCalTimer10Sec[i] = 5;
						gu8_DPAutoCalDoorCnt[i] = 1;
					}
					else
					{
						gu8_DPAutoCalDoorCnt[i]++;
					}
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
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			gu16_DPAutoCalTimer5Min[DP2] = 0;
			gu16_DPAutoCalTimer5Min[DP3] = 0;
			#endif
			//opstr("DoorClose\n");
		}
	}	
	#endif
	//====================================================
	//How long each parameter has been CONTINUOUSLY in near alarm.  Per parameter, not
	//one shared timer: a channel that keeps dropping in and out is not the sustained
	//drift this is meant to catch, so only an unbroken run counts.  One second per
	//SecondTick(), saturating at the threshold so the counter cannot wrap.
	{
		uint8_t i;
		uint8_t nearNow[NEAR_PARAM_COUNT];
		
		nearNow[0] = gu8_DP_NearAlrm[DP1];
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		nearNow[1] = gu8_DP_NearAlrm[DP2];
		nearNow[2] = gu8_DP_NearAlrm[DP3];
		nearNow[3] = NO_ALARM;
		nearNow[4] = NO_ALARM;
		#else
		nearNow[1] = NO_ALARM;
		nearNow[2] = NO_ALARM;
		nearNow[3] = gu8_TM_NearAlrm;
		nearNow[4] = gu8_RH_NearAlrm;
		#endif
		
		gu8_nearAlrmEscalated = 0;
		gu8_nearAlrmActive = 0;
		
		for(i=0; i<NEAR_PARAM_COUNT; i++)
		{
			if(nearNow[i] != NO_ALARM)
			{
				gu8_nearAlrmActive = 1;
				
				if(gu16_nearAlrmTimer[i] < BUZZER_NEAR_ESCALATE_SEC)	gu16_nearAlrmTimer[i]++;
				if(gu16_nearAlrmTimer[i] >= BUZZER_NEAR_ESCALATE_SEC)	gu8_nearAlrmEscalated = 1;
			}
			else
			{
				gu16_nearAlrmTimer[i] = 0;
			}
		}
	}
	
	if(AlarmAckTimer)
	{
		StopBuzzer();
	}
	
	if(gu16_parameterWord & ENABLE_ALERT)
	{
		if(bool_doorStatus==OPEN)
		{
			gu8_doorSensingTimer++;
			if(gu8_doorSensingTimer>=gu8_doorSensingTime)
			{
				gu8_doorSensingTimer=0;
				DOOR_Alrm_ON = 1;
			}	
		}
		else
		{
			gu8_doorSensingTimer=0;
			DOOR_Alrm_ON = 0;
		}
		
		//Work out WHICH condition wants the buzzer.  A real alarm (or an open door)
		//outranks an early warning, so escalating from near to tripped swaps the cadence
		//rather than leaving the gentler pattern running.
		uint8_t buzzerWant = BUZZER_SRC_NONE;
		
		if((DOOR_Alrm_ON) || (DP_Alrm_ON[DP1]!=NO_ALARM)
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			||(DP_Alrm_ON[DP2]!=NO_ALARM)||(DP_Alrm_ON[DP3]!=NO_ALARM)
			#else
			||(TM_Alrm_ON!=NO_ALARM)||(RH_Alrm_ON!=NO_ALARM)
			#endif
		)
		{
			buzzerWant = BUZZER_SRC_ALARM;
		}
		else if((DOOR_Alrm_ON) || (gu8_DP_NearAlrm[DP1]!=NO_ALARM)
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			||(gu8_DP_NearAlrm[DP2]!=NO_ALARM)||(gu8_DP_NearAlrm[DP3]!=NO_ALARM)
			#else
			||(gu8_TM_NearAlrm!=NO_ALARM)||(gu8_RH_NearAlrm!=NO_ALARM)
			#endif
		)
		{
			buzzerWant = BUZZER_SRC_NEAR;
			printf("BUZZER_SRC_NEAR\n");
		}
		
		if(buzzerWant != BUZZER_SRC_NONE)
		{	
			if(bool_buzzeralert==0)
			{	
				StartBuzzerFor(buzzerWant);
				bool_buzzeralert=1;
			}
			else if((buzzerWant != gu8_buzzerSource) && !AlarmAckTimer)
			{
				//The kind changed while sounding - restart on the other cadence.  Skipped
				//while an acknowledge is silencing us, or the swap would undo the ACK.
				StopBuzzer();
				StartBuzzerFor(buzzerWant);
			}
		}
		else
		{
			if(bool_buzzeralert==1)
			{
				StopBuzzer();
				bool_buzzeralert=0;
				gu8_buzzerSource=BUZZER_SRC_NONE;
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
		
		WriteEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],sizeof(DP_Max[DP1]));
		WriteEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],sizeof(DP_Min[DP1]));
	}
	
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	if(gu16_parameterWord & ENABLE_DP2)
	{
		DP_Max[DP2] = DEFAUT_DP2_MAX;
		DP_Min[DP2] = DEFAUT_DP2_MIN;
	
		WriteEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],sizeof(DP_Max[DP2]));
		WriteEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],sizeof(DP_Min[DP2]));
	}
	
	if(gu16_parameterWord & ENABLE_DP3)
	{
		DP_Max[DP3] = DEFAUT_DP3_MAX;
		DP_Min[DP3] = DEFAUT_DP3_MIN;
	
		WriteEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],sizeof(DP_Max[DP3]));
		WriteEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],sizeof(DP_Min[DP3]));
	}
	#else
	if(gu16_parameterWord & ENABLE_TEMP)
	{
		TM_Max = DEFAUT_TEMP_C_MAX;
		TM_Min = DEFAUT_TEMP_C_MIN;
		
		WriteEEPROMData(TEMP_MAXIMUM,(uint8_t*)&TM_Max,sizeof(TM_Max));
		WriteEEPROMData(TEMP_MINIMUM,(uint8_t*)&TM_Min,sizeof(TM_Min));
	}
	
	if(gu16_parameterWord & ENABLE_RH)
	{
		RH_Max = DEFAUT_RH_MAX;
		RH_Min = DEFAUT_RH_MIN;
		
		WriteEEPROMData(RH_MAXIMUM,(uint8_t*)&RH_Max,sizeof(RH_Max));
		WriteEEPROMData(RH_MINIMUM,(uint8_t*)&RH_Min,sizeof(RH_Min));
	}
	#endif
	
	//A clear is something the user asked for and expects to stick, so do not leave
	//it sitting in the mirror until the next timed flush.
	DF_RtFlush();
}

#if (DEVICE_MODE==DP1_TEMP_RH_MODE)
void TMUnitChange(void)
{
	WriteEEPROMData(TEMP_UNIT,(uint8_t*)&TM_Unit,sizeof(TM_Unit));
	
	if(!TM_Unit)
	{
		TM_Upper_Alm_ON = (TM_Upper_Alm_ON-320) / 1.8;
		TM_Upper_Alm_OFF = (TM_Upper_Alm_OFF-320) / 1.8;
		TM_Lower_Alm_ON = (TM_Lower_Alm_ON-320) / 1.8;
		TM_Lower_Alm_OFF = (TM_Lower_Alm_OFF-320) / 1.8;
	}
	else
	{
		TM_Upper_Alm_ON = (TM_Upper_Alm_ON * 1.8) + 320;
		TM_Upper_Alm_OFF = (TM_Upper_Alm_OFF * 1.8) + 320;
		TM_Lower_Alm_ON = (TM_Lower_Alm_ON * 1.8) + 320;
		TM_Lower_Alm_OFF = (TM_Lower_Alm_OFF * 1.8) + 320;
	}
	
	//The calibration corrections are deliberately left alone.  A correction is a
	//difference in tenths of a degree C, subtracted from temperatureC before the
	//display unit is applied, so switching that unit must not change it.  The
	//setpoints above are absolute temperatures held in the displayed unit, which is
	//why those convert here and these do not.
	TM_Cal_float_Value_F = (float)TM_Cal_Value_F/10.0;
	TM_Cal_float_Value_C = (float)TM_Cal_Value_C/10.0;
	
	WriteEEPROMData(TEMP_UP_ALM_ON,(uint8_t*)&TM_Upper_Alm_ON,sizeof(TM_Upper_Alm_ON));
	WriteEEPROMData(TEMP_UP_ALM_OFF,(uint8_t*)&TM_Upper_Alm_OFF,sizeof(TM_Upper_Alm_OFF));
	WriteEEPROMData(TEMP_LO_ALM_ON,(uint8_t*)&TM_Lower_Alm_ON,sizeof(TM_Lower_Alm_ON));
	WriteEEPROMData(TEMP_LO_ALM_OFF,(uint8_t*)&TM_Lower_Alm_OFF,sizeof(TM_Lower_Alm_OFF));
}
#endif

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

#if (DEVICE_MODE==DP1_TEMP_RH_MODE)
void Read_SHT25(void)
{
	float tempvar=0;
	unsigned char error=0;
	error = 0;                                       // reset error status

	if(gu16_parameterWord & ENABLE_RH)
	{	
		// --- measure humidity with "Hold Master Mode (HM)"  ---
		error |= SHT2x_MeasurePoll(HUMIDITY, &sRH);
	}
	else
	{	
		sRH.u16=0;
	}
	
	if(gu16_parameterWord & ENABLE_TEMP)
	{	
		// --- measure temperature with "Polling Mode" (no hold master) ---
		error |= SHT2x_MeasurePoll(TEMP, &sT);
	}
	else
	{	
		sT.u16=0;
	}
		
	if((sT.u16==0xFFFF) || (sRH.u16==0xFFFF))
	{
		bool_RH_TEMP_NC = 1;
		temperatureC = 0;
		temperatureF = 0;
		humidityRH   = 0;
		
		TM_Alrm_ON=NO_ALARM;
		RH_Alrm_ON=NO_ALARM;
	}
	else
	{
		bool_RH_TEMP_NC = 0;
		
		//-- calculate humidity and temperature --
		if(gu16_parameterWord & ENABLE_TEMP)
		{	
			sT.u16>>=2;
			
			sT.u16<<=2;
			
			temperatureC = SHT2x_CalcTemperatureC(sT.u16);
			RealtemperatureC = temperatureC;
			temperatureC -= TM_Cal_float_Value_F;
			temperatureC -= TM_Cal_float_Value_C;
			
			temperatureC = Kalman_Update(&Kalman[TEMPERATURE_ID], temperatureC);
			
			if(TM_Unit) 
			{
				temperatureF = (temperatureC * 1.8) + 32.0;
				RealtemperatureF = temperatureF;
			}
			
			if(!TMRH_StartUpTimer)
			{
				if(!TM_Unit)
				{
					tempvar=temperatureC;
				}
				else
				{
					tempvar=temperatureF;
				}
				
				//Find Temperature Min/Max --------------------------------------------------------
				if(temperatureC > TM_Max)
				{
					TM_Max = temperatureC;
					WriteEEPROMData(TEMP_MAXIMUM,(uint8_t*)&TM_Max,sizeof(TM_Max));
				}
				
				if(temperatureC < TM_Min)
				{
					TM_Min = temperatureC;
					WriteEEPROMData(TEMP_MINIMUM,(uint8_t*)&TM_Min,sizeof(TM_Min));
				}
								
				//Near-alarm band: raised while the temperature is within ALARM_NEAR_THRESHOLD_TM of a
				//setpoint on the APPROACH side, and cleared otherwise - including once the
				//alarm itself trips, since past the setpoint it is no longer 'nearly' there.
				//Evaluated every sample, ahead of the trip chain below, so it never depends
				//on which branch of that chain runs.
				{
					float nearHi = (float)TM_Upper_Alm_ON/10.0;
					float nearLo = (float)TM_Lower_Alm_ON/10.0;
				
					if((tempvar < nearHi) && (tempvar >= (nearHi - ALARM_NEAR_THRESHOLD_TM)))
					{
						gu8_TM_NearAlrm = UPPER_ALARM;
					}
					else if((tempvar > nearLo) && (tempvar <= (nearLo + ALARM_NEAR_THRESHOLD_TM)))
					{
						gu8_TM_NearAlrm = LOWER_ALARM;
					}
					else
					{
						gu8_TM_NearAlrm = NO_ALARM;
					}
				}
				
				//Check Alarm Limit for Temp -----------------------------------------------
				if(tempvar > (float)TM_Upper_Alm_ON/10.0)
				{
					TM_Alrm_ON=UPPER_ALARM;
					
					if(!bool_TMLog)
					{
						LastTM_Alrm_ON=TM_Alrm_ON;
						WriteEEPROMData(LAST_TM_ALRM_STAT,(uint8_t*)&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
						
						FillRamBuffer(TM_ALM_OCCURE_LOG,0,0xFFFF);
						
						bool_autoSendResponse = true;
						bool_TMLog=1;
					}
					else
					{
						if(LastTM_Alrm_ON!=TM_Alrm_ON)
						{
							FillRamBuffer(TM_ALM_RESTORE_LOG,0,0xFFFF);
							
							LastTM_Alrm_ON=NO_ALARM;
							WriteEEPROMData(LAST_TM_ALRM_STAT,(uint8_t*)&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
							
							bool_TMLog=0;
						}
					}
				}
				else if(tempvar < (float)TM_Lower_Alm_ON/10.0)
				{
					TM_Alrm_ON=LOWER_ALARM;
					
					if(!bool_TMLog)
					{
						LastTM_Alrm_ON=TM_Alrm_ON;
						WriteEEPROMData(LAST_TM_ALRM_STAT,(uint8_t*)&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
						
						FillRamBuffer(TM_ALM_OCCURE_LOG,0,0xFFFF);
						
						bool_autoSendResponse = true;
						bool_TMLog=1;
					}
					else
					{
						if(LastTM_Alrm_ON!=TM_Alrm_ON)
						{
							FillRamBuffer(TM_ALM_RESTORE_LOG,0,0xFFFF);
							
							LastTM_Alrm_ON=NO_ALARM;
							WriteEEPROMData(LAST_TM_ALRM_STAT,(uint8_t*)&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
							
							bool_TMLog=0;
						}
					}
				}
				else if((tempvar < ((float)TM_Upper_Alm_OFF/10.0)) && (tempvar > ((float)TM_Lower_Alm_OFF/10.0)))
				{
					TM_Alrm_ON=NO_ALARM;
					
					if(bool_TMLog==1)
					{
						FillRamBuffer(TM_ALM_RESTORE_LOG,0,0xFFFF);
						
						bool_TMLog=0;
						
						LastTM_Alrm_ON=TM_Alrm_ON;
						WriteEEPROMData(LAST_TM_ALRM_STAT,(uint8_t*)&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
					}
				}
			}
		}
		else
		{
			temperatureC=0.0;
			temperatureF=0.0;
			
//			TM_Max=0.0;
//			TM_Min=0.0;
			
			TM_Alrm_ON=NO_ALARM;
		}
		
		if(gu16_parameterWord & ENABLE_RH)
		{	
			sRH.u16>>=2;
			//sRH.u16 += RH_Cal_Count;
			//sRH.u16 += RH_Cal_Count_C;
			
			sRH.u16<<=2;
			
			humidityRH = SHT2x_CalcRH(sRH.u16);
			RealhumidityRH = humidityRH;
			humidityRH -= RH_Cal_float_Value_F;
			humidityRH -= RH_Cal_float_Value_C;
			
			humidityRH = Kalman_Update(&Kalman[HUMIDITY_ID], humidityRH);
			
			if(!TMRH_StartUpTimer)
			{
				//Find RH Min/Max -----------------------------------------------------------------
				if(humidityRH > RH_Max) 
				{
					RH_Max = humidityRH;
					WriteEEPROMData(RH_MAXIMUM,(uint8_t*)&RH_Max,sizeof(RH_Max));
				}
				
				if(humidityRH < RH_Min) 
				{
					RH_Min = humidityRH;
					WriteEEPROMData(RH_MINIMUM,(uint8_t*)&RH_Min,sizeof(RH_Min));
				}
				
				//Near-alarm band: raised while the humidity is within ALARM_NEAR_THRESHOLD_RH of a
				//setpoint on the APPROACH side, and cleared otherwise - including once the
				//alarm itself trips, since past the setpoint it is no longer 'nearly' there.
				//Evaluated every sample, ahead of the trip chain below, so it never depends
				//on which branch of that chain runs.
				{
					float nearHi = (float)RH_Upper_Alm_ON/10.0;
					float nearLo = (float)RH_Lower_Alm_ON/10.0;
				
					if((humidityRH < nearHi) && (humidityRH >= (nearHi - ALARM_NEAR_THRESHOLD_RH)))
					{
						gu8_RH_NearAlrm = UPPER_ALARM;
					}
					else if((humidityRH > nearLo) && (humidityRH <= (nearLo + ALARM_NEAR_THRESHOLD_RH)))
					{
						gu8_RH_NearAlrm = LOWER_ALARM;
					}
					else
					{
						gu8_RH_NearAlrm = NO_ALARM;
					}
				}
				
				//Check Alarm Limit for RH -----------------------------------------------
				if(humidityRH > (float)RH_Upper_Alm_ON/10.0)
				{
					RH_Alrm_ON=UPPER_ALARM;
					
					if(!bool_RHLog)
					{
						LastRH_Alrm_ON=RH_Alrm_ON;
						WriteEEPROMData(LAST_RH_ALRM_STAT,(uint8_t*)&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
						
						FillRamBuffer(RH_ALM_OCCURE_LOG,0,0xFFFF);
						
						bool_autoSendResponse = true;
						bool_RHLog=1;
					}
					else
					{
						if(LastRH_Alrm_ON!=RH_Alrm_ON)
						{
							FillRamBuffer(RH_ALM_RESTORE_LOG,0,0xFFFF);
							
							LastRH_Alrm_ON=NO_ALARM;
							WriteEEPROMData(LAST_RH_ALRM_STAT,(uint8_t*)&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
							
							bool_RHLog=0;
						}
					}
				}
				else if(humidityRH < (float)RH_Lower_Alm_ON/10.0)
				{
					RH_Alrm_ON=LOWER_ALARM;
					
					if(!bool_RHLog)
					{
						LastRH_Alrm_ON=RH_Alrm_ON;
						WriteEEPROMData(LAST_RH_ALRM_STAT,(uint8_t*)&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
						
						FillRamBuffer(RH_ALM_OCCURE_LOG,0,0xFFFF);
						
						bool_autoSendResponse = true;
						bool_RHLog=1;
					}
					else
					{
						if(LastRH_Alrm_ON!=RH_Alrm_ON)
						{
							FillRamBuffer(RH_ALM_RESTORE_LOG,0,0xFFFF);
							
							LastRH_Alrm_ON=NO_ALARM;
							WriteEEPROMData(LAST_RH_ALRM_STAT,(uint8_t*)&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
							
							bool_RHLog=0;
						}
					}
				}
				else if((humidityRH < (float)RH_Upper_Alm_OFF/10.0) && (humidityRH > (float)RH_Lower_Alm_OFF/10.0))
				{
					RH_Alrm_ON=NO_ALARM;
					
					if(bool_RHLog==1)
					{
						FillRamBuffer(RH_ALM_RESTORE_LOG,0,0xFFFF);
						
						bool_RHLog=0;
						
						LastRH_Alrm_ON=RH_Alrm_ON;
						WriteEEPROMData(LAST_RH_ALRM_STAT,(uint8_t*)&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
					}
				}
			}
		}
		else
		{
			humidityRH=0.0;
			
//			RH_Max=0.0;
//			RH_Min=0.0;
			
			RH_Alrm_ON=NO_ALARM;
		}
	}
}
#endif

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
		//Take the length, then clear the receive state BEFORE doing anything with
		//the message.  Two reasons: every exit from here has to leave the receiver
		//armed, or the ISR - which is wrapped in if(!bool_msgRcvOK) - stays gated
		//for good; and serving a message takes tens of milliseconds, during which
		//the next request would otherwise be thrown away with the host none the
		//wiser.
		RxLen = RxInd;
		
		gu8_rxMode=0;
		RxTimeout=0;
		RxInd=0;
		bool_msgRcvOK=0;		//re-arms the ISR - RxInd is cleared first, on purpose
		
		//Shorter than the smallest legal frame (FF ID CMD PID CRC FE) there is
		//nothing to check: RxLen-3 would go negative and reach CalCRC's uint16_t
		//parameter as ~65533, reading far past a 100 byte buffer.
		if(RxLen >= 6)
		{
			crcVal=CalCRC(&RxBuffer1[1],RxLen-3);
			
			#ifdef DEBUG_RCV_CMD
			opstr(0,"\r\nCRC:");
			print_Hex(0,crcVal);
			#endif
			
			if(RxBuffer1[RxLen-2]==crcVal)
			{
				if(!bool_FlashReadCmd && !bool_Flash24ReadCmd && !bool_MinMaxMeanLogReadCmd && !bool_MeanHrLogReadCmd && !bool_RamReadCmd && !bool_RamAllReadCmd)
				{
					for(uint8_t m=0;m<RxLen;m++) RxBuffer[m]=RxBuffer1[m];
					RxBuffer[1]=DeviceID;
					ServePCMsg();
				}
			}
		}
	}
	else
	{
		if(bool_FlashReadCmd)
		{
			//Body gated, not the branch: this is the head of the else-if chain below.
			#if BUILD_REGULAR_LOG
			//A request for nothing to send, so end the transfer rather than leave
			//the flag set - whileTask() serves no commands at all while one is,
			//so the host could not even ask again.
			if(!TotalLog)
			{
				bool_FlashReadCmd=0;
			}
			else if(TotalLog && !bool_logtransferStart)
			{
				//cli();
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				#else
				if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
				#endif
				TxBuffer[4]=RxBuffer[3];
				//TxBuffer[5]=RxBuffer[4];
				//TxBuffer[6]=RxBuffer[5];
				
				ReadLog(templong,&TxBuffer[5],LOG_SIZE);
				
				TxBuffer[68]=CalCRC(&TxBuffer[1],67);
				TxBuffer[69]=0xFC;
				
				templong++;
				if(templong>=TOTAL_REGULAR_LOG)
				{
					templong=0;
				}
				
				//Released, not latched: the next pass sends the next record.  Latching it
				//here stalled the transfer after one record and, because the command gate
				//in whileTask() ignores all traffic while a transfer is armed, left the
				//device deaf until it was power cycled.
				bool_logtransferStart=0;
				
				SetTxmode(TxBuffer,70);
				//sei();
								
				TotalLog--;
				if(!TotalLog)
				{
					bool_FlashReadCmd=0;
				}
			}
			#endif	// BUILD_REGULAR_LOG
		}
		#if BUILD_LOG24_LOG
		else if(bool_Flash24ReadCmd)
		{
			//A request for nothing to send, so end the transfer rather than leave
			//the flag set - whileTask() serves no commands at all while one is,
			//so the host could not even ask again.
			if(!NoOf24Log)
			{
				bool_Flash24ReadCmd=0;
			}
			else if(NoOf24Log && !bool_logtransferStart)
			{
				//cli();
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				#else
				if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
				#endif
				TxBuffer[4]=RxBuffer[3];
				TxBuffer[5]=flash24_StartInd>>8;
				TxBuffer[6]=flash24_StartInd;
				
				ReadEEPROMData(LAST_LOG24_ADDR_OFFSET + ((uint32_t)flash24_StartInd*LOG_SIZE),&TxBuffer[7],LOG_SIZE);
				
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
				
				//Released, not latched: the next pass sends the next record.  Latching it
				//here stalled the transfer after one record and, because the command gate
				//in whileTask() ignores all traffic while a transfer is armed, left the
				//device deaf until it was power cycled.
				bool_logtransferStart=0;
				
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
		#endif	// BUILD_LOG24_LOG
		#if BUILD_MINMAX_LOG
		else if((gu16_parameterWord & ENABLE_M3LOG) && (bool_MinMaxMeanLogReadCmd))
		{
			//A request for nothing to send, so end the transfer rather than leave
			//the flag set - whileTask() serves no commands at all while one is,
			//so the host could not even ask again.
			if(!NoOf24Log)
			{
				bool_MinMaxMeanLogReadCmd=0;
			}
			else if(NoOf24Log && !bool_logtransferStart)
			{
				//cli();
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				#else
				if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
				#endif
				TxBuffer[4]=RxBuffer[3];
				TxBuffer[5]=MinMaxMeanReadParaType;
				TxBuffer[6]=flash24_StartInd;
				
				switch(MinMaxMeanReadParaType)
				{
					case '0':		ul1=LAST_DP1_MIN_MAX_OFFSET;		break;
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					case '1':		ul1=LAST_DP2_MIN_MAX_OFFSET;		break;
					case '2':		ul1=LAST_DP3_MIN_MAX_OFFSET;		break;
					#else
					case '1':		ul1=LAST_TM_MIN_MAX_OFFSET;			break;
					case '2':		ul1=LAST_RH_MIN_MAX_OFFSET;			break;
					#endif
					default:		ul1=LAST_DP1_MIN_MAX_OFFSET;		break;
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
				
				//Released, not latched: the next pass sends the next record.  Latching it
				//here stalled the transfer after one record and, because the command gate
				//in whileTask() ignores all traffic while a transfer is armed, left the
				//device deaf until it was power cycled.
				bool_logtransferStart=0;
				
				SetTxmode(TxBuffer,25);
				
				//sei();
						
				NoOf24Log--;
				if(!NoOf24Log)
				{
					bool_MinMaxMeanLogReadCmd=0;
				}
			}
		}
		#endif	// BUILD_MINMAX_LOG
		#if BUILD_MEAN24_LOG
		else if((gu16_parameterWord & ENABLE_M3LOG) && (bool_MeanHrLogReadCmd))
		{
			//A request for nothing to send, so end the transfer rather than leave
			//the flag set - whileTask() serves no commands at all while one is,
			//so the host could not even ask again.
			if(!NoOf24Log)
			{
				bool_MeanHrLogReadCmd=0;
			}
			else if(NoOf24Log && !bool_logtransferStart)
			{
				//cli();
				
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				#else
				if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
				#endif
				TxBuffer[4]=RxBuffer[3];
				TxBuffer[5]=MinMaxMeanReadParaType;
				TxBuffer[6]=flash24_StartInd;
				
				switch(MinMaxMeanReadParaType)
				{
					case '0':		ul1=DP1_CURR_24HR_MEAN_OFFSET;		break;
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					case '1':		ul1=DP2_CURR_24HR_MEAN_OFFSET;		break;
					case '2':		ul1=DP3_CURR_24HR_MEAN_OFFSET;		break;
					#else
					case '1':		ul1=TM_CURR_24HR_MEAN_OFFSET;		break;
					case '2':		ul1=RH_CURR_24HR_MEAN_OFFSET;		break;
					#endif
					default:		ul1=DP1_CURR_24HR_MEAN_OFFSET;	break;
				}
				ReadMinMaxLog(ul1,flash24_StartInd,&TxBuffer[7],4);
				
				TxBuffer[11]=CalCRC(&TxBuffer[1],10);
				TxBuffer[12]=0xFC;
				
				flash24_StartInd++;
				
				//Released, not latched: the next pass sends the next record.  Latching it
				//here stalled the transfer after one record and, because the command gate
				//in whileTask() ignores all traffic while a transfer is armed, left the
				//device deaf until it was power cycled.
				bool_logtransferStart=0;
				
				SetTxmode(TxBuffer,13);
				
				//sei();
				
				NoOf24Log--;
				if(!NoOf24Log)
				{
					bool_MeanHrLogReadCmd=0;
				}
			}
		}
		#endif	// BUILD_MEAN24_LOG
		#if BUILD_RAM_BUFFER
		else if(bool_RamReadCmd)
		{
			//A request for nothing to send, so end the transfer rather than leave
			//the flag set - whileTask() serves no commands at all while one is,
			//so the host could not even ask again.
			if(!NoOf24Log)
			{
				bool_RamReadCmd=0;
			}
			else if(NoOf24Log && !bool_logtransferStart)
			{
				//cli();
				TxBuffer[0]=0xFD;
				TxBuffer[1]=RxBuffer[1];
				TxBuffer[2]=RxBuffer[2];
				TxBuffer[3]=0x00;
				if(bool_paraIdNotValid) 	TxBuffer[3] |= INVALID_PARA;
				if(!bool_rtcValid)			TxBuffer[3] |= RTC_INVALID;
				if(bool_DP_NC[DP1]) 			TxBuffer[3] |= DP1_FAULTY;
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
				if(bool_DP_NC[DP2]) 			TxBuffer[3] |= DP2_FAULTY;
				if(bool_DP_NC[DP3]) 			TxBuffer[3] |= DP3_FAULTY;
				#else
				if(bool_RH_TEMP_NC) 			TxBuffer[3] |= RH_TEMP_FAULTY;
				#endif
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
				
				//Released, not latched: the next pass sends the next record.  Latching it
				//here stalled the transfer after one record and, because the command gate
				//in whileTask() ignores all traffic while a transfer is armed, left the
				//device deaf until it was power cycled.
				bool_logtransferStart=0;
				
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
		#endif	// BUILD_RAM_BUFFER
	}
	
	#ifdef ENABLE_KEY_LOGIC
	//Both key scanners run on the fixed 50 ms tick.  check_key() used to run at whatever
	//rate the main loop happened to spin at, so its debounce counted loop iterations
	//(microseconds) rather than time, and stalled whenever the loop blocked on flash.
	//check_key() must stay first: it clears key_up_count / key_dn_count on release.
	if(bool_keyScan_flag)
	{
		check_key();		//Check Keyboard
		CheckUpDnKey();		//Check UP and Down key
		bool_keyScan_flag=0;
	}
	#endif
	
	if(bool_msec250_flag)
	{
		//Read_SHT25() only exists in the TEMP/RH build - the differential pressure
		//sampling below runs in BOTH modes, so only this call may be guarded.
		#if (DEVICE_MODE==DP1_TEMP_RH_MODE)
		Read_SHT25();
		#endif
		
		//Read Differential Pressure -----------------------------------------
		if(gu16_parameterWord & ENABLE_DP1) 
		{
			if(StageDP[DP1]==0)
			{
				DP_TriggerConv(DP1);
				StageDP[DP1]=1;
			}
			else
			{
				if(!DP_ReadPressure(DP1,&RealDpressure[DP1]))
				{
					ReadDiffPressure(DP1);
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

//			DP_Max[DP1]=0.0;
//			DP_Min[DP1]=0.0;

			DP_Alrm_ON[DP1]=NO_ALARM;
			
			DP_StartUpTimer=0;
		}
		//-------------------------------------------------------------
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		if(gu16_parameterWord & ENABLE_DP2) 
		{
			if(StageDP[DP2]==0)
			{
				DP_TriggerConv(DP2);
				StageDP[DP2]=1;
			}
			else
			{
				if(!DP_ReadPressure(DP2,&RealDpressure[DP2]))
				{
					ReadDiffPressure(DP2);
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
				DP_TriggerConv(DP3);
				StageDP[DP3]=1;
			}
			else
			{
				if(!DP_ReadPressure(DP3,&RealDpressure[DP3]))
				{
					ReadDiffPressure(DP3);
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
		#endif
		
		bool_msec250_flag=0;
	}
	
	//Step everything that runs on the 50 ms tick, once per tick that has actually
	//elapsed, rather than once per pass of whileTask().  A whileTask()
	//pass takes about 85 ms (it refreshes the display every time), so gating on a
	//boolean flag lost roughly every second tick and made these run ~1.7x long.
	//
	//Reading a free-running counter and consuming the difference is race free: the
	//ISR only ever writes it, main only ever reads it, and unsigned subtraction
	//stays correct across the 16-bit wrap.
	{
		static uint16_t lu16_lastTick50=0;
		uint16_t lu16_now = gu16_tick50;
		uint16_t lu16_elapsed = (uint16_t)(lu16_now - lu16_lastTick50);
		
		lu16_lastTick50 = lu16_now;
		
		while(lu16_elapsed)
		{
			lu16_elapsed--;
			
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
			
			//Run down the UART-acknowledge logo blink.  Only the hold-off is counted
			//here; the 500 ms phase comes from bool_mec500_blink_flag, which the TIM1
			//ISR toggles every 10 of these ticks.
			if(gu16_logoAckBlinkTimer) gu16_logoAckBlinkTimer--;
		}
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
	
	ReadEEPROMData(FIRST_BOOT_CHECK,&FirstTimeCheck,sizeof(FirstTimeCheck));
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	if(FirstTimeCheck != 0xA1)
	{
		FirstTimeCheck=0xA1;
	#else
	if(FirstTimeCheck != 0xB3)
	{
		FirstTimeCheck=0xB3;
	#endif
		WriteEEPROMData(FIRST_BOOT_CHECK,&FirstTimeCheck,sizeof(FirstTimeCheck)); 
		
		gu16_parameterWord=PARAMETER_WORD;
		WriteEEPROMData(DISP_PARA_SELECT,(uint8_t*)&gu16_parameterWord,sizeof(gu16_parameterWord));

		gu8_masterEnable=0;
		WriteEEPROMData(MASTER_ENABLE_ADDR,&gu8_masterEnable,sizeof(gu8_masterEnable));
		
		memset(gu8ar_SrNumber,'0',sizeof(gu8ar_SrNumber));
		WriteEEPROMData(DEVICE_SR_NO,(uint8_t*)&gu8ar_SrNumber[0],sizeof(gu8ar_SrNumber));
		gu32_SrNumber = ascii2hex(&gu8ar_SrNumber[8],8);

		
		if(gu16_parameterWord & ENABLE_DATAFLASH)
		{
			//Reset Data Logging Parameter -------------------------------------------
			FlashOVFByte=0;
			WriteEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
			
			ResetMinMax();	
			
			ResetCurrentLogInd();
			ResetCurrentLog24Ind();
			
			#if BUILD_MINMAX_LOG
			MinMaxMeanDayLogInd=0;
			WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
			#endif
			
			//Clear all Hour mean value.  Buffer1 is the scratch area here - this used to
			//borrow the 2 KB RAMBuffer, which tied a 96-byte clear to a buffer that only
			//exists when the RAM read feature is built.
			memset(&Buffer1[0],0,sizeof(Buffer1));
			#if BUILD_MEAN24_LOG
			WriteLog(DP1_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
			#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
			WriteLog(DP2_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
			WriteLog(DP3_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
			#else
			WriteLog(TM_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
			WriteLog(RH_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
			#endif
			#endif	// BUILD_MEAN24_LOG
			
			//Clear all Min Max Mean Logs.  A whole channel is MIN_MAX_MEAN_LOG_SPACE
			//(240) bytes, more than Buffer1 holds, so clear it one record at a time.
			#if BUILD_MINMAX_LOG
			{
				uint8_t clr;
				for(clr=0; clr<TOTAL_MIN_MAX_MEAN_LOG; clr++)
				{
					WriteLog(LAST_DP1_MIN_MAX_OFFSET,clr,&Buffer1[0],MIN_MAX_MEAN_LOG_SIZE);
					#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
					WriteLog(LAST_DP2_MIN_MAX_OFFSET,clr,&Buffer1[0],MIN_MAX_MEAN_LOG_SIZE);
					WriteLog(LAST_DP3_MIN_MAX_OFFSET,clr,&Buffer1[0],MIN_MAX_MEAN_LOG_SIZE);
					#else
					WriteLog(LAST_TM_MIN_MAX_OFFSET,clr,&Buffer1[0],MIN_MAX_MEAN_LOG_SIZE);
					WriteLog(LAST_RH_MIN_MAX_OFFSET,clr,&Buffer1[0],MIN_MAX_MEAN_LOG_SIZE);
					#endif

					//A first boot on a NOR part can spend a while in here
					IWDG_ReloadCounter();
				}
			}
			#endif	// BUILD_MINMAX_LOG
		}
		
		RTCSetFlag=0;
		WriteEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
		
		//Per-slot DP trim starts at zero on a fresh device - no correction until
		//somebody sets one.  boot_data() also defaults these further down on EVERY
		//boot, which is what covers a unit that is upgraded rather than fresh.
		{
			uint8_t ch,slot;
			
			for(ch=0; ch<MAX_SUPPORTED_DP; ch++)
			{
				for(slot=0; slot<DP_RANGE_SLOTS; slot++)
				{
					DpRangeSlotOffset[ch][slot]=0;
					WriteEEPROMData(DP_SLOT_OFFSET_ADDR + (((ch*DP_RANGE_SLOTS)+slot)*2),
						(uint8_t*)&DpRangeSlotOffset[ch][slot],sizeof(DpRangeSlotOffset[ch][slot]));
					
					IWDG_ReloadCounter();
				}
			}
		}
		
		if(gu16_parameterWord & ENABLE_DP1)
		{
			//DPressure1 Parameter -----------------------------------------------------
			DP_Upper_Alm_ON[DP1]=DEFAULT_DP1_UPPER_ALM_ON;
			WriteEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],sizeof(DP_Upper_Alm_ON[DP1]));
		
			DP_Upper_Alm_OFF[DP1]=DEFAULT_DP1_UPPER_ALM_OFF;
			WriteEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],sizeof(DP_Upper_Alm_OFF[DP1]));
		
			DP_Lower_Alm_ON[DP1]=DEFAULT_DP1_LOWER_ALM_ON;
			WriteEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],sizeof(DP_Lower_Alm_ON[DP1]));
		
			DP_Lower_Alm_OFF[DP1]=DEFAULT_DP1_LOWER_ALM_OFF;
			WriteEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],sizeof(DP_Lower_Alm_OFF[DP1]));
		
			DP_Cal_Value_F[DP1]=0;
			WriteEEPROMData(DP1_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP1],sizeof(DP_Cal_Value_F[DP1]));
			
			DP_Cal_Value_C[DP1]=0;
			WriteEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],sizeof(DP_Cal_Value_C[DP1]));
			
			DP_Cal_float_Value_F[DP1] = 0.0;
			DP_Cal_float_Value_C[DP1] = 0.0;			

			LastDP_Alrm_ON[DP1]=0;
			WriteEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
			
			gu8_DpAlarmSensingTime[DP1]=5;
			WriteEEPROMData(DP1_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP1],sizeof(gu8_DpAlarmSensingTime[DP1]));
			
			su16_dp_offset[DP1]=0;
			f32_dp_offset[DP1]=0.0;
			WriteEEPROMData((DP_OFFSET_ADDR),(uint8_t*)&su16_dp_offset[DP1],sizeof(su16_dp_offset[DP1]));
			
			su16_dp_sw_factor[DP1]=0;
			f32_dp_sw_factor[DP1]=0.0;
			WriteEEPROMData((DP_SW_FACT_ADDR),(uint8_t*)&su16_dp_sw_factor[DP1],sizeof(su16_dp_sw_factor[DP1]));
			
			u16_dp_limit[DP1]=DEFAULT_DP_LIMIT;
			WriteEEPROMData((DP_LIMIT_ADDR),(uint8_t*)&u16_dp_limit[DP1],sizeof(u16_dp_limit[DP1]));
			f32_dp_limit[DP1]=(float)u16_dp_limit[DP1]/10.0;
			
			DP_UserCalDateInd[DP1]=0;
			WriteEEPROMData(DP1_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP1],sizeof(DP_UserCalDateInd[DP1]));
		}
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		if(gu16_parameterWord & ENABLE_DP2)
		{
			//DPressure2 Parameter -----------------------------------------------------
			DP_Upper_Alm_ON[DP2]=DEFAULT_DP2_UPPER_ALM_ON;
			WriteEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],sizeof(DP_Upper_Alm_ON[DP2]));
		
			DP_Upper_Alm_OFF[DP2]=DEFAULT_DP2_UPPER_ALM_OFF;
			WriteEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],sizeof(DP_Upper_Alm_OFF[DP2]));
		
			DP_Lower_Alm_ON[DP2]=DEFAULT_DP2_LOWER_ALM_ON;
			WriteEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],sizeof(DP_Lower_Alm_ON[DP2]));
		
			DP_Lower_Alm_OFF[DP2]=DEFAULT_DP2_LOWER_ALM_OFF;
			WriteEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],sizeof(DP_Lower_Alm_OFF[DP2]));
		
			DP_Cal_Value_F[DP2]=0;
			WriteEEPROMData(DP2_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP2],sizeof(DP_Cal_Value_F[DP2]));
			
			DP_Cal_Value_C[DP2]=0;
			WriteEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],sizeof(DP_Cal_Value_C[DP2]));
			
			DP_Cal_float_Value_F[DP2] = 0.0;
			DP_Cal_float_Value_C[DP2] = 0.0;
			
			LastDP_Alrm_ON[DP2]=0;
			WriteEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
			
			gu8_DpAlarmSensingTime[DP2]=5;
			WriteEEPROMData(DP2_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP2],sizeof(gu8_DpAlarmSensingTime[DP2]));
			
			su16_dp_offset[DP2]=0;
			f32_dp_offset[DP2]=0.0;
			WriteEEPROMData((DP_OFFSET_ADDR+2),(uint8_t*)&su16_dp_offset[DP2],sizeof(su16_dp_offset[DP2]));
				
			su16_dp_sw_factor[DP2]=0;
			f32_dp_sw_factor[DP2]=0.0;
			WriteEEPROMData((DP_SW_FACT_ADDR+2),(uint8_t*)&su16_dp_sw_factor[DP2],sizeof(su16_dp_sw_factor[DP2]));
			
			u16_dp_limit[DP2]=DEFAULT_DP_LIMIT;
			WriteEEPROMData((DP_LIMIT_ADDR+2),(uint8_t*)&u16_dp_limit[DP2],sizeof(u16_dp_limit[DP2]));
			f32_dp_limit[DP2]=(float)u16_dp_limit[DP2]/10.0;
			
			DP_UserCalDateInd[DP2]=0;
			WriteEEPROMData(DP2_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP2],sizeof(DP_UserCalDateInd[DP2]));
		}
		
		if(gu16_parameterWord & ENABLE_DP3)
		{
			//DPressure2 Parameter -----------------------------------------------------
			DP_Upper_Alm_ON[DP3]=DEFAULT_DP3_UPPER_ALM_ON;
			WriteEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],sizeof(DP_Upper_Alm_ON[DP3]));
		
			DP_Upper_Alm_OFF[DP3]=DEFAULT_DP3_UPPER_ALM_OFF;
			WriteEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],sizeof(DP_Upper_Alm_OFF[DP3]));
		
			DP_Lower_Alm_ON[DP3]=DEFAULT_DP3_LOWER_ALM_ON;
			WriteEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],sizeof(DP_Lower_Alm_ON[DP3]));
		
			DP_Lower_Alm_OFF[DP3]=DEFAULT_DP3_LOWER_ALM_OFF;
			WriteEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],sizeof(DP_Lower_Alm_OFF[DP3]));
		
			DP_Cal_Value_F[DP3]=0;
			WriteEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3],sizeof(DP_Cal_Value_F[DP3]));
			
			DP_Cal_Value_C[DP3]=0;
			WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],sizeof(DP_Cal_Value_C[DP3]));
			
			DP_Cal_float_Value_F[DP3] = 0.0;
			DP_Cal_float_Value_C[DP3] = 0.0;
			
			LastDP_Alrm_ON[DP3]=0;
			WriteEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
			
			gu8_DpAlarmSensingTime[DP3]=5;
			WriteEEPROMData(DP3_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP3],sizeof(gu8_DpAlarmSensingTime[DP3]));
			
			su16_dp_offset[DP3]=0;
			f32_dp_offset[DP3]=0.0;
			WriteEEPROMData((DP_OFFSET_ADDR+4),(uint8_t*)&su16_dp_offset[DP3],sizeof(su16_dp_offset[DP3]));
				
			su16_dp_sw_factor[DP3]=0;
			f32_dp_sw_factor[DP3]=0.0;
			WriteEEPROMData((DP_SW_FACT_ADDR+4),(uint8_t*)&su16_dp_sw_factor[DP3],sizeof(su16_dp_sw_factor[DP3]));
			
			u16_dp_limit[DP3]=DEFAULT_DP_LIMIT;
			WriteEEPROMData((DP_LIMIT_ADDR+4),(uint8_t*)&u16_dp_limit[DP3],sizeof(u16_dp_limit[DP3]));
			f32_dp_limit[DP3]=(float)u16_dp_limit[DP3]/10.0;
			
			DP_UserCalDateInd[DP3]=0;
			WriteEEPROMData(DP3_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP3],sizeof(DP_UserCalDateInd[DP3]));
		}
		#else
		if(gu16_parameterWord & ENABLE_TEMP)
		{
			//Temperature Parameter -----------------------------------------------------
			TM_Upper_Alm_ON=DEFAULT_TM_C_UPPER_ALM_ON;
			WriteEEPROMData(TEMP_UP_ALM_ON,(uint8_t*)&TM_Upper_Alm_ON,sizeof(TM_Upper_Alm_ON));
		
			TM_Upper_Alm_OFF=DEFAULT_TM_C_UPPER_ALM_OFF;
			WriteEEPROMData(TEMP_UP_ALM_OFF,(uint8_t*)&TM_Upper_Alm_OFF,sizeof(TM_Upper_Alm_OFF));
		
			TM_Lower_Alm_ON=DEFAULT_TM_C_LOWER_ALM_ON;
			WriteEEPROMData(TEMP_LO_ALM_ON,(uint8_t*)&TM_Lower_Alm_ON,sizeof(TM_Lower_Alm_ON));

			TM_Lower_Alm_OFF=DEFAULT_TM_C_LOWER_ALM_OFF;
			WriteEEPROMData(TEMP_LO_ALM_OFF,(uint8_t*)&TM_Lower_Alm_OFF,sizeof(TM_Lower_Alm_OFF));
			
			TM_Cal_Value_F=0;
			WriteEEPROMData(TM_CAL_VAL_F_ADDR,(uint8_t*)&TM_Cal_Value_F,sizeof(TM_Cal_Value_F));
			
			TM_Cal_Value_C=0;
			WriteEEPROMData(TM_CAL_VAL_C_ADDR,(uint8_t*)&TM_Cal_Value_C,sizeof(TM_Cal_Value_C));
			
			TM_Cal_float_Value_F = 0.0;
			TM_Cal_float_Value_C = 0.0;

			TM_Unit=0;
			WriteEEPROMData(TEMP_UNIT,(uint8_t*)&TM_Unit,sizeof(TM_Unit));
			
			LastTM_Alrm_ON=0;
			WriteEEPROMData(LAST_TM_ALRM_STAT,(uint8_t*)&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
			
			TM_UserCalDateInd=0;
			WriteEEPROMData(TM_USER_CAL_DATE_IND_ADDR,&TM_UserCalDateInd,sizeof(TM_UserCalDateInd));
		}
			
		if(gu16_parameterWord & ENABLE_RH)
		{
			//RHumidity Parameter -----------------------------------------------------
			RH_Upper_Alm_ON=DEFAULT_RH_UPPER_ALM_ON;
			WriteEEPROMData(RH_UP_ALM_ON,(uint8_t*)&RH_Upper_Alm_ON,sizeof(RH_Upper_Alm_ON));

			RH_Upper_Alm_OFF=DEFAULT_RH_UPPER_ALM_OFF;
			WriteEEPROMData(RH_UP_ALM_OFF,(uint8_t*)&RH_Upper_Alm_OFF,sizeof(RH_Upper_Alm_OFF));
		
			RH_Lower_Alm_ON=DEFAULT_RH_LOWER_ALM_ON;
			WriteEEPROMData(RH_LO_ALM_ON,(uint8_t*)&RH_Lower_Alm_ON,sizeof(RH_Lower_Alm_ON));
		
			RH_Lower_Alm_OFF=DEFAULT_RH_LOWER_ALM_OFF;
			WriteEEPROMData(RH_LO_ALM_OFF,(uint8_t*)&RH_Lower_Alm_OFF,sizeof(RH_Lower_Alm_OFF));
		
			RH_Cal_Value_F=0;
			WriteEEPROMData(RH_CAL_VAL_F_ADDR,(uint8_t*)&RH_Cal_Value_F,sizeof(RH_Cal_Value_F));
			
			RH_Cal_Value_C=0;
			WriteEEPROMData(RH_CAL_VAL_C_ADDR,(uint8_t*)&RH_Cal_Value_C,sizeof(RH_Cal_Value_C));
			
			RH_Cal_float_Value_F = 0.0;
			RH_Cal_float_Value_C = 0.0;

			LastRH_Alrm_ON=0;
			WriteEEPROMData(LAST_RH_ALRM_STAT,(uint8_t*)&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
			
			RH_UserCalDateInd=0;
			WriteEEPROMData(RH_USER_CAL_DATE_IND_ADDR,&RH_UserCalDateInd,sizeof(RH_UserCalDateInd));
		}
		#endif
		gu8_broadcast = 0;
		WriteEEPROMData(BROADCAST_ENB_ADDR,&gu8_broadcast,sizeof(gu8_broadcast));
		
		gu8_rly_stat = 0;
		WriteEEPROMData(RELAY_STAT_ADDR,&gu8_rly_stat,sizeof(gu8_rly_stat));

		//RS485 Parameter -------------------------------------------
		DeviceID=DEFAULT_DEVICE_ID;
		WriteEEPROMData(DEVICE_ID,&DeviceID,sizeof(DeviceID));
		
		//Buzzer Parameter -------------------------------------------
		Buzzer_ON_Time=DEFAULT_BUZZER_ON_TIME;
		WriteEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,sizeof(Buzzer_ON_Time));
		
		Buzzer_OFF_Time=DEFAULT_BUZZER_OFF_TIME;
		WriteEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,sizeof(Buzzer_OFF_Time));
		
		if(gu16_parameterWord & ENABLE_LOG)
		{
			//Data Logging Parameter -------------------------------------------
			LogInterval=DEFAULT_LOG_INTERVAL;
			WriteEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,sizeof(LogInterval));
		}
		
		//UART Parameter -------------------------------------------
		UART_BaudRate=DEFAULT_UART_BAUDRATE;	//57600
		WriteEEPROMData(UART_BAUDRATE,&UART_BaudRate,sizeof(UART_BaudRate));
		
		//Customer Password -------------------------------------------
		CustPassword=DEFAULT_CUSTOMER_PWD;
		WriteEEPROMData(CUSTOMER_PASSWORD,(uint8_t*)&CustPassword,sizeof(CustPassword));
		
		//Factory Customer Password -------------------------------------------
		FactCustPassword=DEFAULT_FACTORY_PWD;
		WriteEEPROMData(FAC_CUSTOMER_PASSWORD,(uint8_t*)&FactCustPassword,sizeof(FactCustPassword));
		
		//Acknowledge Parameter -------------------------------------------
		AckTimer=1;
		WriteEEPROMData(ACK_TIMER,(uint8_t*)&AckTimer,sizeof(AckTimer));
		
		AckPwdInd=0;
		WriteEEPROMData(ACK_PWD_IND,&AckPwdInd,sizeof(AckPwdInd));
		
		for(uint8_t i=0;i<NO_OF_ACKPWD;i++)
		{
			AckPwd[i]=0;
			WriteEEPROMData((ACK_PASSWORD+(i*2)),(uint8_t*)&AckPwd[i],sizeof(AckPwd[i]));
		}
		
		gu8_doorSensingPolarity=1;
		WriteEEPROMData(DOOR_SENSE_POLARITY_ADDR,&gu8_doorSensingPolarity,sizeof(gu8_doorSensingPolarity));
		
		gu8_doorSensingTime=60;
		WriteEEPROMData(DOOR_SENSE_TIME_ADDR,&gu8_doorSensingTime,sizeof(gu8_doorSensingTime));
		
		gu8_LCDBrigthnessCnt=DEFAULT_LCD_BRIGHTNESS;
		WriteEEPROMData(LCD_BRIGHT_CNT_ADDR,&gu8_LCDBrigthnessCnt,sizeof(gu8_LCDBrigthnessCnt));
		
		gu8_IsLCDDisable=0;		//display on
		WriteEEPROMData(LCD_CONTROL_ADDR,&gu8_IsLCDDisable,sizeof(gu8_IsLCDDisable));
		
		gu8_IsCOMDisable=0;		//UART answering
		WriteEEPROMData(COM_CONTROL_ADDR,&gu8_IsCOMDisable,sizeof(gu8_IsCOMDisable));
		
		gu8_AutoSentInterval=DEFAULT_AUTO_SENT_INTERVAL;
		WriteEEPROMData(AUTO_SENT_INTERVAL_ADDR,&gu8_AutoSentInterval,sizeof(gu8_AutoSentInterval));
		
		gu8_DeviceInGroup=DEFAULT_DEVICES_IN_GROUP;
		WriteEEPROMData(DEVICES_IN_GROUP_ADDR,&gu8_DeviceInGroup,sizeof(gu8_DeviceInGroup));
		
		gu16_XbeeRstInterval=DEFAULT_XBEE_RST_INTERVAL;
		WriteEEPROMData(XBEE_RST_INTERVAL_ADDR,(uint8_t*)&gu16_XbeeRstInterval,sizeof(gu16_XbeeRstInterval));
		
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
		ReadEEPROMData(DOOR_SENSE_POLARITY_ADDR,&gu8_doorSensingPolarity,sizeof(gu8_doorSensingPolarity));
		if(gu8_doorSensingPolarity > 1)
		{
			gu8_doorSensingPolarity=0;
			WriteEEPROMData(DOOR_SENSE_POLARITY_ADDR,&gu8_doorSensingPolarity,sizeof(gu8_doorSensingPolarity));
		}
		
		ReadEEPROMData(LCD_BRIGHT_CNT_ADDR,&gu8_LCDBrigthnessCnt,sizeof(gu8_LCDBrigthnessCnt));
		if(gu8_LCDBrigthnessCnt > 15)
		{
			gu8_LCDBrigthnessCnt=DEFAULT_LCD_BRIGHTNESS;
			WriteEEPROMData(LCD_BRIGHT_CNT_ADDR,&gu8_LCDBrigthnessCnt,sizeof(gu8_LCDBrigthnessCnt));
		}
		
		//Both of these are set over UART (LCD_CONTROL_ID / COM_CONTROL_ID) and were
		//being stored but never loaded, so the setting silently came back enabled on
		//the next power up.  Anything other than 0 or 1 - a blank cell reads 0xFF -
		//must fall back to 0: coming up with the display blanked, or worse with the
		//UART refusing to answer, would leave no way to correct it from outside.
		ReadEEPROMData(LCD_CONTROL_ADDR,&gu8_IsLCDDisable,sizeof(gu8_IsLCDDisable));
		if(gu8_IsLCDDisable > 1)
		{
			gu8_IsLCDDisable=0;
			WriteEEPROMData(LCD_CONTROL_ADDR,&gu8_IsLCDDisable,sizeof(gu8_IsLCDDisable));
		}
		
		ReadEEPROMData(COM_CONTROL_ADDR,&gu8_IsCOMDisable,sizeof(gu8_IsCOMDisable));
		if(gu8_IsCOMDisable > 1)
		{
			gu8_IsCOMDisable=0;
			WriteEEPROMData(COM_CONTROL_ADDR,&gu8_IsCOMDisable,sizeof(gu8_IsCOMDisable));
		}
		
		ReadEEPROMData(AUTO_SENT_INTERVAL_ADDR,&gu8_AutoSentInterval,sizeof(gu8_AutoSentInterval));
		if((!gu8_AutoSentInterval) || (gu8_AutoSentInterval > 240))
		{
			gu8_AutoSentInterval=DEFAULT_AUTO_SENT_INTERVAL;
			WriteEEPROMData(AUTO_SENT_INTERVAL_ADDR,&gu8_AutoSentInterval,sizeof(gu8_AutoSentInterval));
		}
		
		ReadEEPROMData(DEVICES_IN_GROUP_ADDR,&gu8_DeviceInGroup,sizeof(gu8_DeviceInGroup));
		if((gu8_DeviceInGroup < 2) || (gu8_DeviceInGroup > 100))
		{
			gu8_DeviceInGroup=DEFAULT_DEVICES_IN_GROUP;
			WriteEEPROMData(DEVICES_IN_GROUP_ADDR,&gu8_DeviceInGroup,sizeof(gu8_DeviceInGroup));
		}
		
		ReadEEPROMData(XBEE_RST_INTERVAL_ADDR,(uint8_t*)&gu16_XbeeRstInterval,sizeof(gu16_XbeeRstInterval));
		if(gu16_XbeeRstInterval > 1440)
		{
			gu16_XbeeRstInterval=DEFAULT_XBEE_RST_INTERVAL;
			WriteEEPROMData(XBEE_RST_INTERVAL_ADDR,(uint8_t*)&gu16_XbeeRstInterval,sizeof(gu16_XbeeRstInterval));
		}
		
		ReadEEPROMData(DEVICE_SR_NO,&gu8ar_SrNumber[0],sizeof(gu8ar_SrNumber));
		gu32_SrNumber = ascii2hex(&gu8ar_SrNumber[8],8);
		
		ReadEEPROMData(DOOR_SENSE_TIME_ADDR,&gu8_doorSensingTime,sizeof(gu8_doorSensingTime));
		if(gu8_doorSensingTime > 250)
		{
			gu8_doorSensingTime=60;
			WriteEEPROMData(DOOR_SENSE_TIME_ADDR,&gu8_doorSensingTime,sizeof(gu8_doorSensingTime));
		}
		
		ReadEEPROMData(MASTER_ENABLE_ADDR,&gu8_masterEnable,sizeof(gu8_masterEnable));
		if(gu8_masterEnable > 1)
		{
			gu8_masterEnable=0;
			WriteEEPROMData(MASTER_ENABLE_ADDR,&gu8_masterEnable,sizeof(gu8_masterEnable));
		}
		
		ReadEEPROMData(DISP_PARA_SELECT,(uint8_t*)&gu16_parameterWord,sizeof(gu16_parameterWord));
		
		#if BUILD_MINMAX_LOG
		if(gu16_parameterWord & ENABLE_M3LOG)
		{
			ReadEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
			if(MinMaxMeanDayLogInd>=TOTAL_MIN_MAX_MEAN_LOG)
			{
				MinMaxMeanDayLogInd=0;
				WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
			}
		}
		#endif	// BUILD_MINMAX_LOG

		ReadEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
		if(RTCSetFlag>1)
		{
			RTCSetFlag=0;
			WriteEEPROMData(RTC_SET_FLAG_ADDR,&RTCSetFlag,sizeof(RTCSetFlag));
		}
		
		//Per-slot DP offsets.  Loaded here rather than seeded in the first-boot block so
		//that a unit which has ALREADY had its first boot picks up the default too - these
		//are new addresses, so on any existing device they read back blank.
		//
		//A blank cell is 0xFFFF, which as an int16_t is -1 and would otherwise pass for a
		//valid -0.1 Pa trim, so that exact pattern is treated as 'never written'.  The only
		//cost is that -0.1 Pa cannot be stored; it rounds to 0.
		{
			uint8_t ch,slot;
			uint16_t addr;
			
			for(ch=0; ch<MAX_SUPPORTED_DP; ch++)
			{
				for(slot=0; slot<DP_RANGE_SLOTS; slot++)
				{
					addr = DP_SLOT_OFFSET_ADDR + (((ch*DP_RANGE_SLOTS)+slot)*2);
					
					ReadEEPROMData(addr,(uint8_t*)&DpRangeSlotOffset[ch][slot],sizeof(DpRangeSlotOffset[ch][slot]));
					
					if((DpRangeSlotOffset[ch][slot] == (int16_t)0xFFFF)
						|| (DpRangeSlotOffset[ch][slot] < -DP_SLOT_OFFSET_LIMIT)
						|| (DpRangeSlotOffset[ch][slot] >  DP_SLOT_OFFSET_LIMIT))
					{
						DpRangeSlotOffset[ch][slot] = 0;
						WriteEEPROMData(addr,(uint8_t*)&DpRangeSlotOffset[ch][slot],sizeof(DpRangeSlotOffset[ch][slot]));
					}
					
					IWDG_ReloadCounter();
				}
			}
		}

		if(gu16_parameterWord & ENABLE_DP1)
		{
			//DPressure1 Parameter -----------------------------------------------------
			ReadEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],sizeof(DP_Upper_Alm_ON[DP1]));
			if((DP_Upper_Alm_ON[DP1]<(DP_ALM_LIMIT_MAX*10)) || (DP_Upper_Alm_ON[DP1]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Upper_Alm_ON[DP1]=DEFAULT_DP1_UPPER_ALM_ON;
				WriteEEPROMData(DP1_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP1],sizeof(DP_Upper_Alm_ON[DP1]));
			}
		
			ReadEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],sizeof(DP_Upper_Alm_OFF[DP1]));
			if((DP_Upper_Alm_OFF[DP1]<(DP_ALM_LIMIT_MAX*10)) || (DP_Upper_Alm_OFF[DP1]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Upper_Alm_OFF[DP1]=DEFAULT_DP1_UPPER_ALM_OFF;
				WriteEEPROMData(DP1_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP1],sizeof(DP_Upper_Alm_OFF[DP1]));
			}
		
			ReadEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],sizeof(DP_Lower_Alm_ON[DP1]));
			if((DP_Lower_Alm_ON[DP1]<(DP_ALM_LIMIT_MAX*10)) || (DP_Lower_Alm_ON[DP1]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Lower_Alm_ON[DP1]=DEFAULT_DP1_LOWER_ALM_ON;
				WriteEEPROMData(DP1_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP1],sizeof(DP_Lower_Alm_ON[DP1]));
			}
		
			ReadEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],sizeof(DP_Lower_Alm_OFF[DP1]));
			if((DP_Lower_Alm_OFF[DP1]<(DP_ALM_LIMIT_MAX*10)) || (DP_Lower_Alm_OFF[DP1]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Lower_Alm_OFF[DP1]=DEFAULT_DP1_LOWER_ALM_OFF;
				WriteEEPROMData(DP1_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP1],sizeof(DP_Lower_Alm_OFF[DP1]));
			}
		
			ReadEEPROMData(DP1_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP1],sizeof(DP_Cal_Value_F[DP1]));
			//if((DP_Cal_Value_F[DP1]<(DEFAUT_DP1_MAX*10.0)) || (DP_Cal_Value_F[DP1]>(DEFAUT_DP1_MIN*10.0)))
			//{
				//DP_Cal_Value_F[DP1]=0;
				//WriteEEPROMData(DP1_CAL_VAL_F_ADDR,DP_Cal_Value_F[DP1]);
			//}
			DP_Cal_float_Value_F[DP1] = (float)DP_Cal_Value_F[DP1]/10.0;
			
			ReadEEPROMData(DP1_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP1],sizeof(DP_Cal_Value_C[DP1]));
			//if((DP_Cal_Value_C[DP1]<(DEFAUT_DP1_MAX*10.0)) || (DP_Cal_Value_C[DP1]>(DEFAUT_DP1_MIN*10.0)))
			//{
				//DP_Cal_Value_C[DP1]=0;
				//WriteEEPROMData(DP1_CAL_VAL_C_ADDR,DP_Cal_Value_C[DP1]);
			//}
			DP_Cal_float_Value_C[DP1] = (float)DP_Cal_Value_C[DP1]/10.0;
		
			ReadEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],sizeof(DP_Max[DP1]));
			if(!((DP_Max[DP1]>=DEFAUT_DP1_MAX) && (DP_Max[DP1]<=DEFAUT_DP1_MIN)))
			{
				DP_Max[DP1] = DEFAUT_DP1_MAX;
				WriteEEPROMData(DP1_MAXIMUM,(uint8_t*)&DP_Max[DP1],sizeof(DP_Max[DP1]));
			}
		
			ReadEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],sizeof(DP_Min[DP1]));
			if(!((DP_Min[DP1]>=DEFAUT_DP1_MAX) && (DP_Min[DP1]<=DEFAUT_DP1_MIN)))
			{
				DP_Min[DP1] = DEFAUT_DP1_MIN;
				WriteEEPROMData(DP1_MINIMUM,(uint8_t*)&DP_Min[DP1],sizeof(DP_Min[DP1]));
			}
			
			ReadEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
			if(LastDP_Alrm_ON[DP1]>2)
			{
				LastDP_Alrm_ON[DP1]=0;
				WriteEEPROMData(LAST_DP1_ALRM_STAT,&LastDP_Alrm_ON[DP1],sizeof(LastDP_Alrm_ON[DP1]));
			}
			
			ReadEEPROMData(DP1_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP1],sizeof(gu8_DpAlarmSensingTime[DP1]));
			if(gu8_DpAlarmSensingTime[DP1] > 250)
			{
				gu8_DpAlarmSensingTime[DP1]=5;
				WriteEEPROMData(DP1_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP1],sizeof(gu8_DpAlarmSensingTime[DP1]));
			}
			
			ReadEEPROMData(DP1_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP1],sizeof(DP_UserCalDateInd[DP1]));
			if(DP_UserCalDateInd[DP1]>15)
			{
				DP_UserCalDateInd[DP1]=0;
				WriteEEPROMData(DP1_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP1],sizeof(DP_UserCalDateInd[DP1]));
			}
			
			ReadEEPROMData((DP_SW_FACT_ADDR+(DP1*2)),(uint8_t*)&su16_dp_sw_factor[DP1],sizeof(su16_dp_sw_factor[DP1]));
			if((su16_dp_sw_factor[DP1]<-10000) || (su16_dp_sw_factor[DP1]>10000))
			{	
				su16_dp_sw_factor[DP1]=0;
				WriteEEPROMData((DP_SW_FACT_ADDR+(DP1*2)),(uint8_t*)&su16_dp_sw_factor[DP1],sizeof(su16_dp_sw_factor[DP1]));
			}
			f32_dp_sw_factor[DP1]=(float)su16_dp_sw_factor[DP1]/100.0;
			
			ReadEEPROMData((DP_OFFSET_ADDR),(uint8_t*)&su16_dp_offset[DP1],sizeof(su16_dp_offset[DP1]));
			if((su16_dp_offset[DP1]<-10000) || (su16_dp_offset[DP1]>10000))
			{
				su16_dp_offset[DP1]=0;
				WriteEEPROMData((DP_OFFSET_ADDR),(uint8_t*)&su16_dp_offset[DP1],sizeof(su16_dp_offset[DP1]));
			}
			f32_dp_offset[DP1]=(float)su16_dp_offset[DP1]/100.0;
			
			ReadEEPROMData((DP_LIMIT_ADDR+(DP1*2)),(uint8_t*)&u16_dp_limit[DP1],sizeof(u16_dp_limit[DP1]));
			if((u16_dp_limit[DP1]<500) || (u16_dp_limit[DP1]>9990))
			{
				u16_dp_limit[DP1]=DEFAULT_DP_LIMIT;
				WriteEEPROMData((DP_LIMIT_ADDR+(DP1*2)),(uint8_t*)&u16_dp_limit[DP1],sizeof(u16_dp_limit[DP1]));
			}
			f32_dp_limit[DP1]=(float)u16_dp_limit[DP1]/10.0;
		}
		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		if(gu16_parameterWord & ENABLE_DP2)
		{
			//DPressure2 Parameter -----------------------------------------------------
			ReadEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],sizeof(DP_Upper_Alm_ON[DP2]));
			if((DP_Upper_Alm_ON[DP2]<(DP_ALM_LIMIT_MAX*10)) || (DP_Upper_Alm_ON[DP2]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Upper_Alm_ON[DP2]=DEFAULT_DP2_UPPER_ALM_ON;
				WriteEEPROMData(DP2_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP2],sizeof(DP_Upper_Alm_ON[DP2]));
			}
		
			ReadEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],sizeof(DP_Upper_Alm_OFF[DP2]));
			if((DP_Upper_Alm_OFF[DP2]<(DP_ALM_LIMIT_MAX*10)) || (DP_Upper_Alm_OFF[DP2]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Upper_Alm_OFF[DP2]=DEFAULT_DP2_UPPER_ALM_OFF;
				WriteEEPROMData(DP2_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP2],sizeof(DP_Upper_Alm_OFF[DP2]));
			}
		
			ReadEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],sizeof(DP_Lower_Alm_ON[DP2]));
			if((DP_Lower_Alm_ON[DP2]<(DP_ALM_LIMIT_MAX*10)) || (DP_Lower_Alm_ON[DP2]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Lower_Alm_ON[DP2]=DEFAULT_DP2_LOWER_ALM_ON;
				WriteEEPROMData(DP2_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP2],sizeof(DP_Lower_Alm_ON[DP2]));
			}
		
			ReadEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],sizeof(DP_Lower_Alm_OFF[DP2]));
			if((DP_Lower_Alm_OFF[DP2]<(DP_ALM_LIMIT_MAX*10)) || (DP_Lower_Alm_OFF[DP2]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Lower_Alm_OFF[DP2]=DEFAULT_DP2_LOWER_ALM_OFF;
				WriteEEPROMData(DP2_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP2],sizeof(DP_Lower_Alm_OFF[DP2]));
			}
			
			ReadEEPROMData(DP2_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP2],sizeof(DP_Cal_Value_F[DP2]));
			//if((DP_Cal_Value_F[DP2]<(DEFAUT_DP2_MAX*10.0)) || (DP_Cal_Value_F[DP2]>(DEFAUT_DP2_MIN*10.0)))
			//{
				//DP_Cal_Value_F[DP2]=0;
				//WriteEEPROMData(DP2_CAL_VAL_F_ADDR,DP_Cal_Value_F[DP2]);
			//}
			DP_Cal_float_Value_F[DP2] = (float)DP_Cal_Value_F[DP2]/10.0;
			
			ReadEEPROMData(DP2_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP2],sizeof(DP_Cal_Value_C[DP2]));
			//if((DP_Cal_Value_C[DP2]<(DEFAUT_DP2_MAX*10.0)) || (DP_Cal_Value_C[DP2]>(DEFAUT_DP2_MIN*10.0)))
			//{
				//DP_Cal_Value_C[DP2]=0;
				//WriteEEPROMData(DP2_CAL_VAL_C_ADDR,DP_Cal_Value_C[DP2]);
			//}
			DP_Cal_float_Value_C[DP2] = (float)DP_Cal_Value_C[DP2]/10.0;
		
			ReadEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],sizeof(DP_Max[DP2]));
			if(!((DP_Max[DP2]>=DEFAUT_DP2_MAX) && (DP_Max[DP2]<=DEFAUT_DP2_MIN)))
			{
				DP_Max[DP2] = DEFAUT_DP2_MAX;
				WriteEEPROMData(DP2_MAXIMUM,(uint8_t*)&DP_Max[DP2],sizeof(DP_Max[DP2]));
			}
		
			ReadEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],sizeof(DP_Min[DP2]));
			if(!((DP_Min[DP2]>=DEFAUT_DP2_MAX) && (DP_Min[DP2]<=DEFAUT_DP2_MIN)))
			{
				DP_Min[DP2] = DEFAUT_DP2_MIN;
				WriteEEPROMData(DP2_MINIMUM,(uint8_t*)&DP_Min[DP2],sizeof(DP_Min[DP2]));
			}
			
			ReadEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
			if(LastDP_Alrm_ON[DP2]>2)
			{
				LastDP_Alrm_ON[DP2]=0;
				WriteEEPROMData(LAST_DP2_ALRM_STAT,&LastDP_Alrm_ON[DP2],sizeof(LastDP_Alrm_ON[DP2]));
			}
			
			ReadEEPROMData(DP2_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP2],sizeof(gu8_DpAlarmSensingTime[DP2]));
			if(gu8_DpAlarmSensingTime[DP2] > 250)
			{
				gu8_DpAlarmSensingTime[DP2]=5;
				WriteEEPROMData(DP2_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP2],sizeof(gu8_DpAlarmSensingTime[DP2]));
			}
			
			ReadEEPROMData(DP2_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP2],sizeof(DP_UserCalDateInd[DP2]));
			if(DP_UserCalDateInd[DP2]>15)
			{
				DP_UserCalDateInd[DP2]=0;
				WriteEEPROMData(DP2_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP2],sizeof(DP_UserCalDateInd[DP2]));
			}
			
			ReadEEPROMData((DP_SW_FACT_ADDR+(DP2*2)),(uint8_t*)&su16_dp_sw_factor[DP2],sizeof(su16_dp_sw_factor[DP2]));
			if((su16_dp_sw_factor[DP2]<-10000) || (su16_dp_sw_factor[DP2]>10000))
			{	
				su16_dp_sw_factor[DP2]=0;
				WriteEEPROMData((DP_SW_FACT_ADDR+(DP2*2)),(uint8_t*)&su16_dp_sw_factor[DP2],sizeof(su16_dp_sw_factor[DP2]));
			}
			f32_dp_sw_factor[DP2]=(float)su16_dp_sw_factor[DP2]/100.0;
			
			ReadEEPROMData((DP_OFFSET_ADDR+2),(uint8_t*)&su16_dp_offset[DP2],sizeof(su16_dp_offset[DP2]));
			if((su16_dp_offset[DP2]<-10000) || (su16_dp_offset[DP2]>10000))
			{
				su16_dp_offset[DP2]=0;
				WriteEEPROMData((DP_OFFSET_ADDR+2),(uint8_t*)&su16_dp_offset[DP2],sizeof(su16_dp_offset[DP2]));
			}
			f32_dp_offset[DP2]=(float)su16_dp_offset[DP2]/100.0;
			
			ReadEEPROMData((DP_LIMIT_ADDR+(DP2*2)),(uint8_t*)&u16_dp_limit[DP2],sizeof(u16_dp_limit[DP2]));
			if((u16_dp_limit[DP2]<500) || (u16_dp_limit[DP2]>9990))
			{
				u16_dp_limit[DP2]=DEFAULT_DP_LIMIT;
				WriteEEPROMData((DP_LIMIT_ADDR+(DP2*2)),(uint8_t*)&u16_dp_limit[DP2],sizeof(u16_dp_limit[DP2]));
			}
			f32_dp_limit[DP2]=(float)u16_dp_limit[DP2]/10.0;
		}
		
		if(gu16_parameterWord & ENABLE_DP3)
		{
			//DPressure3 Parameter -----------------------------------------------------
			ReadEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],sizeof(DP_Upper_Alm_ON[DP3]));
			if((DP_Upper_Alm_ON[DP3]<(DP_ALM_LIMIT_MAX*10)) || (DP_Upper_Alm_ON[DP3]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Upper_Alm_ON[DP3]=DEFAULT_DP3_UPPER_ALM_ON;
				WriteEEPROMData(DP3_UP_ALM_ON,(uint8_t*)&DP_Upper_Alm_ON[DP3],sizeof(DP_Upper_Alm_ON[DP3]));
			}
		
			ReadEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],sizeof(DP_Upper_Alm_OFF[DP3]));
			if((DP_Upper_Alm_OFF[DP3]<(DP_ALM_LIMIT_MAX*10)) || (DP_Upper_Alm_OFF[DP3]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Upper_Alm_OFF[DP3]=DEFAULT_DP3_UPPER_ALM_OFF;
				WriteEEPROMData(DP3_UP_ALM_OFF,(uint8_t*)&DP_Upper_Alm_OFF[DP3],sizeof(DP_Upper_Alm_OFF[DP3]));
			}
		
			ReadEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],sizeof(DP_Lower_Alm_ON[DP3]));
			if((DP_Lower_Alm_ON[DP3]<(DP_ALM_LIMIT_MAX*10)) || (DP_Lower_Alm_ON[DP3]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Lower_Alm_ON[DP3]=DEFAULT_DP3_LOWER_ALM_ON;
				WriteEEPROMData(DP3_LO_ALM_ON,(uint8_t*)&DP_Lower_Alm_ON[DP3],sizeof(DP_Lower_Alm_ON[DP3]));
			}
		
			ReadEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],sizeof(DP_Lower_Alm_OFF[DP3]));
			if((DP_Lower_Alm_OFF[DP3]<(DP_ALM_LIMIT_MAX*10)) || (DP_Lower_Alm_OFF[DP3]>(DP_ALM_LIMIT_MIN*10)))
			{
				DP_Lower_Alm_OFF[DP3]=DEFAULT_DP3_LOWER_ALM_OFF;
				WriteEEPROMData(DP3_LO_ALM_OFF,(uint8_t*)&DP_Lower_Alm_OFF[DP3],sizeof(DP_Lower_Alm_OFF[DP3]));
			}
			
			ReadEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3],sizeof(DP_Cal_Value_F[DP3]));
			//if((DP_Cal_Value_F[DP3]<(DEFAUT_DP3_MAX*10.0)) || (DP_Cal_Value_F[DP3]>(DEFAUT_DP3_MIN*10.0)))
			//{
				//DP_Cal_Value_F[DP3]=0;
				//WriteEEPROMData(DP3_CAL_VAL_F_ADDR,(uint8_t*)&DP_Cal_Value_F[DP3],2);
			//}
			DP_Cal_float_Value_F[DP3] = (float)DP_Cal_Value_F[DP3]/10.0;
			
			ReadEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],sizeof(DP_Cal_Value_C[DP3]));
			//if((DP_Cal_Value_C[DP3]<(DEFAUT_DP3_MAX*10.0)) || (DP_Cal_Value_C[DP3]>(DEFAUT_DP3_MIN*10.0)))
			//{
				//DP_Cal_Value_C[DP3]=0;
				//WriteEEPROMData(DP3_CAL_VAL_C_ADDR,(uint8_t*)&DP_Cal_Value_C[DP3],2);
			//}
			DP_Cal_float_Value_C[DP3] = (float)DP_Cal_Value_C[DP3]/10.0;
		
			ReadEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],sizeof(DP_Max[DP3]));
			if(!((DP_Max[DP3]>=DEFAUT_DP3_MAX) && (DP_Max[DP3]<=DEFAUT_DP3_MIN)))
			{
				DP_Max[DP3] = DEFAUT_DP3_MAX;
				WriteEEPROMData(DP3_MAXIMUM,(uint8_t*)&DP_Max[DP3],sizeof(DP_Max[DP3]));
			}
		
			ReadEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],sizeof(DP_Min[DP3]));
			if(!((DP_Min[DP3]>=DEFAUT_DP3_MAX) && (DP_Min[DP3]<=DEFAUT_DP3_MIN)))
			{
				DP_Min[DP3] = DEFAUT_DP3_MIN;
				WriteEEPROMData(DP3_MINIMUM,(uint8_t*)&DP_Min[DP3],sizeof(DP_Min[DP3]));
			}
			
			ReadEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
			if(LastDP_Alrm_ON[DP3]>2)
			{
				LastDP_Alrm_ON[DP3]=0;
				WriteEEPROMData(LAST_DP3_ALRM_STAT,&LastDP_Alrm_ON[DP3],sizeof(LastDP_Alrm_ON[DP3]));
			}
			
			ReadEEPROMData(DP3_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP3],sizeof(gu8_DpAlarmSensingTime[DP3]));
			if(gu8_DpAlarmSensingTime[DP3] > 250)
			{
				gu8_DpAlarmSensingTime[DP3]=5;
				WriteEEPROMData(DP3_ALM_SENSE_TIME_ADDR,&gu8_DpAlarmSensingTime[DP3],sizeof(gu8_DpAlarmSensingTime[DP3]));
			}
			
			ReadEEPROMData(DP3_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP3],sizeof(DP_UserCalDateInd[DP3]));
			if(DP_UserCalDateInd[DP3]>15)
			{
				DP_UserCalDateInd[DP3]=0;
				WriteEEPROMData(DP3_USER_CAL_DATE_IND_ADDR,&DP_UserCalDateInd[DP3],sizeof(DP_UserCalDateInd[DP3]));
			}
			
			ReadEEPROMData((DP_SW_FACT_ADDR+(DP3*2)),(uint8_t*)&su16_dp_sw_factor[DP3],sizeof(su16_dp_sw_factor[DP3]));
			if((su16_dp_sw_factor[DP3]<-10000) || (su16_dp_sw_factor[DP3]>10000))
			{	
				su16_dp_sw_factor[DP3]=0;
				WriteEEPROMData((DP_SW_FACT_ADDR+(DP3*2)),(uint8_t*)&su16_dp_sw_factor[DP3],sizeof(su16_dp_sw_factor[DP3]));
			}
			f32_dp_sw_factor[DP3]=(float)su16_dp_sw_factor[DP3]/100.0;
			
			ReadEEPROMData((DP_OFFSET_ADDR+4),(uint8_t*)&su16_dp_offset[DP3],sizeof(su16_dp_offset[DP3]));
			if((su16_dp_offset[DP3]<-10000) || (su16_dp_offset[DP3]>10000))
			{
				su16_dp_offset[DP3]=0;
				WriteEEPROMData((DP_OFFSET_ADDR+4),(uint8_t*)&su16_dp_offset[DP3],sizeof(su16_dp_offset[DP3]));
			}
			f32_dp_offset[DP3]=(float)su16_dp_offset[DP3]/100.0;
			
			ReadEEPROMData((DP_LIMIT_ADDR+(DP3*2)),(uint8_t*)&u16_dp_limit[DP3],sizeof(u16_dp_limit[DP3]));
			if((u16_dp_limit[DP3]<500) || (u16_dp_limit[DP3]>9990))
			{
				u16_dp_limit[DP3]=DEFAULT_DP_LIMIT;
				WriteEEPROMData((DP_LIMIT_ADDR+(DP3*2)),(uint8_t*)&u16_dp_limit[DP3],sizeof(u16_dp_limit[DP3]));
			}
			f32_dp_limit[DP3]=(float)u16_dp_limit[DP3]/10.0;
		}
		#else
		if(gu16_parameterWord & ENABLE_TEMP)
		{
			//Temperature Parameter -----------------------------------------------------
			ReadEEPROMData(TEMP_UNIT,(uint8_t*)&TM_Unit,sizeof(TM_Unit));
			if(TM_Unit>1)
			{
				TM_Unit=0;
				WriteEEPROMData(TEMP_UNIT,(uint8_t*)&TM_Unit,sizeof(TM_Unit));
			}
			
			ReadEEPROMData(TM_CAL_VAL_F_ADDR,(uint8_t*)&TM_Cal_Value_F,sizeof(TM_Cal_Value_F));			
			ReadEEPROMData(TM_CAL_VAL_C_ADDR,(uint8_t*)&TM_Cal_Value_C,sizeof(TM_Cal_Value_C));
			
			ReadEEPROMData(TEMP_UP_ALM_ON,(uint8_t*)&TM_Upper_Alm_ON,sizeof(TM_Upper_Alm_ON));
			ReadEEPROMData(TEMP_UP_ALM_OFF,(uint8_t*)&TM_Upper_Alm_OFF,sizeof(TM_Upper_Alm_OFF));
			ReadEEPROMData(TEMP_LO_ALM_ON,(uint8_t*)&TM_Lower_Alm_ON,sizeof(TM_Lower_Alm_ON));
			ReadEEPROMData(TEMP_LO_ALM_OFF,(uint8_t*)&TM_Lower_Alm_OFF,sizeof(TM_Lower_Alm_OFF));
		
			if(!TM_Unit)
			{
				if((TM_Upper_Alm_ON<(DEFAUT_TEMP_C_MAX*10)) || (TM_Upper_Alm_ON>(DEFAUT_TEMP_C_MIN*10)))
				{
					TM_Upper_Alm_ON=DEFAULT_TM_C_UPPER_ALM_ON;
					WriteEEPROMData(TEMP_UP_ALM_ON,(uint8_t*)&TM_Upper_Alm_ON,sizeof(TM_Upper_Alm_ON));
				}
			
				if((TM_Upper_Alm_OFF<(DEFAUT_TEMP_C_MAX*10)) || (TM_Upper_Alm_OFF>(DEFAUT_TEMP_C_MIN*10)))
				{
					TM_Upper_Alm_OFF=DEFAULT_TM_C_UPPER_ALM_OFF;
					WriteEEPROMData(TEMP_UP_ALM_OFF,(uint8_t*)&TM_Upper_Alm_OFF,sizeof(TM_Upper_Alm_OFF));
				}
			
				if((TM_Lower_Alm_ON<(DEFAUT_TEMP_C_MAX*10)) || (TM_Lower_Alm_ON>(DEFAUT_TEMP_C_MIN*10)))
				{
					TM_Lower_Alm_ON=DEFAULT_TM_C_LOWER_ALM_ON;
					WriteEEPROMData(TEMP_LO_ALM_ON,(uint8_t*)&TM_Lower_Alm_ON,sizeof(TM_Lower_Alm_ON));
				}
			
				if((TM_Lower_Alm_OFF<(DEFAUT_TEMP_C_MAX*10)) || (TM_Lower_Alm_OFF>(DEFAUT_TEMP_C_MIN*10)))
				{
					TM_Lower_Alm_OFF=DEFAULT_TM_C_LOWER_ALM_OFF;
					WriteEEPROMData(TEMP_LO_ALM_OFF,(uint8_t*)&TM_Lower_Alm_OFF,sizeof(TM_Lower_Alm_OFF));
				}
			}
			else
			{
				if((TM_Upper_Alm_ON<(DEFAUT_TEMP_F_MAX*10)) || (TM_Upper_Alm_ON>(DEFAUT_TEMP_F_MIN*10)))
				{
					TM_Upper_Alm_ON=DEFAULT_TM_F_UPPER_ALM_ON;
					WriteEEPROMData(TEMP_UP_ALM_ON,(uint8_t*)&TM_Upper_Alm_ON,sizeof(TM_Upper_Alm_ON));
				}
			
				if((TM_Upper_Alm_OFF<(DEFAUT_TEMP_F_MAX*10)) || (TM_Upper_Alm_OFF>(DEFAUT_TEMP_F_MIN*10)))
				{
					TM_Upper_Alm_OFF=DEFAULT_TM_F_UPPER_ALM_OFF;
					WriteEEPROMData(TEMP_UP_ALM_OFF,(uint8_t*)&TM_Upper_Alm_OFF,sizeof(TM_Upper_Alm_OFF));
				}
			
				if((TM_Lower_Alm_ON<(DEFAUT_TEMP_F_MAX*10)) || (TM_Lower_Alm_ON>(DEFAUT_TEMP_F_MIN*10)))
				{
					TM_Lower_Alm_ON=DEFAULT_TM_F_LOWER_ALM_ON;
					WriteEEPROMData(TEMP_LO_ALM_ON,(uint8_t*)&TM_Lower_Alm_ON,sizeof(TM_Lower_Alm_ON));
				}
			
				if((TM_Lower_Alm_OFF<(DEFAUT_TEMP_F_MAX*10)) || (TM_Lower_Alm_OFF>(DEFAUT_TEMP_F_MIN*10)))
				{
					TM_Lower_Alm_OFF=DEFAULT_TM_F_LOWER_ALM_OFF;
					WriteEEPROMData(TEMP_LO_ALM_OFF,(uint8_t*)&TM_Lower_Alm_OFF,sizeof(TM_Lower_Alm_OFF));
				}
				
				//Nothing to convert: the stored corrections are already in tenths of a degree
				//C and are subtracted from temperatureC before the display unit is applied.
				//This used to run the absolute C-to-F formula over a difference, scaling it
				//and shifting it by 32 on every start in Fahrenheit.
			}
		
			TM_Cal_float_Value_F = (float)TM_Cal_Value_F/10.0;
			TM_Cal_float_Value_C = (float)TM_Cal_Value_C/10.0;
			
			ReadEEPROMData(TEMP_MAXIMUM,(uint8_t*)&TM_Max,sizeof(TM_Max));
			if(!((TM_Max>=DEFAUT_TEMP_C_MAX) && (TM_Max<=DEFAUT_TEMP_C_MIN)))
			{
				TM_Max = DEFAUT_TEMP_C_MAX;
				WriteEEPROMData(TEMP_MAXIMUM,(uint8_t*)&TM_Max,sizeof(TM_Max));
			}
		
			ReadEEPROMData(TEMP_MINIMUM,(uint8_t*)&TM_Min,sizeof(TM_Min));
			if(!((TM_Min>=DEFAUT_TEMP_C_MAX) && (TM_Min<=DEFAUT_TEMP_C_MIN)))
			{
				TM_Min = DEFAUT_TEMP_C_MIN;
				WriteEEPROMData(TEMP_MINIMUM,(uint8_t*)&TM_Min,sizeof(TM_Min));
			}
			
			ReadEEPROMData(LAST_TM_ALRM_STAT,(uint8_t*)&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
			if(LastTM_Alrm_ON>2)
			{
				LastTM_Alrm_ON=0;
				WriteEEPROMData(LAST_TM_ALRM_STAT,(uint8_t*)&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
			}
			
			ReadEEPROMData(TM_USER_CAL_DATE_IND_ADDR,&TM_UserCalDateInd,sizeof(TM_UserCalDateInd));
			if(TM_UserCalDateInd>15)
			{
				TM_UserCalDateInd=0;
				WriteEEPROMData(TM_USER_CAL_DATE_IND_ADDR,&TM_UserCalDateInd,sizeof(TM_UserCalDateInd));
			}
		}
		
		if(gu16_parameterWord & ENABLE_RH)
		{
			//RHumidity Parameter -----------------------------------------------------
			ReadEEPROMData(RH_UP_ALM_ON,(uint8_t*)&RH_Upper_Alm_ON,sizeof(RH_Upper_Alm_ON));
			if(RH_Upper_Alm_ON>(DEFAUT_RH_MIN*10))
			{
				RH_Upper_Alm_ON=DEFAULT_RH_UPPER_ALM_ON;
				WriteEEPROMData(RH_UP_ALM_ON,(uint8_t*)&RH_Upper_Alm_ON,sizeof(RH_Upper_Alm_ON));
			}
		
			ReadEEPROMData(RH_UP_ALM_OFF,(uint8_t*)&RH_Upper_Alm_OFF,sizeof(RH_Upper_Alm_OFF));
			if(RH_Upper_Alm_OFF>(DEFAUT_RH_MIN*10))
			{
				RH_Upper_Alm_OFF=DEFAULT_RH_UPPER_ALM_OFF;
				WriteEEPROMData(RH_UP_ALM_OFF,(uint8_t*)&RH_Upper_Alm_OFF,sizeof(RH_Upper_Alm_OFF));
			}
		
			ReadEEPROMData(RH_LO_ALM_ON,(uint8_t*)&RH_Lower_Alm_ON,sizeof(RH_Lower_Alm_ON));
			if(RH_Lower_Alm_ON>(DEFAUT_RH_MIN*10))
			{
				RH_Lower_Alm_ON=DEFAULT_RH_LOWER_ALM_ON;
				WriteEEPROMData(RH_LO_ALM_ON,(uint8_t*)&RH_Lower_Alm_ON,sizeof(RH_Lower_Alm_ON));
			}
		
			ReadEEPROMData(RH_LO_ALM_OFF,(uint8_t*)&RH_Lower_Alm_OFF,sizeof(RH_Lower_Alm_OFF));
			if(RH_Lower_Alm_OFF>(DEFAUT_RH_MIN*10))
			{
				RH_Lower_Alm_OFF=DEFAULT_RH_LOWER_ALM_OFF;
				WriteEEPROMData(RH_LO_ALM_OFF,(uint8_t*)&RH_Lower_Alm_OFF,sizeof(RH_Lower_Alm_OFF));
			}
		
			ReadEEPROMData(RH_CAL_VAL_F_ADDR,(uint8_t*)&RH_Cal_Value_F,sizeof(RH_Cal_Value_F));
			//if((RH_Cal_Value_F<(-DEFAUT_RH_MIN*10.0)) || (RH_Cal_Value_F>(DEFAUT_RH_MIN*10.0)))
			//{
				//RH_Cal_Value_F=0;
				WriteEEPROMData(RH_CAL_VAL_F_ADDR,(uint8_t*)&RH_Cal_Value_F,2);
			//}
			RH_Cal_float_Value_F = (float)RH_Cal_Value_F/10.0;
			
			ReadEEPROMData(RH_CAL_VAL_C_ADDR,(uint8_t*)&RH_Cal_Value_C,sizeof(RH_Cal_Value_C));
			//if((RH_Cal_Value_C<(DEFAUT_RH_MAX*10.0)) || (RH_Cal_Value_C>(DEFAUT_RH_MIN*10.0)))
			//{
				//RH_Cal_Value_C=0;
				//WriteEEPROMData(RH_CAL_VAL_C_ADDR,(uint8_t*)&RH_Cal_Value_C,2);
			//}
			RH_Cal_float_Value_C = (float)RH_Cal_Value_C/10.0;
		
			ReadEEPROMData(RH_MAXIMUM,(uint8_t*)&RH_Max,sizeof(RH_Max));
			if(!((RH_Max>=DEFAUT_RH_MAX) && (RH_Max<=DEFAUT_RH_MIN)))
			{
				RH_Max = DEFAUT_RH_MAX;
				WriteEEPROMData(RH_MAXIMUM,(uint8_t*)&RH_Max,sizeof(RH_Max));
			}
		
			ReadEEPROMData(RH_MINIMUM,(uint8_t*)&RH_Min,sizeof(RH_Min));
			if(!((RH_Min>=DEFAUT_RH_MAX) && (RH_Min<=DEFAUT_RH_MIN)))
			{
				RH_Min = DEFAUT_RH_MIN;
				WriteEEPROMData(RH_MINIMUM,(uint8_t*)&RH_Min,sizeof(RH_Min));
			}
			
			ReadEEPROMData(LAST_RH_ALRM_STAT,(uint8_t*)&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
			if(LastRH_Alrm_ON>2)
			{
				LastRH_Alrm_ON=0;
				WriteEEPROMData(LAST_RH_ALRM_STAT,(uint8_t*)&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
			}
			
			ReadEEPROMData(RH_USER_CAL_DATE_IND_ADDR,&RH_UserCalDateInd,sizeof(RH_UserCalDateInd));
			if(RH_UserCalDateInd>15)
			{
				RH_UserCalDateInd=0;
				WriteEEPROMData(RH_USER_CAL_DATE_IND_ADDR,&RH_UserCalDateInd,sizeof(RH_UserCalDateInd));
			}
		}
		#endif
		
		ReadEEPROMData(RELAY_STAT_ADDR,&gu8_rly_stat,sizeof(gu8_rly_stat));
		
		ReadEEPROMData(BROADCAST_ENB_ADDR,&gu8_broadcast,sizeof(gu8_broadcast));
		if(gu8_broadcast>1)
		{	
			gu8_broadcast=0;
			WriteEEPROMData(BROADCAST_ENB_ADDR,&gu8_broadcast,sizeof(gu8_broadcast));
		}
		
		//RS485 Parameter -------------------------------------------
		ReadEEPROMData(DEVICE_ID,&DeviceID,sizeof(DeviceID));
		if(DeviceID>250)
		{
			DeviceID=DEFAULT_DEVICE_ID;
			WriteEEPROMData(DEVICE_ID,&DeviceID,sizeof(DeviceID));
		}
		
		//Buzzer Parameter -------------------------------------------
		ReadEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,sizeof(Buzzer_ON_Time));
		if(Buzzer_ON_Time>60)
		{
			Buzzer_ON_Time=0;
			WriteEEPROMData(BUZZER_ON_TIME,(uint8_t*)&Buzzer_ON_Time,sizeof(Buzzer_ON_Time));
		}
		
		ReadEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,sizeof(Buzzer_OFF_Time));
		if(Buzzer_OFF_Time>960)
		{
			Buzzer_OFF_Time=0;
			WriteEEPROMData(BUZZER_OFF_TIME,(uint8_t*)&Buzzer_OFF_Time,sizeof(Buzzer_OFF_Time));
		}

		//UART Parameter -------------------------------------------
		ReadEEPROMData(UART_BAUDRATE,&UART_BaudRate,sizeof(UART_BaudRate));
		if((UART_BaudRate<3) || (UART_BaudRate>9))
		{
			UART_BaudRate=DEFAULT_UART_BAUDRATE;
			WriteEEPROMData(UART_BAUDRATE,&UART_BaudRate,sizeof(UART_BaudRate));
		}
		
		//Customer Password -------------------------------------------
		ReadEEPROMData(CUSTOMER_PASSWORD,(uint8_t*)&CustPassword,sizeof(CustPassword));
		if(CustPassword>999)
		{
			CustPassword=DEFAULT_CUSTOMER_PWD;
			WriteEEPROMData(CUSTOMER_PASSWORD,(uint8_t*)&CustPassword,sizeof(CustPassword));
		}
		
		//Factory Customer Password -------------------------------------------
		ReadEEPROMData(FAC_CUSTOMER_PASSWORD,(uint8_t*)&FactCustPassword,sizeof(FactCustPassword));
		if(FactCustPassword>9999)
		{
			FactCustPassword=DEFAULT_FACTORY_PWD;
			WriteEEPROMData(FAC_CUSTOMER_PASSWORD,(uint8_t*)&FactCustPassword,sizeof(FactCustPassword));
		}
		
		//Acknowledge Parameter -------------------------------------------
		ReadEEPROMData(ACK_TIMER,(uint8_t*)&AckTimer,sizeof(AckTimer));
		if(AckTimer>1440)
		{
			AckTimer=0;
			WriteEEPROMData(ACK_TIMER,(uint8_t*)&AckTimer,sizeof(AckTimer));
		}
		
		ReadEEPROMData(ACK_PWD_IND,&AckPwdInd,sizeof(AckPwdInd));
		if(AckPwdInd>NO_OF_ACKPWD)
		{
			AckPwdInd=0;
			WriteEEPROMData(ACK_PWD_IND,&AckPwdInd,sizeof(AckPwdInd));
		}
		
		for(uint8_t i=0;i<NO_OF_ACKPWD;i++)
		{
			ReadEEPROMData((ACK_PASSWORD+(i*2)),(uint8_t*)&AckPwd[i],sizeof(AckPwd[i]));
			if(AckPwd[i]>999)
			{
				AckPwd[i]=0;
				WriteEEPROMData((ACK_PASSWORD+(i*2)),(uint8_t*)&AckPwd[i],sizeof(AckPwd[i]));
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
		
		if(gu16_parameterWord & ENABLE_LOG)
		{
			//Data Logging Parameter -------------------------------------------
			ReadEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,sizeof(LogInterval));
			if((LogInterval<MIN_LOG_INTERVAL) || (LogInterval>MAX_LOG_INTERVAL))
			{
				LogInterval=DEFAULT_LOG_INTERVAL;
				WriteEEPROMData(LOG_INTERVAL,(uint8_t*)&LogInterval,sizeof(LogInterval));
			}
		
			ReadEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
			if(FlashOVFByte>1)
			{
				FlashOVFByte=0;
				WriteEEPROMData(FLSH_OVF_IND,&FlashOVFByte,sizeof(FlashOVFByte));
			}
			
			LoadCurrentLogInd();
			LoadCurrentLog24Ind();
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
	#if BUILD_RAM_BUFFER
	memset(&RAMBuffer[0],0,sizeof(RAMBuffer));
	#endif
	
	bool_autoSendResponse = false;
	bool_triggerXbeeReset = false;
	gu32_triggerXbeeResetTimer = (unsigned long)gu16_XbeeRstInterval*60;
	
	//gu8_AutoSentTimer=gu8_AutoSentInterval;
	gu8_groupID = ((DeviceID - 1)/gu8_DeviceInGroup)+1;
	
	logTimer = LogInterval;
	bool_brodcastEnb=0;	
	gu16_logtransfer=0;
	bool_logtransferStart=0;
	DP_StartUpTimer=10;
	TMRH_StartUpTimer=10;
	bool_resetDevice=0;
	
	bool_DPLog[DP1]=0;
	#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
	bool_DPLog[DP2]=0;
	bool_DPLog[DP3]=0;
	#else
	bool_TMLog=0;
	bool_RHLog=0;
	#endif
	
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

	for(i=0;i<5;i++)
	{
		Kalman_Init(&Kalman[i], KALMAN_Q, KALMAN_R, 0.0);	// tuning lives in sb_const.h
	}
	
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

			//The timestamp of the last 24 hour record is how the device works out how many
			//day boundaries it slept through.  Without that ring there is nothing to read,
			//so ep1 stays as initialised and the catch-up below simply does not run.
			#if BUILD_LOG24_LOG
			if(us1)
			{
				us1--;
			}
			else
			{
				us1=LAST_LOG24_ADDR-1;
			}
	
			ReadEEPROMData(LAST_LOG24_ADDR_OFFSET + ((uint32_t)us1*LOG_SIZE),(unsigned char*)&ep1.currentEpochTime,4);
			#endif	// BUILD_LOG24_LOG
	
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
				
					#if (BUILD_MINMAX_LOG || BUILD_MEAN24_LOG)
					if((gu16_parameterWord & ENABLE_DATAFLASH) && (gu16_parameterWord & ENABLE_M3LOG))
					{
						#if BUILD_MINMAX_LOG
						memcpy(&MinMaxMeanDayLogArr[0],(unsigned char*)&ep1.currentEpochTime,4);
				
						//Find DP1 Mean Value from last 24 Hour and Store it ---------------------------------------------
						if(gu16_parameterWord & ENABLE_DP1)
						{
							#if BUILD_MEAN24_LOG
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
							#else
							//24 h mean log not built - the daily record keeps min and max, mean is zero
							DP_Mean[DP1] = 0;
							#endif
					
							memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&DP_Min[DP1],4);
							memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&DP_Max[DP1],4);
							memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&DP_Mean[DP1],4);
							WriteLog(LAST_DP1_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
						}
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
						//Find DP2 Mean Value from last 24 Hour and Store it ---------------------------------------------
						if(gu16_parameterWord & ENABLE_DP2)
						{
							#if BUILD_MEAN24_LOG
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
							#else
							//24 h mean log not built - the daily record keeps min and max, mean is zero
							DP_Mean[DP2] = 0;
							#endif

							memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&DP_Min[DP2],4);
							memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&DP_Max[DP2],4);
							memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&DP_Mean[DP2],4);
							WriteLog(LAST_DP2_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
						}
				
						//Find DP3 Mean Value from last 24 Hour and Store it ---------------------------------------------
						if(gu16_parameterWord & ENABLE_DP3)
						{
							#if BUILD_MEAN24_LOG
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
							#else
							//24 h mean log not built - the daily record keeps min and max, mean is zero
							DP_Mean[DP3] = 0;
							#endif

							memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&DP_Min[DP3],4);
							memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&DP_Max[DP3],4);
							memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&DP_Mean[DP3],4);
							WriteLog(LAST_DP3_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
						}
						#else
						//Find TM Mean Value from last 24 Hour and Store it ---------------------------------------------
						if(gu16_parameterWord & ENABLE_TEMP)
						{
							#if BUILD_MEAN24_LOG
							ReadMinMaxLog(TM_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							TM_Mean=0;
							a2=0;
							for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
							{
								memcpy((unsigned char*)&tempfloat1,&Buffer1[a2],4);
								a2 += 4;
								TM_Mean += tempfloat1;
							}
							TM_Mean /= TOTAL_MEAN_HOUR;
							#else
							//24 h mean log not built - the daily record keeps min and max, mean is zero
							TM_Mean = 0;
							#endif
						
							memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&TM_Min,4);
							memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&TM_Max,4);
							memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&TM_Mean,4);
							WriteLog(LAST_TM_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
						}
						
						//Find RH Mean Value from last 24 Hour and Store it ---------------------------------------------
						if(gu16_parameterWord & ENABLE_RH)
						{
							#if BUILD_MEAN24_LOG
							ReadMinMaxLog(RH_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							RH_Mean=0;
							a2=0;
							for(a1=0;a1<TOTAL_MEAN_HOUR;a1++)
							{
								memcpy((unsigned char*)&tempfloat1,&Buffer1[a2],4);
								a2 += 4;
								RH_Mean += tempfloat1;
							}
							RH_Mean /= TOTAL_MEAN_HOUR;
							#else
							//24 h mean log not built - the daily record keeps min and max, mean is zero
							RH_Mean = 0;
							#endif
					
							memcpy(&MinMaxMeanDayLogArr[4],(unsigned char*)&RH_Min,4);
							memcpy(&MinMaxMeanDayLogArr[8],(unsigned char*)&RH_Max,4);
							memcpy(&MinMaxMeanDayLogArr[12],(unsigned char*)&RH_Mean,4);
							WriteLog(LAST_RH_MIN_MAX_OFFSET,MinMaxMeanDayLogInd,&MinMaxMeanDayLogArr[0],MIN_MAX_MEAN_LOG_SIZE);
						}
						#endif
						#endif	// BUILD_MINMAX_LOG

						//Clear all Hour mean value for next day
						#if BUILD_MEAN24_LOG
						memset(Buffer1,0,100);
						if(gu16_parameterWord & ENABLE_DP1)
						{
							WriteLog(DP1_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							HourDP_Mean[DP1]=0;
							HrDPSampleInd[DP1]=0;
						}
						#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
						#else
						if(gu16_parameterWord & ENABLE_TEMP)
						{
							WriteLog(TM_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							HourTM_Mean=0;
							HrTMSampleInd=0;
						}
						if(gu16_parameterWord & ENABLE_RH)
						{
							WriteLog(RH_CURR_24HR_MEAN_OFFSET,0,&Buffer1[0],HOUR_MEAN_VALUE_SPACE);
							HourRH_Mean=0;
							HrRHSampleInd=0;
						}
						#endif
				
						#endif	// BUILD_MEAN24_LOG

						#if BUILD_MINMAX_LOG
						MinMaxMeanDayLogInd++;
						if(MinMaxMeanDayLogInd>=TOTAL_MIN_MAX_MEAN_LOG) MinMaxMeanDayLogInd=0;
						WriteEEPROMData(MIN_MAX_LOG_IND_ADDR,&MinMaxMeanDayLogInd,sizeof(MinMaxMeanDayLogInd));
						#endif	// BUILD_MINMAX_LOG
					}
					#endif	// (BUILD_MINMAX_LOG || BUILD_MEAN24_LOG)
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
				#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
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
				#else
				if(gu16_parameterWord & ENABLE_TEMP)
				{
					//Serve Watchdog Timer
					IWDG_ReloadCounter();
			
					if(LastTM_Alrm_ON)
					{
						LogReading(TM_ALM_RESTORE_LOG,0,0xFFFF);
						FillRamBuffer(TM_ALM_RESTORE_LOG,0,0xFFFF);
				
						LastTM_Alrm_ON=NO_ALARM;
						WriteEEPROMData(LAST_TM_ALRM_STAT,&LastTM_Alrm_ON,sizeof(LastTM_Alrm_ON));
					}
				}
				
				if(gu16_parameterWord & ENABLE_RH)
				{
					//Serve Watchdog Timer
					IWDG_ReloadCounter();
			
					if(LastRH_Alrm_ON)
					{
						LogReading(RH_ALM_RESTORE_LOG,0,0xFFFF);
						FillRamBuffer(RH_ALM_RESTORE_LOG,0,0xFFFF);
				
						LastRH_Alrm_ON=NO_ALARM;
						WriteEEPROMData(LAST_RH_ALRM_STAT,&LastRH_Alrm_ON,sizeof(LastRH_Alrm_ON));
					}
				}
				#endif
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
}

uint8_t test1=0;

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
	DF_Init();
	DF_ConfigurePageSize();
	
	//Bring-up diagnostic - XM25_SelfTest() only exists in the XM25 build.
	//Result codes are listed in Interface/XM25QH128A.h (0 = pass).
	#if ((DATAFLASH_PART == DATAFLASH_XM25QH128A) && XM25_ENABLE_SELFTEST)
	test1=XM25_SelfTest();
	#endif
	
	boot_data();	//Boot Data from Dataflash

	//-------------------------------------------------------
	//Initialize UART
	//-------------------------------------------------------
	UART_Configure(UART_BaudRate);
	printf("Powered ON\n");
	
	//boot_data() seeds and range-checks the real-time parameters, which on a NOR
	//part land in the RAM mirror rather than in flash.  Commit them now: a fresh
	//unit power-cycled before the first timed flush would otherwise come back up
	//reading an erased (0xFF) record.
	DF_RtFlush();

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
	
	//gu16_logoAckBlinkTimer = LOGO_ACK_BLINK_TICKS;
	
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

