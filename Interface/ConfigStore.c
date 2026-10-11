////////////////////////////////////////////////////////////////////////////////////////
/// @file    ConfigStore.c
/// @brief   The cold configuration block in the MCU's own flash.
///
/// The last 2 KB of the 64 KB part is two 1 KB pages used as an A/B pair.  A save
/// writes the whole block to whichever page is not currently live and only then is the
/// new copy preferred, so a power cut at any point leaves at least one complete copy
/// behind.  Each copy carries a sequence number and a CRC; the loader takes the valid
/// copy with the higher sequence.
///
/// Why the whole block at once rather than individual fields: internal flash erases a
/// page at a time, so there is no such thing as rewriting one byte.  The block is
/// smaller than a page, which is what makes the A/B scheme possible at all - and it is
/// smaller than a page only because the entries that change while the device runs were
/// deliberately left out of it.  See the HOT ENTRIES note in sb_const.h.
////////////////////////////////////////////////////////////////////////////////////////
//hal_flash.h FIRST, before anything pulls in sb_const.h.  That header defines the
//display segment codes as single capitals and lower-case letters - E, r, C, H, L,
//U, A, I, V, t - and the CMSIS register headers further down this chain use some of
//those as structure member names.  Whichever is included first wins.
#include "hal_flash.h"
#include <string.h>

#include "ConfigStore.h"

#if (CONFIG_STORE == CONFIG_STORE_INTERNAL)

//Header in front of the payload.  Programming is by half-word, so everything here is
//half-word sized and the payload starts half-word aligned.
#pragma pack(push,1)
typedef struct
{
    uint16_t magic;
    uint16_t seq;       //higher wins; wraps, compared as a signed difference
    uint16_t len;       //sizeof(CfgCold_t) when it was written
    uint16_t crc;       //over the payload only
} CfgHdr_t;
#pragma pack(pop)

#define CFG_MAGIC           0xC0F6u

//A page has to hold the header, the payload and nothing else.
#define CFG_IMAGE_SIZE      (sizeof(CfgHdr_t) + sizeof(CfgCold_t))

//Fail the build rather than silently overrun a page.  If this ever fires, the block
//has outgrown a page: userCalDate[5][100] is 500 of its bytes and is the first place
//to look.
typedef char cfg_fits_in_a_page[(CFG_IMAGE_SIZE <= CFG_FLASH_PAGE_SIZE) ? 1 : -1];

//Half-word programming cannot write an odd number of bytes.
typedef char cfg_is_even[((CFG_IMAGE_SIZE % 2) == 0) ? 1 : -1];

//Passes of the main loop with no further change before the block is committed.  A pass
//is about 85 ms, so this is a little under half a second - long enough to swallow a
//burst of parameter writes arriving over the UART, short enough that a setting is on
//its way to flash before anybody could reach the power switch.
#define CFG_SETTLE_PASSES   5

static CfgCold_t cfgMirror;
static uint8_t   cfgDirty = 0;
static uint8_t   cfgSettle = 0;
static uint16_t  cfgSeq = 0;
static uint32_t  cfgLivePage = 0;   //0 = nothing valid has been read yet

//-------------------------------------------------------------------------------------
//CRC-16/CCITT.  Only has to catch a torn write, not an adversary.
static uint16_t CfgCrc(const uint8_t *p, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t  b;

    while(len--)
    {
        crc ^= (uint16_t)(*p++) << 8;
        for(i = 0; i < 8; i++)
        {
            b = (crc & 0x8000u) ? 1 : 0;
            crc <<= 1;
            if(b) crc ^= 0x1021u;
        }
    }
    return crc;
}

//-------------------------------------------------------------------------------------
/// TRUE when the page holds a complete, self-consistent copy; *seq returns its sequence.
static uint8_t CfgPageValid(uint32_t page, uint16_t *seq)
{
    const CfgHdr_t *hdr = (const CfgHdr_t *)page;
    const uint8_t  *payload = (const uint8_t *)(page + sizeof(CfgHdr_t));

    if(hdr->magic != CFG_MAGIC)                 return 0;
    if(hdr->len   != sizeof(CfgCold_t))         return 0;   //written by another build
    if(hdr->crc   != CfgCrc(payload, hdr->len)) return 0;

    *seq = hdr->seq;
    return 1;
}

//-------------------------------------------------------------------------------------
void ConfigStore_Load(void)
{
    uint16_t seqA = 0, seqB = 0;
    uint8_t  okA, okB;
    uint32_t use = 0;

    okA = CfgPageValid(CFG_FLASH_PAGE_A, &seqA);
    okB = CfgPageValid(CFG_FLASH_PAGE_B, &seqB);

    if(okA && okB)
    {
        //Signed difference so the counter may wrap without upsetting the comparison.
        use    = (((int16_t)(seqA - seqB)) > 0) ? CFG_FLASH_PAGE_A : CFG_FLASH_PAGE_B;
        cfgSeq = (((int16_t)(seqA - seqB)) > 0) ? seqA : seqB;
    }
    else if(okA) { use = CFG_FLASH_PAGE_A; cfgSeq = seqA; }
    else if(okB) { use = CFG_FLASH_PAGE_B; cfgSeq = seqB; }

    if(use)
    {
        memcpy(&cfgMirror, (const void *)(use + sizeof(CfgHdr_t)), sizeof(cfgMirror));
        cfgLivePage = use;
    }
    else
    {
        //Nothing usable.  Fill with 0xFF, which is what an erased cell in the external
        //part read back, so boot_data()'s "is this blank?" tests - FirstTimeCheck, the
        //XBee MAC, every range check - behave exactly as they always have and the
        //first-boot initialisation runs.
        memset(&cfgMirror, 0xFF, sizeof(cfgMirror));
        cfgLivePage = 0;
    }

    cfgDirty  = 0;
    cfgSettle = 0;
}

