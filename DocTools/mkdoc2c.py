# -*- coding: utf-8 -*-
"""UART_Protocol.docx, part 3: baud codes, the special frames, gotchas, defaults."""

# ================================================================== baud codes
H('9.  Baud rate codes', 1)
P('Parameter 0x41 carries a code, not a rate. The firmware only accepts 3 to 9; codes '
  '0 to 2 exist in the table but are rejected on write, and a stored value outside '
  '3 to 9 is replaced at boot.')
TABLE(['Code', 'Rate', 'Accepted'],
      [['0', '1200', 'no'], ['1', '2400', 'no'], ['2', '4800', 'no'],
       ['3', '9600', 'yes'], ['4', '14400', 'yes'], ['5', '19200', 'yes'],
       ['6', '28800', 'yes'], ['7', '38400', 'yes'],
       ['8', '57600', 'yes, the default'], ['9', '115200', 'yes']],
      [0.8, 1.4, 2.4])
P('The device switches rate as soon as it has sent the reply, so the host must '
  'reconnect at the new rate.', bold=True)

# ================================================================== special frames
H('10.  Frames that are not plain numbers', 1)

H('10.1  Date and time in one exchange  (0x4B)', 2)
P('Read', bold=True)
MONO('  request   FF  ID  10  4B  CRC  FE\n'
     '  reply     FD  ID  10  ST  4B  DDMMYYhhmmss  CRC  FC       (19 bytes)')
P('Twelve ASCII digits: day, month, two-digit year, hour, minute, second.')
P('Write', bold=True)
MONO('  request   FF  ID  11  4B  DDMMYYhhmmss  CRC  FE')
P('The device refuses a time earlier than the one it already holds, so a clock that is '
  'running fast cannot be wound back this way.', bold=True)
P('The field carries a two digit year, so only 2000 to 2099 can be expressed. The '
  'conversion to epoch seconds is undefined below 2000 - the year is held unsigned '
  'and would wrap - but no such date can be entered through this field in the first '
  'place.')
P('The epoch the device reports is seconds since 1 January 1970 measured against its '
  'own local time. No timezone or daylight-saving correction is applied anywhere, so '
  'a host comparing device timestamps with a UTC source has to account for the offset '
  'itself.')

H('10.2  Serial number  (0x40)', 2)
P('Read', bold=True)
MONO('  request   FF  ID  10  40  CRC  FE\n'
     '  reply     FD  ID  10  ST  40  <16 ASCII characters>  CRC  FC     (23 bytes)')
P('Write', bold=True)
MONO('  request   FF  ID  11  40  <16 ASCII characters>  CRC  FE')
P('Exactly sixteen characters, no length field and no padding. They are free-form, so '
  'one of them could be 0xFC; read the reply by its fixed length of 23 bytes rather '
  'than hunting for the terminator.', bold=True)
P('The last eight characters are what serial-number addressing matches against '
  '(section 2).')

H('10.3  Live values  (0x48)', 2)
P('The only reply that is binary rather than ASCII. Fixed at 51 bytes.')
MONO('  request   FF  ID  10  48  CRC  FE\n'
     '\n'
     '  reply, 51 bytes:\n'
     '    [0]      FD\n'
     '    [1]      device address\n'
     '    [2]      command\n'
     '    [3]      status\n'
     '    [4]      48\n'
     '    [5..8]   epoch seconds            uint32, little endian\n'
     '    [9]      sensor fault flags       same bits as the status byte\n'
     '    [10..13] DP1                      float\n'
     '    [14..17] DP2   or temperature     float\n'
     '    [18..21] DP3   or humidity        float\n'
     '    [22..25] DP1 minimum              float\n'
     '    [26..29] DP1 maximum              float\n'
     '    [30..33] DP2 / temperature min    float\n'
     '    [34..37] DP2 / temperature max    float\n'
     '    [38..41] DP3 / humidity min       float\n'
     '    [42..45] DP3 / humidity max       float\n'
     '    [46]     DP1 alarm                0 none, 1 high, 2 low\n'
     '    [47]     DP2 / temperature alarm\n'
     '    [48]     DP3 / humidity alarm\n'
     '    [49]     CRC over [1..48]\n'
     '    [50]     FC')
