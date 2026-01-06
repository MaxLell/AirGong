#include "BlinkLed.h"
#include <Arduino.h>

// ###########################################################################
// # Module state
// ###########################################################################
static struct
{
    u8 pin;
    bool led_state;
    bool initialized;
} blinky_led_cfg = {0};

// ###########################################################################
// # Public API
// ###########################################################################
void blinkled_init(u8 pin)
{
    blinky_led_cfg.pin = pin;
    blinky_led_cfg.led_state = false;
    blinky_led_cfg.initialized = true;

    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void blinkled_enable(void)
{
    if (!blinky_led_cfg.initialized)
    {
        return;
    }

    blinky_led_cfg.led_state = true;
    digitalWrite(blinky_led_cfg.pin, HIGH);
}

void blinkled_disable(void)
{
    if (!blinky_led_cfg.initialized)
    {
        return;
    }

    blinky_led_cfg.led_state = false;
    digitalWrite(blinky_led_cfg.pin, LOW);
}

void blinkled_toggle(void)
{
    if (!blinky_led_cfg.initialized)
    {
        return;
    }

    blinky_led_cfg.led_state = !blinky_led_cfg.led_state;
    digitalWrite(blinky_led_cfg.pin, blinky_led_cfg.led_state ? HIGH : LOW);
}
