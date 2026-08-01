/***********************************************************************************************************************
    @file    main.h
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
#ifndef _MAIN_H_
#define _MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Files include */
#include <stdint.h>
#include "sb_variables.h"
#include "sb_const.h"

/* Exported types *****************************************************************************************************/

/* Exported constants *************************************************************************************************/

/* Exported macro *****************************************************************************************************/

/* Exported variables *************************************************************************************************/
// Kalman filter structure
typedef struct {
	float Q1;       // Process noise covariance
	float R1;       // Measurement noise covariance
	float X1;       // State estimate
	float P1;       // Estimate covariance
	float K1;       // Kalman gain
} KalmanFilter;
static KalmanFilter Kalman[5] = {0};		

typedef struct __attribute__((__packed__))
{
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	
}RTCData;

RTCData rtc,rtc1,rtc2,rtc3;

typedef union
{
	uint32_t currentEpochTime;
	uint8_t cept[4];
}Epoch_t;

Epoch_t ep,ep1;

/* Exported functions *************************************************************************************************/
void AllSegment(uint8_t state);
void InitLEDController(void);
void ReadADCChannel(void);
void disp_value(void);
void conv_value(void);
void chartostr(unsigned short,uint8_t *,uint8_t);
void convert_float(float,uint8_t*,uint8_t);
void convert_char(unsigned short,uint8_t*,uint8_t);
void convert_long(unsigned long,uint8_t*,uint8_t);
////----------------------------------------------------------------------------------------------------------------------------
void Kalman_Init(KalmanFilter *kf, float qr, float rs, float initial_estimate);
float Kalman_Update(KalmanFilter *kf, float measurement);
////----------------------------------------------------------------------------------------------------------------------------
static inline int leapyear (long int year);
unsigned long ydhms_diff (unsigned long int year1, unsigned long int yday1, unsigned int hour1, unsigned int min1, unsigned int sec1, unsigned int year0, unsigned int yday0, unsigned int hour0, unsigned int min0, unsigned int sec0);
unsigned long get_epoch_time(RTCData);
void get_date_time(RTCData* t1,unsigned long epoch);
void Check_RTC(void);
void SecondTick(void);
void boot_data(void);
//----------------------------------------------------------------------------------------------------------------------------
void LogReading(uint8_t logtype,uint8_t userID,uint16_t password);
void FillRamBuffer(uint8_t logtype,uint8_t userID,uint16_t password);
void ResetMinMax(void);
//----------------------------------------------------------------------------------------------------------------------------
void opstr(char *str);
void opchar(uint8_t str);
void SendToUART(uint8_t *str,uint16_t NoOfBytes);
uint8_t find_Checksum(uint16_t Count,uint8_t *msg);
uint8_t fillValue(uint8_t *ptr,long value);
int16_t findValue(uint8_t *ptr,uint8_t NoOfDigit);
uint8_t CalCRC(uint8_t *ptr,uint16_t NoOfByte);
void SetTxmode(uint8_t *buffer,uint16_t bytes);
uint32_t ascii2hex(uint8_t *data, uint8_t NoOfdigit);
uint32_t FindLogIndex(uint32_t EpochTime,uint32_t InitLogInd,uint32_t LastLogInd);
void ServePCMsg(void);
void print_short(long val,char *data1,uint8_t no_of_digit);
//----------------------------------------------------------------------------------------------------------------------------
void ReadDiffPressure(uint8_t, uint32_t);
//----------------------------------------------------------------------------------------------------------------------------
#ifdef ENABLE_KEY_LOGIC
void check_key(void);
void keyboard(void);
void CheckUpDnKey(void);
#endif
////----------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif /* _MAIN_H_ */

/********************************************** (C) Copyright MindMotion **********************************************/

