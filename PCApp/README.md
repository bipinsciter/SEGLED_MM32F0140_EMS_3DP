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

## Rebuilding

No SDK or IDE is needed — the compiler is already on the machine:

```bash
C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /nologo /target:winexe /optimize+ /out:NiyamaConfig.exe /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Core.dll NiyamaConfig.cs
```

`Verify.cs` runs the whole workload against a real device through the application's
own `Link` class - the parameter sweep, the live frame and the indexed parameters -
and reports how many requests needed a retry or went unanswered. It never sends a
write command:

```bash
C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /nologo /target:exe /main:NiyamaConfig.Verify /out:Verify.exe /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Core.dll NiyamaConfig.cs Verify.cs
```

`WriteTest.cs` exercises the write path against a real device: each step reads the
original value, writes a different one, reads it back, restores the original and
confirms the restore.

```bash
C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /nologo /target:exe /main:NiyamaConfig.WriteTest /out:WriteTest.exe /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Core.dll NiyamaConfig.cs WriteTest.cs
```

`NewFeat.cs` tests the serial number (read, write, restore), the Pa-scaled zero
offset and the calibration reads against a real device. It performs no calibration
write:

```bash
C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /nologo /target:exe /main:NiyamaConfig.NewFeat /out:NewFeat.exe /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Core.dll NiyamaConfig.cs NewFeat.cs
```

`SelfTest.cs` checks the protocol layer — CRC, frame layout, value-field encoding and
response parsing — against a transcription of the firmware's own `CalCRC()` and
`findValue()`. It links the real `NiyamaConfig.cs`, so it exercises shipping code:

```bash
C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /nologo /target:exe /main:NiyamaConfig.SelfTest /out:SelfTest.exe /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Core.dll NiyamaConfig.cs SelfTest.cs
```
