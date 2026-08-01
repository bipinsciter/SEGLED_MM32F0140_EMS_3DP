#include "DS1307.h"
#include "i2cmaster.h"

//-------------------------------
// Read 1 byte from I2C
//-------------------------------
unsigned char Read_byte_DS1307(unsigned char addr)
{
   	unsigned char Data;
	
	I2C1_Start();				// Start condition
	Write_Byte_I2C1(RTC_ID);			// Write device address
	Write_Byte_I2C1(addr);			// Write address of register
	I2C1_Start();				// Start condition
	Write_Byte_I2C1(RTC_ID+1);			// Write device address
	Data = Read_Byte_I2C1(NO_ACK);
	I2C1_Stop();              // Send a STOP condition on the TWI bus.		
	
	return Data;
}

//-------------------------------
// Write 1 byte to RTC
//-------------------------------

void Write_byte_DS1307(unsigned char addr,unsigned char msgbyte)
{
	I2C1_Start();				// Start condition
	Write_Byte_I2C1(RTC_ID);			// Write device address
	Write_Byte_I2C1(addr);			// Write address of register
	Write_Byte_I2C1(msgbyte);		// DATA
	I2C1_Stop();              // Send a STOP condition on the TWI bus.
}


//-------------------------------
// Read RTC (all real time)
//-------------------------------
void Read_DS1307(unsigned char addr,unsigned char *buff, unsigned char NoOfByte)
{
	I2C1_Start();				// Start condition
	Write_Byte_I2C1(RTC_ID);			// Write device address
	Write_Byte_I2C1(addr);			// Write address of register
	I2C1_Start();				// Start condition
	Write_Byte_I2C1(RTC_ID+1);			// Write device address
	while(NoOfByte)
	{
		NoOfByte--;
		
		/* Prepare to generate ACK (or NACK in case of End Of Transmission) */
		if(NoOfByte)
		{
			*buff = Read_Byte_I2C1(ACK);
		}
		else
		{
			*buff = Read_Byte_I2C1(NO_ACK);
		}
		buff++;
   	}
	I2C1_Stop();              // Send a STOP condition on the TWI bus.	
}

//-------------------------------
// Write RTC
//-------------------------------
void Write_DS1307(unsigned char addr,unsigned char *buff, unsigned char NoOfByte)
{
	I2C1_Start();				// Start condition
	Write_Byte_I2C1(RTC_ID);			// Write device address
	Write_Byte_I2C1(addr);			// Write address of register
	while(NoOfByte)
	{	
		Write_Byte_I2C1(*buff);				// DATA
	 	buff++;							// Point to Next Location	 
		NoOfByte--;
	}
	I2C1_Stop();              // Send a STOP condition on the TWI bus.
}

//-------------------------------
// Convert BCD 1 byte to HEX 1 byte
//-------------------------------
unsigned char BCD2HEX(unsigned char bcd)
{
	unsigned char temp=0;
	//temp=((bcd>>4)*10)|(bcd & 0x0f);
	temp=((bcd>>4)*10);
	temp+=(bcd & 0x0F);
	
	return temp;
}

//-------------------------------
// Convert HEX 1 byte to BCD 1 byte
//-------------------------------
unsigned char HEX2BCD(unsigned char hex)
{
	unsigned char temp=0;
	//temp = ((hex / 10)<<4) | (hex % 10);
	temp = ((hex / 10)<<4);
	temp |= (hex % 10);
	return temp;
}