P('Read this one by length. The floats are raw IEEE-754 and any of their bytes can be '
  '0xFC, so scanning for the terminator will truncate the frame sooner or later.',
  bold=True)
P('Nothing in the frame says which of the two channel layouts applies. Ask for 0x2E, '
  'which only exists in the DP1+Temp+RH build: an answer means temperature and '
  'humidity, INVALID_PARA means DP2 and DP3.')

H('10.4  Indexed parameters', 2)
P('Four parameters carry index bytes between the identifier and the value. The value '
  'field is a fixed five characters, and a negative value carries its sign inside '
  'those five — so it has one digit fewer.')
TABLE(['ID', 'Layout of a write', 'Value field'],
      [['0x6E', 'FF ID 11 6E <ch> <5 chars>', 'tenths of a Pa'],
       ['0x72', 'FF ID 11 72 <ch> <slot> <5 chars>', 'tenths of a Pa'],
       ['0x71', 'FF ID 11 71 <ch> <sign> <5 digits>', 'hundredths of a Pa'],
       ['0x5F', 'FF ID 11 5F <ch> <sign> <5 digits>', 'span factor']],
      [0.6, 3.0, 3.0])
P('Channel is the ASCII digit 0, 1 or 2 for DP1, DP2, DP3. Slot is 0 to 5.')
P('0x71 and 0x5F are the odd ones out: their sign sits in a byte of its own ahead of a '
  'full five digits, where 0x6E and 0x72 keep the sign inside the five.', bold=True)
MONO('  DP1 clamp to 300.0 Pa      FF 01 11 6E 30 30 33 30 30 30 <crc> FE\n'
     '                                         0  0  3  0  0  0\n'
     '  DP1 slot 3 to -2.5 Pa      FF 01 11 72 30 33 2D 30 30 32 35 <crc> FE\n'
     '                                         0  3  -  0  0  2  5\n'
     '  DP1 zero offset 1.25 Pa    FF 01 11 71 30 2B 30 30 31 32 35 <crc> FE\n'
     '                                         0  +  0  0  1  2  5')
P('A read takes the index bytes but no value: FF ID 10 72 <ch> <slot> CRC FE.')
P('The per-slot offset is added to a reading whose magnitude falls in that slot. The '
  'bands are fixed:')
TABLE(['Slot', 'Applies when the reading is'],
      [['0', 'below 50.0 Pa'], ['1', '50.0 to 100.0 Pa'], ['2', '100.0 to 150.0 Pa'],
       ['3', '150.0 to 200.0 Pa'], ['4', '200.0 to 250.0 Pa'],
       ['5', '250.0 to 300.0 Pa']],
      [0.8, 4.0])
P('An offset is limited to ±500.0 Pa; outside that the device sets INVALID_PARA '
  'and stores nothing.')

H('10.5  Calibration  (0x30, 0x31, 0x67, 0x32, 0x33)', 2)
P('Calibration has to be unlocked first, with 0x37 for factory or 0x38 for customer. '
  'The window stays open for about 60 seconds and every calibration exchange restarts '
  'it.')
P('Writing — send the TRUE value, not a correction', bold=True)
MONO('  request   FF  ID  11  32  <5 chars, tenths>  CRC  FE')
P('The device takes what you send as the true reading and stores the difference from '
  'what it is measuring at that moment, as measured minus reference. It also resets '
  'that channel’s recorded minimum and maximum. Sending 00250 to 0x32 therefore '
  'means “you are actually reading 25.0 right now”.', bold=True)
P('Reading — the reply changes length with the unlock state', bold=True)
TABLE(['State', 'Reply length', 'Contents'],
      [['locked', '12 bytes', 'the stored correction only'],
       ['factory unlocked', '39 bytes',
        'correction, then 12 bytes of calibration date and 15 of certificate'],
       ['customer unlocked', '72 bytes',
        'correction, then 60 bytes of user calibration date history']],
      [1.4, 1.2, 4.0])
