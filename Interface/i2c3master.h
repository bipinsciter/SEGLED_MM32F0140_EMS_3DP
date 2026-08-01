#ifndef _I2C3MASTER_H
#define _I2C3MASTER_H   

#include "hal_conf.h"
#include "..\gpio.h"

#define SDA3_HIGH		GPIO_SetBits(GPIOA, GPIO_Pin_12)
#define SDA3_LOW		GPIO_ResetBits(GPIOA, GPIO_Pin_12)
#define SDA3_SENSE		GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_12)
#define SDA3_DIR_IN		GPIO_Configure_Input(GPIOA, GPIO_Pin_12)
#define SDA3_DIR_OUT	GPIO_Configure_Output(GPIOA, GPIO_Pin_12)

#define SCL3_HIGH		GPIO_SetBits(GPIOA, GPIO_Pin_11)
#define SCL3_LOW		GPIO_ResetBits(GPIOA, GPIO_Pin_11)
#define SCL3_SENSE		GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11)
#define SCL3_DIR_IN		GPIO_Configure_Input(GPIOA, GPIO_Pin_11)
#define SCL3_DIR_OUT	GPIO_Configure_Output(GPIOA, GPIO_Pin_11)

// Error codes
#define ACK_ERROR					0x01

#define ACK				0x01
#define NO_ACK			0x00

//------------------ I2C ROUTINS for DP2 -----------------------------------------------
void I2C3_Init(void);
void I2C3_Start(void);
void I2C3_Stop(void);
unsigned char Write_Byte_I2C3(unsigned char Data);
unsigned char Read_Byte_I2C3(unsigned char ACK_Bit);

#endif
