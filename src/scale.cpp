#include "config.hpp"
#include "rotary.hpp"
#include "scale.hpp"

// Variables for scale functionality
double scaleWeight = 0;       // Current weight measured by the scale
double setWeight = 0;         // Target weight set by the user
double setCupWeight = 0;      // Weight of the cup set by the user
double offset = 0;            // Offset for stopping grinding prior to reaching set weight
bool scaleMode = false;       // Indicates if the scale is used in timer mode
bool grindMode = false;       // Grinder mode: impulse (false) or continuous (true)
bool grinderActive = false;   // Grinder state (on/off)
unsigned int shotCount;  

// Buffer for storing recent weight history
MathBuffer<double, 100> weightHistory;

// Timing and status variables
unsigned long scaleLastUpdatedAt = 0;  // Timestamp of the last scale update
unsigned long lastSignificantWeightChangeAt = 0; // Timestamp of the last significant weight change
unsigned long lastTareAt = 0; // Timestamp of the last tare operation
bool scaleReady = false;      // Indicates if the scale is ready to measure
int scaleStatus = STATUS_EMPTY; // Current status of the scale
double cupWeightEmpty = 0;    // Measured weight of the empty cup
unsigned long startedGrindingAt = 0;  // Timestamp of when grinding started
unsigned long finishedGrindingAt = 0; // Timestamp of when grinding finished
bool greset = false;          // Flag for reset operation
bool newOffset = false;       // Indicates if a new offset value is pending

// Tares the scale (sets the current weight to zero)
void tareScale() {
    Serial.println("Taring scale");
    loadcell.tare(TARE_MEASURES);
    lastTareAt = millis();
}

// Task to continuously update the scale readings
void updateScale(void *parameter) {
    float lastEstimate;
    for (;;) {
        if (lastTareAt == 0) {
            Serial.println("retaring scale");
            Serial.println("current offset");
            Serial.println(offset);
            tareScale();
        }
        if (loadcell.wait_ready_timeout(300)) {
            lastEstimate = kalmanFilter.updateEstimate(loadcell.get_units(5));
            scaleWeight = lastEstimate;
            scaleLastUpdatedAt = millis();
            weightHistory.push(scaleWeight);
            scaleReady = true;
        } else {
            Serial.println("HX711 not found.");
            scaleReady = false;
        }
    }
}

// Toggles the grinder on or off based on mode
void grinderToggle() {
    if (!scaleMode) {
        if (grindMode) {
            grinderActive = !grinderActive;
            digitalWrite(GRINDER_ACTIVE_PIN, grinderActive);
        } else {
            digitalWrite(GRINDER_ACTIVE_PIN, 1);
            delay(100);
            digitalWrite(GRINDER_ACTIVE_PIN, 0);
        }
    }
}

// Task to manage the status of the scale
void scaleStatusLoop(void *p) {
    for (;;) {
        double tenSecAvg = weightHistory.averageSince((int64_t)millis() - 10000);
        if (ABS(tenSecAvg - scaleWeight) > SIGNIFICANT_WEIGHT_CHANGE) {
            lastSignificantWeightChangeAt = millis();
        }

        switch (scaleStatus) {
            case STATUS_EMPTY: {
                if (millis() - lastTareAt > TARE_MIN_INTERVAL && ABS(tenSecAvg) > 0.2 && tenSecAvg < 3 && scaleWeight < 3) {
                    lastTareAt = 0; // Retare if conditions are met
                }
                if (ABS(weightHistory.minSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE &&
                    ABS(weightHistory.maxSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE) {
                    cupWeightEmpty = weightHistory.averageSince((int64_t)millis() - 500);
                    scaleStatus = STATUS_GRINDING_IN_PROGRESS;
                    if (!scaleMode) {
                        newOffset = true;
                        startedGrindingAt = millis();
                    }
                    grinderToggle();
                    continue;
                }
                break;
            }
            case STATUS_GRINDING_IN_PROGRESS: {
                if (!scaleReady) {
                    grinderToggle();
                    scaleStatus = STATUS_GRINDING_FAILED;
                }
                if (scaleMode && startedGrindingAt == 0 && scaleWeight - cupWeightEmpty >= 0.1) {
                    startedGrindingAt = millis();
                    continue;
                }
                if (millis() - startedGrindingAt > MAX_GRINDING_TIME && !scaleMode) {
                    grinderToggle();
                    scaleStatus = STATUS_GRINDING_FAILED;
                    continue;
                }
                if (millis() - startedGrindingAt > 2000 &&
                    scaleWeight - weightHistory.firstValueOlderThan(millis() - 2000) < 1 &&
                    !scaleMode) {
                    grinderToggle();
                    scaleStatus = STATUS_GRINDING_FAILED;
                    continue;
                }
                if (weightHistory.minSince((int64_t)millis() - 200) < cupWeightEmpty - CUP_DETECTION_TOLERANCE && !scaleMode) {
                    grinderToggle();
                    scaleStatus = STATUS_GRINDING_FAILED;
                    continue;
                }
                double currentOffset = offset;
                if (scaleMode) {
                    currentOffset = 0;
                }
                if (weightHistory.maxSince((int64_t)millis() - 200) >= cupWeightEmpty + setWeight + currentOffset) {
                    finishedGrindingAt = millis();
                    grinderToggle();
                    scaleStatus = STATUS_GRINDING_FINISHED;
                    continue;
                }
                break;
            }
            case STATUS_GRINDING_FINISHED: {
                double currentWeight = weightHistory.averageSince((int64_t)millis() - 500);
                if (scaleWeight < 5) {
                    startedGrindingAt = 0;
                    scaleStatus = STATUS_EMPTY;
                    continue;
                } else if (currentWeight != setWeight + cupWeightEmpty && millis() - finishedGrindingAt > 1500 && newOffset) {
                    offset += setWeight + cupWeightEmpty - currentWeight;
                    if (ABS(offset) >= setWeight) {
                        offset = COFFEE_DOSE_OFFSET;
                    }
                    shotCount++;
                    preferences.begin("scale", false);
                    preferences.putDouble("offset", offset);
                    preferences.putUInt("shotCount", shotCount);
                    preferences.end();
                    newOffset = false;
                }
                break;
            }
            case STATUS_GRINDING_FAILED: {
                if (scaleWeight >= GRINDING_FAILED_WEIGHT_TO_RESET) {
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
void setupScale() {
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    rotaryEncoder.setBoundaries(-10000, 10000, true);
    rotaryEncoder.setAcceleration(100);

    loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    pinMode(GRINDER_ACTIVE_PIN, OUTPUT);
    digitalWrite(GRINDER_ACTIVE_PIN, 0);

    preferences.begin("scale", false);
    double scaleFactor = preferences.getDouble("calibration", (double)LOADCELL_SCALE_FACTOR);
    setWeight = preferences.getDouble("setWeight", (double)COFFEE_DOSE_WEIGHT);
    offset = preferences.getDouble("offset", (double)COFFEE_DOSE_OFFSET);
    setCupWeight = preferences.getDouble("cup", (double)CUP_WEIGHT);
    scaleMode = preferences.getBool("scaleMode", false);
    grindMode = preferences.getBool("grindMode", false);
    shotCount = preferences.getUInt("shotCount", 0);
    preferences.end();

    loadcell.set_scale(scaleFactor);

    xTaskCreatePinnedToCore(updateScale, "Scale", 10000, NULL, 0, &ScaleTask, 1);
    xTaskCreatePinnedToCore(scaleStatusLoop, "ScaleStatus", 10000, NULL, 0, &ScaleStatusTask, 1);
}