P('In every case the correction is the first five characters of the payload, zero '
  'padded, in tenths, with a minus sign in the first position when negative.')

H('10.5.1  Which unit a temperature is in', 3)
P('This is not uniform, and it catches people out. The device stores and transmits '
  'temperature in Celsius almost everywhere; only the values a person reads off the '
  'display follow the selected unit.')
TABLE(['Value', 'Unit', 'Converts with 0x2E?'],
      [['Live temperature in the 0x48 frame', 'always Celsius', 'no'],
       ['Temperature in the logs and the RAM buffer', 'always Celsius', 'no'],
       ['Recorded minimum and maximum, 0x20 / 0x21', 'the displayed unit', 'yes'],
       ['Alarm setpoints, 0x09 to 0x0C', 'the displayed unit', 'yes'],
       ['Calibration reference you WRITE to 0x32', 'the displayed unit', 'yes'],
       ['Calibration correction you READ from 0x32', 'always Celsius', 'no']],
      [3.0, 1.8, 1.8])
P('So a host showing Fahrenheit has to convert the live value itself, while the '
  'minimum and maximum arrive already converted. Converting those a second time is '
  'the obvious mistake.', bold=True)
P('The last two rows are the subtle pair. The reference value you send is an absolute '
  'temperature in whatever unit the display is set to, but the correction the device '
  'derives and hands back is a difference, and a difference is held in Celsius. A '
  'difference converts by the 1.8 scale alone - the 32 degree offset belongs to '
  'absolute temperatures only. Firmware before 1.0.5 applied the absolute formula to '
  'that difference in four places, so calibrating while the display was in Fahrenheit '
  'stored a correction that was wrong by tens of degrees, and merely switching the '
  'unit altered a calibration that was already stored.', colour=GREY)
P('Firmware before 1.0.4 sent nothing at all for a locked calibration read — the '
  'branch built a reply and never transmitted it, leaving the host to time out. Fixed '
  'in 1.0.4.', colour=GREY)

H('10.6  Feature word  (0x4E)', 2)
MONO('  read      FF  ID  10  4E  PPPP  CRC  FE\n'
     '  write     FF  ID  11  4E  PPPP  BBBBB  CRC  FE')
P('Four digits of factory password, and for a write five more holding the bit pattern '
  'in decimal. The device saves it and restarts a few seconds later, so a host has to '
  'wait for it to come back before reading anything else. On the bench it answered '
  'again about two seconds after the write.')
P('Which parameters the instrument runs:')
TABLE(['Bit', 'Meaning', 'Build'],
      [['0x0001', 'DP1 channel', 'both'],
       ['0x0002', 'Clock', 'both'],
       ['0x0004', 'Alerts', 'both'],
       ['0x0008', 'DP2 channel', 'DP1+DP2+DP3 only'],
       ['0x0010', 'DP3 channel', 'DP1+DP2+DP3 only'],
       ['0x0020', 'Humidity', 'DP1+Temp+RH only'],
       ['0x0040', 'Temperature', 'DP1+Temp+RH only']],
      [0.9, 2.6, 2.3])
P('The upper four bits do not mean the same thing in both builds, so a host has to '
  'know which one it is talking to before interpreting them - the temperature-unit '
  'probe in section 10.3 settles that. Reading 0x0040 as "DP3" on a Temp+RH unit is '
  'the mistake this invites.', bold=True)
P('The clock bit is the one to be careful with. Every log refuses to write without it, '
  'so clearing it stops all logging while leaving the instrument otherwise healthy.')
P('Logging itself is no longer switched here. It is now settled when the firmware is '
  'built, by the BUILD_REGULAR_LOG, BUILD_LOG24_LOG, BUILD_MINMAX_LOG, BUILD_MEAN24_LOG '
  'and BUILD_RAM_BUFFER macros, and a unit that should not log needs a different build '
  'rather than a different setting.', colour=GREY)

H('10.7  Reading the logs', 2)

