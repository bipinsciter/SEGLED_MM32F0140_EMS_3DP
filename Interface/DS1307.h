#ifndef _DS1307_H
#define _DS1307_H

//#include <avr/io.h>
#include "hal_conf.h"
//#include <util/delay.h>

#define EEPROM_ID		0xA0
#define RTC_ID			0xD0

void Init_DS1307(void);
void Write_DS1307(unsigned char addr,unsigned char *buff, unsigned char NoOfByte);
void Read_DS1307(unsigned char addr,unsigned char *buff, unsigned char NoOfByte);
unsigned char Read_byte_DS1307(unsigned char addr);
void Write_byte_DS1307(unsigned char addr,unsigned char msgbyte);
unsigned char BCD2HEX(unsigned char bcd);
unsigned char HEX2BCD(unsigned char hex);


//unsigned char RTC_data[7]={0};


#endif
