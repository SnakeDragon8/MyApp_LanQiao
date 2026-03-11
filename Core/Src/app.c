#include "app.h"
#include "key.h"
#include "lcd.h"
#include "led.h"
#include "tim.h"
#include "usart.h"
#include "adc.h"
#include "i2c_hal.h"
#include "eeprom.h"
#include "rtc.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "chinese.h"
#include "bsp.h"

SysData_t SysData;
SysData_t SysData_Shadow; // 专门给 EEPROM 用的快照
Msg_t Msg;
State_t State;
volatile Measure_t Measure;

volatile uint16_t adc1_dma_buf[3];

uint8_t rx_buf[RX_BUF_SIZE];
uint8_t rx_flag = 0;

volatile uint8_t run_led = 0x01;

uint8_t SaveReqFlag = 0;
static uint16_t eeprom_idx = 0;

RTC_TimeTypeDef sTime = {0};
RTC_DateTypeDef sDate = {0};

void Task_Key(void);
void Task_Lcd(void);
void Task_Pwm(void);
void Task_Uart(void);
void Task_Eeprom(void);
void Task_RTC(void);

void Request_Save(void);

void Task_Key() {
    switch (State) {
        case SYSD:
            if(Key[0].Single) {
                Key[0].Single = 0;
                State = MEAS;
            }
            break;
        case MEAS:
            if(Key[0].Single) {
                Key[0].Single = 0;
                State = CTRL;
            }
            break;
        case CTRL:
            if(Key[0].Single) {
                Key[0].Single = 0;
                State = SYSD;
            }
            if(Key[1].Single) {
                Key[1].Single = 0;
                SysData.duty += 1;
            }
            if(Key[1].Repeat) {
                Key[1].Repeat = 0;
                SysData.duty += 1;
            }
            if(Key[2].Single) {
                Key[2].Single = 0;
                SysData.freq += 1000;
            }
            if(Key[2].Repeat) {
                Key[2].Repeat = 0;
                SysData.freq += 1000;
            }
            if(Key[3].Long) {
                Key[3].Long = 0;
                Request_Save();
            }
            break;
    }
}

void Task_Pwm() {
    if(SysData.duty > 100) SysData.duty = 0;
    if(SysData.freq > 10000) SysData.freq = 1000;
    if(SysData.freq == 0) SysData.freq = 1000;
    static uint32_t last_freq = 0;
    static uint32_t last_duty = 0;
    if(SysData.freq != last_freq || SysData.duty != last_duty) {
        PWM_Set_Freq_And_Duty(&htim17, TIM_CHANNEL_1, SysData.freq, SysData.duty);
        last_freq = SysData.freq;
        last_duty = SysData.duty;
    } 
}

