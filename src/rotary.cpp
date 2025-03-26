#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.hpp"
#include "rotary.hpp"
#include "display.hpp"
#include "scale.hpp"

// Rotary encoder for user input
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(
    ROTARY_ENCODER_A_PIN,
    ROTARY_ENCODER_B_PIN,
    ROTARY_ENCODER_BUTTON_PIN,
    ROTARY_ENCODER_VCC_PIN,
    ROTARY_ENCODER_STEPS);

// Vars
int encoderDir = 1;   // Direction of the rotary encoder
int encoderValue = 0; // Current value of the rotary encoder
static int clickCount = 0;
const unsigned long clickThreshold = 500; // 500ms max interval for rapid clicks

static void handleSingleClickTask(void *param)
{
    bool *pendingFlag = reinterpret_cast<bool *>(param);
    delay(300); // Wait for the delay period

    // Ensure the task only acts if `scaleStatus` is valid for the menu
    if (*pendingFlag && scaleStatus == STATUS_EMPTY)
    {
        *pendingFlag = false;
        Serial.println("Single click detected. Opening menu...");
        scaleStatus = STATUS_IN_MENU;
        currentMenuItem = 0;
        rotaryEncoder.setAcceleration(0);
        Serial.println("Entering Menu...");
    }
    vTaskDelete(NULL); // End the task
}

// Incase you can't set something you can exit
void exitToMenu()
{
    if (scaleStatus == STATUS_IN_SUBMENU || scaleStatus == STATUS_INFO_MENU)
    {
        scaleStatus = STATUS_IN_MENU;
        currentSetting = -1;
        Serial.println("Exiting to main menu");
    }
    else if (scaleStatus == STATUS_IN_MENU)
    {
        scaleStatus = STATUS_EMPTY;
        Serial.println("Exiting to empty state");
    }
}

bool debugMode = DEBUG_MODE;
// Handles button clicks on the rotary encoder

