# -*- coding: utf-8 -*-
"""UART_Protocol.docx, part 2: the parameter tables. Exec'd in part 1's namespace."""

H('8.  Parameters', 1)
P('Access is what the firmware actually implements: R where only the read dispatch '
  'handles the identifier, W where only the write dispatch does, RW where both do. '
  'Build says which firmware build the parameter exists in — asking a DP1+DP2+DP3 '
  'unit for a humidity parameter returns INVALID_PARA, and the other way round.')

MODE_TEXT = {'DP3': 'DP1+DP2+DP3', 'TEMPRH': 'DP1+Temp+RH', None: 'both'}

META = {
    'DP1UAON_ID':  ('DP1 upper alarm ON', 'Pa', 'hundredths'),
    'DP1UAOFF_ID': ('DP1 upper alarm OFF', 'Pa', 'hundredths'),
    'DP1LAON_ID':  ('DP1 lower alarm ON', 'Pa', 'hundredths'),
    'DP1LAOFF_ID': ('DP1 lower alarm OFF', 'Pa', 'hundredths'),
    'DP2UAON_ID':  ('DP2 upper alarm ON', 'Pa', 'hundredths'),
    'DP2UAOFF_ID': ('DP2 upper alarm OFF', 'Pa', 'hundredths'),
    'DP2LAON_ID':  ('DP2 lower alarm ON', 'Pa', 'hundredths'),
    'DP2LAOFF_ID': ('DP2 lower alarm OFF', 'Pa', 'hundredths'),
    'DP3UAON_ID':  ('DP3 upper alarm ON', 'Pa', 'hundredths'),
    'DP3UAOFF_ID': ('DP3 upper alarm OFF', 'Pa', 'hundredths'),
    'DP3LAON_ID':  ('DP3 lower alarm ON', 'Pa', 'hundredths'),
    'DP3LAOFF_ID': ('DP3 lower alarm OFF', 'Pa', 'hundredths'),
    'TMUAON_ID':   ('Temperature upper alarm ON', 'deg', 'hundredths'),
    'TMUAOFF_ID':  ('Temperature upper alarm OFF', 'deg', 'hundredths'),
    'TMLAON_ID':   ('Temperature lower alarm ON', 'deg', 'hundredths'),
    'TMLAOFF_ID':  ('Temperature lower alarm OFF', 'deg', 'hundredths'),
    'RHUAON_ID':   ('Humidity upper alarm ON', '%RH', 'hundredths'),
    'RHUAOFF_ID':  ('Humidity upper alarm OFF', '%RH', 'hundredths'),
    'RHLAON_ID':   ('Humidity lower alarm ON', '%RH', 'hundredths'),
    'RHLAOFF_ID':  ('Humidity lower alarm OFF', '%RH', 'hundredths'),

    'HR_ID':   ('Clock, hour', 'h', 'plain'),
    'MN_ID':   ('Clock, minute', 'min', 'plain'),
    'SEC_ID':  ('Clock, second', 's', 'plain'),
    'YR_ID':   ('Clock, year', '', 'plain, four digits'),
    'MNTH_ID': ('Clock, month', '', 'plain'),
    'DT_ID':   ('Clock, day of month', '', 'plain'),

    'DP1MIN_ID': ('DP1 recorded minimum', 'Pa', 'hundredths'),
    'DP1MAX_ID': ('DP1 recorded maximum', 'Pa', 'hundredths'),
    'DP2MIN_ID': ('DP2 recorded minimum', 'Pa', 'hundredths'),
    'DP2MAX_ID': ('DP2 recorded maximum', 'Pa', 'hundredths'),
    'DP3MIN_ID': ('DP3 recorded minimum', 'Pa', 'hundredths'),
    'DP3MAX_ID': ('DP3 recorded maximum', 'Pa', 'hundredths'),
    'TMMIN_ID':  ('Temperature recorded minimum', 'deg',
                  'hundredths; converted to Fahrenheit when 0x2E is 1'),
    'TMMAX_ID':  ('Temperature recorded maximum', 'deg',
                  'hundredths; converted to Fahrenheit when 0x2E is 1'),
    'RHMIN_ID':  ('Humidity recorded minimum', '%RH', 'hundredths'),
    'RHMAX_ID':  ('Humidity recorded maximum', '%RH', 'hundredths'),

    'LOGINTVAL_ID': ('Log interval', 'min', 'plain, 1 to 1440; see section 11'),
    'DVCID_ID':     ('Device address', '',
                     'plain; must be written twice, see section 10'),
    'TMUNT_ID':     ('Temperature unit', '', '0 Celsius, 1 Fahrenheit'),
    'SFVER_ID':     ('Firmware version', '',
                     'major*100 + minor*10 + patch, so ' + str(SOFT_VER)
                     + ' is ' + FW),
    'UBRT_ID':      ('Baud rate code', '', 'see section 9; only 3 to 9 are accepted'),
    'MENB_ID':      ('Master enable', '', 'plain'),
    'BZRON_ID':     ('Buzzer ON time', 's', 'plain; 0 in either time silences it'),
    'BZROFF_ID':    ('Buzzer OFF time', 's', 'plain; 0 in either time silences it'),
    'ACK_TIMER_ID': ('Alarm acknowledge silence time', 's', 'plain'),
    'ALM_ACK_ID':   ('Acknowledge the current alarm', '', 'value at offset 5, not 4'),
    'CPWD_ID':      ('Customer password', '', 'plain, up to 999'),
    'FCPWD_ID':     ('Factory customer password', '', 'plain, up to 9999'),
    'CAL_FPWD_ID':  ('Unlock factory calibration', '', 'send the factory password'),
    'CAL_CPWD_ID':  ('Unlock customer calibration', '', 'send the customer password'),
    'ACK_PW_ID':    ('Acknowledge password', '', 'value at offset 5, not 4'),
    'SRNO_ID':      ('Serial number', '', '16 ASCII characters, see section 10'),
    'DOOR_SENSE_POLARITY_ID': ('Door contact polarity', '', 'plain'),
    'DOOR_SENSE_TIME_ID':     ('Door sensing time', 's', 'plain'),
    'DP1_ALM_SENSE_TIME_ID':  ('DP1 alarm sensing time', 's', 'plain'),
    'DP2_ALM_SENSE_TIME_ID':  ('DP2 alarm sensing time', 's', 'plain'),
    'DP3_ALM_SENSE_TIME_ID':  ('DP3 alarm sensing time', 's', 'plain'),
    'LCD_BRIGHT_CNT_ID':      ('LCD brightness', '', 'plain, 0 to 15'),
    'LCD_CONTROL_ID':         ('LCD disable', '', '1 switches the display off'),
    'COM_CONTROL_ID':         ('UART disable', '',
                               '1 switches the port off; see section 11'),
    'AUTO_SENT_INTERVAL_ID':  ('Unprompted send interval', 'min', 'plain, 0 disables it'),
    'XBEE_RST_INTERVAL_ID':   ('Radio reset interval', 'min', 'plain'),
    'DEVICES_IN_GROUP_ID':    ('Devices in group', '', 'plain'),
    'MEAN_HR_ID':             ('24 hour mean start hour', 'h', 'plain'),
    'RELAY_CNTL_ID':          ('Relay control', '', 'relay digit, then 0 or 1'),
    'BRDSTP_ID':              ('Stop broadcasting', '', 'no value'),
    'BRDSTR_ID':              ('Start broadcasting', 'min', 'plain'),
    'EXT_FLASH_ERASE_ID':     ('Erase the external flash', '', 'no value'),
    'DFLT_RTC_ID':            ('Reset the clock to its default', '', 'no value'),
    'DFLT_CAL_ID':            ('Reset calibration', '', 'channel digit 0, 1 or 2'),
    'SET_DPARA_PWD_ID':       ('Set the displayed-parameter word', '',
                               'password then bit pattern, see section 10'),
    'DP1CAL_ID': ('DP1 calibration', 'Pa', 'tenths, see section 10'),
    'DP2CAL_ID': ('DP2 calibration', 'Pa', 'tenths, see section 10'),
    'DP3CAL_ID': ('DP3 calibration', 'Pa', 'tenths, see section 10'),
    'TMCAL_ID':  ('Temperature calibration', 'deg', 'tenths, see section 10'),
    'RHCAL_ID':  ('Humidity calibration', '%RH', 'tenths, see section 10'),
    'DP_LIMIT_ID':       ('DP reading clamp', 'Pa', 'tenths, indexed, see section 10'),
    'DP_OFFSET_ID':      ('DP zero offset', 'Pa',
                          'hundredths, indexed, see section 10'),
    'DP_SW_FACT_ID':     ('DP span factor', '', 'indexed, see section 10'),
    'DP_SLOT_OFFSET_ID': ('DP per-slot offset', 'Pa',
                          'tenths, indexed, see section 10'),
    'DATETIME_ID':       ('Date and time in one exchange', '', 'see section 10'),
    'REALTIME_VAL_ID':   ('Live values', '', 'binary, see section 10'),
    'RAM_ALL_ID':        ('Recent readings, all of them', '', 'binary block, 1507 bytes'),
    'RAM_IND_ID':        ('Recent readings, one index', '', 'binary block'),
    'FLASH24_IND_ID':    ('24 hour log, one index', '', 'binary block'),
    'FLASH24_CUR_IND_ID': ('24 hour log, current index', '', 'plain'),
    'MINMAXMEAN_IND_ID': ('Min/max/mean log, one index', '', 'binary block'),
    'RDLG_DT_ID':        ('Read the log from a date', '', 'binary block'),
    'RDLG_CNT_ID':       ('Records in the regular log', '',
                          'the write index, which is the count until the ring wraps'),
}

