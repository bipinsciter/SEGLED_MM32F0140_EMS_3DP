/***********************************************************************************************\
 * File Name:	XM25QH128A.c																	*
 *																								*
 * XMC XM25QH128A - 128 Mbit (16 MByte) standard SPI NOR flash driver.							*
 * Presents the same API as AT45DB321D.c so the two parts are interchangeable via				*
 * DATAFLASH_PART in sb_const.h.																*
 *																								*
 * Datasheet: XM25QH128A preliminary Rev. H, 2018/08/06											*
\***********************************************************************************************/

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

	return data;
}

/// TRUE when no program/erase cycle is in progress.
uint8_t XM25_Ready(void)
{
	return ((XM25_GetStatus() & XM25_STATUS_WIP) ? false : true);
}

/// Poll WIP until the device is idle.  timeoutMs stops a dead part hanging the CPU;
/// the watchdog is still serviced by the caller's normal 1-second tick.
static void XM25_WaitReady(uint32_t timeoutMs)
{
	while(timeoutMs)
	{
		if(XM25_Ready()) return;

		PLATFORM_DelayMS(1);
		timeoutMs--;
	}
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

	//A software reset also clears any half-finished state left by a warm restart
	XM25_Command(XM25_CMD_RESET_ENABLE);
	XM25_Command(XM25_CMD_RESET);
	PLATFORM_DelayMS(1);

	XM25_WaitReady(XM25_TIMEOUT_SECTOR_ERASE_MS);
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
// Read - a Read Data (0x03) burst crosses page and sector boundaries in hardware,
// so any length can be fetched in one transaction.
//=====================================================================================

void ReadEEPROMData(uint32_t Address,uint8_t *buffer,uint16_t bytes)
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

		ReadEEPROMData(Address,xm25_pageBuf,chunk);

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
		ReadEEPROMData(srcSector + offset,xm25_pageBuf,XM25_PAGE_SIZE);

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

	if(!bytes) return;

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

	ReadEEPROMData(sector,buf,PTR_HDR_SIZE);

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
static uint16_t XM25_PtrScanBank(XM25_PtrStore *ps,uint8_t bank,uint32_t *value)
{
	uint16_t slot,used=0;
	uint8_t buf[4];

	*value = 0;

	for(slot=0; slot<PTR_SLOTS_PER_BANK; slot++)
	{
		buf[3]=0;
		ReadEEPROMData(XM25_PtrSlotAddr(ps,bank,slot),buf,PTR_SLOT_SIZE);

		//erased (0xFF) or torn (tag not yet committed) - nothing usable beyond here
		if(buf[3] != XM25_PTR_SLOT_TAG)	break;

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
#endif	// (DATAFLASH_PART == DATAFLASH_XM25QH128A)
