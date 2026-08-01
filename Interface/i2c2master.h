#ifndef _I2C2MASTER_H
#define _I2C2MASTER_H   

#include "hal_conf.h"
#include "..\gpio.h"

#define SDA2_HIGH		GPIO_SetBits(GPIOA, GPIO_Pin_10)
#define SDA2_LOW		GPIO_ResetBits(GPIOA, GPIO_Pin_10)
#define SDA2_SENSE		GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10)
#define SDA2_DIR_IN		GPIO_Configure_Input(GPIOA, GPIO_Pin_10)
#define SDA2_DIR_OUT	GPIO_Configure_Output(GPIOA, GPIO_Pin_10)

#define SCL2_HIGH		GPIO_SetBits(GPIOA, GPIO_Pin_9)
#define SCL2_LOW		GPIO_ResetBits(GPIOA, GPIO_Pin_9)
#define SCL2_SENSE		GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_9)
#define SCL2_DIR_IN		GPIO_Configure_Input(GPIOA, GPIO_Pin_9)
#define SCL2_DIR_OUT	GPIO_Configure_Output(GPIOA, GPIO_Pin_9)

// Error codes
#define ACK_ERROR					0x01

#define ACK				0x01
#define NO_ACK			0x00

//------------------ I2C ROUTINS for DP2 -----------------------------------------------
void I2C2_Init(void);
void I2C2_Start(void);
void I2C2_Stop(void);
unsigned char Write_Byte_I2C2(unsigned char Data);
unsigned char Read_Byte_I2C2(unsigned char ACK_Bit);

#endif
