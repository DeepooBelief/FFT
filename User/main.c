#include "stm32f10x.h"
#include "Serial.h"
#include "Delay.h"
#include "stdio.h"
#include "AD.h"
#include "stm32_dsp.h"
#include "math.h"
#include "oled.h"


uint32_t lBufOutArray[NPT]; /* FFT 运算的输出数组 */

char code(int x)
{
	return (char)(0xFF << (8 - x));
}

// 定时器1初始化，1s中断一次
void Timer1_Init(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
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

int fpsCount = 0,fps = 0;
// 定时器1中断服务函数，用于计算帧率
void TIM1_UP_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET)
	{
		fps = fpsCount;
		fpsCount = 0;
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
	GPIO_Init(GPIOC, &GPIO_Struct); //点亮PC13，表示程序开始运行
	SerialBegin(); //初始化串口
	I2C_Configuration(); //初始化I2C
	OLED_Init(); //初始化OLED
	OLED_Fill(0x00); //清屏

	NVIC_Configuration(); //初始化DMA中断
	AD_Init(); //初始化ADC
	Timer1_Init(); //初始化定时器1
	TIM3_init(1632, 0); // 44.1kHz ADC sample rate, 72M / 1633
	while (1)
	{
		if (getTrigger())
		{
			#if NPT == 256
			cr4_fft_256_stm32(lBufOutArray, getAD_Value_buf(), NPT);
			#elif NPT == 1024
			cr4_fft_1024_stm32(lBufOutArray, getAD_Value_buf(), NPT);
			#endif
			resetTrigger();
			int i, j;
			int16_t real, imag;
			int magnitude[64];
			// 正常i可以取到NPT/2，但是这里只取到64是因为OLED的只有128列，两列合并一列，所以只取到64
			// i 对应的频率为 i * 44100 / NPT Hz
			for (i = 1; i < 64; i++) 
			{
				real = (lBufOutArray[i] << 16) >> 16;
				imag = (lBufOutArray[i] >> 16);
				magnitude[i] = ((int)sqrt(real * real + imag * imag) >> 3) + 2; //+2是为了让OLED垫高一点,只右移3是因为高频不太明显,正常情况下应该右移5
				// 下面注释掉的才是正确的FFT幅值计算方法
				// float mag;
				// if (i == 0)
				// 	mag = sqrt(real * real + imag * imag);
				// else
				// 	mag = sqrt(real * real + imag * imag) * 2;
			}
			real = (lBufOutArray[0] << 16) >> 16;
			imag = (lBufOutArray[0] >> 16);
			magnitude[0] = (int)sqrt(real * real + imag * imag) >> 6; //4096/64=64，匹配OLDE的高度

			// OLED显示，这里是耗时最多的，FFT计算本身很快
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
			fpsCount++;
			printf("FPS: %d\r\n", fps);
		}
	}
}

