
#ifndef SB_GLOBAL_VAR_H_
#define SB_GLOBAL_VAR_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "sb_const.h"

extern uint16_t gu16_parameterWord;
extern uint8_t UART_BaudRate;
extern uint8_t DeviceID;
extern uint32_t gu32_SrNumber;
extern uint8_t RTC_data[7];
extern uint8_t mode;
extern uint8_t Normal_para_cnt,autoCal_para_cnt,para_cnt1;
extern uint8_t gu8_SetACKPwd,gu8_deviceIDChangeTry;
extern int16_t dummy,dummy1;
extern uint8_t last_sec,last_min,last_hr,current_sec,current_min,current_hr;
extern uint8_t keybyte,key_up_count,key_dn_count;
extern uint8_t debounce;
extern uint16_t gu16_XbeeRstInterval,gu16_logtransfer;

extern uint8_t PCCalibrationTimer,gu8_doorSensingTimer,progTimeout;
extern uint8_t gu8_doorSensingTime,gu8_doorSensingPolarity,gu8_LCDBrigthnessCnt;
extern uint8_t restoreFactoryCalibrationTimer,DPAutoCalModeTimer,MinMaxMeanModeTimer,MeanHrModeTimer,DPAutoCalTimer,ProgModeTimer,gu8_MinMaxClearTimer;

extern uint8_t FlashOVFByte;
extern uint8_t AckPwdInd;
extern uint16_t AckTimer,AckPwd[NO_OF_ACKPWD];
extern uint32_t StartBroadcastTimer,AlarmAckTimer,DP_StartUpTimer,gu8_restartTimer,gu8_deviceIDChangeTryTimer;
extern uint16_t RAMBufferInd,RAMBufferLog;
//uint32_t logTimer; 
//uint8_t FlashlogTimer=60;
extern uint32_t CurrentLogInd;
extern uint16_t flash24_StartInd,flash24_EndInd;
extern uint16_t logTimer,NoOf24Log;
extern uint16_t CurrentLog24Ind;
extern uint16_t CurrentLogIndReadLoc;
extern uint8_t CurrentLog24IndReadLoc;
extern uint8_t MeanHourLogInd;
extern uint16_t Buzzer_ON_Time,Buzzer_OFF_Time,LogInterval;
extern uint16_t buzzerOnTime,buzzerOffTime;
extern uint8_t MinMaxMeanDayLogInd;
extern uint8_t MinMaxMeanDayLogArr[MIN_MAX_MEAN_LOG_SIZE];
extern uint8_t MinMaxMeanDayLogArr4Disp[MIN_MAX_MEAN_LOG_SIZE];
extern uint8_t MinMaxMeanDayLogArr4Disp1[MIN_MAX_MEAN_LOG_SIZE];
extern uint8_t MinMaxMeanDayLogArr4Disp2[MIN_MAX_MEAN_LOG_SIZE];
extern uint8_t MeanHrLogArr4Disp[HOUR_MEAN_VALUE_SPACE];	
extern uint8_t MeanHrLogArr4Disp1[HOUR_MEAN_VALUE_SPACE];	
extern uint8_t MeanHrLogArr4Disp2[HOUR_MEAN_VALUE_SPACE];	
extern uint8_t MinMaxMeanReadParaType;
extern uint8_t dispMinMaxMeanLogInd,dispLogInd;
extern uint8_t min_max_mean_page_disp_cnt,mean_hr_page_disp_cnt;

extern int16_t tempshort;
extern uint8_t tempchar;
extern uint8_t a1,a2,a3;
extern uint8_t b1,b2,b3;
extern uint8_t c1,c2,c3;
extern uint16_t us1,us2,us3;
extern int16_t ss1,ss2,ss3;
extern uint32_t ul1,ul2,ul3;
extern float tempfloat,tempfloat1,tempfloat2;
extern uint32_t templong;

extern uint8_t gu8_IsCOMDisable,gu8_IsLCDDisable;
extern uint8_t gu8_rxMode,RxInd,XbeeRxInd,RxTimeout;
extern uint8_t RxLen;
extern uint8_t crcVal;
extern uint16_t CustPassword,FactCustPassword;

