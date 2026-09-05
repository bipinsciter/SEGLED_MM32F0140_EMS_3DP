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

	pressure_adc  = (uint32_t)data[0];
	pressure_adc <<= 8;
	pressure_adc |= (uint32_t)data[1];
	pressure_adc <<= 8;
	pressure_adc |= (uint32_t)data[2];
	pressure_adc &= 0x7FFFFF;
	
	if(pressure_adc == 0x7FFFFF) 
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
