#include "EberspracherController.h"

// Global instance pointer for ISR access
EberspracherController* g_controller = nullptr;

// Static instance pointer for menu callbacks
static EberspracherController* controllerInstance = nullptr;

EberspracherController::EberspracherController()
  : u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE),
    oneWire(TEMP_SENSOR_PIN),
    sensors(&oneWire),
    button(ENCODER_SW_PIN),
    heaterController(&ds3502, HEATER_CONTROL_PIN),
    inputHandler(&button),
    rtcManager(&rtc),
    display(&u8g2),
    wakeupTimer(&rtcManager),
    currentState(STATE_STARTUP),
    currentTemp(20.0),
    targetTemp(DEFAULT_TARGET_TEMP),
    systemEnabled(true),
    firstRun(true),
    lastTempRead(0),
    lastDisplayUpdate(0),
    lastHeaterUpdate(0),
    stateChangeTime(0),
    tempSensorError(false),
    rtcError(false),
    displayError(false),
    ds3502Error(false) {
  
  g_controller = this;
  controllerInstance = this;
}

bool EberspracherController::begin() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);  // Allow serial to stabilize
  
  DEBUG_PRINTLN_F("Eberspächer TempCtrl v1.0");
  
  if (!setupComponents()) {
    changeState(STATE_ERROR);
    return false;
  }
  
  setupMenuCallbacks();
  changeState(STATE_NORMAL);
  
  DEBUG_PRINTLN_F("Init OK");
  return true;
}

bool EberspracherController::setupComponents() {
  bool success = true;
  
  // Initialize I2C first
  Wire.begin();
  DEBUG_PRINTLN_F("I2C OK");
  
  // Initialize display
  if (!display.begin()) {
    reportError("Display", "Init fail");
    displayError = true;
    success = false;
  }
  
  // Initialize temperature sensors
  sensors.begin();
  sensors.setResolution(12);  // Maximum resolution
  if (sensors.getDeviceCount() == 0) {
    reportError("TempSensor", "No sensors");
    tempSensorError = true;
    success = false;
  } else {
    #if DEBUG_ENABLED
      Serial.print(F("TempSens:"));
      Serial.println(sensors.getDeviceCount());
    #endif
  }
  
  // Initialize RTC
  if (!rtcManager.begin()) {
    reportError("RTC", "Init fail");
    rtcError = true;
    // Non-fatal - we can continue with fallback time
  }
  
  // Initialize DS3502
  if (!heaterController.begin()) {
    reportError("DS3502", "Init fail");
    ds3502Error = true;
    success = false;
  }
  
  // Initialize input handler
  inputHandler.begin();
  
  // Initialize menu system
  menuSystem.begin();
  
  // Initialize power manager
  powerManager.begin();
  
  // Initialize wake-up timer
  if (!wakeupTimer.begin()) {
    reportError("WakeupTimer", "Init fail");
    // Non-fatal - we can continue without wake-up timers
  }
  
  // Initialize heater timing for immediate operation
  heaterController.initializeTiming();
  
  return success;
}

void EberspracherController::setupMenuCallbacks() {
  menuSystem.setHeaterCallbacks(getHeaterEnabled, setHeaterEnabled);
  menuSystem.setTargetTempCallbacks(getTargetTemp, setTargetTemp);
  menuSystem.setTimeSetCallback(enterTimeSetMode);
  menuSystem.setDebugCallback(enterDebugMode);
  menuSystem.setPowerSaveCallback(enterPowerSaveMode);
  menuSystem.setWakeupTimerCallbacks(addWakeupTimerStatic, getWakeupTimerCountStatic, 
                                     getWakeupTimerStatic, removeWakeupTimerStatic);
}

