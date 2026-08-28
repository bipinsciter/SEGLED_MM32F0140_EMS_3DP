# Bug Report — SEGLCD_MM32F0140_EMS_3DP

Reviewed: `main.c` (11,748 lines), `sb_const.h`/`sb_global_var.h`/`sb_variables.h`, and all driver files under the project root and `Interface/` (AT45DB321D, PCF8563, DS1307, SHT25, XGZP6891D, i2c*master, SPI, TM1680, gpio, platform, uart_interrupt, tim1_timebase, wwdg/iwdg watchdogs, mm32f0140_it).

Every finding below was independently confirmed by reading the actual source and cross-checking against the relevant header/const definitions — not just reported by a single pass. Two additional candidate issues raised during review were checked and **ruled out** (see bottom of document) so you don't need to chase them separately.

Line numbers refer to the current `main.c` (post `sizeof()` cleanup).

---

## Critical — silent EEPROM data corruption

### 1. `XBEE_MAC_ADDR` region is 26 bytes too small — overwrites DP limit/offset/LCD-control config
`sb_const.h`:
```c
#define XBEE_MAC_ADDR   (DEVICES_IN_GROUP_ADDR+1)
#define DP_LIMIT_ADDR   (XBEE_MAC_ADDR+6)      // only 6 bytes reserved
```
`NO_OF_XBEE_MAC` is 2 and `XBEE_MAC_SIZE` is 16, so the two MAC entries need 32 bytes, not 6. Both `boot_data()` code paths write/read the full 32 bytes:
```c
// main.c:10546, 10550 (first boot) and 11255, 11260 (normal boot)
WriteEEPROMData(XBEE_MAC_ADDR,(uint8_t*)&gu8arr_XbeeMac[0][0],XBEE_MAC_SIZE);
for(uint8_t i=1;i<NO_OF_XBEE_MAC;i++)
    WriteEEPROMData((XBEE_MAC_ADDR+(i*XBEE_MAC_SIZE)),(uint8_t*)&gu8arr_XbeeMac[i][0],XBEE_MAC_SIZE);
```
**Effect:** every boot (and any XBee-MAC-set command) overwrites `DP_LIMIT_ADDR`, `DP_OFFSET_ADDR`, and `LCD_CONTROL_ADDR` with MAC-address bytes. Fix: give `XBEE_MAC_ADDR` a 32-byte gap (`#define DP_LIMIT_ADDR (XBEE_MAC_ADDR+32)`).

### 2. `CURR_LOG24_IND` round-robin index reserves 2 bytes but needs ~200
```c
#define CURR_LOG24_IND      (CURR_LOG24_IND_RDLC+1)
#define PARA_SCROLL_TIME    (CURR_LOG24_IND+2)   // only 2 bytes reserved
```
`CurrentLog24IndReadLoc` cycles 0–99 (`main.c:4926`), and the write is:
```c
// main.c:4932 and 11304/11308
WriteEEPROMData((CURR_LOG24_IND+(CurrentLog24IndReadLoc*2)), &CurrentLog24Ind, sizeof(CurrentLog24Ind));
```
so the real offset used ranges 0–198. Compare with the sibling `CURR_LOG_IND`, which correctly reserves the full 400 bytes for its own `*4` indexing (`sb_const.h:231`). **Effect:** `PARA_SCROLL_TIME`, `RTC_SET_FLAG_ADDR`, and all five `*_CAL_DATE_ADDR`/`*_CAL_CERT_ADDR` fields get clobbered as soon as the 24-hour log index advances past 0, which happens routinely. Fix: reserve 200 bytes after `CURR_LOG24_IND`.

### 3. DP3 factory-calibration reset uses the wrong EEPROM sub-address (off by one)
```c
// main.c:6735
WriteEEPROMData((DP_SW_FACT_ADDR+3),(uint8_t*)&su16_dp_sw_factor[DP3],sizeof(su16_dp_sw_factor[DP3]));
```
`su16_dp_sw_factor[]` is `int16_t` (2 bytes/element); every other place computing this offset uses `index*2` (DP3 index = 2, so it should be `DP_SW_FACT_ADDR+4` — see the very next line, `DP_OFFSET_ADDR+4`, and the generic handler at `main.c:6446`). **Effect:** sending the DP3 factory-calibration-reset command corrupts the high byte of DP2's stored sw-factor and only half-writes DP3's own slot.

