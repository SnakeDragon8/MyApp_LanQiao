#ifndef __APP_H
#define __APP_H

#include "main.h"

#define RX_BUF_SIZE 100

typedef enum {
    SYSD = 0,
    MEAS,
    CTRL
} State_t;

typedef struct {
    uint32_t freq;
    uint32_t duty;
    uint8_t is_alarm;
    uint32_t boot_count;
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

extern SysData_t SysData;
extern volatile Measure_t Measure;
extern volatile uint8_t run_led;
extern uint8_t rx_buf[RX_BUF_SIZE];
extern uint8_t rx_flag;

#endif