void EberspracherController::loop() {
  const unsigned long now = millis();
  
  // Update all inputs first
  updateInputs();
  
  // Update power management
  updatePower();
  
  // State machine handling
  switch (currentState) {
    case STATE_STARTUP:
      handleStartup();
      break;
      
    case STATE_NORMAL:
      handleNormalOperation();
      break;
      
    case STATE_MENU:
      handleMenuOperation();
      break;
      
    case STATE_DEBUG:
      handleDebugMode();
      break;
      
    case STATE_TIME_SET:
      handleTimeSetMode();
      break;
      
    case STATE_ERROR:
      handleErrorState();
      break;
  }
  
  // Update temperature reading
  if (now - lastTempRead > 2000) {  // Every 2 seconds
    updateTemperature();
    lastTempRead = now;
  }
  
  // Update heater control
  if (now - lastHeaterUpdate > 1000) {  // Every 1 second
    updateHeater();
    lastHeaterUpdate = now;
  }
  
  // Update display
  if (now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  // System health check
  if (now % 10000 == 0) {  // Every 10 seconds
    checkSystemHealth();
  }
}

void EberspracherController::handleStartup() {
  // Startup sequence is handled in begin()
  // This state is mainly for future expansion
  changeState(STATE_NORMAL);
}

void EberspracherController::handleNormalOperation() {
  ButtonEvent buttonEvent = inputHandler.getButtonEvent();
  
  if (buttonEvent == BUTTON_SHORT_PRESS) {
    menuSystem.openMenu();
    changeState(STATE_MENU);
  }
}

void EberspracherController::handleMenuOperation() {
  ButtonEvent buttonEvent = inputHandler.getButtonEvent();
  RotaryEvent rotaryEvent = inputHandler.getRotaryEvent();
  
  menuSystem.handleInput(rotaryEvent, buttonEvent);
  menuSystem.update();
  
  if (!menuSystem.isActive()) {
    changeState(STATE_NORMAL);
  }
}

void EberspracherController::handleDebugMode() {
  ButtonEvent buttonEvent = inputHandler.getButtonEvent();
  
  if (buttonEvent == BUTTON_LONG_PRESS) {
    changeState(STATE_NORMAL);
  }
}

void EberspracherController::handleTimeSetMode() {
  ButtonEvent buttonEvent = inputHandler.getButtonEvent();
  RotaryEvent rotaryEvent = inputHandler.getRotaryEvent();
  
  // TODO: Implement time setting logic
  // For now, just exit on button press
  if (buttonEvent == BUTTON_SHORT_PRESS) {
    changeState(STATE_NORMAL);
  }
}

void EberspracherController::handleErrorState() {
  // In error state, only allow basic recovery actions
  ButtonEvent buttonEvent = inputHandler.getButtonEvent();
  
  if (buttonEvent == BUTTON_LONG_PRESS) {
    // Attempt system restart
    DEBUG_PRINTLN_F("Recovery...");
    changeState(STATE_STARTUP);
  }
}

void EberspracherController::updateTemperature() {
  if (tempSensorError) return;
  
  sensors.requestTemperatures();
  float newTemp = sensors.getTempCByIndex(0);
  
  if (newTemp != DEVICE_DISCONNECTED_C && newTemp > -50 && newTemp < 100) {
    currentTemp = newTemp;
    tempSensorError = false;
    
    #if DEBUG_ENABLED
      Serial.print(F("T:"));
      Serial.println(currentTemp);
    #endif
  } else {
    if (!tempSensorError) {
      reportError("TempSensor", "Read fail");
      tempSensorError = true;
    }
  }
}

void EberspracherController::updateHeater() {
  if (ds3502Error || !systemEnabled) {
    heaterController.setMasterEnabled(false);
    return;
  }
  
  heaterController.setMasterEnabled(true);
  heaterController.update(currentTemp, targetTemp);
  
  // Update power manager with heater state
  bool heaterOn = (heaterController.getState() != HS_OFF);
  powerManager.setHeaterRunning(heaterOn);
}

void EberspracherController::updateDisplay() {
  if (displayError) return;
  
  if (powerManager.shouldDisplayBeOff()) {
    display.setPowerSave(true);
    return;
  } else {
    display.setPowerSave(false);
  }
  
  // Set display mode based on system state
  switch (currentState) {
    case STATE_NORMAL:
      display.setMode(DISPLAY_MAIN);
      break;
    case STATE_MENU:
      display.setMode(DISPLAY_MENU);
      break;
    case STATE_DEBUG:
      display.setMode(DISPLAY_DEBUG);
      break;
    case STATE_TIME_SET:
      display.setMode(DISPLAY_TIME_SET);
      break;
    default:
      display.setMode(DISPLAY_MAIN);
      break;
  }
  
  DisplayData data = buildDisplayData();
  display.update(data);
}

void EberspracherController::updateInputs() {
  inputHandler.update();
  powerManager.update();
  
  // Record activity for power management
  if (inputHandler.hasActivity()) {
    powerManager.recordActivity();
  }
}

void EberspracherController::updatePower() {
  // Power management is handled in updateInputs() and main loop
  // This method is for future expansion
}

DisplayData EberspracherController::buildDisplayData() {
  DisplayData data = {};
  
  // Temperature data
  data.cabinTemp = currentTemp;
  data.targetTemp = targetTemp;
  
  // Time data
  DateTime now = rtcManager.getStableTime();
  data.hour = now.hour();
  data.minute = now.minute();
  data.rtcWorking = rtcManager.isWorking();
  
  // Heater data
  data.heaterState = heaterController.getState();
  data.heaterEnabled = heaterController.isMasterEnabled();
  data.heaterDelayActive = !heaterController.canTurnOn();
  data.delayRemaining = heaterController.getTimeUntilCanTurnOn();
  
  // Menu data
  data.menuActive = menuSystem.isActive();
  data.menuIndex = menuSystem.getCurrentIndex();
  data.menuScrollOffset = menuSystem.getScrollOffset();
  data.menuCount = menuSystem.getMenuItemCount();
  
  // Sub-menu data
  data.inSubMenu = menuSystem.isInSubMenu();
  data.subMenuValue = menuSystem.getSubMenuValue();
  data.subMenuMin = menuSystem.getSubMenuMin();
  data.subMenuMax = menuSystem.getSubMenuMax();
  
  // Wake-up timer flow data
  data.inWakeupFlow = menuSystem.isInWakeupFlow();
  data.wakeupFlowStep = menuSystem.getWakeupFlowStep();
  data.wakeupHour = menuSystem.getWakeupHour();
  data.wakeupMinute = menuSystem.getWakeupMinute();
  data.wakeupTemp = menuSystem.getWakeupTemp();
  data.wakeupDayMask = menuSystem.getWakeupDayMask();
  
  for (int i = 0; i < data.menuCount && i < MAX_MENU_ITEMS; i++) {
    const char* menuText = menuSystem.getMenuItemText(i);
    strncpy_P(data.menuItems[i], menuText, 15);
    data.menuItems[i][15] = '\0';
  }
  
  // Debug info
  data.showDebug = (currentState == STATE_DEBUG);
  if (data.showDebug) {
    snprintf(data.debugLine1, sizeof(data.debugLine1), "Temp: %.1f°C", currentTemp);
    snprintf(data.debugLine2, sizeof(data.debugLine2), "Heater: %d Wiper: %d", 
             (int)data.heaterState, heaterController.getWiperValue());
    snprintf(data.debugLine3, sizeof(data.debugLine3), "Errors: T%d R%d D%d H%d", 
             tempSensorError, rtcError, displayError, ds3502Error);
  }
  
  return data;
}

void EberspracherController::changeState(SystemState newState) {
  if (currentState != newState) {
    currentState = newState;
    stateChangeTime = millis();
    
    #if DEBUG_ENABLED
      Serial.print(F("S:"));
      Serial.println(newState);
    #endif
  }
}

void EberspracherController::checkSystemHealth() {
  // Check for persistent errors and attempt recovery
  if (tempSensorError) {
    // Try to re-initialize temperature sensor
    sensors.begin();
    if (sensors.getDeviceCount() > 0) {
      tempSensorError = false;
      DEBUG_PRINTLN_F("TempSens OK");
    }
  }
  
  if (rtcError && rtcManager.hasValidTime()) {
    rtcError = false;
    DEBUG_PRINTLN_F("RTC OK");
  }
}

void EberspracherController::reportError(const char* component, const char* error) {
  #if DEBUG_ENABLED
    Serial.print(F("ERR["));
    Serial.print(component);
    Serial.print(F("]: "));
    Serial.println(error);
  #endif
}

// Static callback implementations
bool EberspracherController::getHeaterEnabled() {
  return controllerInstance ? controllerInstance->heaterController.isMasterEnabled() : false;
}

void EberspracherController::setHeaterEnabled(bool enabled) {
  if (controllerInstance) {
    controllerInstance->heaterController.setMasterEnabled(enabled);
  }
}

float EberspracherController::getTargetTemp() {
  return controllerInstance ? controllerInstance->targetTemp : DEFAULT_TARGET_TEMP;
}

void EberspracherController::setTargetTemp(float temp) {
  if (controllerInstance) {
    controllerInstance->targetTemp = constrain(temp, MIN_TARGET_TEMP, MAX_TARGET_TEMP);
  }
}

void EberspracherController::enterTimeSetMode() {
  if (controllerInstance) {
    controllerInstance->changeState(STATE_TIME_SET);
  }
}

void EberspracherController::enterDebugMode() {
  if (controllerInstance) {
    controllerInstance->changeState(STATE_DEBUG);
  }
}

void EberspracherController::enterPowerSaveMode() {
  if (controllerInstance) {
    controllerInstance->powerManager.forceLightSleep();
  }
}

// Static wake-up timer callback implementations
bool EberspracherController::addWakeupTimerStatic(uint8_t hour, uint8_t minute, uint8_t temp, uint8_t dayMask, const char* name) {
  if (controllerInstance) {
    return controllerInstance->wakeupTimer.addTimer(hour, minute, temp, dayMask, name);
  }
  return false;
}

uint8_t EberspracherController::getWakeupTimerCountStatic() {
  if (controllerInstance) {
    return controllerInstance->wakeupTimer.getTimerCount();
  }
  return 0;
}

void* EberspracherController::getWakeupTimerStatic(uint8_t index) {
  if (controllerInstance) {
    return controllerInstance->wakeupTimer.getTimer(index);
  }
  return nullptr;
}

bool EberspracherController::removeWakeupTimerStatic(uint8_t index) {
  if (controllerInstance) {
    return controllerInstance->wakeupTimer.removeTimer(index);
  }
  return false;
}

// Public interface methods
void EberspracherController::setSystemEnabled(bool enabled) {
  systemEnabled = enabled;
  heaterController.setMasterEnabled(enabled);
}


void EberspracherController::printSystemStatus() const {
  #if DEBUG_ENABLED
    Serial.print(F("S:"));
    Serial.print(currentState);
    Serial.print(F(" T:"));
    Serial.print(currentTemp);
    Serial.print(F(" E:"));
    Serial.print(tempSensorError);
    Serial.println(ds3502Error);
  #endif
}

void EberspracherController::runDiagnostics() {
  #if DEBUG_ENABLED
    Serial.println(F("DIAG"));
    
    // Test temperature sensor
    sensors.requestTemperatures();
    float testTemp = sensors.getTempCByIndex(0);
    Serial.println((testTemp != DEVICE_DISCONNECTED_C) ? F("TmpOK") : F("TmpFAIL"));
    
    // Test RTC
    Serial.print(F("RTC:"));
    Serial.println(rtcManager.hasValidTime() ? F("OK") : F("FAIL"));
    
    // Test display
    Serial.print(F("Disp:"));
    Serial.println(!displayError ? F("OK") : F("FAIL"));
  #endif
}

void EberspracherController::handleRotaryISR() {
  // Read encoder state
  static uint8_t lastClk = HIGH;
  uint8_t clk = digitalRead(ENCODER_CLK_PIN);
  uint8_t dt = digitalRead(ENCODER_DT_PIN);
  
  if (clk != lastClk) {
    int direction = (clk == dt) ? -1 : 1;
    inputHandler.handleRotaryInterrupt(direction);
  }
  
  lastClk = clk;
}