P('Five logs, all written only while the clock has been set AND is trusted - checked '
  'when each record is written, not merely at start-up. A clock that reads plausibly '
  'is not enough: it has to have been set deliberately, which is what raises the flag '
  'the writers look for. After a first-time initialisation the instrument will run '
  'happily and record nothing until its clock is set.', bold=True)

TABLE(['Log', 'Command', 'Reply frame', 'Holds'],
      [['Regular log', '0x49 RDLG_DT', '70 bytes', '60000 records'],
       ['24 hour ring', '0x47 FLASH24_IND', '72 bytes', '1440 records'],
       ['15 day min/max/mean', '0x53 MINMAXMEAN_IND', '25 bytes', '15 days per channel'],
       ['24 hourly means', '0x54 MEAN_HR', '13 bytes', '24 values per channel'],
       ['RAM buffer', '0x45 RAM_ALL', '1507 bytes', 'last 30 readings'],
       ['RAM buffer, one', '0x46 RAM_IND', '71 bytes', 'one record']],
      [1.7, 1.9, 1.2, 1.8])

P('Four of the five are STREAMED', bold=True)
P('The request arms a transfer and the device then pushes one frame per pass of its '
  'main loop until the count is used up. So a read is one request followed by however '
  'many frames arrive, not a request and a single reply. RAM_ALL is the exception: one '
  'reply carrying all thirty records.')
P('While a transfer is running the device serves no other command, and it releases by '
  'itself when the count reaches zero.')

P('Requests', bold=True)
MONO('  0x49  FF ID 10 49  dd mm yy hh mi ss  dd mm yy hh mi ss  CRC FE\n'
     '                     \\___ start ____/  \\____ end ____/   raw bytes\n'
     '\n'
     '  0x47  FF ID 10 47  SSSS CCCC  CRC FE      start and count, 4 ASCII digits each\n'
     '  0x53  FF ID 10 53  c NN       CRC FE      channel 0-2, count as 2 ASCII digits\n'
     '  0x54  FF ID 10 54  c          CRC FE      channel 0-2; the count is fixed at 24\n'
     '  0x46  FF ID 10 46  ii nn      CRC FE      index and count, raw bytes')

P('A regular-log transfer leads with a short frame giving how many records fall in the '
  'window, as a uint32 - not the ASCII the ordinary replies use.', bold=True)

P('Where the record sits in the frame', bold=True)
P('This differs between them, and a wrong offset still parses and yields plausible '
  'nonsense rather than failing:')
TABLE(['Command', 'Record at', 'Why'],
      [['0x49 regular log', 'byte 5', 'nothing between the header and the record'],
       ['0x47 24 hour ring', 'byte 7', 'a 2 byte index first, big endian'],
       ['0x46 RAM buffer', 'byte 6', 'a 1 byte index first'],
       ['0x45 RAM_ALL', 'byte 5 + n*50', 'thirty records end to end'],
       ['0x53 min/max/mean', 'byte 7', '16 byte record'],
       ['0x54 hourly mean', 'byte 7', 'a single float']],
      [1.8, 1.5, 3.3])

P('The 50 byte reading record', bold=True)
P('The regular log, the 24 hour ring and the RAM buffer all share one layout:')
MONO('  [0..3]   epoch seconds, uint32 little endian\n'
     '  [4]      log type\n'
     '  [5]      user id\n'
     '  [7..8]   password\n'
     '  [9]      sensor fault flags, same bits as the status byte\n'
     '  [10..13] DP1                        float\n'
     '  [14..17] DP2 or temperature         float\n'
     '  [18..21] DP3 or humidity            float\n'
     '  [22..29] DP1 minimum, maximum       float, float\n'
     '  [30..37] DP2 / temperature min, max\n'
     '  [38..45] DP3 / humidity min, max\n'
     '  [46..48] the three alarm states, 0 none 1 high 2 low')
P('Temperature in a record is always Celsius, whatever the display was set to. Bit 7 '
  'of [47] records the unit that was on the display, but the value is not converted.')

P('The 16 byte day record  (0x53)', bold=True)
MONO('  [0..3]   epoch of the day\n'
     '  [4..7]   minimum     float\n'
     '  [8..11]  maximum     float\n'
     '  [12..15] mean        float')
