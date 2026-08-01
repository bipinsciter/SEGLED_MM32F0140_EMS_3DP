
#ifndef SB_VARIABLES_H_
#define SB_VARIABLES_H_

#include <stdint.h>
#include <string.h>
#include "sb_const.h"
#include "sb_global_var.h"

uint16_t gu16_parameterWord = PARAMETER_WORD;
uint8_t UART_BaudRate=BAUD_57600;//,UART_DataBits=3,UART_Parity=0,UART_StopBit=0;
uint8_t DeviceID=0;
uint32_t gu32_SrNumber=0;
uint8_t RTC_data[6]={0};
uint8_t mode=NORMAL_MODE;
uint8_t Normal_para_cnt=0,prog_para_cnt=0,autoCal_para_cnt=0,para_cnt1=0,Lastpara_cnt=0;
uint8_t gu8_SetACKPwd=0,gu8_deviceIDChangeTry=0;
int16_t dummy=0,dummy1=0;
uint8_t last_sec=0,last_min=0,last_hr=0,current_sec=0,current_min=0,current_hr=0;
uint8_t keybyte=0,key_up_count=0,key_dn_count=0;
uint8_t debounce=DEBOUNCE;	
uint16_t gu16_XbeeRstInterval=0,gu16_logtransfer=0;

uint8_t PCCalibrationTimer=0,gu8_doorSensingTimer=0,progTimeout=0;
uint8_t gu8_doorSensingTime=0,gu8_doorSensingPolarity=0,gu8_LCDBrigthnessCnt=0;
uint8_t restoreFactoryCalibrationTimer=0,DPAutoCalModeTimer=0,MinMaxMeanModeTimer=0,MeanHrModeTimer=0,DPAutoCalTimer=0,ProgModeTimer=0,gu8_MinMaxClearTimer=0;

uint8_t FlashOVFByte=0;
uint8_t AckPwdInd=0;
uint16_t AckTimer=0,AckPwd[NO_OF_ACKPWD]={0};
uint32_t StartBroadcastTimer=0,AlarmAckTimer=0,DP_StartUpTimer=0,gu8_restartTimer=0,gu8_deviceIDChangeTryTimer=0;
uint16_t RAMBufferInd=0,RAMBufferLog=0;
//uint32_t logTimer=0; 
//uint8_t FlashlogTimer=60;
uint32_t CurrentLogInd = 0;
uint16_t flash24_StartInd=0,flash24_EndInd=0;
uint16_t logTimer=0,NoOf24Log=0;
uint16_t CurrentLog24Ind = 0;
uint16_t CurrentLogIndReadLoc=0;
uint8_t CurrentLog24IndReadLoc=0;
uint8_t MeanHourLogInd=0;
uint8_t MinMaxMeanDayLogInd=0;
uint8_t MinMaxMeanDayLogArr[MIN_MAX_MEAN_LOG_SIZE]={0};
uint8_t MinMaxMeanDayLogArr4Disp[MIN_MAX_MEAN_LOG_SIZE]={0};
uint8_t MinMaxMeanDayLogArr4Disp1[MIN_MAX_MEAN_LOG_SIZE]={0};
uint8_t MinMaxMeanDayLogArr4Disp2[MIN_MAX_MEAN_LOG_SIZE]={0};
uint8_t MeanHrLogArr4Disp[HOUR_MEAN_VALUE_SPACE]={0};	
uint8_t MeanHrLogArr4Disp1[HOUR_MEAN_VALUE_SPACE]={0};	
uint8_t MeanHrLogArr4Disp2[HOUR_MEAN_VALUE_SPACE]={0};	
uint8_t MinMaxMeanReadParaType=0;
uint8_t dispMinMaxMeanLogInd=0,dispLogInd=0;
uint8_t min_max_mean_page_disp_cnt=0,mean_hr_page_disp_cnt=0;

uint16_t Buzzer_ON_Time=0,Buzzer_OFF_Time=0,LogInterval=0;
uint16_t buzzerOnTime=0,buzzerOffTime=0;

int16_t tempshort=0;
uint8_t tempchar=0;
uint8_t a1=0,a2=0,a3=0;
uint8_t b1=0,b2=0,b3=0;
uint8_t c1=0,c2=0,c3=0;
uint16_t us1=0,us2=0,us3=0;
int16_t ss1=0,ss2=0,ss3=0;
uint32_t ul1=0,ul2=0,ul3=0;
float tempfloat=0,tempfloat1=0,tempfloat2=0;
uint32_t templong=0;

uint8_t gu8_rxMode=0,RxInd=0,XbeeRxInd=0,RxTimeout=0;
uint8_t crcVal=0;
uint16_t CustPassword=0,FactCustPassword=0;

