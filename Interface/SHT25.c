#include "SHT25.h"
#include "i2c3master.h"
#include "..\platform.h"

//==============================================================================
unsigned char SHT2x_CheckCrc(unsigned char data[], unsigned char nbrOfBytes, unsigned char checksum)
//==============================================================================
{
	unsigned char crc = 0;	
	unsigned char byteCtr;

	//calculates 8-Bit checksum with given polynomial
	for (byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr)
	{ 
		crc ^= (data[byteCtr]);
		
		for (unsigned char bit = 8; bit > 0; --bit)
		{ 
			if (crc & 0x80) 
			{
				crc = (crc << 1) ^ POLYNOMIAL;
			}
			else 
			{
				crc = (crc << 1);
			}
		}
	}
	
	if (crc != checksum) return CHECKSUM_ERROR;
	else return 0;
}

//===========================================================================
unsigned char SHT2x_ReadUserRegister(unsigned char *pRegisterValue)
//===========================================================================
{
	unsigned char checksum;   //variable for checksum byte
	unsigned char error=0;    //variable for error code

	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_WRITE);
	error |= Write_Byte_I2C3 (USER_REG_R);
	
	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_READ);
	*pRegisterValue = Read_Byte_I2C3(ACK);
	checksum=Read_Byte_I2C3(NO_ACK);
	
	error |= SHT2x_CheckCrc (pRegisterValue,1,checksum);
	I2C3_Stop();
	
	return error;
}

//===========================================================================
unsigned char SHT2x_WriteUserRegister(unsigned char *pRegisterValue)
//===========================================================================
{
	unsigned char error=0;   //variable for error code

	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_WRITE);
	error |= Write_Byte_I2C3 (USER_REG_W);
	error |= Write_Byte_I2C3 (*pRegisterValue);
	I2C3_Stop();
	
	return error;
}

//===========================================================================
unsigned char SHT2x_MeasureHM(unsigned char eSHT2xMeasureType, nt16 *pMeasurand)
//===========================================================================
{
	unsigned char  checksum;   //checksum
	unsigned char  data[2];    //data array for checksum verification
	unsigned char  error=0;    //error variable
	unsigned short i;          //counting variable

	//-- write I2C sensor address and command --
	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_WRITE); // I2C Adr
	switch(eSHT2xMeasureType)
	{ 
		case HUMIDITY: error |= Write_Byte_I2C3 (TRIG_RH_MEASUREMENT_HM); break;
		case TEMP    : error |= Write_Byte_I2C3 (TRIG_T_MEASUREMENT_HM);  break;
		//default: assert(0);
	}
	
	//-- wait until hold master is released --
	I2C3_Start();//SHT25_I2CStartCondition();
	error |= Write_Byte_I2C3 (SHT25_I2C_READ);
	SCL3_DIR_IN;                     // set SCL I/O port as input
	for(i=0; i<1000; i++)         // wait until master hold is released or
	{ 
		PLATFORM_DelayMS(1);    // a timeout (~1s) is reached
		
		if (SCL3_SENSE==1) break;
	}
	
	//-- check for timeout --
	if(SCL3_SENSE==0) error |= TIME_OUT_ERROR;
	
	SCL3_DIR_OUT;                     // set SCL I/O port as output

	//-- read two data bytes and one checksum byte --
	pMeasurand->s16.u8H = data[0] = Read_Byte_I2C3(ACK);
	pMeasurand->s16.u8L = data[1] = Read_Byte_I2C3(ACK);
	checksum=Read_Byte_I2C3(NO_ACK);

	//-- verify checksum --
	error |= SHT2x_CheckCrc (data,2,checksum);
	I2C3_Stop();
	return error;
}

