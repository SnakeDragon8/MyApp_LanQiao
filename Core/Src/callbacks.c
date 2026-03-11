#include "callbacks.h"
#include "app.h"
#include "led.h"
#include "key.h"
#include <string.h>

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
    if(htim->Instance == TIM7) {
        Key_Driver();
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    // 接收事件回调函数
    if(huart->Instance == USART1) {
        // 防止由于接收满缓冲区导致的数组越界
        if (Size < RX_BUF_SIZE) {
            rx_buf[Size] = '\0';
        } else {
            rx_buf[RX_BUF_SIZE - 1] = '\0';
        }
        rx_flag = 1;
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
