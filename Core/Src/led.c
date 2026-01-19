#include "led.h"

static uint8_t led_state = 0xFF;

static void LED_Write(void) {
    GPIOC->ODR = (GPIOC->ODR & 0x00FF) | (led_state << 8);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
}

void LED_Disp(uint8_t leds) {
    led_state = ~leds;
    LED_Write();
}

void LED_On(uint8_t leds) {
    led_state &= ~leds;
    LED_Write();
}

void LED_Off(uint8_t leds) {
    led_state |= leds;
    LED_Write();
}

void LED_Toggle(uint8_t leds) {
    led_state ^= leds;
    LED_Write();
}