//===========================================================================
unsigned char SHT2x_MeasurePoll(unsigned char eSHT2xMeasureType, nt16 *pMeasurand)
//===========================================================================
{
	unsigned char  checksum;   //checksum
	unsigned char  data[2];    //data array for checksum verification
	unsigned char  error=0;    //error variable
	unsigned short i=0;        //counting variable

	//-- write I2C sensor address and command --
	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_WRITE); // I2C Adr
	switch(eSHT2xMeasureType)
	{ 
		case HUMIDITY: error |= Write_Byte_I2C3 (TRIG_RH_MEASUREMENT_POLL); break;
		case TEMP    : error |= Write_Byte_I2C3 (TRIG_T_MEASUREMENT_POLL);  break;
		//default: assert(0);
	}
	//-- poll every 10ms for measurement ready. Timeout after 20 retries (200ms)--
	do
	{ 
		I2C3_Start();
		PLATFORM_DelayMS(10);  //delay 10ms
		
		if(i++ >= 40) break;
	} while(Write_Byte_I2C3 (SHT25_I2C_READ) == ACK_ERROR);
	
	if (i>=40) error |= TIME_OUT_ERROR;

	//-- read two data bytes and one checksum byte --
	pMeasurand->s16.u8H = data[0] = Read_Byte_I2C3(ACK);
	pMeasurand->s16.u8L = data[1] = Read_Byte_I2C3(ACK);
	checksum=Read_Byte_I2C3(NO_ACK);

	//-- verify checksum --
	error |= SHT2x_CheckCrc (data,2,checksum);
	I2C3_Stop();

	return error;
}

//===========================================================================
unsigned char SHT2x_SoftReset(void)
//===========================================================================
{
	unsigned char  error=0;           //error variable

	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_WRITE); // I2C Adr
	error |= Write_Byte_I2C3 (SOFT_RESET);                            // Command
	I2C3_Stop();

	PLATFORM_DelayMS(15);	// wait till sensor has restarted
	
	return error;
}

//==============================================================================
float SHT2x_CalcRH(unsigned short u16sRH)
//==============================================================================
{
	float humidityRH;              // variable for result

	u16sRH &= ~0x0003;          // clear bits [1..0] (status bits)
	//-- calculate relative humidity [%RH] --

	humidityRH = -6.0 + 125.0/65536 * (float)u16sRH; // RH= -6 + 125 * SRH/2^16
	return humidityRH;
}

//==============================================================================
float SHT2x_CalcTemperatureC(unsigned short u16sT)
//==============================================================================
{
	float temperatureC;            // variable for result

	u16sT &= ~0x0003;           // clear bits [1..0] (status bits)

	//-- calculate temperature [°C] --
	temperatureC= -46.85 + 175.72/65536 *(float)u16sT; //T= -46.85 + 175.72 * ST/2^16
	return temperatureC;
}

//==============================================================================
unsigned char SHT2x_GetSerialNumber(unsigned char u8SerialNumber[])
//==============================================================================
{
	unsigned char  error=0;                          //error variable

	//Read from memory location 1
	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_WRITE);    //I2C address
	error |= Write_Byte_I2C3 (0xFA);         //Command for readout on-chip memory
	error |= Write_Byte_I2C3 (0x0F);         //on-chip memory address
	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_READ);    //I2C address
	u8SerialNumber[5] = Read_Byte_I2C3(ACK); //Read SNB_3
	Read_Byte_I2C3(ACK);                     //Read CRC SNB_3 (CRC is not analyzed)
	u8SerialNumber[4] = Read_Byte_I2C3(ACK); //Read SNB_2
	Read_Byte_I2C3(ACK);                     //Read CRC SNB_2 (CRC is not analyzed)
	u8SerialNumber[3] = Read_Byte_I2C3(ACK); //Read SNB_1
	Read_Byte_I2C3(ACK);                     //Read CRC SNB_1 (CRC is not analyzed)
	u8SerialNumber[2] = Read_Byte_I2C3(ACK); //Read SNB_0
	Read_Byte_I2C3(NO_ACK);                  //Read CRC SNB_0 (CRC is not analyzed)
	I2C3_Stop();

	//Read from memory location 2
	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_WRITE);    //I2C address
	error |= Write_Byte_I2C3 (0xFC);         //Command for readout on-chip memory
	error |= Write_Byte_I2C3 (0xC9);         //on-chip memory address
	I2C3_Start();
	error |= Write_Byte_I2C3 (SHT25_I2C_READ);    //I2C address
	u8SerialNumber[1] = Read_Byte_I2C3(ACK); //Read SNC_1
	u8SerialNumber[0] = Read_Byte_I2C3(ACK); //Read SNC_0
	Read_Byte_I2C3(ACK);                     //Read CRC SNC0/1 (CRC is not analyzed)
	u8SerialNumber[7] = Read_Byte_I2C3(ACK); //Read SNA_1
	u8SerialNumber[6] = Read_Byte_I2C3(ACK); //Read SNA_0
	Read_Byte_I2C3(NO_ACK);                  //Read CRC SNA0/1 (CRC is not analyzed)
	I2C3_Stop();

	return error;
}
