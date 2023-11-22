#include "stm32f10x.h"
#include "Serial.h"
#include "Delay.h"
#include "stdio.h"
#include "AD.h"
#include "stm32_dsp.h"
#include "math.h"
#include "oled.h"

#define NPT 256
uint32_t lBufOutArray[NPT]; /* FFT 运算的输出数组 */
extern uint32_t AD_Value_buf[2][256];
extern uint8_t AD_Value_buf_index;

char code(int x)
{
	return (char)(0xFF << (8 - x));
}

void Timer1_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_Period = 10000 - 1;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM1, ENABLE);
}

int tempi = 0,fps = 0;
void TIM1_UP_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET)
	{
		fps = tempi;
		tempi = 0;
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	}
}

int main(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef GPIO_Struct;
	GPIO_Struct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Struct.GPIO_Pin = GPIO_Pin_13;
	GPIO_Struct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_Struct);
	SerialBegin();
	I2C_Configuration();
	OLED_Init();
	OLED_Fill(0x00);

	NVIC_Configuration();
	AD_Init();
	Timer1_Init();
	TIM3_init(1632, 0); // 44.1kHz ADC sample rate, 72M / 1633
	while (1)
	{
		if (getTrigger())
		{
			uint8_t index = (AD_Value_buf_index + 1) % 2;
			cr4_fft_256_stm32(lBufOutArray, AD_Value_buf[index], NPT);
			resetTrigger();
			int i, j;
			int16_t real, imag;
			int magnitude[64];
			for (i = 1; i < 64; i++)
			{
				real = (lBufOutArray[i] << 16) >> 16;
				imag = (lBufOutArray[i] >> 16);
				magnitude[i] = ((int)sqrt(real * real + imag * imag) >> 3) + 2;
			}
			real = (lBufOutArray[0] << 16) >> 16;
			imag = (lBufOutArray[0] >> 16);
			magnitude[0] = (int)sqrt(real * real + imag * imag) >> 6;
			for (j = 7; j != 0; --j)
			{
				OLED_SetPos(0, j);
				char data;
				uint8_t OLED_DisplayBuf[128] = {0};
				for (i = 0; i < 63; i++)
				{
					if (magnitude[i] > 0)
					{
						if (magnitude[i] > 8)
						{
							magnitude[i] -= 8;
							data = 0xFF;
						}
						else
						{
							data = code(magnitude[i]);
							magnitude[i] = 0;
						}
					}else{
						data = 0x00;
					}
					OLED_DisplayBuf[2*i] = data;
					OLED_DisplayBuf[2*i+1] = data;
				}
				OLED_WriteData(OLED_DisplayBuf, 128);
			}
			tempi++;
			printf("FPS: %d\r\n", fps);
		}
	}
}

