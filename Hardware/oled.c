#include "oled.h"
#include "stm32f10x.h"
#include "codetab.h"
#include "Delay.h"

//DMA部分参考了https://blog.csdn.net/weixin_45911959/article/details/133717957

// 初始化硬件IIC引脚
// SCL ---> PB6
// SDA ---> PB7
// PB4和PB5是被配置成了输出模式,用来给OLED供电的
// 如果PB6和PB7不是IIC功能的话,需要自己修改一下
// 如果你使用的是STM32F103C8T6开发板和4线IIC OLED的话,可以把OLED直接插在开发板的PB4,PB5,PB6,PB7上
void I2C_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	I2C_InitTypeDef I2C_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_NoJTRST, ENABLE);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_5);
	GPIO_ResetBits(GPIOB, GPIO_Pin_4);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

	// PB6——SCL PB7——SDA
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOB, &GPIO_InitStructure);

	I2C_DeInit(I2C1);
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = 400000;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_OwnAddress1 = 0x30;

	I2C_Init(I2C1, &I2C_InitStructure);
	I2C_Cmd(I2C1, ENABLE);
	I2C_DMACmd(I2C1, ENABLE); // 使能I2C1的DMA传输
}

void I2C_DMA_Transmit_InitConfig(uint32_t N, uint32_t Address)
{
	DMA_DeInit(DMA1_Channel6);

	DMA_InitTypeDef DMA_InitStructer;
	DMA_InitStructer.DMA_PeripheralBaseAddr = (uint32_t)&I2C1->DR; // 注意这里取地址不要忘记加上 & 符号
	DMA_InitStructer.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructer.DMA_PeripheralInc = DMA_PeripheralInc_Disable; // DR寄存器位置固定，就那一个，所以不能使得指针传输完移动

	DMA_InitStructer.DMA_MemoryBaseAddr = Address;
	DMA_InitStructer.DMA_MemoryDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructer.DMA_MemoryInc = DMA_MemoryInc_Enable; // 数组这边的指针需要不断自增，这样才能将数据轮流发送出去，而不是只发数组中的第一个

	DMA_InitStructer.DMA_Mode = DMA_Mode_Normal;	  // DMA设定为正常模式
	DMA_InitStructer.DMA_DIR = DMA_DIR_PeripheralDST; // 发送数据到外设
	DMA_InitStructer.DMA_M2M = DMA_M2M_Disable;		  // 注意不适用软件触发

	DMA_InitStructer.DMA_BufferSize = N;
	DMA_InitStructer.DMA_Priority = DMA_Priority_VeryHigh;
	DMA_Init(DMA1_Channel6, &DMA_InitStructer);

	NVIC_InitTypeDef NVIC_InitStructure;
	// 中断优先级NVIC设置
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel6_IRQn;  // DMA中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 先占优先级1级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		  // 从优先级1级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			  // IRQ通道被使能
	NVIC_Init(&NVIC_InitStructure);							  // 初始化NVIC寄存器

	/* 启用DMA Channel6完全传输中断 */
	DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);
}

void OLED_WriteData_DMA(uint8_t *Data, uint8_t N)
{
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
		;

	I2C_GenerateSTART(I2C1, ENABLE); // 产生I2C开始信号
	while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
		;

	I2C_Send7bitAddress(I2C1, OLED_ADDRESS, I2C_Direction_Transmitter); // 发送I2C从机地址
	while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
		;

	DMA_Cmd(DMA1_Channel6, DISABLE);
	DMA_SetCurrDataCounter(DMA1_Channel6, N);
	DMA_Cmd(DMA1_Channel6, ENABLE);
}

// 向OLED寄存器地址写一个byte的数据
void I2C_WriteByte(uint8_t addr, uint8_t data)
{
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
		;
	I2C_GenerateSTART(I2C1, ENABLE);

	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
		;
	I2C_Send7bitAddress(I2C1, OLED_ADDRESS, I2C_Direction_Transmitter);

	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
		;
	I2C_SendData(I2C1, addr);

	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
		;
	I2C_SendData(I2C1, data);

	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
		;
	I2C_GenerateSTOP(I2C1, ENABLE);
}

// 写指令
void WriteCmd(unsigned char I2C_Command)
{
	I2C_WriteByte(0x00, I2C_Command);
}

// 写数据
void WriteData(unsigned char I2C_Data)
{
	I2C_WriteByte(0x40, I2C_Data);
}

void OLED_WriteData(uint8_t *Data, uint8_t N)
{
	I2C_GenerateSTART(I2C1, ENABLE); // 产生I2C开始信号
	while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
		;

	I2C_Send7bitAddress(I2C1, OLED_ADDRESS, I2C_Direction_Transmitter); // 发送I2C从机地址
	while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
		;

	I2C_SendData(I2C1, 0x40); // 发送寄存器地址

	do
	{
		while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING) != SUCCESS)
			;
		I2C_SendData(I2C1, *Data++); // 发送数据
	} while (--N);

	while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
		;

	I2C_GenerateSTOP(I2C1, ENABLE); // 产生I2C停止信号
}

