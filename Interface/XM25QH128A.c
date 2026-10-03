/***********************************************************************************************\
 * File Name:	XM25QH128A.c																	*
 *																								*
 * XMC XM25QH128A - 128 Mbit (16 MByte) standard SPI NOR flash driver.							*
 * Presents the same API as AT45DB321D.c so the two parts are interchangeable via				*
 * DATAFLASH_PART in sb_const.h.																*
 *																								*
 * Datasheet: XM25QH128A preliminary Rev. H, 2018/08/06											*
\***********************************************************************************************/

#include <string.h>
#include "XM25QH128A.h"
#include "..\platform.h"
#include "hal_conf.h"
#include "..\gpio.h"

#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)

//-------------------------------------------------------------------------------------
// One 256-byte staging buffer, shared by the sector read-modify-write path.
// A whole 4 KB sector will NOT fit in RAM on this MCU (8 KB total), which is why
// WriteEEPROMData() stages through a scratch sector instead of a RAM image.
//-------------------------------------------------------------------------------------
static uint8_t xm25_pageBuf[XM25_PAGE_SIZE];

//Diagnostics - non-static so they can be inspected in a debugger watch window
uint8_t xm25_lastId[3]      = {0};
uint8_t xm25_lastStatus     = 0;
uint8_t xm25_selfTestResult = XM25_TEST_NO_DEVICE;
uint8_t xm25_statusAfterWren = 0;
uint8_t xm25_statusAfterProg = 0;
#if XM25_ENABLE_SELFTEST
uint8_t  xm25_eraseWipSeen  = 0;
uint16_t xm25_eraseTimeMs   = 0;
uint8_t  xm25_blockEraseOk  = 0;
uint8_t  xm25_welBeforeErase = 0;
uint8_t  xm25_welAfterErase  = 0;
uint8_t  xm25_chipEraseWip   = 0;
#endif
uint8_t  xm25_sr2 = 0;
uint8_t  xm25_sr3 = 0;
#if XM25_ENABLE_SELFTEST
uint8_t  xm25_eraseAttempts = 0;
#endif

//=====================================================================================
// Low level helpers
//=====================================================================================

/// Send a bare one-byte command.
static void XM25_Command(uint8_t cmd)
{
	SPI_FLASH_CS_L();
	SPI_TxData_Polling(&cmd,1);
	SPI_FLASH_CS_H();
}

/// Build the 4-byte {opcode, A23-A16, A15-A8, A7-A0} header.
static void XM25_FillCmdAddr(uint8_t *hdr,uint8_t cmd,uint32_t Address)
{
	hdr[0] = cmd;
	hdr[1] = (uint8_t)(Address >> 16);
	hdr[2] = (uint8_t)(Address >> 8);
	hdr[3] = (uint8_t)(Address);
}

uint8_t XM25_GetStatus(void)
{
	uint8_t data=0,cmd=XM25_CMD_READ_STATUS1;

	SPI_FLASH_CS_L();
	SPI_TxData_Polling(&cmd,1);
	SPI_RxData_Polling(&data,1);
	SPI_FLASH_CS_H();

	xm25_lastStatus = data;

	return data;
}

//Status registers 2 and 3.  SR2 carries the write/erase SUSPEND bit: while a
//suspend is latched the device IGNORES erase instructions aimed at the suspended
//sector while still accepting programs, which is exactly the symptom of code 10.
void XM25_ReadStatus23(void)
{
	uint8_t cmd,data;

	cmd = XM25_CMD_READ_STATUS2;
	SPI_FLASH_CS_L();
	SPI_TxData_Polling(&cmd,1);
	SPI_RxData_Polling(&data,1);
	SPI_FLASH_CS_H();
	xm25_sr2 = data;

	cmd = XM25_CMD_READ_STATUS3;
	SPI_FLASH_CS_L();
	SPI_TxData_Polling(&cmd,1);
	SPI_RxData_Polling(&data,1);
	SPI_FLASH_CS_H();
	xm25_sr3 = data;
}

/// TRUE when no program/erase cycle is in progress.
uint8_t XM25_Ready(void)
{
	return ((XM25_GetStatus() & XM25_STATUS_WIP) ? false : true);
}

/// Poll WIP until the device is idle.  timeoutMs stops a dead part hanging the CPU;
/// the watchdog is still serviced by the caller's normal 1-second tick.
//Returns 1 when the device went idle, 0 on timeout or if nothing is answering.
//
//The 0xFF check matters: with no device (or MISO stuck high) the status register
//reads 0xFF, whose WIP bit is set, so the old code read that as "busy" and burned
//the FULL timeout on every single call - 1 s before each read.  Dozens of reads in
//boot_data() then look like a hung board rather than a missing flash.
static uint8_t XM25_WaitReady(uint32_t timeoutMs)
{
	uint8_t dead=0;

	while(timeoutMs)
	{
		if(XM25_Ready())				return 1;

		//Require several consecutive all-ones reads before giving up: one glitched
		//status read must not abort a legitimate 0.7 s erase wait.
		if(xm25_lastStatus == 0xFF)
		{
			if(++dead >= 3)				return 0;	//nothing on the bus - do not stall
		}
		else dead = 0;

		PLATFORM_DelayMS(1);
		timeoutMs--;
	}

	return 0;
}

/// WREN - must precede every program, erase and status-register write.
static void XM25_WriteEnable(void)
{
	XM25_Command(XM25_CMD_WRITE_ENABLE);
}

//=====================================================================================
// Identification / power
//=====================================================================================

uint8_t XM25_ReadJedecID(uint8_t *manufacturer,uint8_t *memType,uint8_t *capacity)
{
	uint8_t cmd=XM25_CMD_JEDEC_ID,id[3]={0};

	XM25_WaitReady(XM25_TIMEOUT_PAGE_PROG_MS);

	SPI_FLASH_CS_L();
	SPI_TxData_Polling(&cmd,1);
	SPI_RxData_Polling(&id[0],3);
	SPI_FLASH_CS_H();

	xm25_lastId[0] = id[0];
	xm25_lastId[1] = id[1];
	xm25_lastId[2] = id[2];

	if(manufacturer)	*manufacturer = id[0];
	if(memType)			*memType      = id[1];
	if(capacity)		*capacity     = id[2];

	return ((id[0]==XM25_JEDEC_MANUFACTURER) &&
			(id[1]==XM25_JEDEC_MEMORY_TYPE)  &&
			(id[2]==XM25_JEDEC_CAPACITY)) ? true : false;
}

