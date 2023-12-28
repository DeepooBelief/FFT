#ifndef __ADC_H
#define __ADC_H
#include "stm32f10x.h"
#include "stdint.h"

#define NPT 256

uint32_t* getAD_Value_buf(void);
void AD_Init(void);
uint16_t AD_Read(void);
void TIM3_init(u16 arr,u16 psc);
void ADC_DMA_Init(void);
uint8_t getTrigger(void);
void resetTrigger(void);

#endif
