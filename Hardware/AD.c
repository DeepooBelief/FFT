#include "AD.h"
#include <stddef.h>

static uint32_t AD_Value_buf[2][NPT]; // 双缓冲
static DMA_InitTypeDef DMA_InitStructure;
static uint8_t AD_Value_buf_index = 0; // 当前使用的缓冲区
static uint8_t trigger = 0;

uint8_t getTrigger(){ // 用于判断是否有新数据进行FFT
    return trigger;
}

uint32_t* getAD_Value_buf(){ // 用于获取当前使用的缓冲区
    size_t index = (AD_Value_buf_index + 1) % 2;
    return AD_Value_buf[index];
}

void resetTrigger(){ // 用于清除触发标志
    trigger = 0;
}

// DMA中断优先级设置
void NVIC_Configuration(void){
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1; // 0
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void AD_Init(){
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6); // 72/6 = 12MHz

    //配置ADC1的通道1为239.5个采样周期, 12MHz / 239.5 = 50.125KHz，大于定时器的44.1KHz
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);

    //单通道,非扫描模式,外部触发,数据右对齐
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);

    //DMA配置
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)AD_Value_buf[AD_Value_buf_index];
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = NPT;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    //校准
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1) == SET);
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1) == SET);
}

// 用TIM3作为ADC的外部触发源
void TIM3_init(u16 arr,u16 psc){
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_InternalClockConfig(TIM3);
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
    TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);
    TIM_Cmd(TIM3, ENABLE);
}

// void TIM2_init(){
//     RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
//     TIM_ITRxExternalClockConfig(TIM2, TIM_TS_ITR2);
//     TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_External1);
//     TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
//     TIM_TimeBaseInitStructure.TIM_Period = 255;
//     TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
//     TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
//     TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
//     TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
//     TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
//     TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
//     NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
//     NVIC_InitTypeDef NVIC_InitStructure;
//     NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
//     NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
//     NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
//     NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//     NVIC_Init(&NVIC_InitStructure);
//     TIM_Cmd(TIM2, ENABLE);

// }

uint16_t AD_Read(){
    return ADC_GetConversionValue(ADC1);
}

// 当DMA1_Channel1传输完成时,会触发一次中断
// 在中断中,切换DMA的内存地址,并重新使能DMA
// 这样就可以实现双缓冲
void DMA1_Channel1_IRQHandler(void){
    if(DMA_GetITStatus(DMA1_IT_TC1) != RESET){
        DMA_Cmd(DMA1_Channel1, DISABLE);
        AD_Value_buf_index = (AD_Value_buf_index + 1) % 2;
        DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)AD_Value_buf[AD_Value_buf_index];
        DMA_Init(DMA1_Channel1, &DMA_InitStructure);
        DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
        DMA_Cmd(DMA1_Channel1, ENABLE);
        trigger = 1;
        DMA_ClearITPendingBit(DMA1_IT_TC1);
    }
}