P('Slots that have never been written read back as erased flash, which decodes to '
  'timestamps in the 1970s and values with twenty digits. A host should reject anything '
  'whose epoch predates the product rather than display it.')

# ================================================================== gotchas
H('11.  Things that will catch a host out', 1)

BULLET('A write reply confirms the frame, not the value. A value outside the '
       'parameter’s range is discarded without any error, so read it back.')
BULLET('Check the parameter identifier in byte 4 of every reply. If a reply is ever '
       'lost, the next one arrives against the following request and a value shows up '
       'under the wrong parameter’s name — plausible numbers, silently wrong.')
BULLET('Changing the device address (0x1A) needs the same value written twice within '
       'about ten seconds. The first write only arms it.')
BULLET('Broadcast, address 0, is not usable for reads. Every device on the bus answers '
       'and the replies collide. The reply also carries the responding device’s '
       'own address, never 0.')
BULLET('Disabling the UART (0x70 set to 1) is one-way over the link: once it is off the '
       'only frame the device will still act on is a write of 0x70 itself.')
BULLET('The log interval (0x19) is only loaded from storage on a unit that has logging '
       'enabled. On any other unit a read returns whatever is in RAM, which is 0 — '
       'and 0 cannot be written back, because the accepted range starts at 1.')
BULLET('Reading a parameter that belongs to the other build returns INVALID_PARA, not '
       'silence. Silence means the request was lost; see section 7.')
BULLET('Every value is parsed into a signed 16 bit integer. Do not send anything '
       'outside −32768 to 32767.')
BULLET('A log transfer takes over the link. While one is running the device serves '
       'no other command, so a host must read the stream out before asking for '
       'anything else.')
BULLET('Records cannot be found by scanning for the 0xFC terminator. The floats inside '
       'them contain that byte regularly, so frames have to be taken by their fixed '
       'length.')
BULLET('Temperature is not carried in one consistent unit. The live frame and the logs '
       'are always Celsius; the recorded extremes and the alarm setpoints follow the '
       'displayed unit. See 10.5.1 before writing any conversion.')
BULLET('A recorded DP extreme that lies outside the DP clamp cannot be superseded - a '
       'minimum only moves down and a maximum only moves up, and the clamp stops any '
       'reading reaching it. Firmware 1.0.5 restarts such an extreme from the present '
       'reading; before that, narrowing the clamp stranded the pair at values the '
       'device could no longer produce.')

# ================================================================== defaults
H('12.  Defaults on a fresh device', 1)
TABLE(['Setting', 'Default'],
      [['Device address', '1'],
       ['Baud rate', '57600, code 8'],
       ['Customer password', '100'],
       ['Factory password', '1000'],
       ['Factory parameter-set password', '1234'],
       ['Log interval', '1 minute'],
       ['DP reading clamp', '300.0 Pa'],
       ['Per-slot DP offsets', 'all zero'],
       ['Serial number', 'sixteen ASCII zeros'],
       ['Clock epoch', '1 January 1970']],
      [2.6, 4.0])

H('13.  This build', 1)
TABLE(['Item', 'Value'],
      [['Firmware version', FW + '  (0x34 returns ' + str(SOFT_VER) + ')'],
       ['Channel layout', 'DP1 + Temperature + Humidity'],
       ['Pressure sensor', 'XGZP6891D'],
       ['Data flash', 'AT45DB321D'],
       ['Differential pressure rating', '±500 Pa'],
       ['Microcontroller', 'MM32F0141C6P, 64 KB flash, 8 KB RAM']],
      [2.6, 4.0])

P()
P('Generated from the firmware sources. Parameter access and build applicability were '
  'read out of the dispatch tables in main.c rather than written by hand.',
  italic=True, size=8.5, colour=GREY)

doc.save(OUT)
print('wrote %s' % OUT)
print('%d parameter ids, %d readable, %d writable, %d neither'
      % (len(IDS),
         sum(1 for r in IDS if r['read']),
         sum(1 for r in IDS if r['write']),
         sum(1 for r in IDS if not r['read'] and not r['write'])))
