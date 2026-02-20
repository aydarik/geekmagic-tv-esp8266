#include "button.h"
#include "config.h"
#include "logger.h"

// Button state tracking
static bool lastButtonState = HIGH;
static bool currentButtonState = HIGH;
static unsigned long buttonPressStartTime = 0;
static unsigned long lastDebounceTime = 0;
static bool buttonPressed = false;

void buttonInit() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    lastButtonState = digitalRead(PIN_BUTTON);
    currentButtonState = lastButtonState;
    logPrintf("Button initialized on GPIO%d", PIN_BUTTON);
}

ButtonPress buttonUpdate() {
    // Read current button state (LOW when pressed with pullup)
    const bool reading = digitalRead(PIN_BUTTON);

    // Check if button state changed (for debouncing)
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    // Debounce: only accept state change after debounce time
    if (millis() - lastDebounceTime > BUTTON_DEBOUNCE_MS) {
        // If the button state has changed after debounce
        if (reading != currentButtonState) {
            currentButtonState = reading;
            if (currentButtonState == LOW && !buttonPressed) {
                // Button was just pressed (HIGH -> LOW with pullup)
                buttonPressStartTime = millis();
                buttonPressed = true;
            } else if (currentButtonState == HIGH && buttonPressed) {
                // Button was just released (LOW -> HIGH with pullup)
                const unsigned long pressDuration = millis() - buttonPressStartTime;
                buttonPressed = false;
                if (pressDuration >= BUTTON_LONG_PRESS_MIN_MS) return BUTTON_LONG;
                if (pressDuration <= BUTTON_SHORT_PRESS_MAX_MS) return BUTTON_SHORT;
            }
        }
    }

    lastButtonState = reading;
    return BUTTON_NONE;
}
