#include "app.h"
#include "key.h"
#include "lcd.h"
#include "led.h"
#include "tim.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

SysData_t SysData;
DispState_t DispState;
static char LCD_Cache[10][21];

void Task_Key(void);
void Task_Lcd(void);
void Task_Pwm(void);

void Key_B1(void);
void Key_B2(void);
void Key_B3(void);
void Key_B4(void);

void LCD_Show(uint8_t Line, char *fmt, ...);
void PWM_Set_Freq_And_Duty(TIM_HandleTypeDef *htim, uint32_t Channel, uint32_t Freq_Hz, uint16_t Duty_Percent);

void Task_Key() {
    Key_B1();
    Key_B2();
    Key_B3();
    Key_B4();
}

void Key_B1() {
    if(KeyState[0].DOWN) {
        KeyState[0].DOWN = 0;
        SysData.duty += 10;
    }
    if(KeyState[0].REPEAT) {
        KeyState[0].REPEAT = 0;
        SysData.duty += 1;
    }
}

void Key_B2() {
    if(KeyState[1].DOWN) {
        KeyState[1].DOWN = 0;
        SysData.count++;
    }
    if(KeyState[1].REPEAT) {
        KeyState[1].REPEAT = 0;
        SysData.count++;
    }
}

void Key_B3() {
    if(KeyState[2].SINGLE) {
        KeyState[2].SINGLE = 0;
        sprintf(SysData.hint_msg, "B3:SINGLE    ");
        SysData.hint_time = HAL_GetTick();
    }
    if(KeyState[2].DOUBLE) {
        KeyState[2].DOUBLE = 0;
        sprintf(SysData.hint_msg, "B3:DOUBLE    ");
        SysData.hint_time = HAL_GetTick();
    }
    if(KeyState[2].LONG) {
        KeyState[2].LONG = 0;
        sprintf(SysData.hint_msg, "B3:LONG    ");
        SysData.hint_time = HAL_GetTick();
    }
}

void Key_B4() {
    if(KeyState[3].LONG) {
        KeyState[3].LONG = 0;
        SysData.count = 0;
        sprintf(SysData.hint_msg, "Count Reset! ");
        SysData.hint_time = HAL_GetTick();
    }
    if(KeyState[3].SINGLE) {
        KeyState[3].SINGLE = 0;
        SysData.freq += 1000;
    }
}

void Task_Pwm() {
    
    
    if(SysData.duty > 100) SysData.duty = SysData.duty - 100;
    if(SysData.freq > 10000) SysData.freq = 1000;
    if(SysData.freq == 1000) SysData.freq = 1000;
    static uint32_t last_freq = 0;
    static uint32_t last_duty = 0;
    if(SysData.freq != last_freq || SysData.duty != last_duty) {
        PWM_Set_Freq_And_Duty(&htim17, TIM_CHANNEL_1, SysData.freq, SysData.duty);
        last_freq = SysData.freq;
        last_duty = SysData.duty;
    }
    
}

void Task_Lcd() {
    if(SysData.hint_msg[0] != '\0') {
        if(HAL_GetTick() - SysData.hint_time > 1000) {
            SysData.hint_msg[0] = '\0';
        }
    }
    
    static uint32_t lcd_tick = 0;
    if(HAL_GetTick() - lcd_tick < 100) return;
    lcd_tick = HAL_GetTick();
    
    LCD_Show(Line1, "Count: %-13d", SysData.count);
    LCD_Show(Line2, "abcdefghijklmnopqrst");
    LCD_Show(Line3, "Duty: %d%%         ", SysData.duty);
    LCD_Show(Line4, "Freq: %d Hz        ", SysData.freq);
    
    
    LCD_Show(Line9, "%-20s", SysData.hint_msg);
}


void LCD_Show(uint8_t Line, char *fmt, ...) {
    char buf[21];
    va_list ap;
    uint8_t line_idx = Line / 24;
    
    va_start(ap, fmt);
    vsnprintf(buf, 21, fmt, ap);
    va_end(ap);
    
    if(strcmp(LCD_Cache[line_idx], buf) != 0) {
        LCD_DisplayStringLine(Line, (uint8_t *)buf);
        strcpy(LCD_Cache[line_idx], buf);
    }
}

void PWM_Set_Freq_And_Duty(TIM_HandleTypeDef *htim, uint32_t Channel, uint32_t Freq_Hz, uint16_t Duty_Percent) {
    uint32_t clock_freq = 1000000;
    uint32_t arr = (clock_freq / Freq_Hz) - 1;
    uint32_t crr = (arr + 1) * Duty_Percent / 100;
    __HAL_TIM_SetAutoreload(htim, arr);
    __HAL_TIM_SetCompare(htim, Channel, crr);
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
    
    memset(&SysData, 0, sizeof(SysData));
    memset(&DispState, 0, sizeof(DispState));
    
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
}

void App_Loop() {
    Task_Key();
    Task_Lcd();
    Task_Pwm();
}