GROUPS = [
    ('8.1  Alarm setpoints', ['DP1UAON_ID', 'DP1UAOFF_ID', 'DP1LAOFF_ID', 'DP1LAON_ID',
                              'DP2UAON_ID', 'DP2UAOFF_ID', 'DP2LAOFF_ID', 'DP2LAON_ID',
                              'DP3UAON_ID', 'DP3UAOFF_ID', 'DP3LAOFF_ID', 'DP3LAON_ID',
                              'TMUAON_ID', 'TMUAOFF_ID', 'TMLAOFF_ID', 'TMLAON_ID',
                              'RHUAON_ID', 'RHUAOFF_ID', 'RHLAOFF_ID', 'RHLAON_ID']),
    ('8.2  Recorded extremes', ['DP1MIN_ID', 'DP1MAX_ID', 'DP2MIN_ID', 'DP2MAX_ID',
                                'DP3MIN_ID', 'DP3MAX_ID', 'TMMIN_ID', 'TMMAX_ID',
                                'RHMIN_ID', 'RHMAX_ID']),
    ('8.3  Clock', ['HR_ID', 'MN_ID', 'SEC_ID', 'DT_ID', 'MNTH_ID', 'YR_ID',
                    'DATETIME_ID']),
    ('8.4  Device and identity', ['SFVER_ID', 'DVCID_ID', 'SRNO_ID', 'UBRT_ID',
                                  'TMUNT_ID', 'LOGINTVAL_ID', 'MENB_ID', 'MEAN_HR_ID',
                                  'SET_DPARA_PWD_ID']),
    ('8.5  Alarm behaviour', ['BZRON_ID', 'BZROFF_ID', 'ACK_TIMER_ID', 'ALM_ACK_ID',
                              'ACK_PW_ID', 'RELAY_CNTL_ID']),
    ('8.6  Sensing times', ['DP1_ALM_SENSE_TIME_ID', 'DP2_ALM_SENSE_TIME_ID',
                            'DP3_ALM_SENSE_TIME_ID', 'DOOR_SENSE_TIME_ID',
                            'DOOR_SENSE_POLARITY_ID']),
    ('8.7  Display and communications', ['LCD_BRIGHT_CNT_ID', 'LCD_CONTROL_ID',
                                         'COM_CONTROL_ID', 'AUTO_SENT_INTERVAL_ID',
                                         'XBEE_RST_INTERVAL_ID', 'DEVICES_IN_GROUP_ID']),
    ('8.8  Calibration', ['CAL_FPWD_ID', 'CAL_CPWD_ID', 'CPWD_ID', 'FCPWD_ID',
                          'DP1CAL_ID', 'DP2CAL_ID', 'DP3CAL_ID', 'TMCAL_ID', 'RHCAL_ID',
                          'DP_OFFSET_ID', 'DP_SW_FACT_ID', 'DP_LIMIT_ID',
                          'DP_SLOT_OFFSET_ID', 'DFLT_CAL_ID']),
    ('8.9  Logs and bulk reads', ['RDLG_CNT_ID', 'RDLG_DT_ID', 'RAM_ALL_ID',
                                  'RAM_IND_ID', 'FLASH24_IND_ID', 'FLASH24_CUR_IND_ID',
                                  'MINMAXMEAN_IND_ID', 'REALTIME_VAL_ID']),
    ('8.10  Maintenance', ['EXT_FLASH_ERASE_ID', 'DFLT_RTC_ID', 'BRDSTR_ID',
                           'BRDSTP_ID']),
]

for title, names in GROUPS:
    H(title, 2)
    rows = []
    for n in names:
        rec = BYNAME.get(n)
        if not rec:
            continue
        friendly, unit, note = META.get(n, (n, '', ''))
        acc = ('RW' if rec['read'] and rec['write']
               else 'R' if rec['read'] else 'W' if rec['write'] else '—')
        rows.append([rec['hex'], friendly, unit, acc, MODE_TEXT[rec['mode']], note])
    TABLE(['ID', 'Parameter', 'Unit', 'Access', 'Build', 'Notes'], rows,
          [0.55, 1.9, 0.45, 0.6, 1.0, 2.15])

dead = [r for r in IDS if not r['read'] and not r['write']]
if dead:
    H('8.11  Declared but not implemented', 2)
    P('These identifiers exist in sb_const.h and nothing in the firmware reads or '
      'writes them. The device answers INVALID_PARA for every one. They are listed so '
      'that a host author does not build against them.')
    TABLE(['ID', 'Name'], [[r['hex'], r['name']] for r in dead], [0.8, 5.8])
