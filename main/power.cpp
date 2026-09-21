#include <Arduino.h>
#include "power.h"

#define BAT_CONTROL 7
#define POWER_BUTTON 6
#define POWER_HOLD_TIME 1500

static unsigned long pressStart = 0;
static bool pressed = false;

void Power_Init()
{
    pinMode(BAT_CONTROL, OUTPUT);
    digitalWrite(BAT_CONTROL, HIGH);

    pinMode(POWER_BUTTON, INPUT_PULLUP);
}

void Power_Update()
{
    if (digitalRead(POWER_BUTTON) == LOW)
    {
        if (!pressed)
        {
            pressed = true;
            pressStart = millis();
        }
    }
    else
    {
        if (pressed)
        {
            if (millis() - pressStart > POWER_HOLD_TIME)
            {
                digitalWrite(BAT_CONTROL, LOW);
            }
            pressed = false;
        }
    }
}