extern uint8_t gu8ar_SrNumber[16];
extern uint8_t gu8arr_XbeeMac[NO_OF_XBEE_MAC][XBEE_MAC_SIZE];
extern uint8_t gu8arr_XbeeSelfMac[XBEE_MAC_SIZE];

extern uint8_t Buffer1[100];
extern uint8_t RAMBuffer[RAM_BUF_SIZE];
extern uint8_t RxBuffer1[RX_IND_MAX];
extern uint8_t RxBuffer[RX_IND_MAX];
extern uint8_t TxBuffer[TX_IND_MAX];
extern uint8_t XbeeRxBuffer[XBEE_RX_IND_MAX];

#if (DEVICE_MODE==DP1_TEMP_RH_MODE)
//---- Temperature / RH channel variables (DP1_TEMP_RH_MODE only) ----
extern bool bool_RH_TEMP_NC;
extern bool bool_TMLog;
extern bool bool_RHLog;
extern uint8_t TM_Unit;
extern uint8_t TM_Alrm_ON;
extern uint8_t RH_Alrm_ON;
extern uint8_t LastTM_Alrm_ON;
extern uint8_t LastRH_Alrm_ON;
extern uint8_t gu8_TM_NearAlrm;
extern uint8_t gu8_RH_NearAlrm;
extern float humidityRH;            
extern float temperatureC,temperatureF;  
extern float RealtemperatureC,RealtemperatureF;
extern float RealhumidityRH;
extern float TM_Min,TM_Max,TM_Mean;
extern float RH_Min,RH_Max,RH_Mean;
extern float TM_Cal_float_Value_F,TM_Cal_float_Value_C;
extern float RH_Cal_float_Value_F,RH_Cal_float_Value_C;
extern int16_t TM_Upper_Alm_ON,TM_Upper_Alm_OFF,TM_Lower_Alm_ON,TM_Lower_Alm_OFF;
extern int16_t RH_Upper_Alm_ON,RH_Upper_Alm_OFF,RH_Lower_Alm_ON,RH_Lower_Alm_OFF;
extern uint8_t TMRH_StartUpTimer;
extern uint8_t TM_UserCalDateInd,RH_UserCalDateInd;
extern int16_t TM_Cal_Value_F,RH_Cal_Value_F;
extern int16_t TM_Cal_Value_C,RH_Cal_Value_C;
extern uint8_t HrTMSampleInd,HrRHSampleInd;
extern float HourTM_Mean,HourRH_Mean;
#endif	// (DEVICE_MODE==DP1_TEMP_RH_MODE)

extern bool bool_DP_NC[MAX_SUPPORTED_DP];
extern bool bool_DPLog[MAX_SUPPORTED_DP];
extern bool bool_dp_sw_factor_add[MAX_SUPPORTED_DP];
extern uint8_t HrDPSampleInd[MAX_SUPPORTED_DP];
extern uint8_t StageDP[MAX_SUPPORTED_DP];
extern uint8_t DP_Alrm_ON[MAX_SUPPORTED_DP];
extern uint8_t LastDP_Alrm_ON[MAX_SUPPORTED_DP];
extern uint8_t gu8_DP_NearAlrm[MAX_SUPPORTED_DP];
extern uint8_t gu8_DpAlarmSensingTime[MAX_SUPPORTED_DP];
extern uint8_t gu8_DpAlarmSensingTimer[MAX_SUPPORTED_DP];
extern uint8_t DP_limit[MAX_SUPPORTED_DP];
extern uint8_t DP_UserCalDateInd[MAX_SUPPORTED_DP];	
extern int16_t DP_Cal_Value_F[MAX_SUPPORTED_DP];
extern int16_t DP_Cal_Value_C[MAX_SUPPORTED_DP];
extern int16_t DP_Upper_Alm_ON[MAX_SUPPORTED_DP];
extern int16_t DP_Upper_Alm_OFF[MAX_SUPPORTED_DP];
extern int16_t DP_Lower_Alm_ON[MAX_SUPPORTED_DP];
extern int16_t DP_Lower_Alm_OFF[MAX_SUPPORTED_DP];
extern float DP_Cal_float_Value_F[MAX_SUPPORTED_DP];
extern float DP_Cal_float_Value_C[MAX_SUPPORTED_DP];
extern float RealDpressure[MAX_SUPPORTED_DP];
extern float Dpressure[MAX_SUPPORTED_DP];
extern float TempDpressure[MAX_SUPPORTED_DP];
extern float LastDpressure[MAX_SUPPORTED_DP];
extern float DP_Min[MAX_SUPPORTED_DP];
extern float DP_Max[MAX_SUPPORTED_DP];
extern float DP_Mean[MAX_SUPPORTED_DP];
extern float HourDP_Mean[MAX_SUPPORTED_DP];
extern float f32_dp_sw_factor[MAX_SUPPORTED_DP],f32_dp_offset[MAX_SUPPORTED_DP],f32_dp_limit[MAX_SUPPORTED_DP];
extern uint16_t u16_dp_limit[MAX_SUPPORTED_DP];
extern int16_t DpRangeSlotOffset[MAX_SUPPORTED_DP][DP_RANGE_SLOTS];
extern int16_t su16_dp_sw_factor[MAX_SUPPORTED_DP],su16_dp_offset[MAX_SUPPORTED_DP];
extern uint8_t gu8_dp_sw_factor_add_cnt[MAX_SUPPORTED_DP];
extern uint16_t gu16_DPAutoCalTimer10Sec[MAX_SUPPORTED_DP],gu16_DPAutoCalTimer5Min[MAX_SUPPORTED_DP];
extern uint8_t gu8_DPAutoCalDoorCnt[MAX_SUPPORTED_DP];