### 4. RH calibration-date-index correction writes to DP3's address instead of RH's own
```c
// main.c:11168 (inside boot_data, normal-boot path)
ReadEEPROMData(RH_USER_CAL_DATE_IND_ADDR,&RH_UserCalDateInd,sizeof(RH_UserCalDateInd));
if(RH_UserCalDateInd>15)
{
    RH_UserCalDateInd=0;
    WriteEEPROMData(DP3_USER_CAL_DATE_IND_ADDR,&RH_UserCalDateInd,sizeof(RH_UserCalDateInd));  // wrong address
}
```
**Effect:** RH's own corrupted value is never actually fixed in EEPROM (only the RAM copy is), and DP3's calibration-date-index slot is silently zeroed whenever RH's happens to be out of range.

---

## Major — wrong data / broken functionality

### 5. `get_epoch_time()` returns garbage epoch values (signed/unsigned mismatch)
```c
// main.c:88
unsigned long ydhms_diff (unsigned long int year1, unsigned long int yday1, unsigned int hour1,
    unsigned int min1, unsigned int sec1, unsigned int year0, unsigned int yday0,
    unsigned int hour0, unsigned int min0, unsigned int sec0)
{
    int b4 = SHR (year0, 2) + SHR (TM_YEAR_BASE, 2) - ! (year0 & 3);
    ...
}
// main.c:126
return(ydhms_diff (year1, yday1, t1.hour, t1.minute, t1.second, EPOCH_YEAR - TM_YEAR_BASE, 0, 0, 0, 0));
```
`EPOCH_YEAR - TM_YEAR_BASE` is `1970 - 2000 = -30`, but `year0` is declared `unsigned int`, so it's stored as `0xFFFFFFE2`. `SHR(year0,2)` (`sb_const.h:553`) expands to a plain `>>`, which on an *unsigned* value is a logical shift, not the arithmetic shift the original (glibc-style) algorithm relies on — it yields ~1.07 billion instead of `-8`. This corrupts `intervening_leap_days`, so `get_epoch_time()` returns wildly wrong values for every call. `Check_RTC()` calls this every second (`main.c:219`) and the result drives day-rollover detection and log timestamps. **Fix:** `year0`/`yday0`/`hour0`/`min0`/`sec0` need to be signed types, matching the original algorithm this was ported from.

### 6. RTC read pulls the wrong bytes for month/year (weekday and month get used instead)
```c
// main.c:186, 194, 200-201
Read_PCF8563(RTC_TIMEMIN_REG,&RTC_data[1],5);   // reads registers 3,4,5,6,7
...
rtc.month = BCD2HEX(RTC_data[4]);   // register 6 = weekday, NOT month
rtc.year  = BCD2HEX(RTC_data[5]);   // register 7 = month, NOT year
```
Per the PCF8563 register map in `Interface/PCF8563.h` (min=3, hour=4, day=5, weekday=6, month=7, year=8), a 5-byte sequential read starting at register 3 fills minute/hour/day/**weekday**/**month** — the actual year register (8) is never read. **Effect:** every RTC tick, the displayed/logged "month" is actually the day-of-week number and the "year" is actually the month number; the real year is never retrieved. This also feeds directly into bug #5's already-broken epoch calculation. **Fix:** read 6 bytes (through the year register) and use `RTC_data[5]`/`RTC_data[6]` for month/year, skipping the weekday byte, and enlarge `RTC_data` (currently `uint8_t RTC_data[6]` in `sb_variables.h:14`).

### 7. `ReadLog()` is missing the base-address offset that `WriteLog()` applies
```c
// Interface/AT45DB321D.c:234-238
void ReadLog(uint32_t LogInd,uint8_t *buffer,uint16_t bytes)
{
    uint32_t Address = LogInd * LOG_SIZE;   // no base offset added
    ...
}
```
vs.
```c
// Interface/AT45DB321D.c: WriteLog(uint32_t AddrOffset, uint32_t LogInd, ...)
Address = LogInd * bytes;
Address += AddrOffset;
```
Regular logs are written with `WriteLog(REGULAR_LOG_ADDR, CurrentLogInd, ...)` (`main.c:5126`, `REGULAR_LOG_ADDR` = 2048), but every `ReadLog()` call (`main.c:8005, 8006, 8150, 8295, 8305, 8324, 9763`, etc.) only passes the index — never adding the 2048-byte base. Since `LOG_SIZE` is 50, 2048 isn't even a whole number of records (40.96), so reads land at a byte offset misaligned with the actual record boundaries. **Effect:** historical log retrieval (RDLG_DT_ID/RDLG_CNT_ID commands, `FindLogIndex()` binary search, boot-time `StartEpoch`/`EndEpoch` lookups) all read wrong/misaligned data.

