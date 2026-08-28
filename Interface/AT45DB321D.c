/***********************************************************************************************\
 * AUTHOR: 		Bipin Patel			 															*
 * Date: 		30/08/2013																		*
 * File Name:	SPI.c																			*  		
 ***********************************************************************************************/
#include "AT45DB321D.h"
#include "..\platform.h"
#include "hal_conf.h"
#include "..\gpio.h"

union
{
	uint32_t dummyAddress;
	uint8_t dAddr[4];
}dA;

bool at45d_ready_flag=false;

void WriteEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes)
{
	uint16_t Page=0,StartAddress=0,RemainSpace=0;
	uint8_t nextcheck=0;
	
	//AT45D_ResumeFromPowerDown();
	
	while(!nextcheck)
	{
		Page = (uint16_t)(Address >> AT45D_PAGE_SHIFT);
		StartAddress = (uint16_t)(Address & 0x000001FF);

		//Copy Selected Page to Buffer1 ----------------------------	
		dA.dummyAddress = Address & 0xFFFFFE00;
		
		while(!AT45D_Ready());
		
		uint8_t buffer1[]={AT45D_CMD_MAIN_MEM_PAGE_TO_BUF1, dA.dAddr[2], dA.dAddr[1], dA.dAddr[0]};
		SPI_WriteBuffer(buffer1, 4);
		
		//Write Data to Buffer1 -------------------------------------
		dA.dummyAddress = (uint32_t)StartAddress;
		
		// Set flag to busy
		at45d_ready_flag = false;
		while(!AT45D_Ready());

		buffer1[0]=AT45D_CMD_BUFFER1_WRITE;		// Send command
		buffer1[1]=dA.dAddr[2];					// Send start byte in buffer
		buffer1[2]=dA.dAddr[1];
		buffer1[3]=dA.dAddr[0];
		SPI_FLASH_CS_L();		// Select DataFlash
		SPI_TxData_Polling(buffer1, 4);

		// See if "number of bytes to read" should be clipped
		RemainSpace = AT45D_PAGE_SIZE - StartAddress;
		if(bytes > RemainSpace)
		{
			//RemainBytes = bytes-RemainSpace;
			while(RemainSpace)
			{
				SPI_TxData_Polling(buffer,1);
				buffer++;
				RemainSpace--;
				bytes--;
				Address++;
			}
			nextcheck=0;
		}
		else
		{
			while(bytes)
			{
				SPI_TxData_Polling(buffer,1);
				buffer++;
				bytes--;
				Address++;
			}
			
			nextcheck=1;
		}
		SPI_FLASH_CS_H();		// Deselect DataFlash
		
		//Copy Buffer1 to Selected Page -----------------------------
		dA.dummyAddress = (uint32_t)Page<<9;
		
		// Set flag to busy
		at45d_ready_flag = false;
		while(!AT45D_Ready());
		
		buffer1[0]=AT45D_CMD_BUF1_TO_MAIN_PAGE_PRG_W_ERASE;
		buffer1[1]=dA.dAddr[2];
		buffer1[2]=dA.dAddr[1];
		buffer1[3]=dA.dAddr[0];
		
		SPI_WriteBuffer(buffer1, 4);
		//----------------------------------------------------------
		
		// Set flag to busy
		at45d_ready_flag = false;
		while(!AT45D_Ready());
	}
	
	//AT45D_PowerDown();
}

void ReadEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes)
{
	//AT45D_ResumeFromPowerDown();
	
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());

	uint8_t buffer1[8]={0};
	
	// Send command
	buffer1[0]=AT45D_CMD_CONTINUOUS_ARRAY_READ;
	
	// Send address
	buffer1[1]=(uint8_t)(Address>>16);
	buffer1[2]=(uint8_t)(Address>>8);
	buffer1[3]=(uint8_t)(Address);
	
	// Send dont-care bits
	buffer1[4]=0;
	buffer1[5]=0;
	buffer1[6]=0;
	buffer1[7]=0;
	
	SPI_FLASH_CS_L();		// Select DataFlash
	SPI_TxData_Polling(buffer1, 8);
	SPI_RxData_Polling(buffer,bytes);	// Read data
	SPI_FLASH_CS_H();		// Deselect DataFlash
	
	//AT45D_PowerDown();
}