void OLED_WriteData_Repeat(uint8_t Data, uint8_t N)
{
	I2C_GenerateSTART(I2C1, ENABLE); // 产生I2C开始信号
	while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
		;

	I2C_Send7bitAddress(I2C1, OLED_ADDRESS, I2C_Direction_Transmitter); // 发送I2C从机地址
	while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
		;

	I2C_SendData(I2C1, 0x40); // 发送寄存器地址

	do
	{
		while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING) != SUCCESS)
			;
		I2C_SendData(I2C1, Data); // 发送数据
	} while (--N);

	while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
		;

	I2C_GenerateSTOP(I2C1, ENABLE); // 产生I2C停止信号
}

// 厂家初始化代码
void OLED_Init(void)
{
	Delay_ms(100);
	WriteCmd(0xAE); // display off
	WriteCmd(0x20); // Set Memory Addressing Mode
	WriteCmd(0x10); // 00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	WriteCmd(0xb0); // Set Page Start Address for Page Addressing Mode,0-7
	WriteCmd(0xc8); // Set COM Output Scan Direction
	WriteCmd(0x00); //---set low column address
	WriteCmd(0x10); //---set high column address
	WriteCmd(0x40); //--set start line address
	WriteCmd(0x81); //--set contrast control register
	WriteCmd(0xff); // áá?èμ÷?ú 0x00~0xff
	WriteCmd(0xa1); //--set segment re-map 0 to 127
	WriteCmd(0xa6); //--set normal display
	WriteCmd(0xa8); //--set multiplex ratio(1 to 64)
	WriteCmd(0x3F); //
	WriteCmd(0xa4); // 0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	WriteCmd(0xd3); //-set display offset
	WriteCmd(0x00); //-not offset
	WriteCmd(0xd5); //--set display clock divide ratio/oscillator frequency
	WriteCmd(0xf0); //--set divide ratio
	WriteCmd(0xd9); //--set pre-charge period
	WriteCmd(0x22); //
	WriteCmd(0xda); //--set com pins hardware configuration
	WriteCmd(0x12);
	WriteCmd(0xdb); //--set vcomh
	WriteCmd(0x20); // 0x20,0.77xVcc
	WriteCmd(0x8d); //--set DC-DC enable
	WriteCmd(0x14); //
	WriteCmd(0xaf); //--turn on oled panel
}

// 设置光标起始坐标（x,y）
void OLED_SetPos(unsigned char x, unsigned char y)
{
	WriteCmd(0xb0 + y);
	WriteCmd((x & 0xf0) >> 4 | 0x10);
	WriteCmd((x & 0x0f) | 0x01);
}

// 填充整个屏幕
void OLED_Fill(unsigned char Fill_Data)
{
	unsigned char m;

	for (m = 0; m < 8; m++)
	{
		WriteCmd(0xb0 + m);
		WriteCmd(0x00);
		WriteCmd(0x10);

		OLED_WriteData_Repeat(Fill_Data, 128);
	}
}

// 清屏
void OLED_CLS(void)
{
	OLED_Fill(0x00);
}

// 将OLED从休眠中唤醒
void OLED_ON(void)
{
	WriteCmd(0xAF);
	WriteCmd(0x8D);
	WriteCmd(0x14);
}

// 让OLED休眠 -- 休眠模式下,OLED功耗不到10uA
void OLED_OFF(void)
{
	WriteCmd(0xAE);
	WriteCmd(0x8D);
	WriteCmd(0x10);
}

void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize)
{
	unsigned char c = 0, i = 0, j = 0;

	switch (TextSize)
	{
	case 1:
	{
		while (ch[j] != '\0')
		{
			c = ch[j] - 32;
			if (x > 126)
			{
				x = 0;
				y++;
			}

			OLED_SetPos(x, y);

			for (i = 0; i < 6; i++)
			{
				WriteData(F6x8[c][i]);
			}
			x += 6;
			j++;
		}
	}
	break;

	case 2:
	{
		while (ch[j] != '\0')
		{
			c = ch[j] - 32;

			if (x > 120)
			{
				x = 0;
				y++;
			}

			OLED_SetPos(x, y);

			for (i = 0; i < 8; i++)
			{
				WriteData(F8X16[c * 16 + i]);
			}

			OLED_SetPos(x, y + 1);

			for (i = 0; i < 8; i++)
			{
				WriteData(F8X16[c * 16 + i + 8]);
			}
			x += 8;
			j++;
		}
	}
	break;
	}
}

void OLED_ShowBMP(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char BMP[])
{
	unsigned char x, y;
	unsigned int j = 0;

	if (y1 % 8 == 0)
	{
		y = y1 / 8;
	}
	else
	{
		y = y1 / 8 + 1;
	}

	for (y = y0; y < y1; y++)
	{
		OLED_SetPos(x0, y);

		for (x = x0; x < x1; x++)
		{
			WriteData(BMP[j++]);
		}
	}
}

void DMA1_Channel6_IRQHandler(void)
{
	if (DMA_GetITStatus(DMA1_IT_TC6) != RESET)
	{
		DMA_ClearITPendingBit(DMA1_IT_TC6);
		I2C_GenerateSTOP(I2C1, ENABLE); // 关闭I2C1总线
	}
}
