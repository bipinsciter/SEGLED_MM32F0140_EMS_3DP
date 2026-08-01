/***********************************************************************************************\
 * AUTHOR: 		Bipin Patel			 															*
 * Date: 		30/08/2013																		*
 * File Name:	AT45DB321D.h																			*  		
 ***********************************************************************************************/

#ifndef AT45DB321D_H_
#define AT45DB321D_H_

//#include "SPI.h"
#include "..\spi_master_polling.h"
#include "hal_conf.h"
#include "..\sb_const.h"

#define FRESET_DIR_OUT	GPIO_Configure_Output(GPIOA, GPIO_Pin_7)
#define FRESET_HIGH		GPIO_SetBits(GPIOA, GPIO_Pin_7)
#define FRESET_LOW		GPIO_ResetBits(GPIOA, GPIO_Pin_7)

#define AT45D_PAGE_COUNT          	8192  // 32Mbit / 512B = 8192 pages
#define AT45D_PAGE_SIZE         	512
#define AT45D_FLASH_SIZE          	(AT45D_PAGE_SIZE * AT45D_PAGE_COUNT)
#define AT45D_PAGE_SHIFT         	9

#define AT45D_STATUS_READY      	BIT7			/// Status register - Ready flag
#define AT45D_STATUS_COMP       	BIT6			/// Status register - Compare flag
#define AT45D_DENSITY_MASK      	0x2C		    /// Status register - Density mask
#define AT45D_STATUS_PROTECT    	BIT1			/// Status register - Protect flag
#define AT45D_STATUS_PAGE_SIZE  	BIT0			/// Status register - Page size flag
#define AT45D_FLASH_SIZE_BYTES   	(AT45D_PAGES*AT45D_PAGE_SIZE)		/// Flash size (in bytes)
#define AT45D_ADR_MAX            	(AT45D_FLASH_SIZE_BYTES-1)			/// Maximum adress

#define AT45D_CMD_CONTINUOUS_ARRAY_READ             0xe8
#define AT45D_CMD_MAIN_MEMORY_PAGE_READ             0xd2
#define AT45D_CMD_STATUS_REGISTER_READ              0xd7

#define AT45D_CMD_BUFFER1_WRITE                     0x84
#define AT45D_CMD_BUFFER2_WRITE                     0x87

#define AT45D_CMD_BUF1_TO_MAIN_PAGE_PRG_W_ERASE     0x83
#define AT45D_CMD_BUF2_TO_MAIN_PAGE_PRG_W_ERASE     0x86

#define AT45D_CMD_BUF1_TO_MAIN_PAGE_PRG_WO_ERASE    0x88
#define AT45D_CMD_BUF2_TO_MAIN_PAGE_PRG_WO_ERASE    0x89

#define AT45D_CMD_MAIN_MEM_PROG_THROUGH_BUF1        0x82
#define AT45D_CMD_MAIN_MEM_PROG_THROUGH_BUF2        0x85

#define AT45D_CMD_PAGE_ERASE                        0x81

#define AT45D_CMD_MAIN_MEM_PAGE_TO_BUF1             0x53
#define AT45D_CMD_MAIN_MEM_PAGE_TO_BUF2             0x55

#define AT45D_CMD_AUTO_PAGE_REWRITE_THROUGH_BUF1    0x58
#define AT45D_CMD_AUTO_PAGE_REWRITE_THROUGH_BUF2    0x59

#define AT45D_CMD_MAIN_POWER_DOWN                   0xB9
#define AT45D_CMD_MAIN_RESUME_FROM_POWER_DOWN       0xAB

#define LOG_SIZE	50

void AT45D_Init(void);
void AT45D_ChipErase(void);
void AT45D_SectorErase(uint8_t Sector);
void AT45D_BlockErase(uint16_t Block);
void AT45D_PageErase(uint16_t Page);
void WriteLog(uint32_t AddrOffset,uint32_t LogInd,uint8_t *buffer,uint16_t bytes);
void ReadLog(uint32_t LogInd,uint8_t *buffer,uint16_t bytes);
void ReadMinMaxLog(uint32_t AddrOffset,uint32_t LogInd,uint8_t *buffer,uint16_t bytes);
/// Power down device to minimise power consumption
void AT45D_PowerDown(void);
void WriteEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes);
void ReadEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes);

/// Power up device to resume communication
void AT45D_ResumeFromPowerDown(void);

/**
    Check if DataFlash is ready for the next read or write access.

    When data is written to DataFlash, the microcontroller must wait until the
    operation is finished, before the next one is attempted. This function
    allows the microcontroller to do something else while waiting for the
    operation to finish.

   @retval TRUE     DataFlash is ready for next read or write access.
   @retval FALSE    DataFlash is busy writing.
 */
uint8_t AT45D_Ready(void);

/**
    Read the status register of the DataFlash.

    The status register contains flags (e.g. #AT45D_STATUS_READY) and the
    density value (e.g. #AT45D_DENSITY). This function can be used to check that
    a valid and working DataFlash device is connected.

    @return u8_t Status register value
 */
uint8_t AT45D_GetStatus(void);

/**
    Check if page size of the DataFlash is a power of two.

    The status register contains a flag #AT45D_STATUS_PAGE_SIZE that reports the
    page size: 1 = power of two, 0 = not a power of two.

    @return bool_t TRUE  Page size is a power of two
                   FALSE Page size is not a power of two
 */
uint8_t AT45D_page_size_is_pwr_of_two(void);

/**
    Sets the page size of the DataFlash to a power of two.

    A one-time programmable configuration register is written to configure the
    device for “power of 2” page size. Once it has been written, it can not be
    reconfigured again.
    
    The device must be power cycled for this configuration bit to come into
    effect.
    
    @return bool_t TRUE     page size was set to a power of two
                   FALSE    page size was already set to a power of two
 */
uint8_t AT45D_set_page_size_to_pwr_of_two(void);

#endif /*AT45DB321D_H_*/
