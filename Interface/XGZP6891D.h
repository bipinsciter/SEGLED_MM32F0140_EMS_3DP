#ifndef _SM9543_H
#define _SM9543_H

#include "hal_conf.h"

#define PARA_A 0.000149012
#define PARA_B -625.0000559

void TriggerConvSM9543(uint8_t SensNo);
uint8_t ReadXGZP6891D(uint8_t SensNo, float *value);

#endif
