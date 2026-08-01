#include "SPI.h"
#include "..\platform.h"

void Init_SPI(void)
{
	CS_DIR_OUT;
	SCLK_DIR_OUT;
	SDI_DIR_OUT;
	SDO_DIR_IN;
	
	CS_HIGH;
	SCLK_LOW;
	SDI_LOW;
}


void SPI_WriteByte(unsigned char datum)
{
	unsigned char tempbyte=0x80,i=0;

	SCLK_LOW;
	
	for(i=0;i<8;i++)
	{
		if(datum & tempbyte) SDI_HIGH;
		else SDI_LOW;
		
		SCLK_HIGH;
		PLATFORM_DelayUS(5);
		SCLK_LOW;
		PLATFORM_DelayUS(5);
		
		tempbyte=tempbyte>>1;
	}
	SDI_HIGH;
}


unsigned char SPI_ReadByte(void)
{
	unsigned char tempbyte=0,i=0,datalow=0x80;

	SDI_HIGH;
	
	for(i=0;i<8;i++)
	{		
		if(SENSE_SDO)tempbyte|=datalow;
		
		SCLK_HIGH;
		PLATFORM_DelayUS(5);
		SCLK_LOW;
		PLATFORM_DelayUS(5);
		
		datalow>>=1;
	}
	return(tempbyte);
}

