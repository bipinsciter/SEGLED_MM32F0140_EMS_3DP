#ifndef _PCF8563_H
#define _PCF8563_H

#include "hal_conf.h"

#define RTC_ID			0xA2

#define RTC_CNTRL1_ADDR			0
#define RTC_CNTRL2_ADDR			1

#define RTC_TIMEREG_START		2
#define RTC_TIMESEC_REG		    2
#define RTC_TIMEMIN_REG		    3
#define RTC_TIMEHOUR_REG		4

#define RTC_DATE_REG_START		5
#define RTC_DATE_DATE_REG		5
#define RTC_DATE_MONTH_REG		7
#define RTC_DATE_YEAR_REG		8

//Clock-integrity bits
#define PCF8563_VL_BIT			0x80	//Seconds reg bit7: clock integrity NOT guaranteed
#define PCF8563_STOP_BIT		0x20	//CNTRL1 reg bit5: oscillator halted

void Init_PCF8563(void);
void Write_PCF8563(unsigned char addr,unsigned char *buff, unsigned char NoOfByte);
void Read_PCF8563(unsigned char addr,unsigned char *buff, unsigned char NoOfByte);
unsigned char Read_byte_PCF8563(unsigned char addr);
void Write_byte_PCF8563(unsigned char addr,unsigned char msgbyte);
unsigned char BCD2HEX(unsigned char bcd);
unsigned char HEX2BCD(unsigned char hex);


//unsigned char RTC_data[7]={0};


#endif
