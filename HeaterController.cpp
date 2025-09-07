#include "HeaterController.h"

HeaterController::HeaterController(Adafruit_DS3502* ds3502Ptr, int heaterControlPin)
  : ds3502(ds3502Ptr), controlPin(heaterControlPin), masterEnabled(true), 
    currentState(HS_OFF), wiperValue(WIPER_LOW_SAFE), 
    lastOnMs(0), lastOffMs(0), lastWiperStepMs(0) {
}

bool HeaterController::begin() {
  // Initialize hardware pin
  pinMode(controlPin, OUTPUT);
  digitalWrite(controlPin, LOW);
  
  // Initialize DS3502 (handled by main setup)
  if (!ds3502->begin()) {
    DEBUG_PRINTLN("ERROR: DS3502 not found");
    return false;
  }
  
  // Set initial safe wiper position
  ds3502->setWiper(WIPER_LOW_SAFE);
  wiperValue = WIPER_LOW_SAFE;
  
  DEBUG_PRINTLN("HeaterController initialized");
  return true;
}

void HeaterController::initializeTiming() {
  // Allow immediate turn-on by setting lastOffMs to past value
  lastOffMs = millis() - MIN_OFF_MS - 1000;
  DEBUG_PRINTLN("Heater timing initialized for immediate operation");
}

void HeaterController::setMasterEnabled(bool enabled) {
  if (masterEnabled != enabled) {
    masterEnabled = enabled;
    
    if (!enabled) {
      // Immediately turn off when disabled
      setState(HS_OFF);
      DEBUG_PRINTLN("Heater DISABLED by user");
    } else {
      DEBUG_PRINTLN("Heater ENABLED by user");
    }
  }
}

uint8_t HeaterController::clampWiper(uint8_t value) {
  if (value < WIPER_MIN_SAFE) return WIPER_MIN_SAFE;
  if (value > WIPER_MAX_SAFE) return WIPER_MAX_SAFE;
  return value;
}

void HeaterController::setWiperSmooth(uint8_t targetValue) {
  targetValue = clampWiper(targetValue);
  const unsigned long now = millis();
  
  // Rate limiting for smooth transitions
  if (now - lastWiperStepMs < WIPER_STEP_DELAY_MS) return;
  
  if (wiperValue < targetValue) {
    wiperValue++;
  } else if (wiperValue > targetValue) {
    wiperValue--;
  } else {
    return; // Already at target
  }
  
  ds3502->setWiper(wiperValue);
  lastWiperStepMs = now;
  
  #if DEBUG_HEATER
    DEBUG_PRINT("Wiper → ");
    DEBUG_PRINTLN(wiperValue);
  #endif
}

void HeaterController::setState(HeatState newState) {
  if (currentState == newState) return;
  
  const unsigned long now = millis();
  currentState = newState;
  
  switch (currentState) {
    case HS_OFF:
      digitalWrite(controlPin, LOW);
      lastOffMs = now;
      DEBUG_PRINTLN("Heater: OFF");
      // Park wiper at safe position
      wiperValue = clampWiper(WIPER_LOW_SAFE);
      ds3502->setWiper(wiperValue);
      break;
      
    case HS_LOW:
      digitalWrite(controlPin, HIGH);
      lastOnMs = now;
      DEBUG_PRINTLN("Heater: LOW");
      break;
      
    case HS_MED:
      digitalWrite(controlPin, HIGH);
      lastOnMs = now;
      DEBUG_PRINTLN("Heater: MEDIUM");
      break;
      
    case HS_HIGH:
      digitalWrite(controlPin, HIGH);
      lastOnMs = now;
      DEBUG_PRINTLN("Heater: HIGH");
      break;
  }
}

bool HeaterController::canTurnOn() const {
  return (millis() - lastOffMs) > MIN_OFF_MS;
}

bool HeaterController::canTurnOff() const {
  return (millis() - lastOnMs) > MIN_ON_MS;
}

unsigned long HeaterController::getTimeUntilCanTurnOn() const {
  unsigned long elapsed = millis() - lastOffMs;
  if (elapsed >= MIN_OFF_MS) return 0;
  return MIN_OFF_MS - elapsed;
}

unsigned long HeaterController::getTimeUntilCanTurnOff() const {
  unsigned long elapsed = millis() - lastOnMs;
  if (elapsed >= MIN_ON_MS) return 0;
  return MIN_ON_MS - elapsed;
}

void HeaterController::update(float cabinTemp, float targetTemp) {
  // If master disabled, force OFF and return
  if (!masterEnabled) {
    setState(HS_OFF);
    return;
  }
  
  const float diff = targetTemp - cabinTemp;  // >0 means too cold
  HeatState desiredState = currentState;
  
  #if DEBUG_HEATER
    DEBUG_PRINT("[HEATER] State: ");
    DEBUG_PRINT(currentState);
    DEBUG_PRINT(" Diff: ");
    DEBUG_PRINT(diff);
    DEBUG_PRINT(" canOn: ");
    DEBUG_PRINT(canTurnOn());
    DEBUG_PRINT(" canOff: ");
    DEBUG_PRINTLN(canTurnOff());
  #endif
  
  if (currentState == HS_OFF) {
    // Stay OFF unless sufficiently below target and allowed to start
    if (diff >= HYS_ON && canTurnOn()) {
      if (diff >= DIFF_HIGH) {
        desiredState = HS_HIGH;
      } else if (diff >= DIFF_MED) {
        desiredState = HS_MED;
      } else {
        desiredState = HS_LOW;
      }
    }
  } else {
    // Heater is ON: consider OFF if comfortably above target and min-on met
    if (cabinTemp >= (targetTemp + HYS_OFF) && canTurnOff()) {
      desiredState = HS_OFF;
    } else {
      // Otherwise adjust power based on temperature difference
      if (diff >= DIFF_HIGH) {
        desiredState = HS_HIGH;
      } else if (diff >= DIFF_MED) {
        desiredState = HS_MED;
      } else {
        desiredState = HS_LOW;  // Near setpoint: hold LOW
      }
    }
  }
  
  // Apply state change if needed
  setState(desiredState);
  
  // Drive wiper smoothly toward target for current state
  uint8_t targetWiper = wiperValue;
  switch (currentState) {
    case HS_LOW:  targetWiper = WIPER_LOW_SAFE;  break;
    case HS_MED:  targetWiper = WIPER_MED_SAFE;  break;
    case HS_HIGH: targetWiper = WIPER_HIGH_SAFE; break;
    case HS_OFF:  targetWiper = WIPER_LOW_SAFE;  break;
  }
  
  setWiperSmooth(targetWiper);
}

void HeaterController::printStatus() const {
  DEBUG_PRINT("HeaterController Status - Enabled: ");
  DEBUG_PRINT(masterEnabled);
  DEBUG_PRINT(" State: ");
  DEBUG_PRINT(currentState);
  DEBUG_PRINT(" Wiper: ");
  DEBUG_PRINT(wiperValue);
  DEBUG_PRINT(" canTurnOn: ");
  DEBUG_PRINT(canTurnOn());
  DEBUG_PRINT(" canTurnOff: ");
  DEBUG_PRINTLN(canTurnOff());
}