void rotary_onButtonClick()
{
    unsigned long currentTime = millis();
    static unsigned long lastTimePressed = 0;      // Timestamp of the last button press
    static int clickCount = 0;                     // Number of clicks
    const unsigned long clickDelay = 300;          // Delay to differentiate single vs double click (in ms)
    const unsigned long longPressThreshold = 3000; // Threshold for long press (in ms)
    static bool menuPending = false;               // Flag to track if a single click action is pending

    // Handle rapid clicks for debug mode
    if (currentTime - lastTimePressed < clickThreshold)
    {
        clickCount++;
    }
    else
    {
        clickCount = 1; // Reset click count if too much time has passed
    }
    lastTimePressed = currentTime;
    if (clickCount == 2)
    {
        menuPending = false; // Cancel pending single click action
        Serial.println("Double press detected. Taring scale...");
        tareScale();    // Call the tare function
        clickCount = 0; // Reset click count
        return;
    }

    // Check for 4 rapid clicks to toggle debug mode
    if (clickCount >= 4)
    {
        debugMode = !debugMode; // Toggle debug mode
        Serial.print("Debug Mode: ");
        Serial.println(debugMode ? "Enabled" : "Disabled");

        // Use the display method from display.cpp
        showDebugModeStatus(debugMode);
        menuItemsCount = debugMode ? 10 : 9;
        clickCount = 0; // Reset the click count
        return;         // Exit early to prevent other actions
    }

    // Delay single click action to allow for double-click detection
    if (!menuPending && scaleStatus == STATUS_EMPTY)
    {
        menuPending = true; // Set pending flag

        // Pass a pointer to `menuPending` as a parameter
        xTaskCreatePinnedToCore(
            handleSingleClickTask, // Task function
            "SingleClickDelay",    // Task name
            1000,                  // Stack size
            &menuPending,          // Parameter (pointer to menuPending)
            1,                     // Priority
            NULL,                  // Task handle (can be NULL)
            1                      // Core ID
        );
    }

    if (rotaryEncoder.isEncoderButtonClicked())
    {
        if (lastTimePressed == 0)
        {
            lastTimePressed = currentTime; // Record the time of the initial button press
        }
        // Reset pending flag if necessary
        if (scaleStatus == STATUS_EMPTY || scaleStatus == STATUS_IN_MENU)
        {
            menuPending = false; // Ensure no delayed action interferes
        }
    }

    if (scaleStatus == STATUS_EMPTY)
    {
        // Enter the menu when the scale is empty
        scaleStatus = STATUS_IN_MENU;
        currentMenuItem = 0;
        rotaryEncoder.setAcceleration(0);
        Serial.println("Entering Menu...");
    }
    else if (scaleStatus == STATUS_IN_MENU)
    {
        // Navigate through the menu items
        switch (currentMenuItem)
        {
        case 0: // Cup Weight Menu
            scaleStatus = STATUS_IN_SUBMENU;
            currentSetting = 0;
            tareScale(); // Tare the scale
            delay(500);  // Wait for stabilization

            if (scaleWeight > 0)
            {
                setCupWeight = scaleWeight;
                preferences.begin("scale", false);
                preferences.putDouble("cup", setCupWeight);
                preferences.end();

                Serial.println("Cup weight set successfully");
            }
            else
            {
                Serial.println("Error: Invalid cup weight detected");
            }
            break;

        case 1: // Calibration Menu
        {
            scaleStatus = STATUS_IN_SUBMENU;
            currentSetting = 1;
            Serial.println("Calibration Menu");
            break;
        }
        case 2: // Offset Menu
            scaleStatus = STATUS_IN_SUBMENU;
            currentSetting = 2;
            Serial.println("Offset Menu");
            break;
        case 3: // Scale Mode Menu
            scaleStatus = STATUS_IN_SUBMENU;
            currentSetting = 3;
            Serial.println("Scale Mode Menu");
            break;
        case 4: // Grinding Mode Menu
            scaleStatus = STATUS_IN_SUBMENU;
            currentSetting = 4;
            Serial.println("Grind Mode Menu");
            break;
        case 5: // Info Menu
            scaleStatus = STATUS_IN_SUBMENU;
            currentSetting = 5;
            Serial.println("Info Menu");
            break;
        case 6: // Sleep Timer Menu
            scaleStatus = STATUS_IN_SUBMENU;
            currentSetting = 8;
            Serial.println("Sleep Timer Menu");
            break;
        case 7:                                 // Exit
            menuPending = false;                // Reset pending flag
            scaleStatus = STATUS_EMPTY;         // Reset to the empty state
            currentMenuItem = 0;                // Reset menu index
            rotaryEncoder.setAcceleration(100); // Restore encoder acceleration
            Serial.println("Exited Menu to main screen");
            Serial.print("Exiting menu, scaleStatus: ");
            Serial.println(scaleStatus);

            delay(200); // Debounce to prevent immediate re-trigger
            break;
        case 8: // Reset Menu
            scaleStatus = STATUS_IN_SUBMENU;
            currentSetting = 6;
            Serial.println("Reset Menu");
            break;
        case 9: // Debug Menu
            if (debugMode)
            {
                scaleStatus = STATUS_IN_SUBMENU;
                currentSetting = 9; // Identifier for Debug Menu
                Serial.println("Entering Debug Menu");
            }
            break;
        }
    }
    else if (scaleStatus == STATUS_IN_SUBMENU)
    {
        // Handle submenu actions based on the current setting
        switch (currentSetting)
        {
        case 0: // Cup Weight Menu
        {
            if (scaleWeight > 5)
            { // Ensure cup weight is valid

                double stableCupWeight = scaleWeight;
                unsigned long startTime = millis();
                bool weightStable = false;

                while (millis() - startTime < 1500) // Increased stability time
                {
                    if (abs(scaleWeight - stableCupWeight) > 0.2) // Adjusted sensitivity
                    {
                        Serial.println("⚠️ Cup weight fluctuating... Restarting measurement.");
                        startTime = millis();
                        stableCupWeight = scaleWeight;
                    }
                }

                // If weight is stable, save it
                setCupWeight = scaleWeight;
                Serial.println("✅ Cup weight stabilized and saved.");
                preferences.begin("scale", false);
                preferences.putDouble("cup", setCupWeight);
                preferences.end();

                displayLock = true;
                showCupWeightSetScreen(setCupWeight); // Show confirmation
                displayLock = false;

                exitToMenu();
            }
            else
            {
                Serial.println("Error: Invalid cup weight detected. Setting default value.");
                setCupWeight = 10.0; // Assign a reasonable default value
                preferences.begin("scale", false);
                preferences.putDouble("cup", setCupWeight);
                preferences.end();
                Serial.println("Failsafe: Exiting cup weight menu due to zero weight");
                exitToMenu();
            }
            break;
        }
        case 1: // Calibration Menu
        {
            // 🔹 Force proper tare
            delay(500);
            Serial.println("\n🔹 SCALE SETUP operation...");
            Serial.println("\n🔹 Initializing scale...");

            delay(7000); // Allow settling time
            Serial.println("\n🔹 Taring scale...");
            loadcell.tare();
            delay(2000); // Allow settling time

            Serial.print("✅ Post-Tare ADC Value: ");
            Serial.println(loadcell.get_units(10)); // Check tare effectiveness

            Serial.println("\n⚠️  Place your known weight on the scale NOW...");
            delay(5000); // Give time to place weight

            double beforeWeight = loadcell.read(); // Read ADC before adding weight
            Serial.print("🔹 Raw ADC Value BEFORE weight: ");
            Serial.println(beforeWeight);

            Serial.println("\n⚠️  Now REMOVE the weight...");
            delay(5000); // Give time to remove weight

            double afterWeight = loadcell.read(); // Read ADC after removing weight
            Serial.print("🔹 Raw ADC Value AFTER weight: ");
            Serial.println(afterWeight);

            double adcDifference = abs(afterWeight - beforeWeight);
            Serial.print("\n🔹 ADC Difference: ");
            Serial.println(adcDifference);

            // 🔹 Compute corrected scale factor
            double newScaleFactor = adcDifference / 49.1; // Knife weight = 49.1g
            Serial.print("\n✅ New computed scale factor: ");
            Serial.println(newScaleFactor);

            // 🔹 Save new calibration factor
            preferences.begin("scale", false);
            preferences.putDouble("calibration", newScaleFactor);
            preferences.end();

            loadcell.set_scale(newScaleFactor); // Apply new scale factor

            Serial.println("\n🚀 Final scale setup complete.");

            // Confirm on display
            displayLock = true;
            CenterConfirmationPrintToScreen("Calibrated!", 32);
            delay(2000);
            displayLock = false;

            exitToMenu(); // Exit back to menu after calibration
            break;
        }
        case 2: // Offset Menu
        {
            preferences.begin("scale", false);
            preferences.putDouble("offset", offset);
            preferences.end();
            scaleStatus = STATUS_IN_MENU;
            currentSetting = -1;
            break;
        }
        case 3: // Scale Mode Menu
        {
            preferences.begin("scale", false);
            preferences.putBool("scaleMode", scaleMode);
            preferences.end();
            scaleStatus = STATUS_IN_MENU;
            currentSetting = -1;
            break;
        }
        case 4: // Grinding Mode Menu
        {
            preferences.begin("scale", false);
            preferences.putBool("grindMode", grindMode);
            preferences.end();
            scaleStatus = STATUS_IN_MENU;
            currentSetting = -1;
            break;
        }
        case 5: // Info Menu
        {
            displayLock = true;
            showInfoMenu(); // Display info menu
            delay(3000);
            displayLock = false;
            exitToMenu();
            break;
        }
        case 6: // Reset Menu
        {
            if (greset)
            {
                preferences.begin("scale", false);
                preferences.putDouble("calibration", (double)LOADCELL_SCALE_FACTOR);
                setWeight = (double)COFFEE_DOSE_WEIGHT;
                preferences.putDouble("setWeight", (double)COFFEE_DOSE_WEIGHT);
                offset = (double)COFFEE_DOSE_OFFSET;
                preferences.putDouble("offset", (double)COFFEE_DOSE_OFFSET);
                setCupWeight = (double)CUP_WEIGHT;
                preferences.putDouble("cup", (double)CUP_WEIGHT);
                scaleMode = false;
                preferences.putBool("scaleMode", false);
                grindMode = false;
                preferences.putBool("grindMode", false);
                preferences.putUInt("shotCount", 0);
                loadcell.set_scale((double)LOADCELL_SCALE_FACTOR);
                preferences.end();
            }
            scaleStatus = STATUS_IN_MENU;
            currentSetting = -1;
            break;
        }
        case 8: // Sleep Timer Menu
        {
            preferences.begin("scale", false);
            preferences.putInt("sleepTime", sleepTime);
            preferences.end();
            scaleStatus = STATUS_IN_MENU;
            currentSetting = -1;
            break;
        }
        case 9: // Debug Menu
        {
            int newValue = rotaryEncoder.readEncoder();
            int debugOption = (newValue - encoderValue) % 3;               // Cycle through options
            debugOption = debugOption < 0 ? 3 + debugOption : debugOption; // Wrap negative values
            encoderValue = newValue;

            if (rotaryEncoder.isEncoderButtonClicked())
            {
                switch (debugOption)
                {
                case 0: // Simulate Grinding
                    Serial.println("Simulating Grinding...");
                    scaleStatus = STATUS_GRINDING_IN_PROGRESS;
                    startedGrindingAt = millis();
                    setWeight = 20.0;     // Example weight
                    cupWeightEmpty = 5.0; // Example cup weight
                    break;

                case 1: // Show Weight History
                    Serial.println("Displaying Weight History...");
                    // Add logic to display weight history
                    break;

                case 2: // Reset Shot Count
                    Serial.println("Resetting Shot Count...");
                    shotCount = 0;
                    preferences.begin("scale", false);
                    preferences.putUInt("shotCount", shotCount);
                    preferences.end();
                    break;
                }
            }
            exitToMenu();
            break;
        }
        }
    }
}

