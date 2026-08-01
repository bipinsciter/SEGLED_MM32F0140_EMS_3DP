#include "i2c3master.h"
#include "..\platform.h"

//----------------------------------------------------------------------------------------
// I2C FUNCTIONS
//----------------------------------------------------------------------------------------
void I2C3_Init(void)
{
	SCL3_DIR_OUT;           					// Enable SCL as output.
	SDA3_DIR_OUT;           					// Enable SDA as output.
	
	SDA3_HIGH;           					// Enable pullup on SDA, to set high as released state.
	SCL3_HIGH;						        // Enable pullup on SCL, to set high as released state.
}

/*---------------------------------------------------------------
 Core function for shifting data in and out from the USI.
 Data to be sent has to be placed into the USIDR prior to calling
 this function. Data read, will be return'ed from the function.
---------------------------------------------------------------*/
unsigned char Read_Byte_I2C3(unsigned char ACK_Bit)
{
	unsigned char Data=0,i=0;

    SDA3_DIR_IN;	
	
	for (i=0;i<8;i++)
	{
		SCL3_HIGH;		
		Data<<= 1;
		
		if(SDA3_SENSE) Data  |= 0x01;
		
		PLATFORM_DelayUS(5);
		SCL3_LOW;
		PLATFORM_DelayUS(5);
	}
    
	SDA3_DIR_OUT;
	
 	if (ACK_Bit == 1)
		SDA3_LOW;  // Send ACK		
	else		
		SDA3_HIGH; // Send NO ACK				

	PLATFORM_DelayUS(5);
	SCL3_HIGH;		
	PLATFORM_DelayUS(5);
	SCL3_LOW;
	
	return Data;
}

/*---------------------------------------------------------------
 Function for generating a TWI Start Condition. 
---------------------------------------------------------------*/
void I2C3_Start(void)
{
	SDA3_HIGH;
	PLATFORM_DelayUS(5);
	SCL3_HIGH;
	PLATFORM_DelayUS(5);
	SDA3_LOW;
	PLATFORM_DelayUS(5);
	SCL3_LOW;
	PLATFORM_DelayUS(5);
}

/*---------------------------------------------------------------
 Function for writing byte
---------------------------------------------------------------*/
unsigned char Write_Byte_I2C3(unsigned char datum)
{
	unsigned char i=0,error=0;
	
	SCL3_LOW;                // Pull SCL LOW.
	
	for (i=0;i<8;i++)
	{
		if(datum & 0x80) SDA3_HIGH;
		else 			 SDA3_LOW;
		
		PLATFORM_DelayUS(5);
		
		SCL3_HIGH;
		PLATFORM_DelayUS(5);
		SCL3_LOW;
		PLATFORM_DelayUS(5);
		
		datum<<=1;
	}

	SDA3_DIR_IN;
	
  	SCL3_HIGH; 
	PLATFORM_DelayUS(5);
	if(SDA3_SENSE) error=ACK_ERROR; //check ack from i2c slave
	SCL3_LOW;
	
	SDA3_DIR_OUT;
	
	return error;                       //return error code
}


/*---------------------------------------------------------------
 Function for generating a TWI Stop Condition. Used to release 
 the TWI bus.
---------------------------------------------------------------*/
void I2C3_Stop(void)
{
	SDA3_LOW;	    	
	PLATFORM_DelayUS(5);
	SCL3_HIGH;
	PLATFORM_DelayUS(5);
	SDA3_HIGH;
	PLATFORM_DelayUS(5);
	//SCL3_LOW;
	//PLATFORM_DelayUS(5);
}
