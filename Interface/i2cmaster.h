#ifndef _I2CMASTER_H
#define _I2CMASTER_H   

#include "hal_conf.h"
#include "..\gpio.h"

#define SDA_HIGH		GPIO_SetBits(GPIOB, GPIO_Pin_9)
#define SDA_LOW			GPIO_ResetBits(GPIOB, GPIO_Pin_9)
#define SDA_SENSE		GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9)
#define SDA_DIR_IN		GPIO_Configure_Input(GPIOB, GPIO_Pin_9)
#define SDA_DIR_OUT		GPIO_Configure_Output(GPIOB, GPIO_Pin_9)

#define SCL_HIGH		GPIO_SetBits(GPIOB, GPIO_Pin_8)
#define SCL_LOW			GPIO_ResetBits(GPIOB, GPIO_Pin_8)
#define SCL_SENSE		GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8)
#define SCL_DIR_IN		GPIO_Configure_Input(GPIOB, GPIO_Pin_8)
#define SCL_DIR_OUT		GPIO_Configure_Output(GPIOB, GPIO_Pin_8)

// Error codes
#define ACK_ERROR					0x01

#define ACK				0x01
#define NO_ACK			0x00

//------------------ I2C ROUTINS for IDT1338, HTU25, DP1 -----------------------------------------------
void I2C1_Init(void);
void I2C1_Start(void);
void I2C1_Stop(void);
unsigned char Write_Byte_I2C1(unsigned char Data);
unsigned char Read_Byte_I2C1(unsigned char ACK_Bit);

#endif
