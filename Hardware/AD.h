#ifndef __ADC_H
#define __ADC_H

#include "stdint.h"

void AD_Init(void);
uint16_t AD_Read(void);
void TIM3_init(u16 arr,u16 psc);
void NVIC_Configuration(void);
uint8_t getTrigger(void);
void resetTrigger(void);

#endif
