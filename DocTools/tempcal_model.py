# -*- coding: utf-8 -*-
"""Model the temperature calibration path, as it is and as it should be.

Established from the firmware:

  temperatureC  = sensor reading in C
  temperatureC -= TM_Cal_float_Value_F      <- factory correction, subtracted in C
  temperatureC -= TM_Cal_float_Value_C      <- customer correction, subtracted in C
  if (TM_Unit) temperatureF = temperatureC * 1.8 + 32

So both corrections are stored in TENTHS OF DEGREES CELSIUS, whatever the display
unit happens to be. A correction is a DIFFERENCE, so converting one between scales is
a span conversion - divide or multiply by 1.8, never add or subtract 32.

The host sends the reference value in whatever unit is being displayed, because the C
branch compares it against RealtemperatureC*10.
"""


def clamp_i16(x):
    x = int(x)
    return max(-32768, min(32767, x))


# ---------------------------------------------------------------- as it is now
def factory_cal_now(real_c, reference_tenths, tm_unit):
    """main.c case TMCAL_ID, bool_FactoryCalibrationOn branch."""
    if not tm_unit:
        ss1 = real_c * 10.0
        stored = clamp_i16(ss1 - reference_tenths)
        ram = stored
    else:
        real_f = real_c * 1.8 + 32.0
        ss1 = real_f * 10.0
        v = clamp_i16(ss1 - reference_tenths)
        v = clamp_i16(v * 1.8 + 32.0)        # <- offset applied to a difference
        stored = v
        ram = clamp_i16((v - 320) / 1.8)     # <- and undone with a different constant
    return stored, ram


def boot_now(stored_f, stored_c, tm_unit):
    """main.c boot: the Fahrenheit branch converts the stored corrections."""
    if tm_unit:
        return clamp_i16(stored_f * 1.8 + 32.0), clamp_i16(stored_c * 1.8 + 32.0)
    return stored_f, stored_c


def unit_change_now(cal_f, cal_c, to_fahrenheit):
    """main.c TMUnitChange(): the corrections get the setpoints' absolute conversion."""
    if to_fahrenheit:
        return clamp_i16(cal_f * 1.8 + 320), clamp_i16(cal_c * 1.8 + 320)
    return clamp_i16((cal_f - 320) / 1.8), clamp_i16((cal_c - 320) / 1.8)


# ---------------------------------------------------------------- as it should be
def factory_cal_fixed(real_c, reference_tenths, tm_unit):
    if not tm_unit:
        ss1 = real_c * 10.0
        v = clamp_i16(ss1 - reference_tenths)          # tenths C
    else:
        real_f = real_c * 1.8 + 32.0
        ss1 = real_f * 10.0
        v = clamp_i16((ss1 - reference_tenths) / 1.8)  # tenths F span -> tenths C
    return v, v


def boot_fixed(stored_f, stored_c, tm_unit):
    return stored_f, stored_c          # stored in C already; nothing to convert


def unit_change_fixed(cal_f, cal_c, to_fahrenheit):
    return cal_f, cal_c                # a correction does not depend on the display unit


# ---------------------------------------------------------------- scenarios
def shown(real_c, cal_f, cal_c, tm_unit):
    """What the device ends up displaying."""
    t = real_c - cal_f / 10.0 - cal_c / 10.0
    return (t * 1.8 + 32.0) if tm_unit else t


def report(title, real_c, reference, tm_unit):
    unit = "F" if tm_unit else "C"
    print()
    print("=== %s" % title)
    print("    sensor reads %.1f C  (%.1f F);  operator enters %.1f %s as the truth"
          % (real_c, real_c * 1.8 + 32.0, reference / 10.0, unit))

    s_now, r_now = factory_cal_now(real_c, reference, tm_unit)
    s_fix, r_fix = factory_cal_fixed(real_c, reference, tm_unit)

    ideal = shown(real_c, *(0, 0), tm_unit=tm_unit)
    want = reference / 10.0

    print("      %-9s stored %6d   in RAM %6d   display becomes %8.2f %s"
          % ("now:", s_now, r_now, shown(real_c, r_now, 0, tm_unit), unit))
    print("      %-9s stored %6d   in RAM %6d   display becomes %8.2f %s"
          % ("fixed:", s_fix, r_fix, shown(real_c, r_fix, 0, tm_unit), unit))
    print("      the operator asked for %.2f %s" % (want, unit))

    # after a power cycle the stored value is reloaded
    b_now = boot_now(s_now, 0, tm_unit)[0]
    b_fix = boot_fixed(s_fix, 0, tm_unit)[0]
    print("      after a reboot:   now %8.2f %s        fixed %8.2f %s"
          % (shown(real_c, b_now, 0, tm_unit), unit,
             shown(real_c, b_fix, 0, tm_unit), unit))


report("Calibrating in Fahrenheit, reference matches the sensor exactly",
       25.0, 770, 1)
report("Calibrating in Fahrenheit, sensor reads 1.8 F high",
       25.0, 752, 1)
report("Calibrating in Celsius (for comparison - this path is already right)",
       25.0, 250, 0)

print()
print("=== Switching the display unit with a correction already stored")
print("    A correction of 0.5 C is held. Changing the unit must not alter it.")
for to_f in (True, False):
    n = unit_change_now(5, 0, to_f)[0]
    f = unit_change_fixed(5, 0, to_f)[0]
    print("      to %s:  now %6d tenths (%.1f C)   fixed %6d tenths (%.1f C)"
          % ("F" if to_f else "C", n, n / 10.0, f, f / 10.0))

print()
print("=== Rebooting in Fahrenheit with a correction of 0.5 C stored")
n = boot_now(5, 0, 1)[0]
f = boot_fixed(5, 0, 1)[0]
print("      now   %6d tenths  (%.1f C applied)" % (n, n / 10.0))
print("      fixed %6d tenths  (%.1f C applied)" % (f, f / 10.0))
