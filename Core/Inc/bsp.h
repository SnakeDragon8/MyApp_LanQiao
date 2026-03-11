#ifndef __BSP_H
#define __BSP_H

#include "main.h"

#define FILTER_N 8

void LCD_Show(uint8_t Line, char *fmt, ...);
void PWM_Set_Freq_And_Duty(TIM_HandleTypeDef *htim, uint32_t Channel, uint32_t Freq_Hz, uint16_t Duty_Percent);
uint32_t Filter(uint32_t new_value);
double Get_ADC_Vol(ADC_HandleTypeDef *hadc);
void Set_RTC_Time(uint8_t h, uint8_t m, uint8_t s);
void LCD_CacheClear(void);

#endif
