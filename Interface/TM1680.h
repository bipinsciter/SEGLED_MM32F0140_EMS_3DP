#ifndef _TM1680_H
#define _TM1680_H

#include "hal_conf.h"
//#include <util/delay.h>

#define TM1680_ID 0xE4 //change this address based on A0 and A1

#define SYS_DISABLE  	0x80
#define SYS_ENABLE  	0x81
#define LED_OFF  		0x82
#define LED_ON  		0x83
#define BRIGHTNESS 		0xB0
#define COM_OPTION 		0xA8
#define BLINK_REG 		0x88

#define BLINK_OFF 		0
#define BLINK_500MSEC	1
#define BLINK_1SEC 		2
#define BLINK_2SEC 		3

#define NO_OF_LED_DISP			1

void TM1680Configure(void);
void TM1680WriteCommand(uint8_t cmd);
void TM1680WritePage(uint8_t Address, uint8_t *data, uint8_t NoOfByte);
void TM1680WriteCmdAndPage(uint8_t Cmd, uint8_t Address, uint8_t *data, uint8_t NoOfByte);
void TM1680Brighness(uint8_t Brightness);
void TM1680Blink(uint8_t BlinkRate);

#endif