void WriteLog(uint32_t AddrOffset,uint32_t LogInd,uint8_t *buffer,uint16_t bytes)
{
	uint16_t Page=0,StartAddress=0,RemainSpace=0;
	uint32_t Address=0;
	uint8_t nextcheck=0;
	
	Address = LogInd * bytes;
	Address += AddrOffset;
	
	//AT45D_ResumeFromPowerDown();
	
	while(!nextcheck)
	{
		Page = (uint16_t)(Address >> AT45D_PAGE_SHIFT);
		StartAddress = (uint16_t)(Address & 0x000001FF);

		//Copy Selected Page to Buffer1 ----------------------------
		
		dA.dummyAddress = Address & 0xFFFFFE00;
		
		while(!AT45D_Ready());
		
		uint8_t buffer1[]={AT45D_CMD_MAIN_MEM_PAGE_TO_BUF1, dA.dAddr[2], dA.dAddr[1], dA.dAddr[0]};
		SPI_WriteBuffer(buffer1, 4);
		
		//Write Data to Buffer1 -------------------------------------

		//dA.dummyAddress = 0;
		dA.dummyAddress = (uint32_t)StartAddress;
		
		// Set flag to busy
		at45d_ready_flag = false;
		while(!AT45D_Ready());

		buffer1[0]=AT45D_CMD_BUFFER1_WRITE;		// Send command
		buffer1[1]=dA.dAddr[2];					// Send start byte in buffer
		buffer1[2]=dA.dAddr[1];
		buffer1[3]=dA.dAddr[0];
		SPI_FLASH_CS_L();		// Select DataFlash
		SPI_TxData_Polling(buffer1, 4);

		// See if "number of bytes to read" should be clipped
		RemainSpace = AT45D_PAGE_SIZE - StartAddress;
		if(bytes > RemainSpace)
		{
			//RemainBytes = bytes-RemainSpace;
			while(RemainSpace)
			{
				SPI_TxData_Polling(buffer,1);
				buffer++;
				RemainSpace--;
				bytes--;
				Address++;
			}
			nextcheck=0;
		}
		else
		{
			while(bytes)
			{
				SPI_TxData_Polling(buffer,1);
				buffer++;
				bytes--;
				Address++;
			}
			
			nextcheck=1;
		}
		SPI_FLASH_CS_H();		// Deselect DataFlash
		
		//Copy Buffer1 to Selected Page -----------------------------
		dA.dummyAddress = (uint32_t)Page<<9;
		//dA.dummyAddress <<= 9;
		
		//dA.dummyAddress &= 0xFFFFFE00;
		
		// Set flag to busy
		at45d_ready_flag = false;
		while(!AT45D_Ready());
		
		buffer1[0]=AT45D_CMD_BUF1_TO_MAIN_PAGE_PRG_W_ERASE;
		buffer1[1]=dA.dAddr[2];
		buffer1[2]=dA.dAddr[1];
		buffer1[3]=dA.dAddr[0];
		
		SPI_WriteBuffer(buffer1, 4);
		//----------------------------------------------------------
		
		// Set flag to busy
		at45d_ready_flag = false;
		while(!AT45D_Ready());
	}
	
	//AT45D_PowerDown();
}

void ReadLog(uint32_t LogInd,uint8_t *buffer,uint16_t bytes)
{
	uint32_t Address=0;
	
	Address = REGULAR_LOG_ADDR + (LogInd * LOG_SIZE);
	
	//AT45D_ResumeFromPowerDown();
	
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());

	uint8_t buffer1[8]={0};
	
	// Send command
	buffer1[0]=AT45D_CMD_CONTINUOUS_ARRAY_READ;
	
	// Calculate page, offset and number of bytes remaining in page
	//page               = address >> 8;
	//start_byte_in_page = address & 0xff;
	
	// Send address
	buffer1[1]=(uint8_t)(Address>>16);
	buffer1[2]=(uint8_t)(Address>>8);
	buffer1[3]=(uint8_t)(Address);
	
	// Send dont-care bits
	buffer1[4]=0;
	buffer1[5]=0;
	buffer1[6]=0;
	buffer1[7]=0;
	
	SPI_FLASH_CS_L();		// Select DataFlash
	SPI_TxData_Polling(buffer1, 8);
	SPI_RxData_Polling(buffer,bytes);	// Read data
	SPI_FLASH_CS_H();		// Deselect DataFlash
	
	//AT45D_PowerDown();
}

