#ifndef _SM9543_H
#define _SM9543_H

#include "hal_conf.h"

//---------------------------------------------------------------------------------------
// Transfer function, taken from the "Transfer-function Coefficient" table in the
// datasheet section "Read Pressure":
//
//     Pressure = PARA_A * ADC + PARA_B
//
// where ADC is the plain unsigned 24-bit value built from registers 0x06/0x07/0x08.
// The part fitted here is the -500...+500 Pa row:
//
//     Pressure Range (Pa)   Output AD Span     Transfer-function Coefficient
//     PL       PH           OL        OH       A              B
//     -500     +500         838861    7549746  0.000149012    -625.0000559
//
// Two consequences of that span, both of which this driver depends on:
//
//  * Zero pressure sits at ADC = 2^22 and the calibrated span covers only 5%..45% of
//    the code range, so bit 23 is CLEAR for every in-range reading.  On this part it
//    is NOT a sign bit and must not be masked off - see ReadXGZP6891D().
//  * The straight line remains meaningful outside the calibrated span (it is simply
//    no longer guaranteed accurate), reaching -625 Pa at ADC 0 and +1875 Pa at ADC
//    0xFFFFFF.  That headroom matters: a DP alarm setpoint may be set anywhere up
//    to DP_ALM_LIMIT_MIN (981 Pa), well past the calibrated 500 Pa, so pressures
//    out there have to convert correctly rather than be discarded.
//
// Fitting a different pressure range means taking A and B from the matching row of
// that same table.  Note the table also has a CMH2O section - these two constants are
// from the Pa section, which is the unit the application works in throughout.
//---------------------------------------------------------------------------------------
#define PARA_A 0.000149012
#define PARA_B -625.0000559

void TriggerConvXGZP6891D(uint8_t SensNo);
uint8_t ReadXGZP6891D(uint8_t SensNo, float *value);

#endif