extern uint8_t gu8_AutoSentTimeout,gu8_AutoSentInterval,gu8_DeviceInGroup,gu8_AutoSentTimer,gu8_groupID,gu8_broadcast;
extern uint32_t gu32_triggerXbeeResetTimer;
extern uint32_t LowEpoch,MidEpoch,MidEpoch1,HighEpoch,MidLogInd;
extern uint32_t InitLogInd,LastLogInd,StartEpoch,EndEpoch,StartEpochTime,EndEpochTime,TotalLog,StartLogInd,EndLogInd;
extern uint8_t Temp_RTC_ARR[5];
extern uint8_t gu8_masterEnable,gu8_rly_stat;
extern uint8_t RTCSetFlag;
extern uint16_t logtransfer;

extern bool bool_paraIdNotValid;
extern bool bool_cal_mode;
extern bool bool_buzzerStart;
extern bool bool_noData;
extern bool bool_RTCChangeOccure;
extern bool bool_UARTChanged;
extern bool bool_keybit;
extern bool bool_resetMinMax;
extern bool bool_Sec_blink_flag;
extern bool bool_AM_PM_Flag;
extern bool bool_doorStatus;
extern uint8_t DOOR_Alrm_ON;
extern bool bool_rtcValid;
extern bool bool_FactoryCalibrationOn;
extern bool bool_CustmerCalibrationOn;
extern bool bool_EraseFlash;
extern bool bool_RamReadCmd;
extern bool bool_logtransferStart;
extern bool bool_brodcastEnb;
extern bool bool_Flash24ReadCmd;
extern bool bool_MinMaxMeanLogReadCmd;
extern bool bool_MeanHrLogReadCmd;
extern bool bool_FlashReadCmd;
extern bool bool_msgRcvOK;
extern bool bool_autoSendResponse;
extern bool bool_buzzeralert;
extern uint8_t gu8_buzzerSource;
extern uint8_t gu8_buzzerPulsesLeft;

//Defined in main.c, called from the TIM1 ISR - see the comment on BuzzerTick()
void BuzzerTick(void);
extern uint16_t gu16_nearAlrmTimer[NEAR_PARAM_COUNT];
extern uint8_t gu8_nearAlrmEscalated;
extern uint8_t gu8_nearAlrmActive;
extern uint8_t gu8_nearBlinkOn;
extern uint8_t gu8_nearBlinkPulses;
extern uint16_t gu16_nearBlinkTimer;

//Defined in main.c, called from the TIM1 ISR
void NearBlinkTick(void);
extern bool bool_resetDevice;
extern bool bool_triggerXbeeReset;
extern bool bool_RamAllReadCmd;
extern bool bool_sec_flag,bool_msec_flag,bool_msec250_flag,bool_mec500_blink_flag,bool_keyScan_flag;
extern volatile uint16_t gu16_tick50;
extern uint16_t gu16_logoAckBlinkTimer;

#endif	// SB_GLOBAL_VAR_H_