/// TRUE when a XM25QH128A actually answers on the bus.
uint8_t XM25_IsPresent(void)
{
	return XM25_ReadJedecID(0,0,0);
}

void XM25_Init(void)
{
	FRESET_DIR_OUT;
	FRESET_HIGH;
	PLATFORM_DelayMS(1);
	FRESET_LOW;
	PLATFORM_DelayMS(5);
	FRESET_HIGH;

	//tHRSL - 28 us from RESET# high to the next instruction; 1 ms is comfortable
	PLATFORM_DelayMS(1);

	//Wake the part in case it was left in deep power-down - in that state it ignores
	//every instruction except this one.
	XM25_Command(XM25_CMD_RELEASE_POWER_DOWN);
	PLATFORM_DelayMS(1);

	//A software reset also clears any half-finished state left by a warm restart
	XM25_Command(XM25_CMD_RESET_ENABLE);
	XM25_Command(XM25_CMD_RESET);
	PLATFORM_DelayMS(1);

	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);

	//Clear any latent write/erase suspend.  A suspended erase makes the device ignore
	//further erase instructions while still accepting programs.
	XM25_Command(XM25_CMD_WRITE_RESUME);
	PLATFORM_DelayMS(1);
	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);

	XM25_ReadStatus23();

	//Record whether the part actually answers, and make sure program/erase are allowed.
	xm25_selfTestResult = XM25_IsPresent() ? XM25_TEST_PASS : XM25_TEST_BAD_ID;

	XM25_ClearProtection();

	//Must come after protection is cleared - it opens the store on a blank part
	XM25_RtLoad();
}

//=====================================================================================
// Block protection
//
// SR1 bits BP3..BP0 / EBL / SRP decide whether program and erase are allowed.  When
// any are set the device ACCEPTS the commands and SILENTLY DOES NOTHING - reads keep
// returning the old contents, which looks exactly like "the flash is not working".
// Parts can ship protected, and a stray WRSR leaves them that way, so clear them at
// init.  If they refuse to clear, WP# is almost certainly held low on the board.
//=====================================================================================
uint8_t XM25_ClearProtection(void)
{
	uint8_t sr,buf[2];

	sr = XM25_GetStatus();

	if(sr == 0xFF)	return false;		//nothing answering

	if(!(sr & (XM25_STATUS_BP0|XM25_STATUS_BP1|XM25_STATUS_BP2|
			   XM25_STATUS_BP3|XM25_STATUS_EBL|XM25_STATUS_SRP)))
	{
		return true;					//already unprotected
	}

	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);
	XM25_WriteEnable();

	buf[0] = XM25_CMD_WRITE_STATUS1;
	buf[1] = 0x00;

	SPI_FLASH_CS_L();
	SPI_TxData_Polling(buf,2);
	SPI_FLASH_CS_H();

	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);

	sr = XM25_GetStatus();

	return (sr & (XM25_STATUS_BP0|XM25_STATUS_BP1|XM25_STATUS_BP2|
				  XM25_STATUS_BP3|XM25_STATUS_EBL|XM25_STATUS_SRP)) ? false : true;
}

void XM25_PowerDown(void)
{
	XM25_Command(XM25_CMD_POWER_DOWN);
}

void XM25_ResumeFromPowerDown(void)
{
	XM25_Command(XM25_CMD_RELEASE_POWER_DOWN);
	PLATFORM_DelayMS(1);
}

//=====================================================================================
// Erase
//=====================================================================================

static void XM25_EraseAt(uint8_t cmd,uint32_t Address,uint32_t timeoutMs)
{
	uint8_t hdr[4];

	XM25_WaitReady(timeoutMs);
	XM25_WriteEnable();

	XM25_FillCmdAddr(hdr,cmd,Address);
	SPI_FLASH_CS_L();
	SPI_TxData_Polling(hdr,4);
	SPI_FLASH_CS_H();

	XM25_WaitReady(timeoutMs);
}

/// Erase the 4 KB sector containing Address.
void XM25_SectorErase(uint32_t Address)
{
	XM25_EraseAt(XM25_CMD_SECTOR_ERASE_4K,Address,XM25_TIMEOUT_SECTOR_ERASE_MS);
}

void XM25_BlockErase32K(uint32_t Address)
{
	XM25_EraseAt(XM25_CMD_BLOCK_ERASE_32K,Address,XM25_TIMEOUT_BLOCK_ERASE_MS);
}

void XM25_BlockErase64K(uint32_t Address)
{
	XM25_EraseAt(XM25_CMD_BLOCK_ERASE_64K,Address,XM25_TIMEOUT_BLOCK_ERASE_MS);
}

void XM25_ChipErase(void)
{
	uint8_t cmd=XM25_CMD_CHIP_ERASE;

	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);
	XM25_WriteEnable();

	SPI_FLASH_CS_L();
	SPI_TxData_Polling(&cmd,1);
	SPI_FLASH_CS_H();

	XM25_WaitReady(XM25_TIMEOUT_CHIP_ERASE_MS);
}

//=====================================================================================
// Real-time parameter store
//
// Min/max, alarm state, the log-overflow flag and the log read pointers change while
// the device runs; everything else in the configuration block changes only when a
// user presses a key or sends a UART command.  Writing the fast-moving fields into
// the configuration sector meant a full 4 KB read-modify-write for each one, which
// on a fresh device (min and max start out collapsed onto the first reading, so
// nearly every sample is a new extreme) is enough to stall the whole application.
//
// So they live here instead: mirrored in RAM, addressed by the application as if
// they were still plain EEPROM bytes, and appended to flash as ONE whole record on
// a timer.  A minute of extremes therefore costs one 64-byte page program rather
// than a hundred sector rewrites, and the user's settings sit in a sector that a
// min/max update never touches.  The cost is that up to one flush interval of
// min/max history is lost on an unexpected power cut.
//
// On-flash layout follows the log-pointer store above: two sectors used as a
// ping-pong pair, a generation + magic header in slot 0 of each bank, and a
// trailing tag byte on every record.  Page Program commits bytes in ascending
// address order, so that tag is the last byte written - a record interrupted
// part-way through reads back as incomplete and is ignored.
//=====================================================================================

#define XM25_RT_HDR_MAGIC		0x524Eu		//trailing bytes of a bank header
#define XM25_RT_SLOT_TAG		0xA5		//trailing byte of a complete record

