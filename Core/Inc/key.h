#ifndef __KEY_H
#define __KEY_H

#include "main.h"

#define KEY_MODE_DOUBLE_CLICK   0

#define KEY_TIME_LONG     1000
#define KEY_TIME_DOUBLE   300
#define KEY_TIME_REPEAT   100

typedef enum {
    S0_IDLE = 0,
    S1_HELD,
    S2_WAIT_DOUBLE,
    S3_DOUBLE_HELD,
    S4_LONG_HELD,
} KeyState_t;

typedef struct {
    uint8_t LastHold:1, Hold:1, Down:1, Up:1, Single:1, Double:1, Long:1, Repeat:1;
    KeyState_t State;
    uint32_t Timer;
} Key_t;

extern volatile Key_t Key[4];

void Key_Init(void);
void Key_Driver(void);


#endif
