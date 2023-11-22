#include "stm32f10x.h"
#include "stdio.h"

#define RX_BUF_SIZE 64
uint8_t rx_buffer[RX_BUF_SIZE];
volatile uint8_t RX_buffer_head = 0;
volatile uint8_t rx_buffer_tail = 0;

void SerialBegin(){
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	USART_InitTypeDef USART_InitStruct;
	USART_InitStruct.USART_BaudRate = 115200;
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStruct.USART_Mode =  USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStruct.USART_Parity = USART_Parity_No;
	USART_InitStruct.USART_StopBits = USART_StopBits_1;
	USART_Init(USART1, &USART_InitStruct);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStruct);
	
	USART_Cmd(USART1, ENABLE);
}

void SerialWrite(uint8_t data){
	USART_SendData(USART1, data);
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) != SET);
}

void SerialWriteN(uint8_t *pdata, uint8_t len){
	while(len--){
		SerialWrite(*pdata++);
	}
}

int fputc(int ch, FILE *f){
	SerialWrite((u8)ch);
	return ch;
}

uint8_t Serialavailable(){
	return (RX_buffer_head - rx_buffer_tail + RX_BUF_SIZE) % RX_BUF_SIZE;
}

uint8_t SerialRead(){
	if (RX_buffer_head == rx_buffer_tail) {
		return 0;
	}
	uint8_t result = rx_buffer[rx_buffer_tail];
	rx_buffer_tail = (rx_buffer_tail + 1) % RX_BUF_SIZE;
	return result;
}

void SerialFlush(){
	rx_buffer_tail = RX_buffer_head;
}

void SerialReadString(char *str){
	uint8_t i = 0;
	while(Serialavailable()){
		str[i++] = SerialRead();
	}
	str[i] = '\0';
}

void SerialReadStringUntil(char *str, char terminator){
	uint8_t i = 0;
	while(1){
		while(!Serialavailable());
		str[i] = SerialRead();
		if(str[i] == terminator){
			break;
		}
		i++;
	}
	str[i] = '\0';
}

int fgetc(FILE *f){
	while(!Serialavailable());
	return SerialRead();
}

void USART1_IRQHandler(){
	char RX_data;
	if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET){
		RX_data = USART_ReceiveData(USART1);
		if((RX_buffer_head + 1) % RX_BUF_SIZE != rx_buffer_tail){
			rx_buffer[RX_buffer_head] = RX_data;
			RX_buffer_head = (RX_buffer_head + 1) % RX_BUF_SIZE;
		}
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}