static uint8_t  xm25_rtShadow[RT_PARA_SIZE];
static uint8_t  xm25_rtDirty = 0;
static uint8_t  xm25_rtBank  = 0;		//active bank, 0 = A, 1 = B
static uint16_t xm25_rtSlot  = 1;		//next free slot; slot 0 always holds the header
static uint16_t xm25_rtGen   = 0;

static void XM25_ReadRaw(uint32_t Address,uint8_t *buffer,uint16_t bytes);

static uint32_t XM25_RtBankAddr(uint8_t bank)
{
	return bank ? RT_PARA_SECTOR_B : RT_PARA_SECTOR_A;
}

static uint32_t XM25_RtSlotAddr(uint8_t bank,uint16_t slot)
{
	return XM25_RtBankAddr(bank) + ((uint32_t)slot * RT_SLOT_SIZE);
}

/// TRUE when the bank carries a fully written header; *gen returns its generation.
static uint8_t XM25_RtReadHdr(uint8_t bank,uint16_t *gen)
{
	uint8_t buf[4]={0};

	XM25_ReadRaw(XM25_RtBankAddr(bank),buf,4);

	if((((uint16_t)buf[3]<<8) | (uint16_t)buf[2]) != XM25_RT_HDR_MAGIC)	return false;

	*gen = (((uint16_t)buf[1]<<8) | (uint16_t)buf[0]);
	return true;
}

static void XM25_RtWriteHdr(uint8_t bank,uint16_t gen)
{
	uint8_t buf[4];

	buf[0]=(uint8_t)gen;
	buf[1]=(uint8_t)(gen>>8);
	buf[2]=(uint8_t)XM25_RT_HDR_MAGIC;
	buf[3]=(uint8_t)(XM25_RT_HDR_MAGIC>>8);	//written last - marks the bank live

	XM25_Program(XM25_RtBankAddr(bank),buf,4);
}

/// Scan a bank for its last complete record.  Returns the index of that record
/// (0 = none) and copies its payload into the shadow when one is found.
static uint16_t XM25_RtScanBank(uint8_t bank,uint8_t *found)
{
	uint8_t buf[RT_SLOT_SIZE];
	uint16_t slot,used=0;

	*found = 0;

	for(slot=1; slot<=RT_SLOTS_PER_BANK; slot++)
	{
		XM25_ReadRaw(XM25_RtSlotAddr(bank,slot),buf,RT_SLOT_SIZE);

		//the first incomplete slot ends the run - records are always appended
		if(buf[RT_SLOT_SIZE-1] != XM25_RT_SLOT_TAG)	break;

		memcpy(xm25_rtShadow,buf,RT_PARA_SIZE);
		*found = 1;
		used   = slot;
	}

	return used;
}

/// Recover the newest record at boot.  Called from XM25_Init(), before the
/// application reads any of these fields.
void XM25_RtLoad(void)
{
	uint16_t genA=0,genB=0,used;
	uint8_t hasA,hasB,found;

	//A blank store must read as a blank EEPROM would - boot_data() decides what to
	//seed from FIRST_BOOT_CHECK, not from these bytes.
	memset(xm25_rtShadow,0xFF,RT_PARA_SIZE);
	xm25_rtDirty = 0;

	hasA = XM25_RtReadHdr(0,&genA);
	hasB = XM25_RtReadHdr(1,&genB);

	if(!hasA && !hasB)
	{
		//Nothing recognisable in either bank: a blank part, or a device coming from
		//firmware that used these two sectors for something else.  Erase before
		//opening - Program can only clear bits, so a header written over stale data
		//would never read back as valid and the store could never come up.
		XM25_SectorErase(RT_PARA_SECTOR_A);
		XM25_SectorErase(RT_PARA_SECTOR_B);

		//Open bank A at generation 1
		xm25_rtBank = 0;
		xm25_rtGen  = 1;
		xm25_rtSlot = 1;
		XM25_RtWriteHdr(0,1);
		return;
	}

	//Signed difference so the choice stays right when the generation wraps
	if(hasA && hasB)	xm25_rtBank = ((int16_t)(genB-genA) > 0) ? 1 : 0;
	else				xm25_rtBank = hasB ? 1 : 0;

	xm25_rtGen  = xm25_rtBank ? genB : genA;

	used        = XM25_RtScanBank(xm25_rtBank,&found);
	xm25_rtSlot = used + 1;

	if(!found && hasA && hasB)
	{
		//The newest bank has a header but no complete record - power was lost during
		//a bank swap.  The older bank still holds the last good one.
		XM25_RtScanBank(xm25_rtBank ? 0 : 1,&found);
	}
}

/// Append the shadow as a new record, swapping banks first if the active one is full.
static void XM25_RtAppend(void)
{
	uint8_t buf[RT_SLOT_SIZE];
	uint8_t spare;

	if(xm25_rtSlot > RT_SLOTS_PER_BANK)
	{
		//Erase the OLDER bank and open it at the next generation.  The active bank
		//keeps its full set of records until the new one has been written, so a
		//power loss at any point here still leaves one complete record on the part.
		spare = xm25_rtBank ? 0 : 1;

		XM25_SectorErase(XM25_RtBankAddr(spare));
		XM25_RtWriteHdr(spare,(uint16_t)(xm25_rtGen+1));

		xm25_rtBank = spare;
		xm25_rtGen++;
		xm25_rtSlot = 1;
	}

	memset(buf,0xFF,RT_SLOT_SIZE);
	memcpy(buf,xm25_rtShadow,RT_PARA_SIZE);
	buf[RT_SLOT_SIZE-1] = XM25_RT_SLOT_TAG;		//written last - marks the record complete

	//RT_SLOT_SIZE divides the 256-byte page and every slot is slot-aligned, so this
	//never straddles a page boundary.
	XM25_Program(XM25_RtSlotAddr(xm25_rtBank,xm25_rtSlot),buf,RT_SLOT_SIZE);
	xm25_rtSlot++;
}

/// Commit the shadow if anything has changed since the last commit.  Call this on a
/// timer - see RT_FLUSH_INTERVAL_SEC in sb_const.h for the wear/loss trade-off.
void XM25_RtFlush(void)
{
	if(!xm25_rtDirty)	return;

	XM25_RtAppend();
	xm25_rtDirty = 0;
}

