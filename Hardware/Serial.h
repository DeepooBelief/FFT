#ifndef __SERIAL_H
#define __SERIAL_H
#include "stdint.h"

void SerialBegin(void);
void SerialWrite(uint8_t data);
void SerialWriteN(uint8_t *pdata, uint8_t len);
uint8_t Serialavailable(void);
uint8_t SerialRead(void);
void SerialFlush(void);
void SerialReadString(char *str);
void SerialReadStringUntil(char *str, char terminator);

#endif
