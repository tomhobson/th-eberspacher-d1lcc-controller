#include "PowerManager.h"

// Static instance pointer for ISR access
static PowerManager* powerManagerInstance = nullptr;

PowerManager::PowerManager()
  : currentState(POWER_ACTIVE), lastActivityTime(0), lastWakeTime(0),
    lastWakeupReason(WAKE_UNKNOWN), sleepEnabled(true), heaterRunning(false),
    displayOffTimeout(POWER_SAVE_TIMEOUT), lightSleepTimeout(60000), deepSleepTimeout(300000),
    buttonWakeFlag(false), rotaryWakeFlag(false), timerWakeFlag(false) {
  powerManagerInstance = this;
}

void PowerManager::begin() {
  recordActivity();
  lastWakeTime = millis();
  
  // Setup interrupt pins for wake-up
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  
  DEBUG_PRINTLN("PowerManager initialized");
}

void PowerManager::setSleepEnabled(bool enabled) {
  sleepEnabled = enabled;
  
  if (!enabled && currentState != POWER_ACTIVE) {
    forceWakeUp();
  }
  
  #if DEBUG_ENABLED
    DEBUG_PRINT("Sleep ");
    DEBUG_PRINTLN(enabled ? "enabled" : "disabled");
  #endif
}

void PowerManager::setHeaterRunning(bool running) {
  heaterRunning = running;
  
  // If heater is running, prevent deep sleep
  if (running && currentState == POWER_DEEP_SLEEP) {
    currentState = POWER_LIGHT_SLEEP;
    wakeUp();
  }
}

void PowerManager::recordActivity() {
  lastActivityTime = millis();
  
  // Wake up if we're in any sleep state
  if (currentState != POWER_ACTIVE) {
    wakeUp();
  }
}

void PowerManager::recordButtonActivity() {
  recordActivity();
  lastWakeupReason = WAKE_BUTTON;
}

void PowerManager::recordRotaryActivity() {
  recordActivity();
  lastWakeupReason = WAKE_ROTARY;
}

unsigned long PowerManager::getTimeSinceActivity() const {
  return millis() - lastActivityTime;
}

unsigned long PowerManager::getTimeSinceWake() const {
  return millis() - lastWakeTime;
}

bool PowerManager::shouldDisplayBeOff() const {
  if (!sleepEnabled) return false;
  return getTimeSinceActivity() > displayOffTimeout;
}

bool PowerManager::shouldEnterLightSleep() const {
  if (!sleepEnabled) return false;
  return getTimeSinceActivity() > lightSleepTimeout;
}

bool PowerManager::shouldEnterDeepSleep() const {
  if (!sleepEnabled) return false;
  if (heaterRunning) return false;  // Never deep sleep when heater is running
  return getTimeSinceActivity() > deepSleepTimeout;
}

void PowerManager::update() {
  if (!sleepEnabled) {
    currentState = POWER_ACTIVE;
    return;
  }
  
  // Check for wake-up flags from ISRs
  if (buttonWakeFlag) {
    buttonWakeFlag = false;
    recordButtonActivity();
  }
  
  if (rotaryWakeFlag) {
    rotaryWakeFlag = false;
    recordRotaryActivity();
  }
  
  if (timerWakeFlag) {
    timerWakeFlag = false;
    lastWakeupReason = WAKE_TIMER;
    recordActivity();
  }
  
  // Determine appropriate power state
  PowerState newState = POWER_ACTIVE;
  
  if (shouldEnterDeepSleep()) {
    newState = POWER_DEEP_SLEEP;
  } else if (shouldEnterLightSleep()) {
    newState = POWER_LIGHT_SLEEP;
  } else if (shouldDisplayBeOff()) {
    newState = POWER_DISPLAY_OFF;
  }
  
  // Change state if needed
  if (newState != currentState) {
    switch (newState) {
      case POWER_ACTIVE:
        wakeUp();
        break;
        
      case POWER_DISPLAY_OFF:
        currentState = POWER_DISPLAY_OFF;
        DEBUG_PRINTLN("Display off");
        break;
        
      case POWER_LIGHT_SLEEP:
        enterLightSleep();
        break;
        
      case POWER_DEEP_SLEEP:
        enterDeepSleep();
        break;
    }
  }
}

void PowerManager::enterLightSleep() {
  currentState = POWER_LIGHT_SLEEP;
  DEBUG_PRINTLN("Entering light sleep");
  
  // Setup interrupts for wake-up
  attachInterrupt(digitalPinToInterrupt(ENCODER_SW_PIN), powerButtonISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), powerRotaryISR, CHANGE);
  
  // Setup watchdog for periodic wake-up (8 seconds)
  setupWatchdog(WDTO_8S);
  
  // Enter power-down sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_enable();
  sei();
  sleep_cpu();
  sleep_disable();
  
  // Clean up after wake
  detachInterrupt(digitalPinToInterrupt(ENCODER_SW_PIN));
  detachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN));
  disableWatchdog();
  
  wakeUp();
}

