/***********************************************************************************************\
 * File Name:	dataflash.h																		*
 *																								*
 * Selects the mounted data-flash part and pulls in its driver header.							*
 * Set DATAFLASH_PART in sb_const.h to match the part fitted on the board.						*
 *																								*
 * Both drivers expose the same API (WriteEEPROMData / ReadEEPROMData / WriteLog / ReadLog /		*
 * ReadMinMaxLog), so application code includes this header and never names a part directly.	*
\***********************************************************************************************/

#ifndef DATAFLASH_H_
#define DATAFLASH_H_

#include "..\sb_const.h"

#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)
	#include "XM25QH128A.h"
#else
	#include "AT45DB321D.h"
#endif

//-------------------------------------------------------------------------------------
// Data-map constraints that only apply to a NOR part.
//
// The AT45DB321D rewrites any byte in place, so its regions may start anywhere.  The
// XM25QH128A can only erase whole 4 KB sectors, so if two regions share a sector,
// erasing one to rewrite it destroys the other.  Every region base must therefore sit
// on a 4 KB boundary.
//
// These checks fail the build rather than let a mis-aligned map silently corrupt data
// (with the AT45 map as-is, REGULAR_LOG_ADDR = 2048 shares sector 0 with the whole
// config block - the first log write would erase every stored setting).
//-------------------------------------------------------------------------------------
#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)

	#if ((CONFIG_PARA_ADDR % XM25_SECTOR_SIZE) != 0)
		#error "XM25QH128A: CONFIG_PARA_ADDR must be 4 KB aligned"
	#endif

	#if ((RT_PARA_SECTOR_A % XM25_SECTOR_SIZE) != 0)
		#error "XM25QH128A: RT_PARA_SECTOR_A must be 4 KB aligned"
	#endif

	#if ((RT_SLOT_SIZE % 4) || (RT_PARA_SIZE >= RT_SLOT_SIZE) || (XM25_PAGE_SIZE % RT_SLOT_SIZE))
		#error "XM25QH128A: a real-time slot must hold the payload plus a tag and divide the page size"
	#endif

	#if ((REGULAR_LOG_ADDR % XM25_SECTOR_SIZE) != 0)
		#error "XM25QH128A: REGULAR_LOG_ADDR must be 4 KB aligned - it currently shares a sector with the config block"
	#endif

	#if ((LAST_LOG24_ADDR_OFFSET % XM25_SECTOR_SIZE) != 0)
		#error "XM25QH128A: LAST_LOG24_ADDR_OFFSET (= LAST_LOG_ADDR) must be 4 KB aligned"
	#endif

	#if ((MIN_MAX_LOG_ADDR_OFFSET % XM25_SECTOR_SIZE) != 0)
		#error "XM25QH128A: MIN_MAX_LOG_ADDR_OFFSET must be 4 KB aligned"
	#endif

	#if ((MEAN24_LOG_ADDR_OFFSET % XM25_SECTOR_SIZE) != 0)
		#error "XM25QH128A: MEAN24_LOG_ADDR_OFFSET must be 4 KB aligned"
	#endif

	#if (DATA_FLASH_END_OFFSET > XM25_FLASH_SIZE)
		#error "XM25QH128A: data map does not fit in the device"
	#endif

	#if (DATA_FLASH_END_OFFSET > XM25_SCRATCH_SECTOR)
		#error "XM25QH128A: data map overlaps the scratch sector used by WriteEEPROMData()"
	#endif

#endif	// DATAFLASH_XM25QH128A

//-------------------------------------------------------------------------------------
// Part-neutral wrappers for the few calls the application makes directly.
// Everything else (WriteEEPROMData / ReadEEPROMData / WriteLog / ReadLog /
// ReadMinMaxLog) already has the same name in both drivers.
//-------------------------------------------------------------------------------------
#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)

	#define DF_Init()				XM25_Init()
	#define DF_ConfigurePageSize()	((void)0)		//NOR has no configurable page size
	#define DF_ChipErase()			XM25_ChipErase()

	// Bulk erase walks the 4 KB sectors the data map actually uses - erasing all 4096
	// sectors of a 16 MByte part would be pointless and far slower.  Sector erase is
	// <= 0.7 s, which stays inside the watchdog window provided the caller reloads
	// between units; a 64 KB block erase would not.
	#define DF_ERASE_UNITS			((DATA_FLASH_END_OFFSET + XM25_SECTOR_SIZE - 1UL) / XM25_SECTOR_SIZE)
	#define DF_EraseUnit(i)			XM25_SectorErase((uint32_t)(i) * XM25_SECTOR_SIZE)

	#define DF_RtFlush()			XM25_RtFlush()
	#define DF_RtReset()			XM25_RtReset()

#else

	#define DF_Init()				AT45D_Init()
	#define DF_ConfigurePageSize()	AT45D_set_page_size_to_pwr_of_two()
	#define DF_ChipErase()			AT45D_ChipErase()

	#define DF_ERASE_UNITS			64UL
	#define DF_EraseUnit(i)			AT45D_SectorErase((uint8_t)(i))

	//The AT45 rewrites any byte in place, so the real-time fields stay in the config
	//block and there is nothing to batch or recover.
	#define DF_RtFlush()			((void)0)
	#define DF_RtReset()			((void)0)

#endif

//-------------------------------------------------------------------------------------
// RAM_BUF_SIZE is set in sb_const.h without reference to LOG_SIZE, because headers that
// pull in sb_const.h do not all see the driver header first.  This is where both are
// visible, so this is where they get checked.
//-------------------------------------------------------------------------------------
#if (BUILD_LOG24_LOG && !BUILD_RAM_BUFFER)
	#if (RAM_BUF_SIZE < (RAM_FILL_START + LOG_SIZE))
		#error "RAM_BUF_SIZE must hold one whole log record - update it to match LOG_SIZE"
	#endif
#endif

#endif	/* DATAFLASH_H_ */
