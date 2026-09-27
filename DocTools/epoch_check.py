# -*- coding: utf-8 -*-
"""Check get_epoch_time() against a reference, for every date the RTC can hold.

The firmware's version is the glibc mktime algorithm with TM_YEAR_BASE changed from
the usual 1900 to 2000, which makes the reference year it subtracts from negative
(1970 - 2000 = -30).  That is the part worth checking: the shifts and divisions in
ydhms_diff have to behave correctly for negative years.

Everything below is transcribed from main.c with C semantics preserved:
  - RTCData.year is uint16_t, so `t1.year -= TM_YEAR_BASE` wraps modulo 65536
  - C division truncates toward zero, Python's floors, so `/` is written out
  - SHR(a,b) is an arithmetic right shift on this target, which Python's >> matches
"""
import calendar
import datetime

TM_YEAR_BASE = 2000
EPOCH_YEAR = 1970

MON_YDAY = (
    (0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365),
    (0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366),
)


def c_div(a, b):
    """C integer division: truncates toward zero."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def c_mod(a, b):
    return a - c_div(a, b) * b


def SHR(a, b):
    # #define SHR(a,b) (-1 >> 1 == -1 ? (a) >> (b) : ...)
    # On Cortex-M0 the right shift of a negative int is arithmetic, so the first
    # branch is taken. Python's >> on negative ints floors, which is the same thing.
    return a >> b


def leapyear(year):
    # (year & 3) == 0 && (year % 100 != 0 || ((year / 100) & 3) == (-(TM_YEAR_BASE/100) & 3))
    # C's & and % on negative values follow two's complement / truncation, and
    # Python's & agrees; % is written out via c_mod.
    if (year & 3) != 0:
        return 0
    if c_mod(year, 100) != 0:
        return 1
    rhs = (-(TM_YEAR_BASE // 100)) & 3
    return 1 if (c_div(year, 100) & 3) == rhs else 0


def ydhms_diff(year1, yday1, hour1, min1, sec1, year0, yday0, hour0, min0, sec0):
    a4 = SHR(year1, 2) + SHR(TM_YEAR_BASE, 2) - (0 if (year1 & 3) else 1)
    b4 = SHR(year0, 2) + SHR(TM_YEAR_BASE, 2) - (0 if (year0 & 3) else 1)
    a100 = c_div(a4, 25) - (1 if c_mod(a4, 25) < 0 else 0)
    b100 = c_div(b4, 25) - (1 if c_mod(b4, 25) < 0 else 0)
    a400 = SHR(a100, 2)
    b400 = SHR(b100, 2)
    intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400)

    years = year1 - year0
    days = 365 * years + yday1 - yday0 + intervening_leap_days
    hours = 24 * days + hour1 - hour0
    minutes = 60 * hours + min1 - min0
    seconds = 60 * minutes + sec1 - sec0
    return seconds & 0xFFFFFFFF          # unsigned long, 32 bit on this target


def get_epoch_time(year, month, day, hour, minute, second):
    """year is whatever the caller put in RTCData.year, a uint16_t."""
    year = (year - TM_YEAR_BASE) & 0xFFFF        # uint16_t arithmetic
    month = month - 1

    mon_remainder1 = c_mod(month, 12)
    negative_mon_remainder1 = 1 if mon_remainder1 < 0 else 0
    mon_years1 = c_div(month, 12) - negative_mon_remainder1
    lyear_requested1 = year                       # uint16_t widened to long: never < 0
    year1 = lyear_requested1 + mon_years1

    mon_yday1 = MON_YDAY[leapyear(year1)][mon_remainder1 + 12 * negative_mon_remainder1] - 1
    yday1 = mon_yday1 + day

    return ydhms_diff(year1, yday1, hour, minute, second,
                      EPOCH_YEAR - TM_YEAR_BASE, 0, 0, 0, 0)


def reference(y, mo, d, h, mi, s):
    return calendar.timegm((y, mo, d, h, mi, s, 0, 0, 0))


# ---------------------------------------------------------------- sweep
print("Sweeping every date the RTC can represent (2000-01-01 .. 2099-12-31),")
print("at 00:00:00, 12:34:56 and 23:59:59.")
print()

times = [(0, 0, 0), (12, 34, 56), (23, 59, 59)]
bad = []
n = 0

d = datetime.date(2000, 1, 1)
last = datetime.date(2099, 12, 31)
while d <= last:
    for (h, mi, s) in times:
        got = get_epoch_time(d.year, d.month, d.day, h, mi, s)
        want = reference(d.year, d.month, d.day, h, mi, s)
        n += 1
        if got != (want & 0xFFFFFFFF):
            bad.append((d, h, mi, s, got, want))
    d += datetime.timedelta(days=1)

print("%d conversions checked, %d mismatches" % (n, len(bad)))
for b in bad[:20]:
    print("   %s %02d:%02d:%02d  firmware %d  reference %d  diff %d"
          % (b[0], b[1], b[2], b[3], b[4], b[5], b[4] - b[5]))
if len(bad) > 20:
    print("   ... and %d more" % (len(bad) - 20))

# ---------------------------------------------------------------- leap handling
print()
print("Leap-year handling at the awkward boundaries:")
for y in (2000, 2003, 2004, 2024, 2025, 2096, 2099):
    fw = leapyear(y - TM_YEAR_BASE)
    ref = 1 if calendar.isleap(y) else 0
    flag = "ok  " if fw == ref else "FAIL"
    print("   %s %d  firmware says %s, truth is %s"
          % (flag, y, "leap" if fw else "common", "leap" if ref else "common"))

print()
print("29 February, both directions:")
for y in (2000, 2004, 2024, 2096):
    got = get_epoch_time(y, 2, 29, 12, 0, 0)
    want = reference(y, 2, 29, 12, 0, 0)
    print("   %s %d-02-29 12:00  firmware %d  reference %d"
          % ("ok  " if got == want else "FAIL", y, got, want))

# ---------------------------------------------------------------- what the callers pass
print()
print("The year the caller puts in RTCData.year matters:")
for label, y in [("full year 2026 (what the DATETIME write builds)", 2026),
                 ("two digit 26 (what get_date_time produces)", 26)]:
    got = get_epoch_time(y, 9, 27, 10, 30, 0)
    want = reference(2026, 9, 27, 10, 30, 0)
    if got == want:
        print("   ok    %s -> %d, correct" % (label, got))
    else:
        try:
            as_date = datetime.datetime(1970, 1, 1) + datetime.timedelta(seconds=got)
            shown = as_date.strftime("%Y-%m-%d %H:%M:%S")
        except OverflowError:
            shown = "out of range"
        print("   WRONG %s -> %d  (%s), expected %d" % (label, got, shown, want))

print()
print("Dates before 2000, which RTCData.year cannot express (uint16_t underflow):")
got = get_epoch_time(1999, 12, 31, 23, 59, 59)
print("   1999-12-31 23:59:59 -> %d   reference %d"
      % (got, reference(1999, 12, 31, 23, 59, 59) & 0xFFFFFFFF))