void PowerManager::enterDeepSleep() {
  currentState = POWER_DEEP_SLEEP;
  DEBUG_PRINTLN("Entering deep sleep");
  
  // Disable more peripherals for maximum power saving
  disableUnusedPeripherals();
  
  // Setup interrupts for wake-up
  attachInterrupt(digitalPinToInterrupt(ENCODER_SW_PIN), powerButtonISR, FALLING);
  
  // Setup watchdog for longer periodic wake-up (8 seconds, but we'll count cycles)
  setupWatchdog(WDTO_8S);
  
  // Enter deepest sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_enable();
  sei();
  sleep_cpu();
  sleep_disable();
  
  // Clean up after wake
  detachInterrupt(digitalPinToInterrupt(ENCODER_SW_PIN));
  disableWatchdog();
  enableRequiredPeripherals();
  
  wakeUp();
}

void PowerManager::wakeUp() {
  if (currentState != POWER_ACTIVE) {
    currentState = POWER_ACTIVE;
    lastWakeTime = millis();
    
    #if DEBUG_ENABLED
      DEBUG_PRINT("Woke up: ");
      switch (lastWakeupReason) {
        case WAKE_BUTTON: DEBUG_PRINTLN("Button"); break;
        case WAKE_ROTARY: DEBUG_PRINTLN("Rotary"); break;
        case WAKE_TIMER: DEBUG_PRINTLN("Timer"); break;
        case WAKE_HEATER_CYCLE: DEBUG_PRINTLN("Heater"); break;
        default: DEBUG_PRINTLN("Unknown"); break;
      }
    #endif
  }
}

void PowerManager::disableUnusedPeripherals() {
  // Disable unused peripherals to save power
  // Note: Only disable if not needed by I2C, SPI, etc.
  #ifdef power_adc_disable
    power_adc_disable();
  #endif
  // Don't disable SPI/I2C as they're needed for display and sensors
}

void PowerManager::enableRequiredPeripherals() {
  // Re-enable required peripherals
  #ifdef power_adc_enable
    power_adc_enable();
  #endif
  // SPI/I2C should remain enabled
}

void PowerManager::setupWatchdog(uint8_t prescaler) {
  // Use Arduino's watchdog library for portability
  wdt_enable(prescaler);
}

void PowerManager::disableWatchdog() {
  // Use Arduino's watchdog library for portability
  wdt_disable();
}

void PowerManager::forceDisplayOff() {
  currentState = POWER_DISPLAY_OFF;
}

void PowerManager::forceLightSleep() {
  enterLightSleep();
}

void PowerManager::forceDeepSleep() {
  if (!heaterRunning) {
    enterDeepSleep();
  } else {
    forceLightSleep();
  }
}

void PowerManager::forceWakeUp() {
  wakeUp();
}

void PowerManager::setDisplayOffTimeout(unsigned long timeout) {
  displayOffTimeout = timeout;
}

void PowerManager::setLightSleepTimeout(unsigned long timeout) {
  lightSleepTimeout = timeout;
}

void PowerManager::setDeepSleepTimeout(unsigned long timeout) {
  deepSleepTimeout = timeout;
}

void PowerManager::handleButtonInterrupt() {
  buttonWakeFlag = true;
}

void PowerManager::handleRotaryInterrupt() {
  rotaryWakeFlag = true;
}

void PowerManager::handleWatchdogInterrupt() {
  timerWakeFlag = true;
}

void PowerManager::printStatus() const {
  DEBUG_PRINT("PowerManager Status - State: ");
  DEBUG_PRINT(currentState);
  DEBUG_PRINT(" Sleep: ");
  DEBUG_PRINT(sleepEnabled);
  DEBUG_PRINT(" Heater: ");
  DEBUG_PRINT(heaterRunning);
  DEBUG_PRINT(" Activity: ");
  DEBUG_PRINT(getTimeSinceActivity());
  DEBUG_PRINTLN("ms ago");
}

void PowerManager::printPowerStats() const {
  DEBUG_PRINTLN("Power Statistics:");
  DEBUG_PRINT("  Time since activity: ");
  DEBUG_PRINT(getTimeSinceActivity());
  DEBUG_PRINTLN("ms");
  DEBUG_PRINT("  Time since wake: ");
  DEBUG_PRINT(getTimeSinceWake());
  DEBUG_PRINTLN("ms");
  DEBUG_PRINT("  Last wake reason: ");
  DEBUG_PRINTLN(lastWakeupReason);
}

// ISR implementations
extern "C" {
  void powerButtonISR() {
    if (powerManagerInstance) {
      powerManagerInstance->handleButtonInterrupt();
    }
  }
  
  void powerRotaryISR() {
    if (powerManagerInstance) {
      powerManagerInstance->handleRotaryInterrupt();
    }
  }
  
  void powerWatchdogISR() {
    if (powerManagerInstance) {
      powerManagerInstance->handleWatchdogInterrupt();
    }
  }
}

// Watchdog ISR (comment out if not using watchdog)
// ISR(WDT_vect) {
//   powerWatchdogISR();
// }