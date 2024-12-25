#ifndef CONFIG_HPP
#define CONFIG_HPP

// Sleep time in milliseconds before turning off the display
#define SLEEP_AFTER_MS 60000 // 1 minute of inactivity

// Pins for the rotary encoder
#define ROTARY_ENCODER_A_PIN 21
#define ROTARY_ENCODER_B_PIN 22
#define ROTARY_ENCODER_BUTTON_PIN 23
#define ROTARY_ENCODER_VCC_PIN -1 // Set to -1 if not using VCC
#define ROTARY_ENCODER_STEPS 4

// Pins for the display
#define DISPLAY_SCL_PIN 22
#define DISPLAY_SDA_PIN 21

// Default load cell settings
#define LOADCELL_DOUT_PIN 32
#define LOADCELL_SCK_PIN 33
#define LOADCELL_SCALE_FACTOR 2280.0 // Adjust to your specific load cell

// Other constants
#define GRINDER_ACTIVE_PIN 25
#define TARE_MEASURES 10
#define CUP_WEIGHT 200.0 // Default cup weight in grams
#define COFFEE_DOSE_WEIGHT 18.0 // Default coffee dose weight
#define COFFEE_DOSE_OFFSET 2.0  // Default offset in grams

#endif // CONFIG_HPP
