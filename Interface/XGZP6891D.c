#include "XGZP6891D.h"
#include "..\sb_const.h"
#include "i2cmaster.h"
#include "i2c2master.h"
#include "i2c3master.h"
#include "..\platform.h"

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
	uint8_t data[3]={0};
	uint8_t DpError;
	
	switch(SensNo)
	{
		case DP1:
			
			I2C1_Start();						// Start condition
			Write_Byte_I2C1(0xFE); 				// Write device address
			Write_Byte_I2C1(0x06);
			I2C1_Start();						// Start condition
			Write_Byte_I2C1(0xFE|1); 			// Write device address
			data[0] = Read_Byte_I2C1(ACK);
			data[1] = Read_Byte_I2C1(ACK);
			data[2] = Read_Byte_I2C1(NO_ACK);
			I2C1_Stop();              			// Send a STOP condition on the TWI bus.

		break;
		
		case DP2:
			
			I2C2_Start();						// Start condition
			Write_Byte_I2C2(0xFE); 				// Write device address
			Write_Byte_I2C2(0x06);
			I2C2_Start();						// Start condition
			Write_Byte_I2C2(0xFE|1); 			// Write device address
			data[0] = Read_Byte_I2C2(ACK);
			data[1] = Read_Byte_I2C2(ACK);
			data[2] = Read_Byte_I2C2(NO_ACK);
			I2C2_Stop();              			// Send a STOP condition on the TWI bus.

		break;
			
		case DP3:
			
			I2C3_Start();						// Start condition
			Write_Byte_I2C3(0xFE); 				// Write device address
			Write_Byte_I2C3(0x06);
			I2C3_Start();						// Start condition
			Write_Byte_I2C3(0xFE|1); 			// Write device address
			data[0] = Read_Byte_I2C3(ACK);
			data[1] = Read_Byte_I2C3(ACK);
			data[2] = Read_Byte_I2C3(NO_ACK);
			I2C3_Stop();              			// Send a STOP condition on the TWI bus.

		break;
	}
	
	pressure_adc = data[0];
	pressure_adc<<=8;
	pressure_adc |= data[1];
	pressure_adc<<=8;
	pressure_adc |= data[2];
	pressure_adc &= 0x7FFFFF;
	
	if(pressure_adc == 0x7FFFFF) 
	{
		DpError=1;
	}
	else
	{
		DpError=0;
		*value = (pressure_adc * PARA_A) + PARA_B;
	}
	
	return DpError;
}	
	

//uint32_t ReadXGZP6891D(uint8_t SensNo)
//{	
//	uint8_t temp=0;
//	uint32_t differanceDP=0;
//	uint8_t data[3]={0};

//	if(SensNo==DP1)
//	{
//		//Start Command Mode ----------------------------------
//		I2C1_Start();						// Start condition
//		Write_Byte_I2C1(0xFE); 				// Write device address
//		Write_Byte_I2C1(0x30);				// Write command
//		Write_Byte_I2C1(0x0A);
//		I2C1_Stop();     			        // Send a STOP condition on the TWI bus.
//		
//		PLATFORM_DelayMS(1);
//		
//		do
//		{
//			I2C1_Start();						// Start condition
//			Write_Byte_I2C1(0xFE); 				// Write device address
//			Write_Byte_I2C1(0x30);				// Write command
//			I2C1_Start();						// Start condition
//			Write_Byte_I2C1(0xFE|1); 			// Read command
//			data[1] = Read_Byte_I2C1(NO_ACK);
//			I2C1_Stop();              			// Send a STOP condition on the TWI bus.
//			
//			PLATFORM_DelayMS(4);
//			if(++temp>4) break;
//			
//		}while(data[1] & 0x08);
//		
//		I2C1_Start();						// Start condition
//		Write_Byte_I2C1(0xFE); 				// Write device address
//		Write_Byte_I2C1(0x06);
//		I2C1_Start();						// Start condition
//		Write_Byte_I2C1(0xFE|1); 			// Write device address
//		data[0] = Read_Byte_I2C1(ACK);
//		data[1] = Read_Byte_I2C1(ACK);
//		data[2] = Read_Byte_I2C1(NO_ACK);
//		I2C1_Stop();              			// Send a STOP condition on the TWI bus.
//	}
//	else if(SensNo==DP2)
//	{
//		//Start Command Mode ----------------------------------
//		I2C2_Start();						// Start condition
//		Write_Byte_I2C2(0xFE); 				// Write device address
//		Write_Byte_I2C2(0x30);
//		Write_Byte_I2C2(0x0A);
//		I2C2_Stop();              			// Send a STOP condition on the TWI bus.
//		
//		PLATFORM_DelayMS(1);
//		
//		do
//		{
//			I2C2_Start();						// Start condition
//			Write_Byte_I2C2(0xFE); 				// Write device address
//			Write_Byte_I2C2(0x30);
//			I2C2_Start();						// Start condition
//			Write_Byte_I2C2(0xFE|1); 			// Write device address
//			data[1] = Read_Byte_I2C2(NO_ACK);
//			I2C2_Stop();              // Send a STOP condition on the TWI bus.
//			
//			PLATFORM_DelayMS(4);
//			if(++temp>4) break;
//			
//		}while(data[1] & 0x08);
//		
//		I2C2_Start();						// Start condition
//		Write_Byte_I2C2(0xFE); 				// Write device address
//		Write_Byte_I2C2(0x06);
//		I2C2_Start();						// Start condition
//		Write_Byte_I2C2(0xFE|1); 			// Write device address
//		data[0] = Read_Byte_I2C2(ACK);
//		data[1] = Read_Byte_I2C2(ACK);
//		data[2] = Read_Byte_I2C2(NO_ACK);
//		I2C2_Stop();              // Send a STOP condition on the TWI bus.
//	}
//	else if(SensNo==DP3)
//	{
//		//Start Command Mode ----------------------------------
//		I2C3_Start();						// Start condition
//		Write_Byte_I2C3(0xFE); 				// Write device address
//		Write_Byte_I2C3(0x30);
//		Write_Byte_I2C3(0x0A);
//		I2C3_Stop();              // Send a STOP condition on the TWI bus.
//		
//		PLATFORM_DelayMS(1);
//		
//		do
//		{
//			I2C3_Start();						// Start condition
//			Write_Byte_I2C3(0xFE); 				// Write device address
//			Write_Byte_I2C3(0x30);
//			I2C3_Start();						// Start condition
//			Write_Byte_I2C3(0xFE|1); 			// Write device address
//			data[1] = Read_Byte_I2C3(NO_ACK);
//			I2C3_Stop();              // Send a STOP condition on the TWI bus.
//			
//			PLATFORM_DelayMS(4);
//			if(++temp>4) break;
//			
//		}while(data[1] & 0x08);
//		
//		I2C3_Start();						// Start condition
//		Write_Byte_I2C3(0xFE); 				// Write device address
//		Write_Byte_I2C3(0x06);
//		I2C3_Start();						// Start condition
//		Write_Byte_I2C3(0xFE|1); 			// Write device address
//		data[0] = Read_Byte_I2C3(ACK);
//		data[1] = Read_Byte_I2C3(ACK);
//		data[2] = Read_Byte_I2C3(NO_ACK);
//		I2C3_Stop();              // Send a STOP condition on the TWI bus.
//	}
//	
//	differanceDP = data[0];
//	differanceDP<<=8;
//	differanceDP |= data[1];
//	differanceDP<<=8;
//	differanceDP |= data[2];
//	
//	differanceDP &= 0x7FFFFF;
//	
//	return differanceDP;
//}	