/// Re-open the store after a bulk erase has blanked both banks.
void XM25_RtReset(void)
{
	XM25_SectorErase(RT_PARA_SECTOR_A);
	XM25_SectorErase(RT_PARA_SECTOR_B);

	xm25_rtBank = 0;
	xm25_rtGen  = 1;
	xm25_rtSlot = 1;
	XM25_RtWriteHdr(0,1);

	//Whatever the application currently holds is the truth - commit it immediately
	xm25_rtDirty = 1;
	XM25_RtFlush();
}

/// TRUE when the address belongs to the real-time mirror.  *bytes is clipped to the
/// end of the region: an address past it is a map error, and letting it through to
/// the bus would program a truncated 24-bit address somewhere inside the data map.
static uint8_t XM25_RtRange(uint32_t Address,uint16_t *offset,uint16_t *bytes)
{
	uint32_t off;

	if(Address < RT_PARA_ADDR)	return false;

	off = Address - RT_PARA_ADDR;

	if(off >= RT_PARA_SIZE)
	{
		*offset = 0;
		*bytes  = 0;
		return true;
	}

	*offset = (uint16_t)off;

	if((off + *bytes) > RT_PARA_SIZE)	*bytes = (uint16_t)(RT_PARA_SIZE - off);

	return true;
}

//=====================================================================================
// Read - a Read Data (0x03) burst crosses page and sector boundaries in hardware,
// so any length can be fetched in one transaction.
//=====================================================================================

/// Straight bus read.  Used by everything inside this driver, so that the internal
/// machinery can never be caught by the real-time redirection below.
static void XM25_ReadRaw(uint32_t Address,uint8_t *buffer,uint16_t bytes)
{
	uint8_t hdr[4];

	if(!bytes) return;

	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);

	XM25_FillCmdAddr(hdr,XM25_CMD_READ_DATA,Address);

	SPI_FLASH_CS_L();
	SPI_TxData_Polling(hdr,4);
	SPI_RxData_Polling(buffer,bytes);
	SPI_FLASH_CS_H();
}

void ReadEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes)
{
	uint16_t offset;

	if(!bytes) return;

	if(XM25_RtRange(Address,&offset,&bytes))
	{
		if(bytes)	memcpy(buffer,&xm25_rtShadow[offset],bytes);
		return;
	}

	XM25_ReadRaw(Address,buffer,bytes);
}

void ReadLog(uint32_t LogInd,uint8_t *buffer,uint16_t bytes)
{
	ReadEEPROMData(REGULAR_LOG_ADDR + (LogInd * LOG_SIZE),buffer,bytes);
}

void ReadMinMaxLog(uint32_t AddrOffset,uint32_t LogInd,uint8_t *buffer,uint16_t bytes)
{
	ReadEEPROMData(AddrOffset + (LogInd * bytes),buffer,bytes);
}

//=====================================================================================
// Program
//=====================================================================================

/// Program into already-erased space.  A Page Program must not cross a 256-byte page
/// boundary - the device would wrap back to the start of the same page - so the
/// transfer is split on page boundaries here.
void XM25_Program(uint32_t Address,uint8_t *buffer,uint16_t bytes)
{
	uint8_t hdr[4];
	uint16_t chunk;

	while(bytes)
	{
		//bytes left in the current 256-byte page
		chunk = (uint16_t)(XM25_PAGE_SIZE - (Address & XM25_PAGE_MASK));
		if(chunk > bytes) chunk = bytes;

		XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);
		XM25_WriteEnable();

		XM25_FillCmdAddr(hdr,XM25_CMD_PAGE_PROGRAM,Address);

		SPI_FLASH_CS_L();
		SPI_TxData_Polling(hdr,4);
		SPI_TxData_Polling(buffer,chunk);
		SPI_FLASH_CS_H();

		XM25_WaitReady(XM25_TIMEOUT_PAGE_PROG_MS);

		Address += chunk;
		buffer  += chunk;
		bytes   -= chunk;
	}
}

/// Program for the append-only log rings.
///
/// The rings advance monotonically, so a sector only needs erasing when a write is the
/// first to step into it - i.e. when the write starts at or before that sector's base.
/// This relies on each ring STARTING on a 4 KB boundary; if a ring starts mid-sector its
/// first record shares a sector with whatever precedes it and that sector is never
/// erased for it.  See the alignment note in sb_const.h.
void XM25_ProgramLogStyle(uint32_t Address,uint8_t *buffer,uint16_t bytes)
{
	uint32_t sector,lastSector;

	if(!bytes) return;

	sector     = XM25_SECTOR_BASE(Address);
	lastSector = XM25_SECTOR_BASE(Address + bytes - 1);

	for(; sector<=lastSector; sector+=XM25_SECTOR_SIZE)
	{
		//"first write to enter this sector" - erase it before programming
		if(Address <= sector)
		{
			XM25_SectorErase(sector);
		}
	}

	XM25_Program(Address,buffer,bytes);
}

//WriteLog() serves two DIFFERENT access patterns in this application, and on NOR they
//need different handling:
//
//  append-only rings  - REGULAR_LOG_ADDR and LAST_LOG24_ADDR_OFFSET.  The index only ever
//                       advances, so a sector can be erased as the ring steps into it and
//                       every record then lands in erased space.  Fast.
//
//  rewritten slots    - the min/max and 24-hour-mean regions.  Their index cycles (0..14,
//                       0..23) and overwrites earlier slots, so they need the same
//                       read-modify-write treatment as WriteEEPROMData().
//
//Dispatching on the base address keeps both correct without changing the call sites.
void WriteLog(uint32_t AddrOffset,uint32_t LogInd,uint8_t *buffer,uint16_t bytes)
{
	uint32_t Address = AddrOffset + (LogInd * bytes);

	if((AddrOffset == REGULAR_LOG_ADDR) || (AddrOffset == LAST_LOG24_ADDR_OFFSET))
	{
		XM25_ProgramLogStyle(Address,buffer,bytes);
	}
	else
	{
		WriteEEPROMData(Address,buffer,bytes);
	}
}