### 8. DP1 offset and DP2 sw-factor calibration are never restored from EEPROM on normal boot
In `boot_data()`'s per-DP restore blocks: DP1's block (`main.c:~10725-10738`) only restores `su16_dp_sw_factor[DP1]`, never `su16_dp_offset[DP1]`. DP2's block (`main.c:~10749-10843`) only restores `su16_dp_offset[DP2]`, never `su16_dp_sw_factor[DP2]`. Each block ends up restoring only one of the two calibration values it needs. **Effect:** DP1's offset calibration and DP2's switch-factor calibration silently reset to 0 on every reboot.

### 9. DP1/DP2 pressure limit is never loaded at boot — readings clamp to 0
Only the DP3 block reads `DP_LIMIT_ADDR` (`main.c:10942`, into `u16_dp_limit[DP3]`/`f32_dp_limit[DP3]`); there's no equivalent for DP1 or DP2 anywhere in `boot_data()`. `f32_dp_limit[]` defaults to 0, and `ReadDiffPressure()` (`main.c:8615-8624`) clamps `Dpressure[SensNo]` to `±f32_dp_limit[SensNo]`. **Effect:** after every reset, any nonzero DP1/DP2 reading is immediately clamped to 0 until a runtime "set limit" command re-populates it.

### 10. DP1 sw-factor sanity check can never trigger (`&&` should be `||`)
```c
// main.c:10733
if((su16_dp_sw_factor[DP1]<-10000) && (su16_dp_sw_factor[DP1]>10000))
```
A value can't be both `< -10000` and `> 10000` at once, so this is always false (compare with the correct `||` used for the analogous `su16_dp_offset[DP2]` check a few blocks later). **Effect:** a corrupted/out-of-range EEPROM value for DP1's sw-factor is silently accepted and used in pressure calculations instead of being reset to a safe default.

### 11. `DP_OFFSET_ID` query returns the wrong value (shares a case body with `DP_SW_FACT_ID`)
```c
// main.c:7431-7439
case DP_OFFSET_ID:
case DP_SW_FACT_ID:
    index=RxBuffer[4]-'0';
    if(index<MAX_SUPPORTED_DP)
        tempshort = su16_dp_sw_factor[index];   // DP_OFFSET_ID never reads su16_dp_offset[index]
break;
```
The write path treats offset and sw-factor as distinct, separately-stored values. **Effect:** reading back `DP_OFFSET_ID` over the RS-485 protocol always returns the sw-factor value instead.

### 12. RAM_ALL_ID response never reports DP2/DP3 sensor faults (wrong buffer)
```c
// main.c:7664-7666
if(bool_DP_NC[DP1])  RAMBuffer[3] |= DP1_FAULTY;   // correct buffer
#if (DEVICE_MODE==DP1_DP2_DP3_MODE)
if(bool_DP_NC[DP2])  TxBuffer[3] |= DP2_FAULTY;    // wrong buffer — should be RAMBuffer
if(bool_DP_NC[DP3])  TxBuffer[3] |= DP3_FAULTY;    // wrong buffer — should be RAMBuffer
```
Only `RAMBuffer` is actually transmitted for this command, so DP2/DP3 fault bits never reach the host in a full RAM dump response.

### 13. `ACK_PW_ID` command: index 0 underflows to an out-of-bounds write
```c
// main.c:7263-7272
case ACK_PW_ID:
    if(RxBuffer[4]<=NO_OF_ACKPWD)     // no lower-bound check
    {
        AckPwdInd=RxBuffer[4];
        tempchar=RxBuffer[4]-1;       // RxBuffer[4]==0 wraps tempchar (uint8_t) to 255
        AckPwd[tempchar] = tempshort; // out-of-bounds write, array size is 15
        WriteEEPROMData((ACK_PASSWORD+(2*tempchar)),(uint8_t*)&AckPwd[tempchar],sizeof(AckPwd[tempchar]));
    }
```
A crafted RS-485 command with password-index byte `0` writes past `AckPwd[15]` and to EEPROM address `ACK_PASSWORD+510`, ~480 bytes past the intended 30-byte block. The read path (`main.c:7552`, `AckPwd[RxBuffer[4]-1]`) has no bounds check at all either.

