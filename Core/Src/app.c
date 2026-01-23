#include "app.h"
#include "key.h"
#include "lcd.h"
#include "led.h"
#include "tim.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "chinese.h"

SysData_t SysData;
DispState_t DispState;
volatile Measure_t Measure;

uint8_t rx_buf[RX_BUF_SIZE];
char process_buf[RX_BUF_SIZE];
uint8_t process_flag = 0;

static char LCD_Cache[10][21];
volatile static uint8_t run_led = 0x01;

void Task_Key(void);
void Task_Lcd(void);
void Task_Pwm(void);
void Task_Uart(void);

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
    if(SysData.hint_msg[0] != '\0') {
        if(HAL_GetTick() - SysData.hint_time > 1000) {
            SysData.hint_msg[0] = '\0';
        }
    }
    
    static uint32_t lcd_tick = 0;
    if(HAL_GetTick() - lcd_tick < 100) return;
    lcd_tick = HAL_GetTick();
    
    LCD_Show(Line0, "Count: %-13d", SysData.count);
    LCD_Show(Line1, "PA7Duty: %d%%      ", SysData.duty);
    LCD_Show(Line2, "PA7Freq: %dHz      ", SysData.freq);
    LCD_Show(Line3, "PA6Freq: %dHz      ", Measure.pa6_freq);
    LCD_Show(Line4, "PA15Freq: %.1fHz   ", Measure.pa15_freq);
    LCD_Show(Line5, "PA15Duty: %.1f%%   ", Measure.pa15_duty);

    LCD_Show(Line9, "%-20s", SysData.hint_msg);
}

void Task_Uart() {
    if(process_flag == 1) {
        strncpy(SysData.hint_msg, process_buf, sizeof(SysData.hint_msg) - 1);
        SysData.hint_msg[sizeof(SysData.hint_msg) - 1] = '\0';
        SysData.hint_time = HAL_GetTick();
        memset(rx_buf, 0, RX_BUF_SIZE);
        process_flag = 0;
    }
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
    
    SysData.duty = 50;
    SysData.freq = 1000;
    
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
    
    HAL_TIM_IC_Start_IT(&htim16, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
    
    HAL_TIM_Base_Start_IT(&htim4);
    
    printf("Hello World\r\n");
    
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, RX_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    
    LCD_Show_Chinese(Line7, 320, White, Black);
}

void App_Loop() {
    Task_Key();
    Task_Lcd();
    Task_Pwm();
    Task_Uart();
}

// 重定向printf
int fputc(int ch, FILE *f) {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
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

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    // 定时器输入捕获回调函数
    
    if(htim->Instance == TIM2) {
        if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            uint32_t pa15_period_cnt = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            uint32_t pa15_high_cnt = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
            if(pa15_period_cnt != 0) {
                Measure.pa15_freq = 1000000.0 / pa15_period_cnt;
                Measure.pa15_duty = ((float)pa15_high_cnt / pa15_period_cnt) * 100.0f;
            }
        }
    }
    
    if(htim->Instance == TIM16) {
        if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            static uint16_t last_pa6 = 0;
            uint16_t cur_pa6 = (uint16_t)HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            uint16_t pa6_val = cur_pa6 - last_pa6;
            last_pa6 = cur_pa6;
            if(pa6_val > 0) {
                Measure.pa6_freq = 1000000 / pa6_val; // 1MHz
            }
        }
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    // 定时器周期溢出回调函数（更新中断回调）
    if(htim->Instance == TIM4) {
        LED_Disp(run_led);
        run_led = run_led << 1;
        if(run_led == 0) run_led = 0x01;
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    // 接收事件回调函数
    if(huart->Instance == USART1) {
        memcpy(process_buf, rx_buf, Size);
        process_buf[Size] = '\0';
        process_flag = 1;
        HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_buf, RX_BUF_SIZE);
        __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
    }
}