void Task_Lcd() {
    if(Msg.hint_msg[0] != '\0') {
        if(HAL_GetTick() - Msg.hint_time > 1000) {
            Msg.hint_msg[0] = '\0';
        }
    }
    
    static uint32_t tick = 0;
    if(HAL_GetTick() - tick < 100) return;
    tick = HAL_GetTick();
    
    static State_t lastState = SYSD;
    if(lastState != State) {
        LCD_Clear(Black);
        LCD_CacheClear();
        lastState = State;
    }
    
    switch (State) {
        case SYSD:
            LCD_Show(Line0, "     SystemData");
            LCD_Show(Line2, "Time:%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
            LCD_Show(Line3, "Date:20%02d-%02d-%02d", sDate.Year, sDate.Month, sDate.Date);
            LCD_Show(Line4, "PA7Duty: %d%%      ", SysData.duty);
            LCD_Show(Line5, "PA7Freq: %dHz      ", SysData.freq);
            LCD_Show(Line6, "CoreTemp:%.1fC  ", Measure.r38[1]);
            LCD_Show(Line7, "IsAlarm:%d ", SysData.is_alarm);
            LCD_Show(Line8, "Boot:%d    ", SysData.boot_count);
            break;
        case MEAS:
            LCD_Show(Line0, "     MeasureData");
            LCD_Show(Line2, "PA6Freq: %dHz      ", Measure.pa6_freq);
            LCD_Show(Line3, "PA15Freq: %.1fHz   ", Measure.pa15_freq);
            LCD_Show(Line4, "R37:%.2fV", Measure.r37);
            LCD_Show(Line5, "R38:%.2fV Vdda:%.2fV", Measure.r38[0], Measure.r38[2]);
            break;
        case CTRL:
            LCD_Show(Line0, "     SystemCtrl");
            LCD_Show(Line2, "PA7Duty: %d%%      ", SysData.duty);
            LCD_Show(Line3, "PA7Freq: %dHz      ", SysData.freq);
            break;
    }

    LCD_Show(Line9, "%-20s", Msg.hint_msg);
}

void Task_Uart() {
    if(rx_flag == 1) {
        strncpy(Msg.hint_msg, (char *)rx_buf, sizeof(Msg.hint_msg) - 1);
        Msg.hint_msg[sizeof(Msg.hint_msg) - 1] = '\0';
        Msg.hint_time = HAL_GetTick();
        rx_flag = 0;
    }
}

void Task_Adc() {
    static uint32_t tick = 0;
    if(HAL_GetTick() - tick < 200) return;
    tick = HAL_GetTick();
    
    Measure.r37 = Get_ADC_Vol(&hadc2);
    
    Measure.r38[0] = (adc1_dma_buf[0] / 16.0 * 3.3) / 4095.0;      // 电压
    Measure.r38[2] = (1.212 * 4095.0) / adc1_dma_buf[2] * 16.0;    // 电源电压Vdda
    #define TS_CAL1_ADDR ((uint16_t*) ((uint32_t)0x1FFF75A8)) // 30°C 校准值地址
    #define TS_CAL2_ADDR ((uint16_t*) ((uint32_t)0x1FFF75CA)) // 110°C 校准值地址
    uint16_t ts_cal1 = *TS_CAL1_ADDR;
    uint16_t ts_cal2 = *TS_CAL2_ADDR;
    
    uint16_t filtedTemp = (uint16_t)Filter((uint32_t)adc1_dma_buf[1]);
    double raw_temp_3v = (double)filtedTemp / 16.0 * Measure.r38[2] / 3.0;
    // 线性插值公式计算温度
    // Temp = 30 + (110 - 30) * (raw_temp - ts_cal1) / (ts_cal2 - ts_cal1)
    // 芯片温度
    Measure.r38[1] = 30.0f + (110.0f - 30.0f) * (raw_temp_3v - ts_cal1) / (ts_cal2 - ts_cal1);
    
    if(Measure.r38[0] >= 0.5f && Measure.r38[0] <= 3.0f) {
        SysData.is_alarm = 0; // 只有正常时才复位
    } else {
        SysData.is_alarm = 1; // 越界时强制置位 (双重保险，配合看门狗中断)
    }
}

void Task_Eeprom() {
    if(SaveReqFlag == 0) return;
    if(EEPROM_IsReady() == 0) return;

    uint8_t *pData = (uint8_t *)&SysData_Shadow;
    uint8_t val = pData[eeprom_idx];
    uint8_t addr = 0x01 + eeprom_idx;
    uint8_t old_val = EEPROM_Read(addr);
    if(old_val != val) {
        EEPROM_Write(addr, val);
    }
    
    eeprom_idx++;
    
    if(eeprom_idx >= sizeof(SysData)) {
        eeprom_idx = 0;
        SaveReqFlag = 0;
        
        sprintf(Msg.hint_msg, "Save Done!");
        Msg.hint_time = HAL_GetTick();
    }
}

void Task_RTC() {
    static uint32_t tick = 0;
    if(HAL_GetTick() - tick < 500) return;
    tick = HAL_GetTick();
    
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

void App_Init() {
    HAL_Delay(50);
    LED_Disp(0x00);
    Key_Init();
    HAL_Delay(50);
    LCD_Init();
    LCD_Clear(Black);
    LCD_SetBackColor(Black);
    LCD_SetTextColor(White);
    
    memset(&State, 0, sizeof(State));
    I2CInit();
    if(EEPROM_Read(0x00) != 0xA5) {
        memset(&SysData, 0, sizeof(SysData));
        SysData.duty = 50;
        SysData.freq = 1000;
        SysData.boot_count = 1;
        EEPROM_Write_Buffer(0x01, &SysData, sizeof(SysData));
        EEPROM_Write_Delay(0x00, 0xA5);
    } else {
        EEPROM_Read_Buffer(0x01, &SysData, sizeof(SysData));
        SysData.boot_count++;
        EEPROM_Write_Buffer(0x01, &SysData, sizeof(SysData));
    }
    
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
    
    HAL_TIM_IC_Start_IT(&htim16, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
    
    HAL_TIM_Base_Start_IT(&htim4);
    
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, RX_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1_dma_buf, 3);
    HAL_TIM_Base_Start(&htim6);
    HAL_TIM_Base_Start_IT(&htim7);
    
    printf("Hello World\r\n");
    LCD_Show_Chinese(Line7, 320, White, Black);
}

void App_Loop() {
    Task_Key();
    Task_Lcd();
    Task_Pwm();
    Task_Uart();
    Task_Adc();
    Task_Eeprom();
    Task_RTC();
}

void Request_Save(void) {
    if(SaveReqFlag == 0) {
        SysData_Shadow = SysData;
        eeprom_idx = 0;
        SaveReqFlag = 1;
    }
}

