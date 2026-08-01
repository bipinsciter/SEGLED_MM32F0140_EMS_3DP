#ifndef SPI_H_
#define SPI_H_

#include "hal_conf.h"
#include "..\gpio.h"

#define TRUE			1
#define FALSE			0

#define SCLK_DIR_OUT	GPIO_Configure_Output(GPIOB, GPIO_Pin_13)
#define SCLK_HIGH		GPIO_SetBits(GPIOB, GPIO_Pin_13)
#define SCLK_LOW		GPIO_ResetBits(GPIOB, GPIO_Pin_13)

#define SDI_DIR_OUT		GPIO_Configure_Output(GPIOB, GPIO_Pin_15)
#define SDI_HIGH		GPIO_SetBits(GPIOB, GPIO_Pin_15)
#define SDI_LOW			GPIO_ResetBits(GPIOB, GPIO_Pin_15)

#define CS_DIR_OUT		GPIO_Configure_Output(GPIOB, GPIO_Pin_12)
#define CS_HIGH			GPIO_SetBits(GPIOB, GPIO_Pin_12)
#define CS_LOW			GPIO_ResetBits(GPIOB, GPIO_Pin_12)

#define SDO_DIR_IN		GPIO_Configure_Input(GPIOB, GPIO_Pin_14)
#define SDO_DIR_OUT		GPIO_Configure_Output(GPIOB, GPIO_Pin_14)
#define SENSE_SDO		GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14)

void Init_SPI(void);
void SPI_WriteByte(unsigned char datum);
unsigned char SPI_ReadByte(void);

#endif /*SPI_H_*/