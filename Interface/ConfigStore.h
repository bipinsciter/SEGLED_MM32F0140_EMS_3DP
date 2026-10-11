////////////////////////////////////////////////////////////////////////////////////////
/// @file    ConfigStore.h
/// @brief   The cold configuration block, held in RAM and backed by flash.
///
/// CfgCold_t itself is declared in sb_const.h, next to the address macros that index
/// it, so that the structure and the names the call sites use cannot drift apart.
///
/// Call sites do not use this interface directly.  They go on calling
/// ReadEEPROMData()/WriteEEPROMData() with the same names as before; those functions
/// recognise the CFG_COLD_ADDR base and come here instead of going to the bus.
////////////////////////////////////////////////////////////////////////////////////////
#ifndef CONFIG_STORE_H_
#define CONFIG_STORE_H_

#include "../sb_const.h"

#if (CONFIG_STORE == CONFIG_STORE_INTERNAL)

/// Read the saved block into the RAM mirror.  Call once, early in start-up, BEFORE
/// anything reads a setting.  If neither page holds a good copy - a virgin part, or a
/// save interrupted before either page was complete - the mirror is left filled with
/// 0xFF, which is what an erased external part used to read back, so every existing
/// "is this cell blank?" check behaves exactly as it did.
void ConfigStore_Load(void);

/// Copy bytes out of the mirror.  `offset` is relative to the start of CfgCold_t.
void ConfigStore_Read(uint32_t offset, uint8_t *buffer, uint16_t bytes);

/// Copy bytes into the mirror and mark it dirty.  Nothing reaches flash here: the
/// write is committed by ConfigStore_Tick() once the changes stop coming.
void ConfigStore_Write(uint32_t offset, const uint8_t *buffer, uint16_t bytes);

/// Run the settle timer and commit when it expires.  Call once per pass of the main
/// loop.  Coalescing matters: a parameter sweep from the PC application writes several
/// dozen settings a few tens of milliseconds apart, and committing each one separately
/// would spend a page erase on every single parameter.
void ConfigStore_Tick(void);

/// Commit immediately if there is anything outstanding.  Call before a deliberate
/// restart, where waiting for the settle timer would lose the change.
void ConfigStore_Flush(void);

/// TRUE while a change is waiting to be committed.
uint8_t ConfigStore_Dirty(void);

#endif  // CONFIG_STORE_INTERNAL

#if !ENABLE_EXT_DATAFLASH
//---------------------------------------------------------------------------------
// Stand-ins for the data-flash API when no part is fitted.
//
// The application calls these in several hundred places and most of those calls are
// about configuration, which is served from the internal store.  The rest refer to
// entries that only exist alongside the logs; with no part to hold them a read
// reports erased (0xFF) so every existing "is this blank?" check applies its default,
// and a write is discarded.
//---------------------------------------------------------------------------------
void WriteEEPROMData(uint32_t Address, uint8_t *buffer, uint16_t bytes);
void ReadEEPROMData(uint32_t Address, uint8_t *buffer, uint16_t bytes);
#endif

#endif  // CONFIG_STORE_H_
