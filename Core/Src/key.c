#include "key.h"

volatile KeyState_t KeyState[4];

void Key_Init() {
    for(int i=0;i<4;i++) {
        KeyState[i].LAST_HOLD = 0;
        KeyState[i].HOLD = 0;
        KeyState[i].UP = 0;
        KeyState[i].DOWN = 0;
        KeyState[i].SINGLE = 0;
        KeyState[i].DOUBLE = 0;
        KeyState[i].LONG = 0;
        KeyState[i].REPEAT = 0;
        
        KeyState[i].State = S0_IDLE;
        KeyState[i].Timer = 0;
        
    }
}

void Key_Scan() {
    KeyState[0].HOLD = ((GPIOB->IDR & 0x01) == 0) ? 1 : 0;
    KeyState[1].HOLD = ((GPIOB->IDR & 0x02) == 0) ? 1 : 0;
    KeyState[2].HOLD = ((GPIOB->IDR & 0x04) == 0) ? 1 : 0;
    KeyState[3].HOLD = ((GPIOA->IDR & 0x01) == 0) ? 1 : 0;
    
    for(int i=0;i<4;i++) {
        if(KeyState[i].LAST_HOLD ^ KeyState[i].HOLD) {
            if(KeyState[i].HOLD) {
                KeyState[i].DOWN = 1;
            } else {
                KeyState[i].UP = 1;
            }
        }
        KeyState[i].LAST_HOLD = KeyState[i].HOLD;
    }
}

void Key_Driver() {
    Key_Scan();
    
    for(int i=0;i<4;i++) {
        switch(KeyState[i].State) {
            case S0_IDLE:
                if(KeyState[i].HOLD) {
                    KeyState[i].Timer = HAL_GetTick();
                    KeyState[i].State = S1_PRESSED;
                }
                break;
            case S1_PRESSED:
                if(!KeyState[i].HOLD) {
#if KEY_MODE_DOUBLE_CLICK == 1
                    KeyState[i].Timer = HAL_GetTick();
                    KeyState[i].State = S2_WAIT_DOUBLE;
#else
                    KeyState[i].SINGLE = 1;   
                    KeyState[i].State = S0_IDLE;
#endif
                }
                else if(HAL_GetTick() - KeyState[i].Timer > KEY_TIME_LONG) {
                    KeyState[i].LONG = 1;
                    KeyState[i].Timer = HAL_GetTick();
                    KeyState[i].State = S4_LONG_HELD;
                }
                break;
#if KEY_MODE_DOUBLE_CLICK == 1
            case S2_WAIT_DOUBLE:
                if(KeyState[i].HOLD) {
                    KeyState[i].DOUBLE = 1;
                    KeyState[i].State = S3_DOUBLE_HELD;
                }
                else if(HAL_GetTick() - KeyState[i].Timer > KEY_TIME_DOUBLE) {
                    KeyState[i].SINGLE = 1;
                    KeyState[i].State = S0_IDLE;
                }
                break;
            case S3_DOUBLE_HELD:
                if(!KeyState[i].HOLD) {
                    KeyState[i].State = S0_IDLE;
                }
                break;
#endif
            case S4_LONG_HELD:
                if(!KeyState[i].HOLD) {
                    KeyState[i].State = S0_IDLE;
                }
                else if(HAL_GetTick() - KeyState[i].Timer > KEY_TIME_REPEAT) {
                    KeyState[i].REPEAT = 1;
                    KeyState[i].Timer = HAL_GetTick();
                }
                break;
            default:
                break;
        }
    }
    
}
