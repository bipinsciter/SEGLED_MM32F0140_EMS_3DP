/***********************************************************************************************\
 * File Name:	XM25QH128A.h																	*
 *																								*
 * XMC XM25QH128A - 128 Mbit (16 MByte) standard SPI NOR flash.									*
 * Alternative data-flash part to the AT45DB321D, selected by DATAFLASH_PART in sb_const.h.		*
 *																								*
 * !! THE TWO PARTS DO NOT SHARE THE SAME WRITE MODEL !!										*
 *																								*
 *   AT45DB321D  - DataFlash with on-chip SRAM buffers.  A page program is internally a			*
 *                 read-modify-write, so ANY byte can be rewritten at ANY time and no explicit	*
 *                 erase is ever needed.														*
 *																								*
 *   XM25QH128A  - plain NOR.  A program can only change bits 1 -> 0.  To rewrite a byte, its	*
 *                 whole 4 KB sector must first be erased back to 0xFF.							*
 *																								*
 * WriteEEPROMData() below hides that difference so the application keeps working unchanged,	*
 * but it cannot hide the cost - see the block comment above WriteEEPROMData() in				*
 * XM25QH128A.c before using this part in production.											*
 *																								*
 * Datasheet: XM25QH128A preliminary Rev. H, 2018/08/06											*
\***********************************************************************************************/

#ifndef XM25QH128A_H_
#define XM25QH128A_H_

#include "..\spi_master_polling.h"
#include "hal_conf.h"
#include "..\sb_const.h"

//-------------------------------------------------------------------------------------
// Board wiring - same reset pin as the AT45DB321D footprint
//-------------------------------------------------------------------------------------
#define FRESET_DIR_OUT	GPIO_Configure_Output(GPIOA, GPIO_Pin_8)
#define FRESET_HIGH		GPIO_SetBits(GPIOA, GPIO_Pin_8)
#define FRESET_LOW		GPIO_ResetBits(GPIOA, GPIO_Pin_8)

//-------------------------------------------------------------------------------------
// Geometry (datasheet page 1 / Table 2)
//   128 Mbit = 16,384 KByte, 65,536 pages of 256 bytes, 4096 uniform sectors of 4 KByte
//-------------------------------------------------------------------------------------
#define XM25_PAGE_SIZE			256UL
#define XM25_PAGE_COUNT			65536UL
#define XM25_SECTOR_SIZE		4096UL			// smallest erasable unit
#define XM25_SECTOR_COUNT		4096UL
#define XM25_BLOCK32_SIZE		32768UL
#define XM25_BLOCK64_SIZE		65536UL
#define XM25_FLASH_SIZE			(XM25_PAGE_SIZE * XM25_PAGE_COUNT)		// 16,777,216
#define XM25_ADR_MAX			(XM25_FLASH_SIZE - 1UL)

#define XM25_PAGE_MASK			(XM25_PAGE_SIZE - 1UL)
#define XM25_SECTOR_MASK		(XM25_SECTOR_SIZE - 1UL)
#define XM25_SECTOR_BASE(a)		((a) & ~XM25_SECTOR_MASK)

//-------------------------------------------------------------------------------------
// Scratch sector used by WriteEEPROMData() to preserve a sector across an erase.
// Defaults to the LAST sector of the chip so it can never collide with the data map.
//-------------------------------------------------------------------------------------
#define XM25_SCRATCH_SECTOR		(XM25_FLASH_SIZE - XM25_SECTOR_SIZE)	// 0x00FFF000

//-------------------------------------------------------------------------------------
// Instruction set (Tables 5A - 5D)
//-------------------------------------------------------------------------------------
#define XM25_CMD_WRITE_ENABLE			0x06
#define XM25_CMD_VOL_SR_WRITE_ENABLE	0x50
#define XM25_CMD_WRITE_DISABLE			0x04

#define XM25_CMD_READ_STATUS1			0x05
#define XM25_CMD_READ_STATUS2			0x09
#define XM25_CMD_READ_STATUS3			0x95
#define XM25_CMD_WRITE_STATUS1			0x01
#define XM25_CMD_WRITE_STATUS3			0xC0

#define XM25_CMD_READ_DATA				0x03	// 24-bit address, no dummy byte
#define XM25_CMD_FAST_READ				0x0B	// 24-bit address, 1 dummy byte

#define XM25_CMD_PAGE_PROGRAM			0x02	// 24-bit address, max 256 bytes, no page crossing

#define XM25_CMD_SECTOR_ERASE_4K		0x20
#define XM25_CMD_BLOCK_ERASE_32K		0x52
#define XM25_CMD_BLOCK_ERASE_64K		0xD8
#define XM25_CMD_CHIP_ERASE				0xC7	// 0x60 is an accepted alias

#define XM25_CMD_WRITE_SUSPEND			0xB0
#define XM25_CMD_WRITE_RESUME			0x30