//=====================================================================================
// WriteEEPROMData - the AT45DB321D compatibility shim
//=====================================================================================
//
// The AT45DB321D lets the application rewrite any byte at any time.  On NOR that is only
// possible by erasing the byte's whole 4 KB sector first, which destroys the other 4095
// bytes, so they have to be preserved somewhere across the erase.
//
// A 4 KB RAM image does not fit (this MCU has 8 KB of RAM and the application already
// uses ~4.8 KB), so the sector is staged through a dedicated scratch sector instead:
//
//     1. erase the scratch sector
//     2. copy target sector -> scratch, substituting the new bytes on the way
//        (scratch now holds the FINAL content)
//     3. erase the target sector
//     4. copy scratch -> target
//
// Staging the final content in step 2 means a power loss between steps 3 and 4 leaves a
// complete good copy in the scratch sector, which a recovery routine can restore.
//
// COST, and why this is not a free drop-in:
//   - a fast path avoids all of it when the affected bytes are still erased (0xFF),
//     which covers a freshly erased map
//   - otherwise ONE config byte costs 2 sector erases + 32 page programs, roughly 1.4 s
//     worst case, and consumes 2 erase cycles of the target sector's ~100k endurance
//
// That is fine for genuinely occasional settings changes.  It is NOT fine for the
// per-log-record pointer updates this application currently performs - see the note in
// the handover; that write pattern needs a wear-levelled storage layer before this part
// can ship.
//=====================================================================================

/// TRUE when every byte in the range is still 0xFF, so it can be programmed directly.
static uint8_t XM25_RangeIsErased(uint32_t Address,uint16_t bytes)
{
	uint16_t chunk,i;

	while(bytes)
	{
		chunk = (bytes > XM25_PAGE_SIZE) ? XM25_PAGE_SIZE : bytes;

		XM25_ReadRaw(Address,xm25_pageBuf,chunk);

		for(i=0; i<chunk; i++)
		{
			if(xm25_pageBuf[i] != 0xFF) return false;
		}

		Address += chunk;
		bytes   -= chunk;
	}

	return true;
}

/// Copy one 4 KB sector, optionally substituting a run of new bytes on the way.
/// subLen == 0 copies the sector verbatim.
static void XM25_CopySector(uint32_t srcSector,uint32_t dstSector,
							uint32_t subAddr,uint8_t *subData,uint16_t subLen)
{
	uint32_t offset;
	uint16_t i;
	uint32_t abs;

	for(offset=0; offset<XM25_SECTOR_SIZE; offset+=XM25_PAGE_SIZE)
	{
		XM25_ReadRaw(srcSector + offset,xm25_pageBuf,XM25_PAGE_SIZE);

		//overlay the caller's new bytes wherever they fall inside this page
		for(i=0; i<XM25_PAGE_SIZE; i++)
		{
			abs = srcSector + offset + i;

			if(subLen && (abs >= subAddr) && (abs < (subAddr + subLen)))
			{
				xm25_pageBuf[i] = subData[abs - subAddr];
			}
		}

		XM25_Program(dstSector + offset,xm25_pageBuf,XM25_PAGE_SIZE);
	}
}

void WriteEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes)
{
	uint32_t sector;
	uint32_t sectorEnd;
	uint16_t part;
	uint16_t offset;

	if(!bytes) return;

	if(XM25_RtRange(Address,&offset,&bytes))
	{
		//Real-time field - update the mirror and let the flush timer commit it.
		//Comparing first keeps a rewrite of the same value from costing a flush.
		if(bytes && memcmp(&xm25_rtShadow[offset],buffer,bytes))
		{
			memcpy(&xm25_rtShadow[offset],buffer,bytes);
			xm25_rtDirty = 1;
		}
		return;
	}

	//Split the request so each pass touches exactly one 4 KB sector
	while(bytes)
	{
		sector    = XM25_SECTOR_BASE(Address);
		sectorEnd = sector + XM25_SECTOR_SIZE;

		part = (uint16_t)((Address + bytes > sectorEnd) ? (sectorEnd - Address) : bytes);

		if(XM25_RangeIsErased(Address,part))
		{
			//Fast path - nothing to preserve, program straight in
			XM25_Program(Address,buffer,part);
		}
		else
		{
			//Slow path - stage the whole sector through the scratch sector
			XM25_SectorErase(XM25_SCRATCH_SECTOR);
			XM25_CopySector(sector,XM25_SCRATCH_SECTOR,Address,buffer,part);

			XM25_SectorErase(sector);
			XM25_CopySector(XM25_SCRATCH_SECTOR,sector,0,0,0);
		}

		Address += part;
		buffer  += part;
		bytes   -= part;
	}
}

//=====================================================================================
// Power-fail-safe log-pointer store
//
// Each pointer owns TWO sectors used as a ping-pong pair.  Layout of a bank:
//
//     offset 0   header : generation (2 B) then magic (2 B)
//     offset 4.. slots  : 24-bit value (3 B) then validity tag (1 B), 4 bytes each
//
// Saving appends to the next free slot of the active bank, so it always lands in
// erased space and costs one page program - no erase.
//
// TWO things make this survive a power loss:
//
//  1. Swap order.  When the active bank fills:
//        a. write the header into the spare bank (already erased)
//        b. carry the current pointer into the spare's first slot
//        c. ONLY THEN erase the old bank
//     A loss during (a) or (b) leaves the old bank complete; a loss during (c) leaves
//     the new bank complete.  At no instant is there no valid pointer.
//
//  2. Trailing validity markers.  A Page Program shifts bytes in ascending address
//     order, so the LAST byte of a record is the last one committed.  Both the header
//     magic and the slot tag live in those trailing bytes, so a record interrupted
//     part-way through still reads as invalid and is ignored - it can never be
//     mistaken for a plausible pointer.  (An erase only drives bits to 1, so a
//     half-erased bank cannot match the magic either.)
//
// Values are stored in 24 bits, which covers TOTAL_REGULAR_LOG (60000) and
// LAST_LOG24_ADDR (1440) with room to spare.
//=====================================================================================

typedef struct
{
	uint32_t sectorA;
	uint32_t sectorB;
	uint8_t  bank;			//active bank, 0 = A, 1 = B
	uint16_t slot;			//next free slot in the active bank
	uint16_t gen;			//generation of the active bank
}XM25_PtrStore;

static XM25_PtrStore xm25_ptr[2]=
{
	{CURR_LOG_IND_SECTOR_A,   CURR_LOG_IND_SECTOR_B,   0, 0, 0},
	{CURR_LOG24_IND_SECTOR_A, CURR_LOG24_IND_SECTOR_B, 0, 0, 0}
};

static uint32_t XM25_PtrBankAddr(XM25_PtrStore *ps,uint8_t bank)
{
	return bank ? ps->sectorB : ps->sectorA;
}

static uint32_t XM25_PtrSlotAddr(XM25_PtrStore *ps,uint8_t bank,uint16_t slot)
{
	return XM25_PtrBankAddr(ps,bank) + PTR_HDR_SIZE + ((uint32_t)slot * PTR_SLOT_SIZE);
}

