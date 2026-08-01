#include "i2c2master.h"
#include "..\platform.h"

//----------------------------------------------------------------------------------------
// I2C FUNCTIONS
//----------------------------------------------------------------------------------------
void I2C2_Init(void)
{
	SCL2_DIR_OUT;           					// Enable SCL as output.
	SDA2_DIR_OUT;           					// Enable SDA as output.
	
	SDA2_HIGH;           					// Enable pullup on SDA, to set high as released state.
	SCL2_HIGH;						        // Enable pullup on SCL, to set high as released state.
}

/*---------------------------------------------------------------
 Core function for shifting data in and out from the USI.
 Data to be sent has to be placed into the USIDR prior to calling
 this function. Data read, will be return'ed from the function.
---------------------------------------------------------------*/
unsigned char Read_Byte_I2C2(unsigned char ACK_Bit)
{
	unsigned char Data=0,i=0;

    SDA2_DIR_IN;	
	
	for (i=0;i<8;i++)
	{
		SCL2_HIGH;		
		Data<<= 1;
		
		if(SDA2_SENSE) Data  |= 0x01;
		
		PLATFORM_DelayUS(5);
		SCL2_LOW;
		PLATFORM_DelayUS(5);
	}
    
	SDA2_DIR_OUT;
	
 	if (ACK_Bit == 1)
		SDA2_LOW;  // Send ACK		
	else		
		SDA2_HIGH; // Send NO ACK				

	PLATFORM_DelayUS(5);
	SCL2_HIGH;		
	PLATFORM_DelayUS(5);
	SCL2_LOW;
	
	return Data;
}

/*---------------------------------------------------------------
 Function for generating a TWI Start Condition. 
---------------------------------------------------------------*/
void I2C2_Start(void)
{
	SDA2_HIGH;
	PLATFORM_DelayUS(5);
	SCL2_HIGH;
	PLATFORM_DelayUS(5);
	SDA2_LOW;
	PLATFORM_DelayUS(5);
	SCL2_LOW;
	PLATFORM_DelayUS(5);
}

/*---------------------------------------------------------------
 Function for writing byte
---------------------------------------------------------------*/
unsigned char Write_Byte_I2C2(unsigned char datum)
{
	unsigned char i=0,error=0;
	
	SCL2_LOW;                // Pull SCL LOW.
	
	for (i=0;i<8;i++)
	{
		if(datum & 0x80) SDA2_HIGH;
		else 			 SDA2_LOW;
		
		PLATFORM_DelayUS(5);
		
		SCL2_HIGH;
		PLATFORM_DelayUS(5);
		SCL2_LOW;
		PLATFORM_DelayUS(5);
		
		datum<<=1;
	}

	SDA2_DIR_IN;
	
  	SCL2_HIGH; 
	PLATFORM_DelayUS(5);
	if(SDA2_SENSE) error=ACK_ERROR; //check ack from i2c slave
	SCL2_LOW;
	
	SDA2_DIR_OUT;
	
	return error;                       //return error code
}


/*---------------------------------------------------------------
 Function for generating a TWI Stop Condition. Used to release 
 the TWI bus.
---------------------------------------------------------------*/
void I2C2_Stop(void)
{
	SDA2_LOW;	    	
	PLATFORM_DelayUS(5);
	SCL2_HIGH;
	PLATFORM_DelayUS(5);
	SDA2_HIGH;
	PLATFORM_DelayUS(5);
//	SCL2_LOW;
//	PLATFORM_DelayUS(5);
}
