#include "XGZP6891D.h"
#include "..\sb_const.h"
#include "i2cmaster.h"
#include "i2c2master.h"
#include "i2c3master.h"
#include "..\platform.h"

#if (PRESSURE_SENSOR_PART == PRESSURE_SENSOR_XGZP6891D)

void TriggerConvSM9543(uint8_t SensNo)
{
	switch(SensNo)
	{
		case DP1:
			
			//Start Command Mode ----------------------------------
			I2C1_Start();						// Start condition
			Write_Byte_I2C1(0xFE); 				// Write device address
			Write_Byte_I2C1(0x30);				// Write command
			Write_Byte_I2C1(0x0A);
			I2C1_Stop();     			        // Send a STOP condition on the TWI bus.

		break;
		
		case DP2:
			
			//Start Command Mode ----------------------------------
			I2C2_Start();						// Start condition
			Write_Byte_I2C2(0xFE); 				// Write device address
			Write_Byte_I2C2(0x30);				// Write command
			Write_Byte_I2C2(0x0A);
			I2C2_Stop();     			        // Send a STOP condition on the TWI bus.

		break;
			
		case DP3:
			
			//Start Command Mode ----------------------------------
			I2C3_Start();						// Start condition
			Write_Byte_I2C3(0xFE); 				// Write device address
			Write_Byte_I2C3(0x30);				// Write command
			Write_Byte_I2C3(0x0A);
			I2C3_Stop();     			        // Send a STOP condition on the TWI bus.

		break;
	}
}

uint8_t ReadXGZP6891D(uint8_t SensNo, float *value)
{	
	uint32_t pressure_adc=0;
	uint8_t data[3]={0xFF,0xFF,0xFF};
	uint8_t DpError=0;
	uint8_t ackErr=0;
	
	switch(SensNo)
	{
		case DP1:
			
			I2C1_Start();						// Start condition
			ackErr |= Write_Byte_I2C1(0xFE); 				// Write device address
			ackErr |= Write_Byte_I2C1(0x06);
			I2C1_Start();						// Start condition
			ackErr |= Write_Byte_I2C1(0xFE|1); 			// Write device address
			data[0] = Read_Byte_I2C1(ACK);
			data[1] = Read_Byte_I2C1(ACK);
			data[2] = Read_Byte_I2C1(NO_ACK);
			I2C1_Stop();              			// Send a STOP condition on the TWI bus.

		break;
		
		case DP2:
			
			I2C2_Start();						// Start condition
			ackErr |= Write_Byte_I2C2(0xFE); 				// Write device address
			ackErr |= Write_Byte_I2C2(0x06);
			I2C2_Start();						// Start condition
			ackErr |= Write_Byte_I2C2(0xFE|1); 			// Write device address
			data[0] = Read_Byte_I2C2(ACK);
			data[1] = Read_Byte_I2C2(ACK);
			data[2] = Read_Byte_I2C2(NO_ACK);
			I2C2_Stop();              			// Send a STOP condition on the TWI bus.

		break;
			
		case DP3:
			
			I2C3_Start();						// Start condition
			ackErr |= Write_Byte_I2C3(0xFE); 				// Write device address
			ackErr |= Write_Byte_I2C3(0x06);
			I2C3_Start();						// Start condition
			ackErr |= Write_Byte_I2C3(0xFE|1); 			// Write device address
			data[0] = Read_Byte_I2C3(ACK);
			data[1] = Read_Byte_I2C3(ACK);
			data[2] = Read_Byte_I2C3(NO_ACK);
			I2C3_Stop();              			// Send a STOP condition on the TWI bus.

		break;

		default:
		return 1;
	}
	
	//A sensor that does not acknowledge its address is absent or dead.  The 0x7FFFFF
	//test below already catches a fully floating bus, but it cannot tell that apart from
	//a genuine full-scale reading, and it misses a sensor that NAKs part way through.
	//Checking the ACK catches the fault where it actually happens.
	if(ackErr & ACK_ERROR)
	{
		return 1;
	}

	//Registers 0x06/0x07/0x08 form ONE plain unsigned 24-bit value - the datasheet
	//builds it exactly this way in its own reference code, and does not sign-extend it
	//the way it does for the 16-bit temperature.
	//
	//This used to mask with 0x7FFFFF.  For the -500..+500 Pa part fitted here the
	//calibrated span is codes 838861..7549746, so bit 23 is clear on every in-range
	//reading and the mask looked harmless - but above +625 Pa the code passes 2^23 and
	//the mask wrapped it round to a small value, i.e. a strong POSITIVE pressure was
	//displayed as a strong negative one.  That is inside this product's working range:
	//an alarm setpoint may be set as high as DP_ALM_LIMIT_MIN (981 Pa).  See
	//XGZP6891D.h for the transfer-function table this all comes from.
	pressure_adc  = (uint32_t)data[0];
	pressure_adc <<= 8;
	pressure_adc |= (uint32_t)data[1];
	pressure_adc <<= 8;
	pressure_adc |= (uint32_t)data[2];
	
	//Every 24-bit code maps to some pressure between -625 and +1875 Pa, so a dead bus
	//cannot be spotted from the converted value - it has to be caught here.  A bus
	//floating high reads all ones and one held low reads all zeros; both would
	//otherwise pass for a reading at the extreme end of the line.
	if((pressure_adc == 0x00FFFFFFUL) || (pressure_adc == 0x00000000UL))
	{
		DpError=1;
	}
	else
	{
		DpError=0;
		*value = ((float)pressure_adc * PARA_A) + PARA_B;
	}
	
	return DpError;
}	


#endif	// (PRESSURE_SENSOR_PART == PRESSURE_SENSOR_XGZP6891D)
