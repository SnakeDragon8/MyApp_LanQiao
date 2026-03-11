#include "key.h"

volatile Key_t Key[4];

void Key_Init() {
    for(int i=0;i<4;i++) {
        Key[i].LastHold = 0;
        Key[i].Hold = 0;
        Key[i].Up = 0;
        Key[i].Down = 0;
        
        Key[i].Single = 0;
        Key[i].Double = 0;
        Key[i].Long = 0;
        Key[i].Repeat = 0;
        
        Key[i].State = S0_IDLE;
        Key[i].Timer = 0;
    }
}

void Key_Scan() {
    Key[0].Hold = !(GPIOB->IDR & 0x01);
    Key[1].Hold = !(GPIOB->IDR & 0x02);
    Key[2].Hold = !(GPIOB->IDR & 0x04);
    Key[3].Hold = !(GPIOA->IDR & 0x01);
    
    for(int i=0;i<4;i++) {
        if(Key[i].LastHold ^ Key[i].Hold) {
            if(Key[i].Hold) {
                Key[i].Down = 1;
            } else {
                Key[i].Up = 1;
            }
        }
        Key[i].LastHold = Key[i].Hold;
    }
}

void Key_Driver() {
    Key_Scan();
    
    for(int i=0;i<4;i++) {
        switch(Key[i].State) {
            case S0_IDLE:
                if(Key[i].Hold) {
                    Key[i].Timer = HAL_GetTick();
                    Key[i].State = S1_HELD;
                }
                break;
            case S1_HELD:
                if(!Key[i].Hold) {
#if KEY_MODE_DOUBLE_CLICK == 1
                    Key[i].Timer = HAL_GetTick();
                    Key[i].State = S2_WAIT_DOUBLE;
#else
                    Key[i].Single = 1;   
                    Key[i].State = S0_IDLE;
#endif
                }
                else if(HAL_GetTick() - Key[i].Timer > KEY_TIME_LONG) {
                    Key[i].Long = 1;
                    Key[i].Timer = HAL_GetTick();
                    Key[i].State = S4_LONG_HELD;
                }
                break;
#if KEY_MODE_DOUBLE_CLICK == 1
            case S2_WAIT_DOUBLE:
                if(Key[i].Hold) {
                    Key[i].Double = 1;
                    Key[i].State = S3_DOUBLE_HELD;
                }
                else if(HAL_GetTick() - Key[i].Timer > KEY_TIME_DOUBLE) {
                    Key[i].Single = 1;
                    Key[i].State = S0_IDLE;
                }
                break;
            case S3_DOUBLE_HELD:
                if(!Key[i].Hold) {
                    Key[i].State = S0_IDLE;
                }
                break;
#endif
            case S4_LONG_HELD:
                if(!Key[i].Hold) {
                    Key[i].State = S0_IDLE;
                }
                else if(HAL_GetTick() - Key[i].Timer > KEY_TIME_REPEAT) {
                    Key[i].Repeat = 1;
                    Key[i].Timer = HAL_GetTick();
                }
                break;
            default:
                break;
        }
    }
    
}
