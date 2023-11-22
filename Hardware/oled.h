#ifndef _oled_h_
#define _oled_h_

#include "stm32f10x.h"
#define OLED_ADDRESS 0x78

void I2C_Configuration(void);
void I2C_WriteByte(uint8_t addr,uint8_t data);
void WriteCmd(unsigned char I2C_Command);
void WriteData(unsigned char I2C_Data);
void OLED_Init(void);
void OLED_SetPos(unsigned char x,unsigned char y);
void OLED_Fill(unsigned char Fill_Data);
void OLED_CLS(void);
void OLED_ON(void);
void OLED_OFF(void);
void OLED_ShowStr(unsigned char x,unsigned char y,unsigned char ch[],unsigned char TextSize);
void OLED_ShowBMP(unsigned char x0,unsigned char y0,unsigned char x1,unsigned char y1,unsigned char BMP[]);
void OLED_WriteData(uint8_t *Data, uint8_t N);
void OLED_WriteData_Repeat(uint8_t Data, uint8_t N);
#endif

