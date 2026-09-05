/***********************************************************************************************\
 * File Name:	WF200DP.h																		*
 *																								*
 * Weifengheng WF200DPZ0.005BGS16DT - digital differential pressure sensor, I2C.				*
 * Alternative pressure sensor to the XGZP6891D, selected by PRESSURE_SENSOR_PART in			*
 * sb_const.h.																					*
 *																								*
 * The two parts share the SAME register map and command set (CMD at 0x30, pressure at			*
 * 0x06..0x08, temperature at 0x09..0x0A), so the transaction sequence is identical.				*
 * They differ in three ways that matter:														*
 *																								*
 *   1. I2C address   XGZP6891D 0xFE/0xFF (7-bit 0x7F)											*
 *                    WF200DP   0xDA/0xDB (7-bit 0x6D), LSB set by the SDO/ADDR pin				*
 *																								*
 *   2. Full scale    XGZP6891D as fitted: +/-625 Pa											*
 *                    WF200DP  WF200DPZ0.005B = 0.005 bar = +/-500 Pa							*
 *																								*
 *   3. Data format   XGZP6891D driver treats the reading as offset binary over 23 bits			*
 *                    WF200DP is two's complement over 24 bits, zero at 0x800000					*
 *																								*
 * Datasheet: WF200DP Series, Low-Power High-Resolution Pressure Sensor (wfsensors.com)			*
\***********************************************************************************************/

#ifndef WF200DP_H_
#define WF200DP_H_

#include "hal_conf.h"

//-------------------------------------------------------------------------------------
// I2C device address (8-bit, R/W bit included)
//
// The datasheet gives the default 8-bit write address as 11011010b = 0xDA, i.e. 7-bit
// 0x6D.  The LSB of the 7-bit address follows the SDO/ADDR pin, so tying it low gives
// 7-bit 0x6C -> 0xD8 / 0xD9.  Change these two lines if the boards strap ADDR low.
//-------------------------------------------------------------------------------------
#define WF200DP_I2C_ADDR_W		0xDA
#define WF200DP_I2C_ADDR_R		(WF200DP_I2C_ADDR_W | 0x01)

//-------------------------------------------------------------------------------------
// Register map
//-------------------------------------------------------------------------------------
#define WF200DP_REG_PRESSURE_MSB	0x06		// Data out <23:16>
#define WF200DP_REG_PRESSURE_CSB	0x07		// Data out <15:8>
#define WF200DP_REG_PRESSURE_LSB	0x08		// Data out <7:0>
#define WF200DP_REG_TEMP_MSB		0x09		// Temp out <15:8>
#define WF200DP_REG_TEMP_LSB		0x0A		// Temp out <7:0>
#define WF200DP_REG_CMD				0x30		// Measurement_control<3:0>

//-------------------------------------------------------------------------------------
// Measurement_control<3:0> values
//-------------------------------------------------------------------------------------
#define WF200DP_CMD_TEMP			0x08		// single-shot temperature conversion
#define WF200DP_CMD_PRESSURE		0x09		// single-shot sensor conversion
#define WF200DP_CMD_COMBINED		0x0A		// temperature then sensor, back to back

//-------------------------------------------------------------------------------------
// Scaling
//
// The 24-bit reading is two's complement with zero at 0x800000, normalised to -1.0 .. +1.0
// by dividing by 8388608, then multiplied by the part's full-scale pressure.
//
// NOTE: the datasheet says the pressure conversion "needs to be converted according to
// the pressure range" and recommends the vendor's own C code.  The linear form below is
// the documented one; if WFH supply a compensated routine, it drops in here.
//-------------------------------------------------------------------------------------
#define WF200DP_ZERO_COUNT			8388608.0f	// 2^23
#define WF200DP_FULL_SCALE_PA		500.0f		// WF200DPZ0.005B -> 0.005 bar -> 500 Pa

//-------------------------------------------------------------------------------------
// API - deliberately the same shape as XGZP6891D.h so the part can be swapped by
// changing PRESSURE_SENSOR_PART alone.
//
// Use is two-phase, matching the existing application flow: trigger a conversion on one
// pass of the sample loop, read the result on the next.
//-------------------------------------------------------------------------------------
void TriggerConvWF200DP(uint8_t SensNo);

/// Returns 0 on success (*value = pressure in Pa), non-zero if the sensor did not
/// respond or returned an invalid reading.
uint8_t ReadWF200DP(uint8_t SensNo, float *value);

#endif /* WF200DP_H_ */
