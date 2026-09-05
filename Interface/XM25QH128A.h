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
// NOT the last sector: sector 4095 is where the OTP sector is mapped, and erase
// commands are disabled there while the part is in OTP mode.  Sector 1024 is an
// ordinary sector, far above the data map (which tops out around 0x2F4690).
#define XM25_SCRATCH_SECTOR		0x00400000UL					// sector 1024

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
//An erase is only executed if CS# rises cleanly after the eighth bit of the last
//address byte.  A page program has 256 more bytes of margin after that point,
//which is why a marginal CS# edge can void every erase while programs still pass.
//These guard bands cost nothing and remove that class of fault.
#define XM25_CS_SETTLE_US				5UL
#define XM25_ERASE_RETRIES				3

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
//-------------------------------------------------------------------------------------
// Self-test result codes (XM25_SelfTest)
//-------------------------------------------------------------------------------------
#define XM25_TEST_PASS			0	//everything below passed
#define XM25_TEST_NO_DEVICE		1	//status reads 0xFF - nothing answering on MISO
#define XM25_TEST_BAD_ID		2	//JEDEC ID is not 20 70 18
#define XM25_TEST_PROTECTED		3	//block-protect bits will not clear (check WP# pin)
#define XM25_TEST_BUSY			4	//WIP never cleared within the timeout
#define XM25_TEST_ERASE_FAIL	5	//sector did not read back as 0xFF after erase
#define XM25_TEST_PROGRAM_FAIL	6	//data did not read back as written
#define XM25_TEST_WREN_FAIL		7	//WREN did not set the WEL bit - no write can work
#define XM25_TEST_ERASE_STUCK	8	//erase started but WIP never cleared
#define XM25_TEST_PROG1_FAIL	9	//a SINGLE byte would not program - no write reaches the array

//Erase failed - these three split code 5 into its actual causes
#define XM25_TEST_ERASE_REJECTED	10	//device never went busy: instruction refused
#define XM25_TEST_ERASE_RAN_FAIL	11	//erase cycle ran to completion but did not blank
#define XM25_TEST_ERASE_NEEDS_64K	12	//4 KB sector erase ignored, 64 KB block erase works
#define XM25_TEST_WEL_LOST			13	//WEL was not set at the moment the erase was issued
#define XM25_TEST_NO_ERASE_ENGINE	15	//NO erase opcode starts a cycle - not even Chip
										//Erase, which is a bare 1-byte frame like WREN
#define XM25_TEST_ERASE_NOT_DECODED	14	//WEL still set afterwards: the erase instruction
										//was never recognised (framing / clock count / CS#)

//Last values seen by the driver - handy in a debugger watch window
extern uint8_t xm25_lastId[3];
extern uint8_t xm25_lastStatus;
extern uint8_t xm25_selfTestResult;
extern uint8_t xm25_statusAfterWren;	//SR1 straight after a WREN - WEL (0x02) must be set
extern uint8_t xm25_statusAfterProg;	//SR1 straight after a page program

//Erase instrumentation, filled in by XM25_SelfTest()
extern uint8_t  xm25_sr2;			//status register 2 (0x09) - carries the suspend bit
extern uint8_t  xm25_sr3;			//status register 3 (0x95)

void XM25_Init(void);

/// Read SR1 and clear the block-protect bits if any are set.
/// Returns 1 if the device ends up unprotected.
uint8_t XM25_ClearProtection(void);

/// Read status register 2 (0x09) / 3 (0x95) into xm25_sr2 / xm25_sr3.
void XM25_ReadStatus23(void);

#if XM25_ENABLE_SELFTEST

/// End-to-end check: ID, status, unprotect, erase, program, verify.
/// Uses the scratch sector only, so it never touches the data map.
/// Returns XM25_TEST_PASS (0) or the code of the first failing step.
uint8_t XM25_SelfTest(void);

//Filled in by the self test - inspect these in a debugger watch window
extern uint8_t  xm25_welBeforeErase;
extern uint8_t  xm25_welAfterErase;
extern uint8_t  xm25_eraseWipSeen;
extern uint16_t xm25_eraseTimeMs;
extern uint8_t  xm25_blockEraseOk;
extern uint8_t  xm25_chipEraseWip;
extern uint8_t  xm25_eraseAttempts;

#endif	// XM25_ENABLE_SELFTEST

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

//-------------------------------------------------------------------------------------
// Real-time parameter store.  The fields listed under HOT PARAMETERS in sb_const.h are
// mirrored in RAM and committed as one record by XM25_RtFlush(); the application still
// reads and writes them through ReadEEPROMData()/WriteEEPROMData() as before.
//-------------------------------------------------------------------------------------
void XM25_RtLoad(void);		//recover the newest record  (called by XM25_Init)
void XM25_RtFlush(void);	//commit the mirror if it has changed
void XM25_RtReset(void);	//re-open the store after a bulk erase

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
