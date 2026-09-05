/***********************************************************************************************\
 * File Name:	pressure_sensor.h																*
 *																								*
 * Selects the mounted differential pressure sensor and pulls in its driver header.				*
 * Set PRESSURE_SENSOR_PART in sb_const.h to match the part fitted on the board.					*
 *																								*
 * Application code includes this header and uses DP_TriggerConv() / DP_ReadPressure(),			*
 * so it never names a part directly.															*
\***********************************************************************************************/

#ifndef PRESSURE_SENSOR_H_
#define PRESSURE_SENSOR_H_

#include "..\sb_const.h"

#if (PRESSURE_SENSOR_PART == PRESSURE_SENSOR_WF200DP)
	#include "WF200DP.h"
#else
	#include "XGZP6891D.h"
#endif

//-------------------------------------------------------------------------------------
// Part-neutral wrappers
//
// Both drivers are two-phase: trigger a conversion on one pass of the sample loop, read
// the result on the next.  DP_ReadPressure() returns 0 on success and non-zero if the
// sensor did not respond.
//-------------------------------------------------------------------------------------
#if (PRESSURE_SENSOR_PART == PRESSURE_SENSOR_WF200DP)

	#define DP_TriggerConv(n)			TriggerConvWF200DP(n)
	#define DP_ReadPressure(n,v)		ReadWF200DP((n),(v))

	//Sensor full scale, in Pa.  Used for range checking only - the drivers apply their
	//own scaling internally.
	#define DP_SENSOR_FULL_SCALE_PA		WF200DP_FULL_SCALE_PA

#else

	#define DP_TriggerConv(n)			TriggerConvSM9543(n)
	#define DP_ReadPressure(n,v)		ReadXGZP6891D((n),(v))

	//XGZP6891D as fitted: PARA_A * 2^23 = 1250 Pa span, offset by PARA_B -> +/-625 Pa
	#define DP_SENSOR_FULL_SCALE_PA		625.0f

#endif

#endif /* PRESSURE_SENSOR_H_ */
