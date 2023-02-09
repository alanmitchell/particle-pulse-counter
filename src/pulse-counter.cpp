/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "/home/alan/particle/particle-pulse-counter/src/pulse-counter.ino"
/*
 * Project Particle.io pulse-counter
 * Description: Counts cumulative pulses and publishes total to Particle cloud.
 * Author:  Alan Mitchell
 * Date: February 5, 2023
 */

#include "Particle.h"

void setup();
void loop();
void pulseArrived();
void checkPulse();
void saveCountEEPROM();
#line 10 "/home/alan/particle/particle-pulse-counter/src/pulse-counter.ino"
SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

// logger used for debugging
SerialLogHandler logHandler;

// *** Pins
// The pin where the pulse input is connected.  This pin is set to pull-up, so a
// dry switch contact or open-collector switch pulse source should be attached.
const pin_t PULSE_INPUT = D2;

// *** Settings

// The interval between Cloud Publish operations that send the pulse count
// to the cloud
std::chrono::milliseconds publishInterval = 30min;

// The interval between storing the pulse count into EEPROM so that it is retained
// across a reboot or power outage.  If pulse count has not changed since last
// EEPROM store, the storage operation does not occur so save EEPROM wear.
// Also, pulse count is stored after Publish events.
std::chrono::milliseconds eepromInterval = 2min;

// Time to wait to make sure a switch closure is real and not noise
const uint32_t pulseWaitInterval = 20;   // ms

// Minimum spacing between pulses; don't count pulses occurring before this 
// amount of time.
const uint32_t minPulseSpacing = 700;    // ms

// *** Application-wide variables

// variable to track the system time when the last pulse was registered
system_tick_t lastPulseTime = 0;

// variable to track the system time of when the last pulse count Publish occurred
system_tick_t lastPublish = 0;

// variable to track the system time of when the pulse count was last stored in the
// EEPROM
system_tick_t lastEEPROM = 0;

// Timer used to make sure pulse isn't noise
Timer pulseWaitTimer(pulseWaitInterval, checkPulse, true);

// The total cumulative pulse count
volatile uint32_t pulseCount;

// String buffer used for all publish statements
String data = ""; 

// -- Variables and constants related to EEPROM storage of pulse count

// These are bytes to know if the EEPROM has been initialized
const uint32_t eepromDataMagic = 0x797b0d25;

// This is the offset in the EEPROM for our data to start
const int eepromDataOffset = 0;

// This is the data structure saved to EEPROM
typedef struct {    
    uint32_t        magic;        // if set to the magic number above, EEPROM has been initialized
    uint32_t        pulseCount;   // total pulse count
} EEPROMData;

// variable holding data read or to be written to EEPROM.
EEPROMData eepromCache;

// ---

void setup() {
    
    Particle.connect();

    // Read the EEPROM data and restore the pulse count if EEPROM has been
    // initialized
    EEPROM.get(eepromDataOffset, eepromCache);
    if (eepromCache.magic != eepromDataMagic) {
        // EEPROM has not been used before, initialize it and the pulse count.
        eepromCache.magic = eepromDataMagic;
        eepromCache.pulseCount = 0;
        pulseCount = 0;
        EEPROM.put(eepromDataOffset, eepromCache);
        Log.info("Initialized EEPROM");
    } else {
        pulseCount = eepromCache.pulseCount;
        Log.info("Restored pulse count = %lu", pulseCount);
    }

    // Set pulse input pin to pull-up and attach an interrupt to it.
    pinMode(PULSE_INPUT, INPUT_PULLUP);
    attachInterrupt(PULSE_INPUT, pulseArrived, FALLING);
    
    // pulseCount = 0;    // temporary override for testing

}

void loop() {
    
    // flag to determine whether pulse count should be saved to EEPROM in this loop
    // iteration.
    bool saveEEPROM = false;
    
    if (Particle.connected()) {
        if (millis() - lastPublish >= publishInterval.count()) {
            // time to publish
            lastPublish = millis();
            Log.info("count=%lu", pulseCount);
            Particle.publish("bmon_store", data.format("count=%lu", pulseCount));
            saveEEPROM = true;   // always save count to EEPROM after publish
        }
    }

    // if EEPROM save time has elapsed, set save flag
    if (millis() - lastEEPROM >= eepromInterval.count()) {
        saveEEPROM = true;
    }

    if (saveEEPROM) {
        saveCountEEPROM();
        lastEEPROM = millis();
    }
}

void pulseArrived() {
    // Don't incrment pulse here, wait to see if pin is still low after the pulseWaitInterval.
    // The timer calls the checkPulse() function when done.
    // If this interrupt runs again before timer is finished, the startFromISR() method restarts
    // the timer.
    pulseWaitTimer.startFromISR();
}

void checkPulse() {
    // This function is called after pulseWaitTimer has expired.
    // Increment pulse count if pulse input pin is still pulled down and make sure 
    // there is enough gap between prior pulse.
    if ((digitalRead(PULSE_INPUT) == 0) && (millis() - lastPulseTime > minPulseSpacing)) {
        lastPulseTime = millis();
        pulseCount++;
        // Log.info("new pulse=%lu", pulseCount);
    }
}

void saveCountEEPROM() {
    // save the current pulse count in the EEPROM if it has changed
    if (pulseCount != eepromCache.pulseCount) {
        eepromCache.magic = eepromDataMagic;     // include magic number to indicate valid EEPROM data
        eepromCache.pulseCount = pulseCount;
        EEPROM.put(eepromDataOffset, eepromCache);
        Log.info("Updated EEPROM = %lu", pulseCount);
    } else {
        Log.info("Pulse count has not changed since last EEPROM store = %lu.", pulseCount);
    }
}