### 14. DP3 auto-calibration is unreachable from the front panel
```c
// main.c:3484
case DP_AUTO_CAL_MODE:
    progTimeout=60;
    autoCal_para_cnt=1;    // hard-set, not incremented/wrapped
break;
```
`autoCal_para_cnt` is only ever set to `0` or `1` anywhere in the file — never `2`. The confirm-calibration state machine (`main.c:3282`, `switch(autoCal_para_cnt){case 0:...DP1; case 1:...DP2; case 2:...DP3}`) can therefore never reach `case 2`. Looks like a leftover from an incomplete edit (was likely meant to increment/wrap).

### 15. Negative-sign segment never lights for TM/RH in "upper alarm" state
```c
// main.c:829, 946, 1070, 1192
if(lcd.Sym_TM_MIN) { ... }   // should be Sym_TM_MIN_ALM, as the parallel DP1 block correctly does at line 708
```
`conv_value()` sets `Sym_TM_MIN_ALM`/`Sym_RH_MIN_ALM` (not the plain `_MIN` flag) while an alarm is active. **Effect:** whenever TM or RH is in the upper-alarm state and the value is negative, the minus sign silently disappears from the display.

### 16. Wrong variable used in MIN/MAX/MEAN temperature display branch
```c
// main.c:2436
else if(tempfloat < 100.0)   // every sibling line in this chain correctly uses tempfloat1
```
Copy-paste leftover from the DP1 version of this block. **Effect:** the decimal-precision branch chosen for the MIN/MAX/MEAN temperature display can be based on a stale, unrelated value, producing a garbled or wrongly-rounded display.

