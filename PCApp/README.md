# NIYAMA_3DP Configuration Tool

A Windows application for configuring and monitoring the segment-LCD environmental
monitor over its RS485/UART link. It speaks the protocol described in
`UART_Protocol.docx`.

## Running it

Copy `NiyamaConfig.exe` anywhere and double-click it. Nothing needs to be installed:
it targets .NET Framework 4.x, which is part of Windows 8 and later. The whole
application is that single 40 KB file.

Pick the COM port, the baud rate and the device ID, then press **Connect**. The
firmware defaults are **57600** and device ID **1**.

## What each tab does

**Live** — firmware version, the device's own clock, the serial number, and the three
channels with their current value, recorded minimum and maximum, and alarm state,
refreshed every second. A channel in sensor fault shows `fault` in orange; a channel
in alarm turns red. *Set device clock to PC time* writes this PC's clock to the
device. The serial number is exactly 16 characters and is read back after writing.

**Parameters** — every readable and writable setting in one grid: alarm setpoints,
recorded extremes, log interval, buzzer times, sensing times, display and comms
settings. Read them all, or select rows and read or write just those. Edit a Value
cell, select the row, and press *Write selected*; the application confirms the list
before sending anything and reads the values back afterwards.

**Calibration** — the DP reading clamp, the six per-slot DP offsets, the DP zero
offset and temperature / humidity calibration. Every pressure field on this tab is
entered in Pa; the conversion to the tenths or hundredths the wire carries happens
inside the application.

Calibration must be unlocked first, with either the customer or the factory password;
the device keeps the window open for about 60 seconds and every calibration exchange
restarts it. For temperature and humidity you enter the **true value from a reference
instrument**, not a correction — the device works out the correction itself as its own
reading minus what you enter, and resets that channel's recorded minimum and maximum
at the same time.

**Logs** — reads the device's five logs and builds a report. Choose which to read,
the date range for the regular log and how many records to pull from the ring, then
*Save report and CSV*: a report meant to be read by a person, and the same records as
CSV for a spreadsheet.

The report leads with the instrument and its clock — serial number, firmware, address,
channel layout, feature word, the device clock against this PC's and whether the
device still trusts it. A log is worth little without knowing that, so it comes before
the data rather than after. Every settable parameter follows, then the log sections.

**Log** — every exchange, with an option to record each frame in hex.

## Things the firmware does that the application works around

- **The real-time reply is binary, not ASCII.** Its 51 bytes contain IEEE floats, and
  a float can easily contain `0xFC`, which is also the end-of-frame byte. The
  application reads that reply by length instead of scanning for the terminator.
- **Scaling is not uniform.** Alarm setpoints and min/max travel as hundredths of the
  engineering unit; the DP clamp and the slot offsets travel as tenths; the zero
  offset as hundredths. Each parameter carries its own factor, so the grid always
  shows real units.
- **Values are parsed into an `int16_t`.** Anything past 32767 wraps silently on the
  device, so the application refuses to send a value that would not survive the trip.
- **A device-ID change needs two writes** within ten seconds — the first only arms it.
  The application sends both.
- **The clock cannot be set backwards.** The firmware discards a time earlier than the
  one it already holds.
- **The zero offset needs the calibration password first**, and the unlock lasts about
  60 seconds.
- **Which channels exist depends on the firmware build**, and the real-time frame
  carries no flag saying which. The application probes the temperature-unit parameter,
  which only exists in the DP1+Temp+RH build, and hides the rows that do not apply.
- **Broadcasting (device ID 0) is not usable for reads.** Every device on the bus
  answers, so the replies collide. Address one device at a time.
- **The device needs room between requests.** It serves one message at a time and
  cannot buffer the next while it is replying. The application paces every request
  about 85 ms apart and, on a miss, backs off 300 ms and then 1200 ms before trying
  again.

  Firmware before 1.0.4 was far worse than that: a failed checksum latched its
  receive-pending flag and gated the interrupt permanently, so the unit went deaf for
  seconds at a time. Back-to-back frames lost 43 of 60. Fixed in 1.0.4, which brought
  the same test to 2 of 60. The pacing is kept regardless, and it is what makes the
  tool work against older firmware.
- **Narrowing the DP clamp used to strand the recorded extremes.** A minimum only
  moves down and a maximum only moves up, so an extreme captured under a wider clamp
  could never be superseded once the clamp was reduced — the pair sat at values the
  device could no longer produce. Seen on hardware: clamp 100.0 Pa, live reading
  5.5 Pa, extremes stuck at −299.20 and +293.20. The boot check did not catch it
  because it validates against the sensor rating, not the configured clamp. Fixed in
  firmware: an extreme outside the clamp restarts from the present reading.