uint8_t gu8ar_SrNumber[16]={0};
uint8_t gu8arr_XbeeMac[NO_OF_XBEE_MAC][XBEE_MAC_SIZE]={0};
uint8_t gu8arr_XbeeSelfMac[XBEE_MAC_SIZE]={'0'};

uint8_t Buffer1[100]={0};
uint8_t RAMBuffer[RAM_BUF_SIZE]={0};
uint8_t RxBuffer1[RX_IND_MAX]={0};
uint8_t RxBuffer[RX_IND_MAX]={0};
uint8_t TxBuffer[TX_IND_MAX]={0};
uint8_t XbeeRxBuffer[XBEE_RX_IND_MAX]={0};

bool bool_DP_NC[MAX_SUPPORTED_DP]={0};
bool bool_DPLog[MAX_SUPPORTED_DP]={0};
bool bool_dp_sw_factor_add[MAX_SUPPORTED_DP]={0};
uint8_t HrDPSampleInd[MAX_SUPPORTED_DP]={0};
uint8_t StageDP[MAX_SUPPORTED_DP]={0};
uint8_t DP_Alrm_ON[MAX_SUPPORTED_DP]={0};
uint8_t LastDP_Alrm_ON[MAX_SUPPORTED_DP]={0};
uint8_t gu8_DpAlarmSensingTime[MAX_SUPPORTED_DP]={0};
uint8_t gu8_DpAlarmSensingTimer[MAX_SUPPORTED_DP]={0};
uint8_t DP_limit[MAX_SUPPORTED_DP]={0};
uint8_t DP_UserCalDateInd[MAX_SUPPORTED_DP]={0};	
int16_t DP_Cal_Value_F[MAX_SUPPORTED_DP]={0};
int16_t DP_Cal_Value_C[MAX_SUPPORTED_DP]={0};
int16_t DP_Upper_Alm_ON[MAX_SUPPORTED_DP]={0};
int16_t DP_Upper_Alm_OFF[MAX_SUPPORTED_DP]={0};
int16_t DP_Lower_Alm_ON[MAX_SUPPORTED_DP]={0};
int16_t DP_Lower_Alm_OFF[MAX_SUPPORTED_DP]={0};
float DP_Cal_float_Value_F[MAX_SUPPORTED_DP]={0};
float DP_Cal_float_Value_C[MAX_SUPPORTED_DP]={0};
float RealDpressure[MAX_SUPPORTED_DP]={0};
float Dpressure[MAX_SUPPORTED_DP]={0};
float TempDpressure[MAX_SUPPORTED_DP]={0};
float LastDpressure[MAX_SUPPORTED_DP]={0};
float DP_Min[MAX_SUPPORTED_DP]={0};
float DP_Max[MAX_SUPPORTED_DP]={0};
float DP_Mean[MAX_SUPPORTED_DP]={0};
float HourDP_Mean[MAX_SUPPORTED_DP]={0};
float f32_dp_sw_factor[MAX_SUPPORTED_DP]={0},f32_dp_offset[MAX_SUPPORTED_DP]={0},f32_dp_limit[MAX_SUPPORTED_DP]={0};
uint16_t u16_dp_limit[MAX_SUPPORTED_DP]={0};
int16_t su16_dp_sw_factor[MAX_SUPPORTED_DP]={0},su16_dp_offset[MAX_SUPPORTED_DP]={0};
uint8_t gu8_dp_sw_factor_add_cnt[MAX_SUPPORTED_DP]={0};
uint16_t gu16_DPAutoCalTimer10Sec[MAX_SUPPORTED_DP] = {0},gu16_DPAutoCalTimer5Min[MAX_SUPPORTED_DP] = {0};
uint8_t gu8_DPAutoCalDoorCnt[MAX_SUPPORTED_DP] = {0};

uint32_t LowEpoch=0,MidEpoch=0,MidEpoch1=0,HighEpoch=0,MidLogInd=0;
uint32_t InitLogInd=0,LastLogInd=0,StartEpoch=0,EndEpoch=0,StartEpochTime=0,EndEpochTime=0,TotalLog=0,StartLogInd=0,EndLogInd=0;

uint8_t gu8_AutoSentTimeout=60,gu8_AutoSentInterval=DEFAULT_AUTO_SENT_INTERVAL,gu8_DeviceInGroup=DEFAULT_DEVICES_IN_GROUP,gu8_AutoSentTimer=0,gu8_groupID=0,gu8_broadcast=0,gu8_Mac2ValidTimer = 0;
uint32_t gu32_triggerXbeeResetTimer=0;

uint8_t Temp_RTC_ARR[5]={0};

uint8_t gu8_masterEnable=0,gu8_rly_stat=0;

uint8_t RTCSetFlag=0;
uint16_t logtransfer=0;