### 17. UART baud-rate display overflows `data[]` by 3 bytes for 115200 baud
```c
// main.c:3150
case BAUD_115200: convert_float(115200,&data[4],6); break;
```
Every sibling case uses `convert_char(value,&data[4],N)` with `N` = total digit count; for 115200 (too big for `convert_char`'s `unsigned short` parameter) the author switched to `convert_float`, but that function's third argument is *digits after the decimal point*, not total digits. With no fractional part, it still emits 6 more bytes after the integer digits, writing into `data[10..15]` — `data[]` is declared `uint8_t data[NO_DIGIT]` (`NO_DIGIT`=13, valid indices 0–12), so this overflows 3 bytes past the array, into the adjacent `seg_code[]` lookup table in memory, and also stomps the "BDr" label that was just written. Fix: `convert_float(115200,&data[4],0)`.

### 18. Temperature calibration miscalibrated by ~28.8° when switching to Fahrenheit
```c
// main.c:9225-9226 (TMUnitChange)
TM_Cal_Value_F = ((float)TM_Cal_Value_F * 1.8) + 32.0;   // should be +320
TM_Cal_Value_C = ((float)TM_Cal_Value_C * 1.8) + 32.0;   // should be +320
```
These values are ×10 fixed-point (confirmed by `TM_Cal_float_Value_F = TM_Cal_Value_F/10.0` right after, and by the correct `+320`/`-320` used for the alarm-threshold conversions two lines above/below). Using `+32.0` instead of `+320` is off by 288 (≈28.8° after the later `/10.0`).

### 19. Independent watchdog timeout is ~8x longer than configured
```c
// iwdg_systemmonitor.c:70, 82
uint16_t Reload = LSI_VALUE / 1000 * Timeout / 32;
...
IWDG_SetPrescaler(IWDG_Prescaler_256);
```
The reload formula assumes a `/32` prescaler, but `/256` is what's actually configured (8x larger). `main.c:11720` calls `IWDG_Configure(1250)` intending ~1.25s of watchdog protection; the real timeout ends up ~10 seconds. Not a false trigger risk, but it means a firmware hang takes 8x longer than intended to auto-recover from.

---

## Lower confidence / edge-case / debug-only

These were flagged during review but are either gated behind debug builds, depend on conditions I couldn't fully confirm are reachable from this codebase alone, or are minor:

- **`print_short()`** (`main.c:9393-9401`): the digit-writing pointer is decremented one too many times, so `opstr()` is called one byte before the actual digit string for non-negative values (sends one garbage byte first). Negative values happen to be unaffected because the extra position lines up with the sign slot.
- **`print_float()`** (`main.c:9317-9376`): the pointer passed to `opstr()` at the end has already been walked to the terminating NUL, so it appears to always transmit an empty string. All call sites are inside `#ifdef DEBUG_RCV_CMD`/`#ifdef ENABLE_PRINTF`, so this likely only affects debug builds.
- **`FindLogIndex()`** (`main.c:8305, 8374`): `MidLogInd-1` can underflow (`uint32_t` 0−1) if the binary search reaches index 0 with a target time older than the earliest log. Plausible but I couldn't fully confirm from this range alone whether callers always keep the search bounded away from 0.
- **`SHT2x_MeasureHM()`** (`Interface/SHT25.c:94`): the clock-stretch wait loop calls `PLATFORM_DelayMS(1000)` up to 1000 times with a comment claiming "~1s timeout" — since `PLATFORM_DelayMS` takes milliseconds, the real worst-case wait is ~1000 seconds, which would stall the whole firmware if the sensor ever fails to release the clock line.

---

## Checked and ruled out (not bugs)

- **UART RX "message complete" flag (`bool_msgRcvOK`) never cleared, permanently blocking reception** — this was raised during review but is incorrect: `ServePCMsg()` unconditionally resets both `bool_msgRcvOK` and `RxInd` to 0 at the very end of the function (`main.c:8271-8272`), on every normal completion path. Reception is not blocked.
- **`gpio.c` KEY1-4/DOOR pin comment doesn't match the configured pins** (`gpio.c:132-142`) — the comment block is simply stale documentation from an earlier hardware revision. The pins actually configured as inputs there (`GPIO_Pin_1`, `GPIO_Pin_2`, `GPIO_Pin_15` on GPIOA) correctly match `INPUT1_SENSE`, `INPUT2_SENSE`, and `PARA_SELECT_KEY` as defined in `gpio.h`. The real key/door pins (`UP_KEY`, `DN_KEY`, `PROG_ENT_KEY`, `DOOR_SENSE`) live on GPIOB/GPIOD, configured correctly elsewhere in the same file.

---

## Suggested priority order

1. Fix the two EEPROM address-map gaps (#1, #2) — these actively corrupt other config data on every boot.
2. Fix the RTC/epoch bugs (#5, #6) — everything date/log-related downstream depends on these.
3. Fix `ReadLog()`'s missing offset (#7) — affects all historical log retrieval.
4. Fix the boot-time calibration restore gaps (#3, #4, #8, #9, #10) — silent loss of calibration data on every reset.
5. Fix the protocol-level bugs (#11, #12, #13, #14) — affect what the host/technician actually sees or can do.
6. Fix the display bugs (#15, #16, #17, #18) and the watchdog timing (#19) as time allows.

---

## Follow-up audit: `sb_const.h` address-map continuity (lines 159–304 and 433–459)

Requested as a dedicated re-check after the fixes below were applied. Method: resolved every `#define` in both ranges to an absolute integer address/offset (substituting each macro's dependencies), then (a) checked the sequence is strictly non-decreasing, (b) cross-checked every `WriteEEPROMData`/`ReadEEPROMData` call's actual `sizeof()` against the reserved gap to the next constant, and (c) for the data-flash section, computed real byte ranges for every named region and did an exhaustive pairwise overlap check (not just adjacent-neighbor gaps).

### Lines 159–304 (EEPROM parameter map): now clean

Both previously-reported issues in this range are fixed and confirmed correct:
- `PARA_SCROLL_TIME` is now `(CURR_LOG24_IND+200)` (was `+2`) — matches the actual usage, which indexes up to `CurrentLog24IndReadLoc*2` for `CurrentLog24IndReadLoc` in 0–99 (max offset 198).
- `DP_LIMIT_ADDR` is now `(XBEE_MAC_ADDR+32)` (was `+6`) — matches `NO_OF_XBEE_MAC(2) × XBEE_MAC_SIZE(16) = 32` bytes actually written.
- (Also confirmed fixed, though outside these two line ranges: the DP3 factory-cal write now correctly uses `DP_SW_FACT_ADDR+4` instead of `+3`.)

Re-ran the check exhaustively across all 132 constants in this range: the whole chain (`FIRST_BOOT_CHECK` → `COM_CONTROL_ADDR`) is one continuous, strictly-increasing sequence with no back-references, and every `WriteEEPROMData`/`ReadEEPROMData` call's actual transferred size now fits inside its reserved gap to the next constant. **No remaining overlaps in this range.** (`FIRST_BOOT_CHECK` was also changed to `CONFIG_PARA_ADDR` instead of a bare `0` — this is a good change, since it makes explicit what was previously only an assumption: that this whole parameter map lives inside the `CONFIG_PARA_ADDR` 2048-byte region defined later in the file. The map currently ends at byte 1676, comfortably inside that 2048-byte allowance.)

### Lines 433–459 (data-flash log addresses): one confirmed overlap bug, still present

```c
#define MIN_MAX_LOG_ADDR_OFFSET   ((LAST_LOG24_ADDR_OFFSET+LAST_LOG24_ADDR)*LOG_SIZE)
#define LAST_DP1_MIN_MAX_OFFSET   (MIN_MAX_LOG_ADDR_OFFSET)
#define LAST_DP2_MIN_MAX_OFFSET   (LAST_DP1_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)
#define LAST_DP3_MIN_MAX_OFFSET   (LAST_DP2_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)
#define LAST_TM_MIN_MAX_OFFSET    (LAST_DP3_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)
#define LAST_RH_MIN_MAX_OFFSET    (LAST_TM_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)

#define DP1_CURR_24HR_MEAN_OFFSET (LAST_DP3_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)   // <-- bug
#define DP2_CURR_24HR_MEAN_OFFSET (DP1_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)
#define DP3_CURR_24HR_MEAN_OFFSET (DP2_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)
#define TM_CURR_24HR_MEAN_OFFSET  (DP3_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)
#define RH_CURR_24HR_MEAN_OFFSET  (TM_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)
```

**Bug A — `DP1_CURR_24HR_MEAN_OFFSET` is derived from the wrong predecessor, colliding with TM/RH's min-max log storage.**

There are 5 min-max-log regions (DP1, DP2, DP3, TM, RH), each `MIN_MAX_MEAN_LOG_SPACE` (15×16=240) bytes, laid out back-to-back. `DP1_CURR_24HR_MEAN_OFFSET` should start *after all five* (i.e. after `LAST_RH_MIN_MAX_OFFSET`), but the formula adds one more `MIN_MAX_MEAN_LOG_SPACE` to `LAST_DP3_MIN_MAX_OFFSET` instead — which is arithmetically identical to `LAST_TM_MIN_MAX_OFFSET`. So `DP1_CURR_24HR_MEAN_OFFSET == LAST_TM_MIN_MAX_OFFSET` exactly, and everything chained after it (`DP2/DP3/TM/RH_CURR_24HR_MEAN_OFFSET`) lands inside what should be TM's and RH's min-max log space instead of after it. Computed byte ranges (using `LOG_SIZE=50`, `MIN_MAX_MEAN_LOG_SPACE=240`, `HOUR_MEAN_VALUE_SPACE=96`):

| Region | Start | End (excl.) |
|---|---|---|
| `LAST_DP1_MIN_MAX_OFFSET` | 3,424,400 | 3,424,640 |
| `LAST_DP2_MIN_MAX_OFFSET` | 3,424,640 | 3,424,880 |
| `LAST_DP3_MIN_MAX_OFFSET` | 3,424,880 | 3,425,120 |
| `LAST_TM_MIN_MAX_OFFSET` | 3,425,120 | 3,425,360 |
| `LAST_RH_MIN_MAX_OFFSET` | 3,425,360 | 3,425,600 |
| `DP1_CURR_24HR_MEAN_OFFSET` | 3,425,120 | 3,425,216 |
| `DP2_CURR_24HR_MEAN_OFFSET` | 3,425,216 | 3,425,312 |
| `DP3_CURR_24HR_MEAN_OFFSET` | 3,425,312 | 3,425,408 |
| `TM_CURR_24HR_MEAN_OFFSET` | 3,425,408 | 3,425,504 |
| `RH_CURR_24HR_MEAN_OFFSET` | 3,425,504 | 3,425,600 |

All five `*_CURR_24HR_MEAN_OFFSET` regions fall entirely inside `LAST_TM_MIN_MAX_OFFSET`/`LAST_RH_MIN_MAX_OFFSET`'s byte ranges. Since both regions are actively written (`WriteLog(LAST_TM_MIN_MAX_OFFSET,...)`/`WriteLog(LAST_RH_MIN_MAX_OFFSET,...)` and `WriteLog(DP1_CURR_24HR_MEAN_OFFSET,...)` etc. all appear in `main.c`), TM's and RH's daily min/max/mean history and the rolling 24-hour mean data for all five channels overwrite each other in flash.
**Fix:** `#define DP1_CURR_24HR_MEAN_OFFSET (LAST_RH_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)`.

**Bug B (related, worth checking) — `MIN_MAX_LOG_ADDR_OFFSET`'s formula mixes a byte address with a record count, placing the whole min-max/24hr-mean block inside the tail of the 24-hour log's own real storage.**

`LAST_LOG24_ADDR_OFFSET` (=`LAST_LOG_ADDR`=67,048) is a byte address; `LAST_LOG24_ADDR` (=1440) is a record count. The formula `(LAST_LOG24_ADDR_OFFSET + LAST_LOG24_ADDR) * LOG_SIZE` adds these two dissimilar quantities together *before* scaling by `LOG_SIZE`, rather than scaling the record count alone and adding it to the byte address (`LAST_LOG24_ADDR_OFFSET + LAST_LOG24_ADDR*LOG_SIZE`, which would give 139,048). The actual formula instead yields 3,424,400.

Tracing where the real 24-hour log data lives (`WriteLog(REGULAR_LOG_ADDR, LAST_LOG24_ADDR_OFFSET+CurrentLog24Ind, ...)`, and `CurrentLog24Ind` cycles 0–1439, confirmed by the wraparound check at `main.c:4953`): actual address = `(67048+CurrentLog24Ind)*50 + 2048`, ranging from byte **3,354,448** to **3,426,448** as `CurrentLog24Ind` cycles through its full range over normal long-term operation. `MIN_MAX_LOG_ADDR_OFFSET` (3,424,400) — and therefore the entire min-max-log and current-24hr-mean block above — falls *inside* that range (specifically, inside the last ~41 of the 1440 24-hour-log slots). This isn't a rare edge case: `CurrentLog24Ind` legitimately reaches that final ~41-slot range once per full 1440-cycle during ordinary operation, so this collision recurs periodically, corrupting whichever of the two data sets was written most recently.

I'm confident in the concrete numbers above (independently computed and cross-checked against the actual `WriteLog` call sites), but the *correct* fix depends on which addressing convention you want to standardize on for this file — a pure byte-address chain (like the lines 159–304 section) or the `LogInd`-multiplied-by-`LOG_SIZE` convention `WriteLog`/`ReadLog` use internally. I'd recommend picking one and re-deriving `MIN_MAX_LOG_ADDR_OFFSET` (and everything chained after it) explicitly in bytes, with a comment stating the chip capacity you're budgeting against, rather than patching the existing formula in place.

### Re-verification (second pass) — both bugs above fixed; one new issue caught and corrected

On the next edit, both of the above were addressed:
- `DP1_CURR_24HR_MEAN_OFFSET` now correctly reads `(LAST_RH_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)` — Bug A resolved.
- `MIN_MAX_LOG_ADDR_OFFSET` was changed to scale the record count by `LOG_SIZE` before adding it to the byte address, matching the fix direction above.

That second edit introduced a **missing closing parenthesis**, which I fixed directly (one-character change, low risk to call out and apply without waiting):
```c
// as edited (broken — 3 '(' vs 2 ')', one unclosed):
#define MIN_MAX_LOG_ADDR_OFFSET		((LAST_LOG24_ADDR_OFFSET+(LAST_LOG24_ADDR*LOG_SIZE))

// fixed (balanced):
#define MIN_MAX_LOG_ADDR_OFFSET		(LAST_LOG24_ADDR_OFFSET+(LAST_LOG24_ADDR*LOG_SIZE))
```
Because `#define` bodies are substituted verbatim, this unbalanced paren would have propagated into every macro that references `MIN_MAX_LOG_ADDR_OFFSET` (`LAST_DP1_MIN_MAX_OFFSET` and everything chained after it), breaking the build wherever any of them get used.

With the paren fixed, I re-ran the full check: `MIN_MAX_LOG_ADDR_OFFSET` now resolves to byte **139,048** (`LAST_LOG_ADDR` 67,048 + `1440×50` = 72,000), which no longer falls inside the real 24-hour log's actual byte range (3,354,448–3,426,448) — Bug B is resolved too. Re-ran the exhaustive pairwise overlap check across all 12 named regions in this section (`CONFIG_PARA_ADDR` through `RH_CURR_24HR_MEAN_OFFSET`): **no overlaps found.** Lines 433–459 are now clean.
