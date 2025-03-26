#include "config.hpp"
#include "rotary.hpp"
#include "scale.hpp"

// Variables for scale functionality
double scaleWeight = 0;     // Current weight measured by the scale
double setWeight = 0;       // Target weight set by the user
double setCupWeight = 0;    // Weight of the cup set by the user
double offset = 0;          // Offset for stopping grinding prior to reaching set weight
bool scaleMode = false;     // Indicates if the scale is used in timer mode
bool grindMode = true;     // Grinder mode: impulse (false) or continuous (true)
bool grinderActive = false; // Grinder state (on/off)
unsigned int shotCount;
#define SMOOTHING_FACTOR 0.2  // Adjust for stronger/weaker smoothing
double smoothedWeight = 0.0;

// For debugging purposes
bool enableWeightReadout = true; // Set to false to pause output

// Buffer for storing recent weight history
MathBuffer<double, 100> weightHistory;

// Timing and status variables
unsigned long scaleLastUpdatedAt = 0;            // Timestamp of the last scale update
unsigned long lastSignificantWeightChangeAt = 0; // Timestamp of the last significant weight change
unsigned long lastTareAt = 0;                    // Timestamp of the last tare operation
bool scaleReady = false;                         // Indicates if the scale is ready to measure
int scaleStatus = STATUS_EMPTY;                  // Current status of the scale
double cupWeightEmpty = 0;                       // Measured weight of the empty cup
unsigned long startedGrindingAt = 0;             // Timestamp of when grinding started
unsigned long finishedGrindingAt = 0;            // Timestamp of when grinding finished
bool greset = false;                             // Flag for reset operation
bool newOffset = false;                          // Indicates if a new offset value is pending

void tareScale()
{
    Serial.println("Taring scale...");
    loadcell.set_scale();         // Reset scale factor before taring
    loadcell.tare(TARE_MEASURES); // Perform tare
    delay(500);                   // Allow some settling time

    double tareReading = loadcell.get_units(10); // Read raw tare value
    Serial.print("Post-tare reading: ");
    Serial.println(tareReading);

    if (abs(tareReading) > 0.1)
    { // If it’s not close to zero, retry
        Serial.println("Tare failed, retrying...");
        loadcell.tare(TARE_MEASURES);
        delay(500);
    }

    lastTareAt = millis();
    Serial.println("Scale tared successfully.");
}

// Task to continuously update the scale readings
void updateScale(void *parameter)
{
    float lastEstimate;
    for (;;)
    {
        if (loadcell.wait_ready_timeout(200))
        {
            double rawWeight = loadcell.get_units(7);

            //Ignore only extreme changes (prevents stuck readings)
            if (abs(rawWeight - scaleWeight) > 1000)
            {
                Serial.println("⚠️ Ignoring extreme fluctuation...");
                continue;
            }

            //Apply smoothing (low-pass filter)
            smoothedWeight = (SMOOTHING_FACTOR * rawWeight) + ((1 - SMOOTHING_FACTOR) * smoothedWeight);

            //Round weight to the nearest 0.1g
            double displayWeight = round(smoothedWeight * 10.0) / 10.0;

            //Ignore noise below 0.2g (Dead Zone)
            if (abs(displayWeight) < 0.15)
            {
                displayWeight = 0.0;
            }

            scaleWeight = displayWeight; // Accept real changes
            scaleLastUpdatedAt = millis();
            weightHistory.push(displayWeight);

            //Ensure weight updates even on startup
            if (!scaleReady)
            {
                scaleReady = true;
            }

            if (enableWeightReadout)
            {
                Serial.print("Raw weight: ");
                Serial.println(rawWeight);
                Serial.print("Filtered weight: ");
                Serial.println(scaleWeight);
            }
        }
        else
        {
            Serial.println("⚠️ HX711 not responding.");
            scaleReady = false;
        }
    }
}