// Handles rotary encoder input for menu navigation and adjustments
void rotary_loop()
{
    if (rotaryEncoder.encoderChanged())
    {
        // Wake the screen if it's asleep
        if (millis() - lastSignificantWeightChangeAt > sleepTime)
        {
            Serial.println("Screen waking due to rotary movement...");
            wakeScreen();
        }
        switch (scaleStatus)
        {
        case STATUS_EMPTY:
        {
            if (screenJustWoke)
            {
                // Skip modifying the set weight if the screen just woke up
                screenJustWoke = false; // Reset the flag
                break;
            }
            // Adjust weight when in scale mode
            if (setWeight < 0)
            {
                setWeight = 0;
                Serial.println("Grind weight cannot be less than 0. Reset to 0.");
            }
            int newValue = rotaryEncoder.readEncoder();
            setWeight += ((float)newValue - (float)encoderValue) / 10 * encoderDir;
            encoderValue = newValue;
            preferences.begin("scale", false);
            preferences.putDouble("setWeight", setWeight);
            preferences.end();
            break;
        }
        case STATUS_IN_MENU:
        {
            // Navigate through menu items
            int newValue = rotaryEncoder.readEncoder();
            currentMenuItem = (currentMenuItem + (newValue - encoderValue) * -encoderDir) % menuItemsCount;
            currentMenuItem = currentMenuItem < 0 ? menuItemsCount + currentMenuItem : currentMenuItem;
            encoderValue = newValue;
            Serial.println(currentMenuItem);
            break;
        }
        case STATUS_IN_SUBMENU:
        {
            int newValue = rotaryEncoder.readEncoder();
            if (currentSetting == 2)
            { // Offset menu
                offset += ((float)newValue - (float)encoderValue) * encoderDir / 100;
                encoderValue = newValue;
                if (abs(offset) >= setWeight)
                {
                    offset = setWeight; // Prevent nonsensical offsets
                }
            }
            else if (currentSetting == 3)
            {
                scaleMode = !scaleMode;
            }
            else if (currentSetting == 4)
            {
                grindMode = !grindMode;
            }
            else if (currentSetting == 6)
            {
                greset = !greset;
            }
            else if (currentSetting == 8)
            {                                                  // Sleep Timer menu
                sleepTime += (newValue - encoderValue) * 1000; // Adjust by seconds
                if (sleepTime < 5000)
                {
                    sleepTime = 5000; // Minimum sleep time: 5 seconds
                }
                if (sleepTime > 600000)
                {
                    sleepTime = 600000; // Maximum sleep time: 10 minutes
                }
                encoderValue = newValue;

                // Save the updated sleep time to preferences
                preferences.begin("scale", false);
                preferences.putInt("sleepTime", sleepTime);
                preferences.end();
            }
            else if (scaleStatus == STATUS_IN_SUBMENU && currentSetting == 9) // Debug Menu
            {
                int newValue = rotaryEncoder.readEncoder();
                currentDebugMenuItem = (currentDebugMenuItem + (newValue - encoderValue) * -encoderDir) % debugMenuItemsCount;
                currentDebugMenuItem = currentDebugMenuItem < 0 ? debugMenuItemsCount + currentDebugMenuItem : currentDebugMenuItem;
                encoderValue = newValue;
                showDebugMenu(); // Update the Debug Menu display
            }
            break;
        }
        case STATUS_GRINDING_FAILED:
        {
            Serial.println("Exiting Grinding Failed state to Main Menu...");
            scaleStatus = STATUS_IN_MENU;
            currentMenuItem = 0; // Reset to the main menu
            return;              // Exit early to avoid further processing
        }
        }
    }
    if (rotaryEncoder.isEncoderButtonClicked())
    {
        if (scaleStatus == STATUS_IN_SUBMENU && currentSetting == 9) // Debug Menu
        {
            handleDebugMenuAction(); // Perform the selected debug menu action
        }
        else
        {
            rotary_onButtonClick(); // Existing button click handling
        }
    }
}

// ISR for reading encoder changes
void readEncoderISR()
{
    rotaryEncoder.readEncoder_ISR();
}