#ifndef __ENCODER_H
#define __ENCODER_H
#include <stdint.h>

#include "BSP_Common.h"

void ENC_Init(void);      // ≥ı ºªØ GPIO
int ENC_GetDir(void);  // ∑µªÿ 1/-1/0

#endif