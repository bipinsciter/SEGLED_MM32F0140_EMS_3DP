# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Bare-metal firmware for a **MindMotion MM32F0141C6P** (Cortex-M0, 64 KB flash / 8 KB RAM) environmental-monitoring device: a differential-pressure / temperature / humidity monitor with a segment LCD (TM1680 driver), RS485 host link, data-logging to external flash, and an XBee radio. There is no OS — a single `while(1)` superloop in `main.c`.

## Build

There is no command-line build. This is an IDE project built by flashing to hardware over a CMSIS-DAP debugger (MM32-LINK):

- **Keil MDK-ARM** (primary): open `MDK-ARM/NIYAMA_3DP.uvprojx`, Build (F7), Download (F8). Output binary `NIYAMA_3DP`, map at `MDK-ARM/Listings/NIYAMA_3DP.map`. Compiler: ARMCC, `-O0`, Cortex-M0.
- **IAR EWARM** (secondary/stale): `EWARM/GPIO_LED_Toggle.eww`.

There is no test suite, linter, or CI. Verification is done on-target. The `.uvprojx`/`.uvoptx`/`.uvguix.*` and `.map` files churn on every build — that's expected noise, not meaningful changes.

Include paths (set in the `.uvprojx`, replicate if adding files): project root, `Device/CMSIS/KEIL_Core`, `Device/MM32F0140/HAL_Lib/Inc`, `Device/MM32F0140/Include`, `Interface`.

## Compile-time configuration — `sb_const.h`

Almost all product variation is selected by `#define`s at the top of `sb_const.h`, resolved at compile time. Changing a board/part means editing these, not the drivers:

- `FW_MAJOR/FW_MINOR/FW_PATCH` — firmware version; bump on release (git tags follow `V1.0.x`).
- `DEVICE_MODE` — `DP1_DP2_DP3_MODE` (three pressure channels) **or** `DP1_TEMP_RH_MODE` (one pressure + SHT25 temp/humidity). This flips `PARAMETER_WORD` and is `#if`-gated throughout `main.c` (e.g. DP2/DP3 faults vs. RH/TEMP faults).
- `DATAFLASH_PART` — `AT45DB321D` (byte-rewritable) vs. `XM25QH128A` (NOR, 4 KB sector-erase). See the driver-selection layer below.
- `PRESSURE_SENSOR_PART` — `XGZP6891D` vs. `WF200DP`.
- `ENABLE_KEY_LOGIC` — compiles in the front-panel keypad handling.

## Hardware-abstraction / driver-selection layer

Application code never names a specific part. Two shim headers in `Interface/` pick the driver from the `sb_const.h` selectors and expose a part-neutral API — when adding or swapping a part, add a branch here rather than `#if`-ing the call sites:

- `Interface/dataflash.h` → `DF_Init()`, `DF_ConfigurePageSize()`, `DF_ChipErase()`, `DF_EraseUnit()`, `DF_ERASE_UNITS`, plus shared `WriteEEPROMData / ReadEEPROMData / WriteLog / ReadLog / ReadMinMaxLog`. **Critical:** for the NOR part (`XM25QH128A`) every EEPROM region base must be 4 KB-aligned because erase is per-sector — `dataflash.h` enforces this with `#error` build-time assertions. Two regions sharing a sector means rewriting one erases the other.
- `Interface/pressure_sensor.h` → `DP_TriggerConv(n)` / `DP_ReadPressure(n,v)`. Both drivers are two-phase: trigger on one loop pass, read the result on the next.

Note `Interface/BitBangSPI/` and `Interface/InternalSPI/` hold alternate `AT45DB321D`/`SPI` implementations; the active flash SPI is `spi_master_polling.c` at the project root.

### Bus map

Dataflash is on SPI. Everything else is on one of three separate bit-banged I2C buses, each with its own driver header — pick the right one when adding a device:

| Bus | Header | Devices |
|---|---|---|
| I2C1 | `i2cmaster.h` (`I2C1_Start`, `Write_Byte_I2C1`, …) | DP1 pressure sensor, TM1680 segment-LCD driver, PCF8563 RTC |
| I2C2 | `i2c2master.h` | DP2 pressure sensor (`DP1_DP2_DP3_MODE` only) |
| I2C3 | `i2c3master.h` | DP3 pressure sensor (`DP1_DP2_DP3_MODE`) **or** SHT25 temp/humidity (`DP1_TEMP_RH_MODE`) |

The pressure-sensor drivers `switch` on the channel number to choose the bus, so `DP_TriggerConv(n)`/`DP_ReadPressure(n,v)` are the only call sites that need to know.

## Runtime architecture

`main()` (`main.c:11453`) initializes peripherals in order — platform, GPIO, SPI, dataflash, `boot_data()`, UART, three I2C buses, PCF8563 RTC, LED controller, TIM1, variables, then `IWDG_Configure(1250)` (independent watchdog) — then loops:

```c
while (1) {
    if (bool_sec_flag) { SecondTick(); bool_sec_flag = 0; }  // 1 Hz work
    whileTask();                                             // every pass
}
```

- **`SecondTick()`** (`main.c:8584`) — 1 Hz tasks: read sensors, RTC upkeep, alarms, logging cadence, LCD refresh. Driven by a flag set from the TIM1 ISR.
- **`whileTask()`** (`main.c:9515`) — runs every superloop pass: services the RS485 host protocol, streams flash-log reads, handles XBee reset, and `bool_resetDevice` traps into `while(1)` to let the watchdog reboot the device.
- **`ServePCMsg()`** (`main.c:5577`) — the RS485 command dispatcher. Command opcodes and parameter IDs are the `*_CMD` / `*_ID` defines in `sb_const.h`; frames are CRC-checked (`CalCRC`).
- Interrupts live in `mm32f0140_it.c`: `TIM1_BRK_UP_TRG_COM_IRQHandler` (timebase → `bool_sec_flag` and timers) and `UART1_IRQHandler` (RS485 RX into `RxBuffer1`).
- `boot_data()` loads all persisted config/calibration from dataflash on startup and re-writes defaults on first boot.

## Persistent-data map — handle with care

EEPROM/flash regions are laid out as a chain of address `#define`s in `sb_const.h`, each `(PREVIOUS_ADDR + size_of_previous)`. Because every base is computed from the one above, **an under-sized region silently overruns the next**, corrupting unrelated config. When touching any `*_ADDR` define, verify the gap matches the actual bytes written at every call site (count × element size), and keep NOR 4 KB-alignment intact. `BUG_REPORT.md` documents several real instances of exactly this class of bug (e.g. `XBEE_MAC_ADDR` reserving 6 bytes for 32 bytes of data) — read it before editing the data map.

## Conventions

- Globals are declared `extern` in `sb_global_var.h` / `sb_variables.h` and defined once via the `EXTERN`-macro pattern (see `platform.h`: the `.c` that owns them `#define`s `_<FILE>_C_` so `EXTERN` expands to empty). Hungarian-ish prefixes: `gu8_`/`gu16_`/`gu32_` (global unsigned), `bool_` (flags), `su16_` (static).
- `main.c` is ~11.5k lines and holds almost all application logic; `Interface/` holds peripheral drivers; project-root `.c` files are board/MCU support (gpio, uart, spi, i2c, timers, watchdogs, platform).
- Sensor smoothing uses a small `KalmanFilter` (`main.h`); timekeeping uses epoch conversion helpers (`get_epoch_time` / `get_date_time`) against `EPOCH_YEAR`.