bool bool_paraIdNotValid=0;
bool bool_cal_mode=0;
bool bool_buzzerStart=0;
bool bool_noData=0;
bool bool_RTCChangeOccure=0;
bool bool_UARTChanged=0;
bool bool_keybit=0;
bool bool_resetMinMax=0;
bool bool_Sec_blink_flag=0;
bool bool_AM_PM_Flag=0;
bool bool_doorStatus=CLOSE;
//bool bool_rtcValid=0;

bool bool_FactoryCalibrationOn=0;
bool bool_CustmerCalibrationOn=0;
bool bool_EraseFlash=0;
bool bool_RamReadCmd=0;
bool bool_logtransferStart=0;
bool bool_brodcastEnb=0;
bool bool_Flash24ReadCmd=0;
bool bool_MinMaxMeanLogReadCmd=0;
bool bool_MeanHrLogReadCmd=0;
bool bool_FlashReadCmd=0;
bool bool_msgRcvOK=0;
bool bool_autoSendResponse=0;
bool bool_buzzeralert=0;
bool bool_resetDevice=0;
bool bool_triggerXbeeReset=0;
bool bool_RamAllReadCmd=0;
bool bool_sec_flag=0,bool_msec_flag=0,bool_msec50_flag=0,bool_msec250_flag=0,bool_mec500_blink_flag=0,bool_mec500_blink_flag1=0;

struct lcdbits
{
	uint8_t Sym_LOGO : 1;	
	
	uint8_t Sym_DOOR : 1;	
	uint8_t Sym_DOOR_SYM : 1;	
	uint8_t Sym_DOOR_ALM : 1;	
	
	uint8_t Sym_MIN : 1;	
	uint8_t Sym_MAX : 1;	
	uint8_t Sym_MEAN : 1;	
	uint8_t Sym_SET : 1;
	uint8_t Sym_ID : 1;	
	uint8_t Sym_ACK : 1;	
	
	uint8_t Sym_RTC_AM : 1;	
	uint8_t Sym_RTC_PM : 1;	
	uint8_t Sym_RTC_BC1 : 1;	
	uint8_t Sym_RTC_COL : 1;	
	
	uint8_t Sym_DP_LOGO : 1;
	uint8_t Sym_DP_LOGO_ALM : 1;
	uint8_t Sym_DP_UNIT : 1;
	uint8_t Sym_DP_MIN : 1;
	uint8_t Sym_DP_MIN_ALM : 1;
	
	uint8_t Sym_RH_LOGO : 1;
	uint8_t Sym_RH_LOGO_ALM : 1;
	uint8_t Sym_RH_UNIT : 1;
	uint8_t Sym_RH_MIN : 1;
	uint8_t Sym_RH_MIN_ALM : 1;
	
	uint8_t Sym_TM_LOGO : 1;
	uint8_t Sym_TM_LOGO_ALM : 1;
	uint8_t Sym_TM_UNIT_C : 1;
	uint8_t Sym_TM_UNIT_F : 1;
	uint8_t Sym_TM_MIN : 1;
	uint8_t Sym_TM_MIN_ALM : 1;
	uint8_t reserver1 : 1;
	uint8_t reserver2 : 1;
	
}lcd={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
uint8_t final_buffer[32];
uint8_t disp_buffer[NO_DIGIT];
uint8_t data[NO_DIGIT];
	
uint8_t seg_code[]=
{
	0x3F,  //code for 0
	0x06,  //code for 1
	0x5B,  //code for 2
	0x4F,  //code for 3
	0x66,  //code for 4
	0x6D,  //code for 5
	0x7D,  //code for 6
	0x07,  //code for 7
	0x7F,  //code for 8
	0x6F,  //code for 9
	0xBF,  //code for 0.
	0x86,  //code for 1.
	0xDB,  //code for 2.
	0xCF,  //code for 3.
	0xE6,  //code for 4.
	0xED,  //code for 5.
	0xFD,  //code for 6.
	0x87,  //code for 7.
	0xFF,  //code for 8.
	0xEF,  //code for 9.
	0x00,  //code for BLANK
	0x40,  //code for -
	0x39,  //code for C
	0x77,  //code for A
	0x38,  //code for L
	0x5E,  //code for d
	0x54,  //code for n
	0x79,  //code for E
	0x7C,  //code for B
	0x78,  //code for t
	0x50,  //code for r
	0x73,  //code for P
	0x30,  //code for I
	0x71,  //code for F
	0x3E,  //code for U
	0x76,  //code for H
	0x15,  //code for M
	0x6E,  //code for small y
	0x2A,  //code for V
	0xC0   //code for -.
};	

const unsigned short int __mon_yday[2][13] =
{
	/* Normal years.  */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* Leap years.   */
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

const int _ytab[2][12] =
{
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

#endif	// SB_VARIABLES_H_