#define XM25_CMD_POWER_DOWN				0xB9
#define XM25_CMD_RELEASE_POWER_DOWN		0xAB	// also returns the device ID
#define XM25_CMD_MANUF_DEVICE_ID		0x90
#define XM25_CMD_JEDEC_ID				0x9F

#define XM25_CMD_RESET_ENABLE			0x66
#define XM25_CMD_RESET					0x99

//-------------------------------------------------------------------------------------
// Status register 1 bits (Table 7.1, normal mode)
//-------------------------------------------------------------------------------------
#define XM25_STATUS_WIP					0x01	// SR.0  1 = write/erase in progress
#define XM25_STATUS_WEL					0x02	// SR.1  1 = write enable latch set
#define XM25_STATUS_BP0					0x04
#define XM25_STATUS_BP1					0x08
#define XM25_STATUS_BP2					0x10
#define XM25_STATUS_BP3					0x20
#define XM25_STATUS_EBL					0x40
#define XM25_STATUS_SRP					0x80

//-------------------------------------------------------------------------------------
// Expected identification (Table 6)
//   RDID  (0x9F) -> 0x20 (XMC), 0x70 (type), 0x18 (capacity, 2^24 = 16 MByte)
//   MFDID (0x90) -> 0x20 (XMC), 0x17 (device)
//-------------------------------------------------------------------------------------
#define XM25_JEDEC_MANUFACTURER			0x20
#define XM25_JEDEC_MEMORY_TYPE			0x70
#define XM25_JEDEC_CAPACITY				0x18
#define XM25_DEVICE_ID					0x17

//-------------------------------------------------------------------------------------
// Worst-case program / erase times, used only as watchdog-friendly poll ceilings.
// The driver always polls the WIP bit; these just stop a dead part hanging the CPU.
// Values are the datasheet maxima rounded up (Table 19 - tSE max was raised to 0.7 s
// in datasheet revision G, so confirm against the revision you are building against).
//-------------------------------------------------------------------------------------
#define XM25_TIMEOUT_PAGE_PROG_MS		10UL
#define XM25_TIMEOUT_SECTOR_ERASE_MS	1000UL
#define XM25_TIMEOUT_BLOCK_ERASE_MS		4000UL
#define XM25_TIMEOUT_CHIP_ERASE_MS		200000UL

//-------------------------------------------------------------------------------------
// Log record size - kept identical to the AT45DB321D build so the data map is unchanged
//-------------------------------------------------------------------------------------
#ifndef LOG_SIZE
#define LOG_SIZE	50
#endif

//-------------------------------------------------------------------------------------
// Public API - deliberately identical to AT45DB321D.h so the part can be swapped by
// changing DATAFLASH_PART alone.
//-------------------------------------------------------------------------------------
void XM25_Init(void);

uint8_t XM25_GetStatus(void);
uint8_t XM25_Ready(void);
uint8_t XM25_ReadJedecID(uint8_t *manufacturer,uint8_t *memType,uint8_t *capacity);
uint8_t XM25_IsPresent(void);

void XM25_ChipErase(void);
void XM25_SectorErase(uint32_t Address);		// 4 KB,  address = any byte inside the sector
void XM25_BlockErase32K(uint32_t Address);
void XM25_BlockErase64K(uint32_t Address);

void XM25_PowerDown(void);
void XM25_ResumeFromPowerDown(void);

/// Program into already-erased space.  Splits across 256-byte page boundaries.
/// Does NOT erase - bits can only go 1 -> 0.
void XM25_Program(uint32_t Address,uint8_t *buffer,uint16_t bytes);

/// Program, erasing any 4 KB sector this write is the first to enter.
/// Correct for the append-only log rings, which advance monotonically.
void XM25_ProgramLogStyle(uint32_t Address,uint8_t *buffer,uint16_t bytes);

void WriteLog(uint32_t AddrOffset,uint32_t LogInd,uint8_t *buffer,uint16_t bytes);
void ReadLog(uint32_t LogInd,uint8_t *buffer,uint16_t bytes);
void ReadMinMaxLog(uint32_t AddrOffset,uint32_t LogInd,uint8_t *buffer,uint16_t bytes);

void WriteEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes);

//-------------------------------------------------------------------------------------
// Power-fail-safe log-pointer store (see the block comment in XM25QH128A.c)
//-------------------------------------------------------------------------------------
#define XM25_PTR_HDR_MAGIC		0xA5C3u	//header magic, stored in the LAST 2 header bytes
#define XM25_PTR_SLOT_TAG		0x5Au	//slot validity tag, stored in the LAST slot byte

#define XM25_PTR_LOG			0		//regular log write pointer
#define XM25_PTR_LOG24			1		//24-hour log write pointer

void		XM25_PtrSave(uint8_t which,uint32_t value);
uint32_t	XM25_PtrLoad(uint8_t which);
void		XM25_PtrReset(uint8_t which,uint32_t value);
void ReadEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes);

#endif /* XM25QH128A_H_ */