/// TRUE when the bank carries a fully written header; *gen returns its generation.
static uint8_t XM25_PtrReadHdr(uint32_t sector,uint16_t *gen)
{
	uint8_t buf[4]={0};

	XM25_ReadRaw(sector,buf,PTR_HDR_SIZE);

	//magic occupies the trailing bytes, so a torn write fails here
	if((((uint16_t)buf[3]<<8) | (uint16_t)buf[2]) != XM25_PTR_HDR_MAGIC)	return false;

	*gen = (((uint16_t)buf[1]<<8) | (uint16_t)buf[0]);
	return true;
}

static void XM25_PtrWriteHdr(uint32_t sector,uint16_t gen)
{
	uint8_t buf[4];

	buf[0]=(uint8_t)gen;
	buf[1]=(uint8_t)(gen>>8);
	buf[2]=(uint8_t)XM25_PTR_HDR_MAGIC;
	buf[3]=(uint8_t)(XM25_PTR_HDR_MAGIC>>8);

	XM25_Program(sector,buf,PTR_HDR_SIZE);
}

static void XM25_PtrWriteSlot(XM25_PtrStore *ps,uint8_t bank,uint16_t slot,uint32_t value)
{
	uint8_t buf[4];

	buf[0]=(uint8_t)value;
	buf[1]=(uint8_t)(value>>8);
	buf[2]=(uint8_t)(value>>16);
	buf[3]=XM25_PTR_SLOT_TAG;		//written last - marks the slot complete

	XM25_Program(XM25_PtrSlotAddr(ps,bank,slot),buf,PTR_SLOT_SIZE);
}

/// Scan a bank for the last complete slot.  Returns how many slots are used and
/// leaves the newest value in *value.
///
/// The WHOLE bank is read and the LAST tagged slot wins.  Stopping at the first
/// untagged slot - which is what this used to do - turned a single torn or dropped
/// slot program into a permanent wall.  The running device never noticed, because its
/// index lives in RAM and goes on advancing, but the next restart read the pointer
/// from BEFORE the gap and the log resumed there, overwriting everything written
/// since.  Measured on hardware (2 Oct 2026): a write index of 167 came back as 3
/// across a restart, and the 164 records in between were overwritten.  The gap can
/// form days before the restart that exposes it, which is what makes the loss so hard
/// to attribute to anything.
///
/// Erased slots read 0xFF, which is not the tag, so the unused tail of the bank is
/// skipped as before.  The cost is PTR_SLOTS_PER_BANK short reads once, at boot.
static uint16_t XM25_PtrScanBank(XM25_PtrStore *ps,uint8_t bank,uint32_t *value)
{
	uint16_t slot,used=0;
	uint8_t buf[4];

	*value = 0;

	for(slot=0; slot<PTR_SLOTS_PER_BANK; slot++)
	{
		buf[3]=0;
		XM25_ReadRaw(XM25_PtrSlotAddr(ps,bank,slot),buf,PTR_SLOT_SIZE);

		//erased (0xFF) or torn (tag not committed) - step over it and keep looking,
		//because a later slot may still carry a newer pointer
		if(buf[3] != XM25_PTR_SLOT_TAG)	continue;

		*value = (uint32_t)buf[0] | ((uint32_t)buf[1]<<8) | ((uint32_t)buf[2]<<16);
		used   = slot + 1;
	}

	return used;
}

void XM25_PtrSave(uint8_t which,uint32_t value)
{
	XM25_PtrStore *ps;
	uint8_t spare;
	uint16_t gen;

	if(which > XM25_PTR_LOG24)	return;
	ps = &xm25_ptr[which];

	if(ps->slot >= PTR_SLOTS_PER_BANK)
	{
		//Active bank is full - ping-pong onto the spare, which is already erased
		spare = ps->bank ^ 1;
		gen   = ps->gen + 1;

		XM25_PtrWriteHdr(XM25_PtrBankAddr(ps,spare),gen);	//a. claim the spare
		XM25_PtrWriteSlot(ps,spare,0,value);				//b. carry the pointer across

		XM25_SectorErase(XM25_PtrBankAddr(ps,ps->bank));	//c. release the old bank

		ps->bank = spare;
		ps->gen  = gen;
		ps->slot = 1;
		return;
	}

	XM25_PtrWriteSlot(ps,ps->bank,ps->slot,value);
	ps->slot++;

	//Read it back.  A slot that did not take would otherwise sit in the bank as a gap,
	//and gaps are what cost the log its records before the scan above was taught to
	//step over them.  Belt and braces: this stops the gap forming, the scan survives one
	//that forms anyway.  Four bytes of a 4 KB bank, once a log record.
	{
		uint8_t chk[4]={0};

		XM25_ReadRaw(XM25_PtrSlotAddr(ps,ps->bank,ps->slot-1),chk,PTR_SLOT_SIZE);

		if((chk[3] != XM25_PTR_SLOT_TAG)
		|| (chk[0] != (uint8_t)value)
		|| (chk[1] != (uint8_t)(value>>8))
		|| (chk[2] != (uint8_t)(value>>16)))
		{
			//Abandon the bad slot and put the pointer in the next one.  Not retried past
			//that: the caller is the once-a-minute log write and will be back.
			if(ps->slot < PTR_SLOTS_PER_BANK)
			{
				XM25_PtrWriteSlot(ps,ps->bank,ps->slot,value);
				ps->slot++;
			}
		}
	}
}

uint32_t XM25_PtrLoad(uint8_t which)
{
	XM25_PtrStore *ps;
	uint16_t genA=0,genB=0,usedA=0,usedB=0;
	uint32_t valA=0,valB=0;
	uint8_t okA,okB,active;

	if(which > XM25_PTR_LOG24)	return 0;
	ps = &xm25_ptr[which];

	okA = XM25_PtrReadHdr(ps->sectorA,&genA);
	okB = XM25_PtrReadHdr(ps->sectorB,&genB);

	if(!okA && !okB)
	{
		//Virgin (or wiped) store - open bank A
		XM25_SectorErase(ps->sectorA);
		XM25_SectorErase(ps->sectorB);
		XM25_PtrWriteHdr(ps->sectorA,1);

		ps->bank = 0;
		ps->gen  = 1;
		ps->slot = 0;
		return 0;
	}

	if(okA)	usedA = XM25_PtrScanBank(ps,0,&valA);
	if(okB)	usedB = XM25_PtrScanBank(ps,1,&valB);

	//Newer generation wins (signed difference so the 16-bit counter may wrap)
	if(okA && okB)	active = (((int16_t)(genA - genB)) > 0) ? 0 : 1;
	else			active = okA ? 0 : 1;

	//A bank claimed but lost power before its first pointer landed has nothing to
	//offer - fall back to the other one, which is still complete.
	if((active==0) && !usedA && okB && usedB)	active = 1;
	if((active==1) && !usedB && okA && usedA)	active = 0;

	ps->bank = active;
	ps->gen  = active ? genB : genA;
	ps->slot = active ? usedB : usedA;

	//Finish an interrupted swap so a ready-erased spare always exists
	if(okA && okB)	XM25_SectorErase(XM25_PtrBankAddr(ps,active^1));

	return active ? valB : valA;
}

