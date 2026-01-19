#ifndef __KEY_H
#define __KEY_H

#include "main.h"

#define KEY_MODE_DOUBLE_CLICK   1

#define KEY_TIME_LONG     800
#define KEY_TIME_DOUBLE   300
#define KEY_TIME_REPEAT   100

typedef enum {
    S0_IDLE = 0,
    S1_PRESSED,
    S2_WAIT_DOUBLE,
    S3_DOUBLE_HELD,
    S4_LONG_HELD,
} KeyState_Enum;

typedef struct {
    uint8_t LAST_HOLD   :1;
    uint8_t HOLD        :1;
    uint8_t DOWN        :1;
    uint8_t UP          :1;
    uint8_t SINGLE      :1;
    uint8_t DOUBLE      :1;
    uint8_t LONG        :1;
    uint8_t REPEAT      :1;
    
    KeyState_Enum State;
    uint32_t Timer;
} KeyState_t;

extern volatile KeyState_t KeyState[4];

void Key_Init(void);
void Key_Driver(void);


#endif
