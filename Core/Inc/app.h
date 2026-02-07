#ifndef __APP_H
#define __APP_H

#include "main.h"

#define RX_BUF_SIZE 100
#define FILTER_N 8

typedef enum {
    DISP_DATA = 0,
    DISP_PARA,
    DISP_NUM
} DispState_t;

typedef struct {
    uint32_t count;
    uint32_t freq;
    uint32_t duty;
    uint8_t is_alarm;
} SysData_t;

typedef struct {
    char hint_msg[21];
    uint32_t hint_time;
} Msg_t;

typedef struct {
    uint32_t pa6_freq;
    float pa15_freq;
    double r37;
    double r38[3];
} Measure_t;

void App_Init(void);
void App_Loop(void);

#endif
