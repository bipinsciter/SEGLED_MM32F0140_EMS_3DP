#include "i2cmaster.h"
#include "..\platform.h"

//----------------------------------------------------------------------------------------
// I2C FUNCTIONS
//----------------------------------------------------------------------------------------
void I2C1_Init(void)
{
	SCL_DIR_OUT;           					// Enable SCL as output.
	SDA_DIR_OUT;           					// Enable SDA as output.
	
	SDA_HIGH;           					// Enable pullup on SDA, to set high as released state.
	SCL_HIGH;						        // Enable pullup on SCL, to set high as released state.
}

/*---------------------------------------------------------------
 Core function for shifting data in and out from the USI.
 Data to be sent has to be placed into the USIDR prior to calling
 this function. Data read, will be return'ed from the function.
---------------------------------------------------------------*/
unsigned char Read_Byte_I2C1(unsigned char ACK_Bit)
{
	unsigned char Data=0,i=0;

    SDA_DIR_IN;	
	
	for (i=0;i<8;i++)
	{
		SCL_HIGH;		
		Data<<= 1;
		
		if(SDA_SENSE) Data  |= 0x01;
		
		PLATFORM_DelayUS(5);
		SCL_LOW;
		PLATFORM_DelayUS(5);
	}
    
	SDA_DIR_OUT;
	
 	if (ACK_Bit == 1)
		SDA_LOW;  // Send ACK		
	else		
		SDA_HIGH; // Send NO ACK	

	PLATFORM_DelayUS(5);
	SCL_HIGH;		
	PLATFORM_DelayUS(5);
	SCL_LOW;
	
	return Data;
}

/*---------------------------------------------------------------
 Function for generating a TWI Start Condition. 
---------------------------------------------------------------*/
void I2C1_Start(void)
{
	SDA_HIGH;
	PLATFORM_DelayUS(5);
	SCL_HIGH;
	PLATFORM_DelayUS(5);
	SDA_LOW;
	PLATFORM_DelayUS(5);
	SCL_LOW;
	PLATFORM_DelayUS(5);
}

/*---------------------------------------------------------------
 Function for writing byte
---------------------------------------------------------------*/
unsigned char Write_Byte_I2C1(unsigned char datum)
{
	unsigned char i=0,error=0;
	
	SCL_LOW;                // Pull SCL LOW.
	
	for (i=0;i<8;i++)
	{
		if(datum & 0x80) SDA_HIGH;
		else 			 SDA_LOW;
		
		PLATFORM_DelayUS(5);
		
		SCL_HIGH;
		PLATFORM_DelayUS(5);
		SCL_LOW;
		PLATFORM_DelayUS(5);
		
		datum<<=1;
	}

	SDA_DIR_IN;
	
  	SCL_HIGH; 
	PLATFORM_DelayUS(5);
	if(SDA_SENSE) error=ACK_ERROR; //check ack from i2c slave
	SCL_LOW;
	
	SDA_DIR_OUT;
	
	return error;                       //return error code
}


/*---------------------------------------------------------------
 Function for generating a TWI Stop Condition. Used to release 
 the TWI bus.
---------------------------------------------------------------*/
void I2C1_Stop(void)
{
	SDA_LOW;	    	
	PLATFORM_DelayUS(5);
	SCL_HIGH;
	PLATFORM_DelayUS(5);
	SDA_HIGH;
	PLATFORM_DelayUS(5);
	//SCL_LOW;
	//PLATFORM_DelayUS(5);
}
