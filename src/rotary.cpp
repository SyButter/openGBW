#include "rotary.hpp"
#include "config.hpp"
#include "display.hpp" // To control display state
#include "scale.hpp"   // If needed for interactions with scale logic

// Rotary encoder instance
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(
    ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN,
    ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN,
    ROTARY_ENCODER_STEPS
);

static bool displayOn = true; // Track the display state

// Implement the setupRotary function
void setupRotary() {
    rotaryEncoder.begin();
    rotaryEncoder.setup([] { rotaryEncoder.readEncoder_ISR(); });
    rotaryEncoder.setAcceleration(100); // Configure acceleration
}

// Rotary encoder logic...
void handleRotary() {
    static unsigned long lastActionTime = 0;

    // Check for button press
    if (rotaryEncoder.isEncoderButtonClicked()) {
        wakeDisplay(); // Wake the display
        lastActionTime = millis();
    }

    // Check for rotation
    if (rotaryEncoder.encoderChanged()) {
        wakeDisplay(); // Wake the display
        lastActionTime = millis();
        Serial.printf("Rotary Value: %d\n", rotaryEncoder.readEncoder());
    }

    // Example: Turn off display after inactivity
    if (millis() - lastActionTime > SLEEP_AFTER_MS && isDisplayOn()) {
        u8g2.clearBuffer(); // Clear display buffer
        u8g2.sendBuffer();
        Serial.println("Display turned off due to inactivity");
    }
}


bool isDisplayOn() {
    return displayOn;
}

void wakeDisplay() {
    if (!displayOn) {
        displayOn = true;
        Serial.println("Display turned on");
        // Redraw any necessary UI elements here
    }
}
