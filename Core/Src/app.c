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

SysData_t SysData;
SysData_t SysData_Shadow; // 专门给 EEPROM 用的快照
Msg_t Msg;
DispState_t DispState;
volatile Measure_t Measure;

volatile uint16_t adc1_dma_buf[3];

uint8_t rx_buf[RX_BUF_SIZE];
char process_buf[RX_BUF_SIZE];
uint8_t process_flag = 0;

static char LCD_Cache[10][21];
volatile static uint8_t run_led = 0x01;

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

void Key_B1(void);
void Key_B2(void);
void Key_B3(void);
void Key_B4(void);

void LCD_Show(uint8_t Line, char *fmt, ...);
void PWM_Set_Freq_And_Duty(TIM_HandleTypeDef *htim, uint32_t Channel, uint32_t Freq_Hz, uint16_t Duty_Percent);
uint32_t Filter(uint32_t new_value);
double Get_ADC_Vol(ADC_HandleTypeDef *hadc);
void Request_Save(void);

void Task_Key() {
    Key_B1();
    Key_B2();
    Key_B3();
    Key_B4();
}

void Key_B1() {
    if(KeyState[0].SINGLE) {
        KeyState[0].SINGLE = 0;
        SysData.duty += 10;
    }
    if(KeyState[0].LONG) {
        KeyState[0].LONG = 0;
        Request_Save();
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
        sprintf(Msg.hint_msg, "B3:SINGLE    ");
        Msg.hint_time = HAL_GetTick();
    }
    if(KeyState[2].DOUBLE) {
        KeyState[2].DOUBLE = 0;
        sprintf(Msg.hint_msg, "B3:DOUBLE    ");
        Msg.hint_time = HAL_GetTick();
    }
    if(KeyState[2].LONG) {
        KeyState[2].LONG = 0;
        sprintf(Msg.hint_msg, "B3:LONG    ");
        Msg.hint_time = HAL_GetTick();
    }
}

void Key_B4() {
    if(KeyState[3].LONG) {
        KeyState[3].LONG = 0;
        SysData.count = 0;
        sprintf(Msg.hint_msg, "Count Reset! ");
        Msg.hint_time = HAL_GetTick();
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
    if(Msg.hint_msg[0] != '\0') {
        if(HAL_GetTick() - Msg.hint_time > 1000) {
            Msg.hint_msg[0] = '\0';
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
    LCD_Show(Line5, "R37:%.2fV", Measure.r37);
    //LCD_Show(Line6, "R38:%.2fV Vdda:%.2fV", Measure.r38[0], Measure.r38[2]);
    //LCD_Show(Line7, "Temp:%.1fC  ", Measure.r38[1]);
    LCD_Show(Line6, "Time:%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
    LCD_Show(Line7, "Date:20%02d-%02d-%02d", sDate.Year, sDate.Month, sDate.Date);
    LCD_Show(Line8, "IsAlarm:%d Boot:%d", SysData.is_alarm, SysData.boot_count);

    LCD_Show(Line9, "%-20s", Msg.hint_msg);
}

void Task_Uart() {
    if(process_flag == 1) {
        strncpy(Msg.hint_msg, process_buf, sizeof(Msg.hint_msg) - 1);
        Msg.hint_msg[sizeof(Msg.hint_msg) - 1] = '\0';
        Msg.hint_time = HAL_GetTick();
        memset(rx_buf, 0, RX_BUF_SIZE);
        process_flag = 0;
    }
}

void Task_Adc() {
    static uint32_t adc_tick = 0;
    if(HAL_GetTick() - adc_tick < 200) return;
    adc_tick = HAL_GetTick();
    
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
    static uint32_t rtc_tick = 0;
    if(HAL_GetTick() - rtc_tick < 500) return;
    rtc_tick = HAL_GetTick();
    
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
    
    memset(&DispState, 0, sizeof(DispState));
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
uint32_t Filter(uint32_t new_value)
{
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

void Request_Save(void) {
    if(SaveReqFlag == 0) {
        SysData_Shadow = SysData;
        eeprom_idx = 0;
        SaveReqFlag = 1;
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    // 定时器输入捕获回调函数
    
    if(htim->Instance == TIM2) {
        if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            // 硬件清零
            uint32_t pa15_val = (uint32_t)HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            if(pa15_val != 0) {
                // 如果测量结果不稳定, 加滤波
                // pa15_val = Filter(pa15_val);
                Measure.pa15_freq = 1000000.0 / pa15_val;
            }
        }
    }
    
    if(htim->Instance == TIM16) {
        if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            // TIM16没有从模式:复位模式(硬件清零), 选用作差法
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

// 模拟看门狗回调函数 (当电压越界时，硬件自动调用此函数)
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        SysData.is_alarm = 1;
    }
}