- **A calibration reply changes length with the unlock state** — 12 bytes locked,
  39 with factory calibration open, 72 with customer calibration open, because the
  calibration date history rides behind the value. The application reads the length it
  expects from its own unlock state and falls back to scanning for the terminator if
  the window closed on the device in the meantime.
- **Firmware before this release answered nothing at all** to a calibration read while
  calibration was locked: that branch built a reply and never sent it. Fixed; it now
  returns the stored correction on its own.
- **A lost reply can desynchronise the exchange**, so the next reply arrives against
  the following request and a value shows up under the wrong parameter's name. Both
  reply types echo the parameter ID in byte 4; the application checks it and discards
  anything that does not match what it just asked for.

## The logs

Five of them, all gated at run time by the feature word (0x4E, factory password 1234)
and all refusing to write unless the clock has been set and is trusted:

| log | command | frame | holds |
|---|---|---|---|
| regular log | `RDLG_DT` 0x49 | 70 bytes, record at 5 | 60000 records |
| 24 hour ring | `FLASH24_IND` 0x47 | 72 bytes, record at 7 | 1440 records |
| 15 day min/max/mean | `MINMAXMEAN_IND` 0x53 | 25 bytes, record at 7 | 15 days |
| 24 hourly means | `MEAN_HR` 0x54 | 13 bytes, float at 7 | 24 values |
| RAM buffer | `RAM_ALL` 0x45 | 1507 bytes, 30 records from 5 | last 30 readings |

Four of the five are **streamed**: the request arms a transfer and the device pushes
one frame per main-loop pass until its count runs out. So a read is one request and
then however many frames arrive. `RAM_ALL` is the exception, one reply with the lot.

Two things make the records easy to misread. They are all 50 bytes and all open the
same way - an epoch, then the log type, user, password, fault flags, and the readings
from offset 10 - so a wrong offset still parses and yields plausible nonsense. And the
frames cannot be split by scanning for the 0xFC terminator, because the floats inside
contain that byte; they are taken by fixed length instead. The count frame that leads
a regular-log transfer carries a uint32, not the ASCII the ordinary replies use.

## Rebuilding

No SDK or IDE is needed — the compiler is already on the machine:

```bash
C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /nologo /target:winexe /optimize+ /out:NiyamaConfig.exe /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Core.dll NiyamaConfig.cs Logs.cs Report.cs
```

## Tests

Every test links the real `NiyamaConfig.cs`, so what they exercise is what the
application ships. They all build the same way — only the `/main:` class changes:

```bash
C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /nologo /target:exe /main:NiyamaConfig.<Class> /out:<Class>.exe /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Core.dll NiyamaConfig.cs Logs.cs Report.cs <Class>.cs
```

| file | what it covers | writes to the device? |
|---|---|---|
| `SelfTest.cs` | CRC, frame layout, value encoding, response parsing, against a transcription of the firmware's own `CalCRC()` and `findValue()` | no hardware needed |
| `Verify.cs` | the whole parameter sweep, live frame and indexed reads, reporting retries and losses | no |
| `MinProbe.cs` | repeated reads of one parameter, to tell a real fault from frame loss | no |
| `EpochTest.cs` | the device's epoch against its own RTC fields | no |
| `MinMax.cs` | watches the DP extremes, flags any stranded outside the clamp | no |
| `ReportTest.cs` | reads all five logs and writes a report and CSV | no |
| `WriteTest.cs` | the write path: read, write, verify, restore | yes, restores |
| `PaTest.cs` | the Pa-scaled clamp and slot offsets | yes, restores |
| `NewFeat.cs` | serial number, zero offset, calibration reads | yes, restores |
| `StrandTest.cs` | proves a stranded DP extreme recovers | yes, restores |
| `TempCalTest.cs` | customer temperature calibration in Fahrenheit | yes, restores |
| `FactCalTest.cs` | factory temperature calibration in Fahrenheit | yes, restores |
| `PersistTest.cs` | a calibration across a power cycle; `arm` then `check` | yes, restores |
| `ParamWordTest.cs` | the parameter word: write, restart, read back, restore | yes, restores |

The ones that write refuse to run if real work is already stored - a calibration test
will not overwrite an existing calibration - and each puts back what it changed,
including on failure.

Two of them need you: `PersistTest` wants a power cycle between `arm` and `check`, and
`ParamWordTest` restarts the device twice of its own accord.