// Toggles the grinder on or off based on mode
void grinderToggle()
{
    if (!scaleMode)
    {
        if (grindMode)
        {
            grinderActive = !grinderActive;
            digitalWrite(GRINDER_ACTIVE_PIN, grinderActive);
            Serial.print("Grinder toggled: ");
            Serial.println(grinderActive ? "ON" : "OFF");
        }
        else
        {
            Serial.println("Grinder ON (Impulse Mode)");
            digitalWrite(GRINDER_ACTIVE_PIN, 1);
            delay(100);
            digitalWrite(GRINDER_ACTIVE_PIN, 0);
            Serial.println("Grinder OFF");
        }
    }
}

// Task to manage the status of the scale
void scaleStatusLoop(void *p)
{
    for (;;)
    {
        double tenSecAvg = weightHistory.averageSince((int64_t)millis() - 10000);
        if (ABS(tenSecAvg - scaleWeight) > SIGNIFICANT_WEIGHT_CHANGE)
        {
            lastSignificantWeightChangeAt = millis();
        }

        switch (scaleStatus)
        {
        case STATUS_EMPTY:
        {
            if (millis() - lastTareAt > TARE_MIN_INTERVAL && ABS(tenSecAvg) > 0.2 && tenSecAvg < 3 && scaleWeight < 3)
            {
                static unsigned long tareStartTime = 0;

                if (tareStartTime == 0)
                    tareStartTime = millis();        // Start timer
                if (millis() - tareStartTime > 2000) // Ensure 2-second stability before auto-tare
                {
                    Serial.println("✅ Auto-Taring Scale...");
                    tareScale();
                    tareStartTime = 0; // Reset timer after taring
                }
            }
            if (ABS(weightHistory.minSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE &&
                ABS(weightHistory.maxSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE)
            {
                cupWeightEmpty = weightHistory.averageSince((int64_t)millis() - 500);
                scaleStatus = STATUS_GRINDING_IN_PROGRESS;
                if (!scaleMode)
                {
                    newOffset = true;
                    startedGrindingAt = millis();
                }
                grinderToggle();
                continue;
            }
            break;
        }
        case STATUS_GRINDING_IN_PROGRESS:
        {
            if (scaleWeight <= 0)
            { // Avoid restarting grinding with zero or negative weight
                Serial.println("Negative or zero weight detected. Skipping grinding.");
                grinderToggle(); // Ensure grinder is off
                scaleStatus = STATUS_GRINDING_FAILED;
                continue;
            }
            if (!scaleReady)
            {
                grinderToggle();
                scaleStatus = STATUS_GRINDING_FAILED;
            }
            if (scaleMode && startedGrindingAt == 0 && scaleWeight - cupWeightEmpty >= 0.1)
            {
                startedGrindingAt = millis();
                continue;
            }
            if (millis() - startedGrindingAt > MAX_GRINDING_TIME && !scaleMode)
            {
                grinderToggle();
                scaleStatus = STATUS_GRINDING_FAILED;
                continue;
            }
            if (millis() - startedGrindingAt > 2000 &&
                scaleWeight - weightHistory.firstValueOlderThan(millis() - 2000) < 1 &&
                !scaleMode)
            {
                grinderToggle();
                scaleStatus = STATUS_GRINDING_FAILED;
                continue;
            }
            if (weightHistory.minSince((int64_t)millis() - 200) < cupWeightEmpty - CUP_DETECTION_TOLERANCE && !scaleMode)
            {
                grinderToggle();
                scaleStatus = STATUS_GRINDING_FAILED;
                continue;
            }
            double currentOffset = offset;
            if (scaleMode)
            {
                currentOffset = 0;
            }
            if (weightHistory.maxSince((int64_t)millis() - 200) >= cupWeightEmpty + setWeight + currentOffset)
            {
                finishedGrindingAt = millis();
                grinderToggle();
                scaleStatus = STATUS_GRINDING_FINISHED;
                continue;
            }
            break;
        }
        case STATUS_GRINDING_FINISHED:
        {
            static unsigned long grindingFinishedAt = 0;

            // Record the time when grinding finished if not already recorded
            if (grindingFinishedAt == 0)
            {
                grindingFinishedAt = millis();
                Serial.print("Grinder was on for: ");
                Serial.print(grindingFinishedAt);
                Serial.println(" seconds");
            }

            double currentWeight = weightHistory.averageSince((int64_t)millis() - 500);
            if (scaleWeight < 5)
            {
                startedGrindingAt = 0;
                grindingFinishedAt = 0; // Reset the timestamp
                scaleWeight = 0;
                scaleStatus = STATUS_EMPTY;
                continue;
            }
            else if (currentWeight != setWeight + cupWeightEmpty && millis() - finishedGrindingAt > 1500 && newOffset)
            {
                offset += setWeight + cupWeightEmpty - currentWeight;
                if (ABS(offset) >= setWeight)
                {
                    offset = COFFEE_DOSE_OFFSET;
                }
                shotCount++;
                preferences.begin("scale", false);
                preferences.putDouble("offset", offset);
                preferences.putUInt("shotCount", shotCount);
                preferences.end();
                newOffset = false;
            }

            // Timeout to transition back to the main menu after grinding finishes
            if (millis() - grindingFinishedAt > 5000)
            { // 5-second delay after grinding finishes
                if (scaleWeight >= 3)
                { // If weight is still on the scale, wait for cup removal
                    Serial.println("Waiting for cup to be removed...");
                }
                else
                {
                    startedGrindingAt = 0;
                    grindingFinishedAt = 0; // Reset the timestamp
                    scaleStatus = STATUS_EMPTY;
                    Serial.println("Grinding finished. Transitioning to main menu.");
                }
            }
            break;
        }
        case STATUS_GRINDING_FAILED:
        {
            if (scaleWeight >= GRINDING_FAILED_WEIGHT_TO_RESET)
            {
                scaleStatus = STATUS_EMPTY;
                continue;
            }
            break;
        }
        }
        rotary_loop();
        delay(50);
    }
}

// Initializes the scale hardware and settings
void setupScale()
{
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    rotaryEncoder.setBoundaries(-10000, 10000, true);
    rotaryEncoder.setAcceleration(100);

    loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    pinMode(GRINDER_ACTIVE_PIN, OUTPUT);
    digitalWrite(GRINDER_ACTIVE_PIN, 0);

    // 🔹 Load stored calibration factor
    preferences.begin("scale", false);
    double scaleFactor = preferences.getDouble("calibration", 1.0); // Default to 1.0
    setWeight = preferences.getDouble("setWeight", (double)COFFEE_DOSE_WEIGHT);
    offset = preferences.getDouble("offset", (double)COFFEE_DOSE_OFFSET);
    setCupWeight = preferences.getDouble("cup", (double)CUP_WEIGHT);
    scaleMode = preferences.getBool("scaleMode", false);
    grindMode = preferences.getBool("grindMode", false);
    shotCount = preferences.getUInt("shotCount", 0);
    sleepTime = preferences.getInt("sleepTime", SLEEP_AFTER_MS);
    preferences.end();

    // 🔹 Ensure a valid scale factor
    if (scaleFactor <= 0) {
        Serial.println("⚠️ Invalid calibration factor! Resetting to default.");
        scaleFactor = 1.0;
    }
    
    Serial.print("✅ Loaded calibration factor: ");
    Serial.println(scaleFactor);
    
    loadcell.set_scale(scaleFactor);

    // 🔹 Ensure proper tare at startup
    Serial.println("\n🔹 Taring scale...");
    loadcell.tare();
    delay(1000); // Allow settling time

    double postTareReading = loadcell.get_units(10);
    Serial.print("✅ Post-Tare ADC Value: ");
    Serial.println(postTareReading);

    // 🔹 Ensure tare was successful
    if (abs(postTareReading) > 0.5)
    {
        Serial.println("⚠️ Tare drift detected! Retrying...");
        loadcell.tare();
        delay(500);
        postTareReading = loadcell.get_units(10);
        Serial.print("✅ Retried Tare Value: ");
        Serial.println(postTareReading);
    }

    scaleReady = true; // Mark scale as ready

    // 🔹 Start update tasks
    xTaskCreatePinnedToCore(updateScale, "Scale", 10000, NULL, 0, &ScaleTask, 1);
    xTaskCreatePinnedToCore(scaleStatusLoop, "ScaleStatus", 10000, NULL, 0, &ScaleStatusTask, 1);
}

