/***********************************************************************************************\
 * File Name:	WF200DP.c																		*
 *																								*
 * Weifengheng WF200DPZ0.005BGS16DT differential pressure sensor driver, I2C.					*
 * Same API as XGZP6891D.c - see WF200DP.h for the differences between the two parts.			*
 *																								*
 * DP1, DP2 and DP3 each sit on their own I2C bus (I2C1/I2C2/I2C3), exactly as with the			*
 * XGZP6891D, so all three sensors may keep the same device address.							*
\***********************************************************************************************/

#include "WF200DP.h"
#include "..\sb_const.h"
#include "i2cmaster.h"
#include "i2c2master.h"
#include "i2c3master.h"
#include "..\platform.h"

#if (PRESSURE_SENSOR_PART == PRESSURE_SENSOR_WF200DP)

//=====================================================================================
// Start a combined (temperature + pressure) conversion
//=====================================================================================
void TriggerConvWF200DP(uint8_t SensNo)
{
	switch(SensNo)
	{
		case DP1:

			I2C1_Start();								// Start condition
			Write_Byte_I2C1(WF200DP_I2C_ADDR_W);		// Device address + write
			Write_Byte_I2C1(WF200DP_REG_CMD);			// Command register
			Write_Byte_I2C1(WF200DP_CMD_COMBINED);		// Temperature then pressure
			I2C1_Stop();								// Send a STOP condition on the TWI bus.

		break;

		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		case DP2:

			I2C2_Start();
			Write_Byte_I2C2(WF200DP_I2C_ADDR_W);
			Write_Byte_I2C2(WF200DP_REG_CMD);
			Write_Byte_I2C2(WF200DP_CMD_COMBINED);
			I2C2_Stop();

		break;

		case DP3:

			I2C3_Start();
			Write_Byte_I2C3(WF200DP_I2C_ADDR_W);
			Write_Byte_I2C3(WF200DP_REG_CMD);
			Write_Byte_I2C3(WF200DP_CMD_COMBINED);
			I2C3_Stop();

		break;
		#endif
	}
}

//=====================================================================================
// Read the 24-bit pressure result and convert it to Pa
//=====================================================================================
uint8_t ReadWF200DP(uint8_t SensNo, float *value)
{
	uint32_t pressure_adc=0;
	int32_t  pressure_signed=0;
	uint8_t  data[3]={0xFF,0xFF,0xFF};
	uint8_t  ackErr=0;

	switch(SensNo)
	{
		case DP1:

			I2C1_Start();
			ackErr |= Write_Byte_I2C1(WF200DP_I2C_ADDR_W);
			ackErr |= Write_Byte_I2C1(WF200DP_REG_PRESSURE_MSB);
			I2C1_Start();								// Repeated start
			ackErr |= Write_Byte_I2C1(WF200DP_I2C_ADDR_R);
			data[0] = Read_Byte_I2C1(ACK);				// <23:16>
			data[1] = Read_Byte_I2C1(ACK);				// <15:8>
			data[2] = Read_Byte_I2C1(NO_ACK);			// <7:0>
			I2C1_Stop();

		break;

		#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
		case DP2:

			I2C2_Start();
			ackErr |= Write_Byte_I2C2(WF200DP_I2C_ADDR_W);
			ackErr |= Write_Byte_I2C2(WF200DP_REG_PRESSURE_MSB);
			I2C2_Start();
			ackErr |= Write_Byte_I2C2(WF200DP_I2C_ADDR_R);
			data[0] = Read_Byte_I2C2(ACK);
			data[1] = Read_Byte_I2C2(ACK);
			data[2] = Read_Byte_I2C2(NO_ACK);
			I2C2_Stop();

		break;

		case DP3:

			I2C3_Start();
			ackErr |= Write_Byte_I2C3(WF200DP_I2C_ADDR_W);
			ackErr |= Write_Byte_I2C3(WF200DP_REG_PRESSURE_MSB);
			I2C3_Start();
			ackErr |= Write_Byte_I2C3(WF200DP_I2C_ADDR_R);
			data[0] = Read_Byte_I2C3(ACK);
			data[1] = Read_Byte_I2C3(ACK);
			data[2] = Read_Byte_I2C3(NO_ACK);
			I2C3_Stop();

		break;
		#endif

		default:
		return 1;
	}

	//A missing sensor leaves the pulled-up bus reading 0xFFFFFF, which in two's
	//complement is -1 LSB - i.e. almost exactly 0 Pa.  That would look like a healthy
	//sensor reporting no pressure, so the NAK check below is what actually catches an
	//absent part; the all-ones test is a second line of defence.
	if(ackErr & ACK_ERROR)
	{
		return 1;
	}

	pressure_adc  = (uint32_t)data[0];
	pressure_adc <<= 8;
	pressure_adc |= (uint32_t)data[1];
	pressure_adc <<= 8;
	pressure_adc |= (uint32_t)data[2];

	if(pressure_adc == 0x00FFFFFF)
	{
		return 1;
	}

	//24-bit two's complement, zero at 0x800000
	if(pressure_adc & 0x00800000UL)
	{
		pressure_signed = (int32_t)pressure_adc - 16777216L;
	}
	else
	{
		pressure_signed = (int32_t)pressure_adc;
	}

	*value = ((float)pressure_signed / WF200DP_ZERO_COUNT) * WF200DP_FULL_SCALE_PA;

	return 0;
}

#endif	// (PRESSURE_SENSOR_PART == PRESSURE_SENSOR_WF200DP)
