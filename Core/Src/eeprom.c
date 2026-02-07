#include "eeprom.h"
#include "i2c_hal.h"

#define EEPROM_ADDR_W 0xA0
#define EEPROM_ADDR_R 0xA1

uint32_t EEPROM_Last_Tick = 0;

uint8_t EEPROM_IsReady(void) {
    if(HAL_GetTick() - EEPROM_Last_Tick < 5) return 0;
    return 1;
}

void EEPROM_Write(uint8_t addr, uint8_t dat) {
    I2CStart();
    
    I2CSendByte(EEPROM_ADDR_W);
    I2CWaitAck();
    
    I2CSendByte(addr);
    I2CWaitAck();
    
    I2CSendByte(dat);
    I2CWaitAck();
    
    I2CStop();
    
    EEPROM_Last_Tick = HAL_GetTick();
}

uint8_t EEPROM_Read(uint8_t addr) {
    uint32_t wait_start = HAL_GetTick();
    while(EEPROM_IsReady() == 0) {
        if(HAL_GetTick() - wait_start > 10) return 0xFF;
    }
    uint8_t dat;
    
    I2CStart();
    I2CSendByte(EEPROM_ADDR_W);
    I2CWaitAck();
    I2CSendByte(addr);
    I2CWaitAck();
    
    I2CStart();
    I2CSendByte(EEPROM_ADDR_R);
    I2CWaitAck();
    
    dat = I2CReceiveByte();
    I2CSendNotAck();
    I2CStop();
    
    return dat;
}

void EEPROM_Write_Delay(uint8_t addr, uint8_t dat) {
    I2CStart();
    
    I2CSendByte(EEPROM_ADDR_W);
    I2CWaitAck();
    
    I2CSendByte(addr);
    I2CWaitAck();
    
    I2CSendByte(dat);
    I2CWaitAck();
    
    I2CStop();
    
    HAL_Delay(5);
}

void EEPROM_Write_Buffer(uint8_t addr, void *pBuffer, uint8_t len) {
    uint8_t *pDat = (uint8_t *)pBuffer;
    for(int i=0; i<len; i++) {
        EEPROM_Write_Delay(addr+i, pDat[i]);
    }
}

void EEPROM_Read_Buffer(uint8_t addr, void *pBuffer, uint8_t len) {
    uint8_t *pDat = (uint8_t *)pBuffer;
    for(int i=0; i<len; i++) {
        pDat[i] = EEPROM_Read(addr+i);
    }
}