void XM25_PtrReset(uint8_t which,uint32_t value)
{
	XM25_PtrStore *ps;

	if(which > XM25_PTR_LOG24)	return;
	ps = &xm25_ptr[which];

	XM25_SectorErase(ps->sectorA);
	XM25_SectorErase(ps->sectorB);
	XM25_PtrWriteHdr(ps->sectorA,1);

	ps->bank = 0;
	ps->gen  = 1;
	ps->slot = 0;

	XM25_PtrSave(which,value);
}
//=====================================================================================
// Self test
//
// Works only inside XM25_SCRATCH_SECTOR, so it can be run at any time without
// disturbing the config or the logs.  Returns the first failing step.
//=====================================================================================
#if XM25_ENABLE_SELFTEST

//Issue an erase and measure what the device actually does with it.
//  xm25_eraseWipSeen == 0  ->  the device never went busy, i.e. it REJECTED the
//                             instruction outright (protection / CS timing / opcode)
//  xm25_eraseWipSeen == 1  ->  the erase cycle really ran; if the data is still not
//                             0xFF afterwards the cycle aborted (supply droop) or the
//                             part is faulty
static void XM25_TimedErase(uint8_t cmd,uint32_t Address)
{
	uint8_t hdr[4],sr;

	xm25_eraseWipSeen = 0;
	xm25_eraseTimeMs  = 0;

	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);
	XM25_WriteEnable();

	//WEL must be set right here or the erase cannot possibly execute.  Step 5 proved
	//WREN works in isolation; this proves it is still latched in the erase path.
	xm25_welBeforeErase = XM25_GetStatus();

	XM25_ReadStatus23();

	//Those reads sit between the WREN and the opcode, so re-arm WREN here: the erase
	//then gets a clean WREN -> opcode sequence with nothing in between, and the
	//measurement above cannot be blamed for the result.
	XM25_WriteEnable();

	XM25_FillCmdAddr(hdr,cmd,Address);

	SPI_FLASH_CS_L();
	PLATFORM_DelayUS(XM25_CS_SETTLE_US);		//CS# low well before the first clock
	SPI_TxData_Polling(hdr,4);
	PLATFORM_DelayUS(XM25_CS_SETTLE_US);		//last address bit fully latched
	SPI_FLASH_CS_H();
	PLATFORM_DelayUS(XM25_CS_SETTLE_US);		//tSHSL before polling starts

	//The decisive reading.  A device that DECODED the erase clears WEL, whether it
	//then runs the cycle or declines it.  A device that never recognised a valid
	//erase instruction at all leaves WEL exactly as WREN set it.
	xm25_welAfterErase = XM25_GetStatus();

	while(xm25_eraseTimeMs < XM25_TIMEOUT_BLOCK_ERASE_MS)
	{
		sr = XM25_GetStatus();

		if(sr == 0xFF)				break;			//bus gone - nothing to measure

		if(sr & XM25_STATUS_WIP)	xm25_eraseWipSeen = 1;
		else						break;			//idle: either finished or never started

		PLATFORM_DelayMS(1);
		xm25_eraseTimeMs++;
	}
}

//TRUE when the first 256 bytes of the sector are all erased
static uint8_t XM25_ScratchIsBlank(void)
{
	uint16_t i;

	ReadEEPROMData(XM25_SCRATCH_SECTOR,xm25_pageBuf,XM25_PAGE_SIZE);

	for(i=0; i<XM25_PAGE_SIZE; i++)
	{
		if(xm25_pageBuf[i] != 0xFF)	return false;
	}

	return true;
}

//Last-resort probe: does ANY erase opcode start a cycle?
//
//Chip Erase carries no address and no data - it is a bare one-byte frame, exactly
//the same shape as WREN, which provably works on this board.  So if WIP starts
//here the erase engine is alive and the problem is address related; if it does not,
//the die is refusing erase operations as a class and nothing in software can help.
//
//NOTE: if this DOES start, it erases the entire device.  That is acceptable only
//because it is reached solely after every targeted erase has already failed.
static void XM25_ProbeChipErase(void)
{
	uint8_t cmd=XM25_CMD_CHIP_ERASE,sr;
	uint16_t ms;

	xm25_chipEraseWip = 0;

	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);
	XM25_WriteEnable();

	SPI_FLASH_CS_L();
	PLATFORM_DelayUS(XM25_CS_SETTLE_US);
	SPI_TxData_Polling(&cmd,1);
	PLATFORM_DelayUS(XM25_CS_SETTLE_US);
	SPI_FLASH_CS_H();
	PLATFORM_DelayUS(XM25_CS_SETTLE_US);

	//Only look for the cycle STARTING - a real chip erase runs for up to 200 s and
	//we have no reason to sit and wait for it here.
	for(ms=0; ms<50; ms++)
	{
		sr = XM25_GetStatus();

		if(sr == 0xFF)				break;
		if(sr & XM25_STATUS_WIP)	{ xm25_chipEraseWip = 1; break; }

		PLATFORM_DelayMS(1);
	}
}

