#include "bsp.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "usart.h"
#include "lcd.h"
#include "rtc.h"

static char LCD_Cache[10][21];

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


/* * 通用滑动窗口平均滤波器
 * 输入：新采集的原始数据 (Raw Data)
 * 返回：平滑后的数据
 */
uint32_t Filter(uint32_t new_value) {
    static uint32_t buf[FILTER_N] = {0};
    static uint32_t sum = 0;
    static uint8_t idx = 0;

    // 1. 启动时的快速填充（可选，防止刚上电是0）
    if (sum == 0 && buf[0] == 0) {
        for(int i=0; i<FILTER_N; i++) {
            buf[i] = new_value;
            sum += new_value;
        }
    }
    else {
        // 2. 减去最老，加上最新
        sum = sum - buf[idx] + new_value;
        buf[idx] = new_value;
    }

    // 3. 索引移动
    idx++;
    if (idx >= FILTER_N) idx = 0;

    // 4. 返回平均值
    return sum / FILTER_N;
}

double Get_ADC_Vol(ADC_HandleTypeDef *hadc) {
    uint16_t adc_val = 0;
    HAL_ADC_Start(hadc);
    if(HAL_OK == HAL_ADC_PollForConversion(hadc, 10)) {
        adc_val = HAL_ADC_GetValue(hadc);
    }
    return (adc_val * 3.3) / 4095.0;
}


void Set_RTC_Time(uint8_t h, uint8_t m, uint8_t s) {
    RTC_TimeTypeDef sTime = {0};

    sTime.Hours = h;
    sTime.Minutes = m;
    sTime.Seconds = s;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    // 设置时间
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
        // Error Handler
    }
    
}

void LCD_CacheClear() {
    memset(&LCD_Cache, 0, sizeof(LCD_Cache));
}