//-------------------------------------------------------------------------------------
void ConfigStore_Read(uint32_t offset, uint8_t *buffer, uint16_t bytes)
{
    //Clamp rather than trust. A bad offset here would read off the end of the mirror
    //and hand the caller whatever follows it in RAM.
    if(offset >= sizeof(cfgMirror)) return;
    if((offset + bytes) > sizeof(cfgMirror)) bytes = (uint16_t)(sizeof(cfgMirror) - offset);

    memcpy(buffer, ((const uint8_t *)&cfgMirror) + offset, bytes);
}

//-------------------------------------------------------------------------------------
void ConfigStore_Write(uint32_t offset, const uint8_t *buffer, uint16_t bytes)
{
    uint8_t *dst;

    if(offset >= sizeof(cfgMirror)) return;
    if((offset + bytes) > sizeof(cfgMirror)) bytes = (uint16_t)(sizeof(cfgMirror) - offset);

    dst = ((uint8_t *)&cfgMirror) + offset;

    //Writing the value it already holds is not a change.  Worth the comparison: the
    //application rewrites settings it has merely re-read in several places, and each
    //one of those would otherwise cost a page erase.
    if(memcmp(dst, buffer, bytes) == 0) return;

    memcpy(dst, buffer, bytes);
    cfgDirty  = 1;
    cfgSettle = CFG_SETTLE_PASSES;
}

//-------------------------------------------------------------------------------------
uint8_t ConfigStore_Dirty(void) { return cfgDirty; }

//-------------------------------------------------------------------------------------
/// Write the mirror to the page that is NOT live, then adopt it.
static void CfgCommit(void)
{
    uint32_t target = (cfgLivePage == CFG_FLASH_PAGE_A) ? CFG_FLASH_PAGE_B
                                                        : CFG_FLASH_PAGE_A;
    CfgHdr_t hdr;

    hdr.magic = CFG_MAGIC;
    hdr.seq   = (uint16_t)(cfgSeq + 1);
    hdr.len   = (uint16_t)sizeof(CfgCold_t);
    hdr.crc   = CfgCrc((const uint8_t *)&cfgMirror, (uint16_t)sizeof(CfgCold_t));

    //Erase the spare and write the whole image into it.  The live page is untouched
    //throughout, so losing power here costs the new copy and keeps the old one.
    exFLASH_EraseEE(target);
    exFLASH_ProgramEE((uint16_t *)&hdr, target, (uint16_t)sizeof(hdr));
    exFLASH_ProgramEE((uint16_t *)&cfgMirror, target + sizeof(hdr),
                      (uint16_t)sizeof(CfgCold_t));

    //Only now does the new copy outrank the old one - the sequence number is already
    //in flash, so this is just bookkeeping to pick the right spare next time.
    if(CfgPageValid(target, &hdr.seq))
    {
        cfgSeq      = hdr.seq;
        cfgLivePage = target;
        cfgDirty    = 0;
    }
    //If it did not take, cfgDirty stays set and the next tick tries again. The old
    //copy is still live and still correct.
}

//-------------------------------------------------------------------------------------
void ConfigStore_Flush(void)
{
    if(cfgDirty) CfgCommit();
    cfgSettle = 0;
}

//-------------------------------------------------------------------------------------
void ConfigStore_Tick(void)
{
    if(!cfgDirty) return;

    if(cfgSettle)
    {
        cfgSettle--;
        return;
    }
    CfgCommit();
}

#endif  // CONFIG_STORE_INTERNAL

#if !ENABLE_EXT_DATAFLASH
//---------------------------------------------------------------------------------
// The data-flash API with no data flash.
//
// Configuration goes to the internal store exactly as it does when a part IS fitted.
// Anything else is an entry that only exists alongside the logs - the recorded
// extremes, the last alarm state - and there is nowhere to keep it, so a read reports
// erased and a write is dropped.  Reporting 0xFF rather than zero matters: it is what
// an unwritten cell always gave, so boot_data() applies its defaults instead of
// believing a stored zero.
//---------------------------------------------------------------------------------
void ReadEEPROMData(uint32_t Address, uint8_t *buffer, uint16_t bytes)
{
#if (CONFIG_STORE == CONFIG_STORE_INTERNAL)
	if(Address >= CFG_COLD_ADDR)
	{
		ConfigStore_Read(Address - CFG_COLD_ADDR, buffer, bytes);
		return;
	}
#endif
	memset(buffer, 0xFF, bytes);
}

void WriteEEPROMData(uint32_t Address, uint8_t *buffer, uint16_t bytes)
{
#if (CONFIG_STORE == CONFIG_STORE_INTERNAL)
	if(Address >= CFG_COLD_ADDR)
	{
		ConfigStore_Write(Address - CFG_COLD_ADDR, buffer, bytes);
		return;
	}
#endif
	(void)Address; (void)buffer; (void)bytes;
}
#endif  // !ENABLE_EXT_DATAFLASH