//Erase the scratch sector and, if it will not blank, work out WHY.
//Returns 0 on success; on failure sets xm25_selfTestResult to one of
//XM25_TEST_ERASE_REJECTED / _RAN_FAIL / _NEEDS_64K and returns non-zero.
//
//Both erase points in the self test go through here, so an erase failure always
//reports its cause no matter which step trips first.  That matters because once a
//run has programmed the sector and failed to erase it, the NEXT run finds the
//sector already dirty and fails at the earlier step.
static uint8_t XM25_TryEraseScratch(void)
{
	uint8_t  sectorWip,i;
	uint16_t sectorMs;

	xm25_blockEraseOk   = 0;
	xm25_eraseAttempts  = 0;
	sectorWip = 0;
	sectorMs  = 0;

	//Retry a few times.  If the fault is a marginal CS# edge rather than a flat
	//refusal, some attempts will start a cycle and xm25_eraseAttempts will show it.
	for(i=0; i<XM25_ERASE_RETRIES; i++)
	{
		XM25_TimedErase(XM25_CMD_SECTOR_ERASE_4K,XM25_SCRATCH_SECTOR);

		if(xm25_eraseWipSeen)	sectorWip = 1;		//a cycle started on some attempt
		if(xm25_eraseTimeMs)	sectorMs  = xm25_eraseTimeMs;

		if(XM25_ScratchIsBlank())
		{
			xm25_eraseAttempts = (uint8_t)(i + 1);
			return 0;
		}
	}

	//WREN did not survive to the erase - nothing else below can be trusted
	if(!(xm25_welBeforeErase & XM25_STATUS_WEL))
	{
		xm25_selfTestResult = XM25_TEST_WEL_LOST;
		return 1;
	}

	//4 KB sector erase did not blank it.  Does a 64 KB block erase over the same
	//area work?  If so the part is not honouring 4 KB granularity.
	XM25_TimedErase(XM25_CMD_BLOCK_ERASE_64K,XM25_SCRATCH_SECTOR);

	if(XM25_ScratchIsBlank())
	{
		xm25_blockEraseOk   = 1;
		xm25_selfTestResult = XM25_TEST_ERASE_NEEDS_64K;
	}
	else if(!sectorWip)
	{
		//Neither the 4 KB nor the 64 KB erase started a cycle.  Try the one erase
		//opcode that carries no address at all before blaming the driver.
		XM25_ProbeChipErase();

		if(!xm25_chipEraseWip)
		{
			//Not even a bare one-byte erase frame starts a cycle, on a bus that
			//demonstrably carries WREN, reads and page programs correctly.
			xm25_selfTestResult = XM25_TEST_NO_ERASE_ENGINE;
		}
		else if(xm25_welAfterErase & XM25_STATUS_WEL)
		{
			//erase engine is alive, but the addressed erase was never decoded
			xm25_selfTestResult = XM25_TEST_ERASE_NOT_DECODED;
		}
		else
		{
			//decoded and declined by the die itself
			xm25_selfTestResult = XM25_TEST_ERASE_REJECTED;
		}
	}
	else
	{
		//the cycle really ran and still did not blank - not a software fault
		xm25_selfTestResult = XM25_TEST_ERASE_RAN_FAIL;
	}

	xm25_eraseWipSeen = sectorWip;
	xm25_eraseTimeMs  = sectorMs;

	return 1;
}

//=====================================================================================
// Self test
//
// Works only inside XM25_SCRATCH_SECTOR, so it can be run at any time without
// disturbing the config or the logs.  Returns the first failing step.
//=====================================================================================
uint8_t XM25_SelfTest(void)
{
	uint16_t i;
	uint8_t  sr;

	//1. is anything there at all?
	sr = XM25_GetStatus();
	if(sr == 0xFF)							{ xm25_selfTestResult=XM25_TEST_NO_DEVICE;   return xm25_selfTestResult; }

	//2. correct part?
	if(!XM25_IsPresent())					{ xm25_selfTestResult=XM25_TEST_BAD_ID;      return xm25_selfTestResult; }

	//3. idle?
	if(!XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS))
											{ xm25_selfTestResult=XM25_TEST_BUSY;        return xm25_selfTestResult; }

	//4. program/erase actually permitted?
	if(!XM25_ClearProtection())				{ xm25_selfTestResult=XM25_TEST_PROTECTED;   return xm25_selfTestResult; }

	//5. does WREN actually latch?  ClearProtection() above returns early when the
	//   BP bits are already clear, so until here NO write has been proven to work.
	XM25_WriteEnable();
	xm25_statusAfterWren = XM25_GetStatus();

	if(!(xm25_statusAfterWren & XM25_STATUS_WEL))
											{ xm25_selfTestResult=XM25_TEST_WREN_FAIL;   return xm25_selfTestResult; }

	//6. Put the sector into a known blank state.  If a previous run programmed it and
	//   could not erase it, this is where we find out - and TryEraseScratch() reports
	//   the actual cause rather than a bare "not blank".
	if(XM25_TryEraseScratch())				return xm25_selfTestResult;

	//7. Can a SINGLE byte be programmed?  This separates "no write ever reaches the
	//   array" from "the long data phase is corrupted".
	xm25_pageBuf[0] = 0x00;
	XM25_Program(XM25_SCRATCH_SECTOR,xm25_pageBuf,1);

	xm25_statusAfterProg = XM25_GetStatus();

	xm25_pageBuf[0] = 0xAA;
	ReadEEPROMData(XM25_SCRATCH_SECTOR,xm25_pageBuf,1);

	if(xm25_pageBuf[0] != 0x00)				{ xm25_selfTestResult=XM25_TEST_PROG1_FAIL;  return xm25_selfTestResult; }

	//8. Full 256-byte page.  Start from a fresh erase so the pattern can be written.
	if(XM25_TryEraseScratch())				return xm25_selfTestResult;

	for(i=0; i<XM25_PAGE_SIZE; i++)	xm25_pageBuf[i] = (uint8_t)(i ^ 0x5A);

	XM25_Program(XM25_SCRATCH_SECTOR,xm25_pageBuf,XM25_PAGE_SIZE);

	for(i=0; i<XM25_PAGE_SIZE; i++)	xm25_pageBuf[i] = 0;

	ReadEEPROMData(XM25_SCRATCH_SECTOR,xm25_pageBuf,XM25_PAGE_SIZE);

	for(i=0; i<XM25_PAGE_SIZE; i++)
	{
		if(xm25_pageBuf[i] != (uint8_t)(i ^ 0x5A))
											{ xm25_selfTestResult=XM25_TEST_PROGRAM_FAIL; return xm25_selfTestResult; }
	}

	//9. THE REAL ERASE TEST - the page above is now full of pattern, so if the sector
	//   comes back blank the erase genuinely worked.
	if(XM25_TryEraseScratch())				return xm25_selfTestResult;

	xm25_selfTestResult = XM25_TEST_PASS;
	return xm25_selfTestResult;
}

#endif	// XM25_ENABLE_SELFTEST

#endif	// (DATAFLASH_PART == DATAFLASH_XM25QH128A)
