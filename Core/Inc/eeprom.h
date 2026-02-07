#ifndef __EEPROM_H
#define __EEPROM_H

#include "main.h"

uint8_t EEPROM_IsReady(void);
void EEPROM_Write(uint8_t addr, uint8_t dat);
uint8_t EEPROM_Read(uint8_t addr);

void EEPROM_Write_Delay(uint8_t addr, uint8_t dat);
void EEPROM_Write_Buffer(uint8_t addr, void *pBuffer, uint8_t len);
void EEPROM_Read_Buffer(uint8_t addr, void *pBuffer, uint8_t len);


#endif
