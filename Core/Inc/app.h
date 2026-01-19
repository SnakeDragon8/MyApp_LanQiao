#ifndef __APP_H
#define __APP_H

#include "main.h"

typedef enum {
    DISP_DATA = 0,
    DISP_PARA,
    DISP_NUM
} DispState_t;

typedef struct {
    uint32_t count;
    uint8_t duty;
    
    char hint_msg[21];
    uint32_t hint_time;
} SysData_t;

void App_Init(void);
void App_Loop(void);

#endif