void ReadMinMaxLog(uint32_t AddrOffset,uint32_t LogInd,uint8_t *buffer,uint16_t bytes)
{
	uint32_t Address=0;
	
	Address = LogInd * bytes;
	Address += AddrOffset;
	//AT45D_ResumeFromPowerDown();
	
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());

	uint8_t buffer1[8]={0};
	
	// Send command
	buffer1[0]=AT45D_CMD_CONTINUOUS_ARRAY_READ;
	
	// Calculate page, offset and number of bytes remaining in page
	//page               = address >> 8;
	//start_byte_in_page = address & 0xff;
	
	// Send address
	buffer1[1]=(uint8_t)(Address>>16);
	buffer1[2]=(uint8_t)(Address>>8);
	buffer1[3]=(uint8_t)(Address);
	
	// Send dont-care bits
	buffer1[4]=0;
	buffer1[5]=0;
	buffer1[6]=0;
	buffer1[7]=0;
	
	SPI_FLASH_CS_L();		// Select DataFlash
	SPI_TxData_Polling(buffer1, 8);
	SPI_RxData_Polling(buffer,bytes);	// Read data
	SPI_FLASH_CS_H();		// Deselect DataFlash

	//AT45D_PowerDown();
}

void AT45D_Init(void)
{
	FRESET_DIR_OUT;
	FRESET_HIGH;
	PLATFORM_DelayMS(1);
	FRESET_LOW;
	PLATFORM_DelayMS(5);
	FRESET_HIGH;
}

void AT45D_ChipErase(void)
{
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());
	
	uint8_t buffer[]={0xC7, 0x94, 0x80, 0x9A};
	SPI_WriteBuffer(buffer, 4);
	
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());
}

void AT45D_SectorErase(uint8_t Sector)
{
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());
	
	uint8_t buffer[]={0x7C, Sector, 0x00, 0x00};
	SPI_WriteBuffer(buffer, 4);
	
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());
}

void AT45D_BlockErase(uint16_t Block)
{
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());
	
	Block <<= 4;
	
	uint8_t buffer[]={0x50, Block>>8, Block & 0x00FF, 0x00};
	SPI_WriteBuffer(buffer, 4);
	
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());
}

void AT45D_PageErase(uint16_t Page)
{
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());
	
	Page <<= 1;
	
	uint8_t buffer[]={0x81, Page>>8, Page & 0x00FF, 0x00};
	SPI_WriteBuffer(buffer, 4);
	
	// Wait until DataFlash is not busy
	at45d_ready_flag = false;
	while(!AT45D_Ready());
}

void AT45D_PowerDown(void)
{
	uint8_t cmd=AT45D_CMD_MAIN_POWER_DOWN;
	SPI_WriteBuffer(&cmd, 1);
}

void AT45D_ResumeFromPowerDown(void)
{
	uint8_t cmd=AT45D_CMD_MAIN_RESUME_FROM_POWER_DOWN;
	SPI_WriteBuffer(&cmd, 1);
}

uint8_t AT45D_Ready(void)
{
	uint8_t data;

    // If flag has already been set, then take short cut
    if(at45d_ready_flag)
    {
        return true;
    }

    // Get DataFlash status
    data = AT45D_GetStatus();

    // See if DataFlash is ready
    if(data & AT45D_STATUS_READY)
    {
        // Set flag
        at45d_ready_flag = true;
        return true;
    }
    else
    {
        return false;
    }
}

uint8_t AT45D_GetStatus(void)
{
	uint8_t data,cmd=AT45D_CMD_STATUS_REGISTER_READ;
	
	SPI_FLASH_CS_L();		// Select DataFlash
	SPI_TxData_Polling(&cmd, 1);
	SPI_RxData_Polling(&data,1);	// Read data
	SPI_FLASH_CS_H();		// Deselect DataFlash

    return data;
}

uint8_t AT45D_page_size_is_pwr_of_two(void)
{
	uint8_t data = AT45D_GetStatus();

    if(data & AT45D_STATUS_PAGE_SIZE)
    {
        // Page size is a power of two
        return true;
    }
    else
    {
        // Page size is not a power of two
        return false;
    }
}

uint8_t AT45D_set_page_size_to_pwr_of_two(void)
{
	if(AT45D_page_size_is_pwr_of_two())
    {
        // Page size is already a power of two
        return false;
    }

    // Wait until DataFlash is not busy
	at45d_ready_flag = false;
    while(!AT45D_Ready());
	
	uint8_t buffer[]={0x3D, 0x2A, 0x80, 0xA6};
	SPI_WriteBuffer(buffer, 4);

    // Wait until DataFlash is not busy
	at45d_ready_flag = false;
    while(!AT45D_Ready());

    return true;
}


