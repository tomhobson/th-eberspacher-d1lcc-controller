#include "MenuSystem.h"

// Static member pointers for callbacks (needed for static functions)
static MenuSystem* menuSystemInstance = nullptr;

MenuSystem::MenuSystem()
  : menuActive(false), currentIndex(0), scrollOffset(0), menuOpenTime(0), lastActivity(0),
    menuItemCount(0), inSubMenu(false), activeSubMenu(MENU_MAIN),
    subMenuValue(0), subMenuMin(0), subMenuMax(100),
    inWakeupTimerFlow(false), wakeupHour(7), wakeupMinute(0), wakeupTemp(20),
    wakeupDayMask(0x3E), wakeupFlowStep(0),
    heaterEnabledCallback(nullptr), setHeaterEnabledCallback(nullptr),
    getTargetTempCallback(nullptr), setTargetTempCallback(nullptr),
    enterTimeSetCallback(nullptr), enterDebugCallback(nullptr),
    enterPowerSaveCallback(nullptr), addWakeupTimerCallback(nullptr),
    getWakeupTimerCountCallback(nullptr), getWakeupTimerCallback(nullptr),
    removeWakeupTimerCallback(nullptr) {
  menuSystemInstance = this;
  strcpy(wakeupName, "Wake-up");
}

void MenuSystem::begin() {
  initializeMenuItems();
  DEBUG_PRINTLN_F("Menu OK");
}

// Store menu strings in flash memory (shortened to save space)
const char menuStr0[] PROGMEM = "Heater On/Off";
const char menuStr1[] PROGMEM = "Set Target";
const char menuStr2[] PROGMEM = "Wakeup Timers";
const char menuStr3[] PROGMEM = "Add Timer";
const char menuStr4[] PROGMEM = "View Timers";
const char menuStr5[] PROGMEM = "Set Time";
const char menuStr6[] PROGMEM = "Debug";
const char menuStr7[] PROGMEM = "Sleep";
const char menuStr8[] PROGMEM = "Exit";

void MenuSystem::initializeMenuItems() {
  menuItems[0] = {MENU_HEATER_TOGGLE, menuStr0, alwaysEnabled, nullptr};
  menuItems[1] = {MENU_SET_TARGET, menuStr1, alwaysEnabled, nullptr};
  menuItems[2] = {MENU_WAKEUP_TIMERS, menuStr2, alwaysEnabled, nullptr};
  menuItems[3] = {MENU_ADD_WAKEUP, menuStr3, alwaysEnabled, nullptr};
  menuItems[4] = {MENU_VIEW_WAKEUPS, menuStr4, alwaysEnabled, nullptr};
  menuItems[5] = {MENU_SET_TIME, menuStr5, alwaysEnabled, nullptr};
  menuItems[6] = {MENU_DEBUG_INFO, menuStr6, alwaysEnabled, nullptr};
  menuItems[7] = {MENU_POWER_SAVE, menuStr7, alwaysEnabled, nullptr};
  menuItems[8] = {MENU_EXIT, menuStr8, alwaysEnabled, nullptr};
  
  menuItemCount = 9;
}

void MenuSystem::setHeaterCallbacks(bool (*getEnabled)(), void (*setEnabled)(bool)) {
  heaterEnabledCallback = getEnabled;
  setHeaterEnabledCallback = setEnabled;
}

void MenuSystem::setTargetTempCallbacks(float (*getTemp)(), void (*setTemp)(float)) {
  getTargetTempCallback = getTemp;
  setTargetTempCallback = setTemp;
}

void MenuSystem::setTimeSetCallback(void (*enterTimeSet)()) {
  enterTimeSetCallback = enterTimeSet;
}

void MenuSystem::setDebugCallback(void (*enterDebug)()) {
  enterDebugCallback = enterDebug;
}

void MenuSystem::setPowerSaveCallback(void (*enterPowerSave)()) {
  enterPowerSaveCallback = enterPowerSave;
}

void MenuSystem::setWakeupTimerCallbacks(
  bool (*addTimer)(uint8_t, uint8_t, uint8_t, uint8_t, const char*),
  uint8_t (*getCount)(),
  void* (*getTimer)(uint8_t),
  bool (*removeTimer)(uint8_t)) {
  
  addWakeupTimerCallback = addTimer;
  getWakeupTimerCountCallback = getCount;
  getWakeupTimerCallback = getTimer;
  removeWakeupTimerCallback = removeTimer;
}

void MenuSystem::openMenu() {
  if (!menuActive) {
    menuActive = true;
    currentIndex = 0;
    scrollOffset = 0;
    inSubMenu = false;
    menuOpenTime = millis();
    recordActivity();
    
    DEBUG_PRINTLN_F("Menu+");
  }
}

void MenuSystem::closeMenu() {
  if (menuActive) {
    menuActive = false;
    inSubMenu = false;
    
    DEBUG_PRINTLN_F("Menu-");
  }
}

void MenuSystem::handleInput(RotaryEvent rotaryEvent, ButtonEvent buttonEvent) {
  if (!menuActive) return;
  
  recordActivity();
  
  if (inWakeupTimerFlow) {
    handleWakeupTimerFlow(rotaryEvent, buttonEvent);
  } else if (inSubMenu) {
    handleSubMenuNavigation(rotaryEvent, buttonEvent);
  } else {
    handleMainMenuNavigation(rotaryEvent, buttonEvent);
  }
}

void MenuSystem::handleMainMenuNavigation(RotaryEvent rotaryEvent, ButtonEvent buttonEvent) {
  // Handle rotary encoder for menu navigation
  if (rotaryEvent == ROTARY_CW) {
    currentIndex = (currentIndex + 1) % menuItemCount;
    updateScrollPosition();
    
    #if DEBUG_INPUT
      DEBUG_PRINT("Menu index: ");
      DEBUG_PRINTLN(currentIndex);
    #endif
  } else if (rotaryEvent == ROTARY_CCW) {
    currentIndex = (currentIndex - 1 + menuItemCount) % menuItemCount;
    updateScrollPosition();
    
    #if DEBUG_INPUT
      DEBUG_PRINT("Menu index: ");
      DEBUG_PRINTLN(currentIndex);
    #endif
  }
  
  // Handle button events
  if (buttonEvent == BUTTON_SHORT_PRESS) {
    executeMenuItem(menuItems[currentIndex].id);
  } else if (buttonEvent == BUTTON_LONG_PRESS) {
    closeMenu();
  }
}

void MenuSystem::handleSubMenuNavigation(RotaryEvent rotaryEvent, ButtonEvent buttonEvent) {
  // Handle rotary encoder for value adjustment
  if (rotaryEvent == ROTARY_CW && subMenuValue < subMenuMax) {
    subMenuValue++;
    
    #if DEBUG_INPUT
      DEBUG_PRINT("SubMenu value: ");
      DEBUG_PRINTLN(subMenuValue);
    #endif
  } else if (rotaryEvent == ROTARY_CCW && subMenuValue > subMenuMin) {
    subMenuValue--;
    
    #if DEBUG_INPUT
      DEBUG_PRINT("SubMenu value: ");
      DEBUG_PRINTLN(subMenuValue);
    #endif
  }
  
  // Handle button events
  if (buttonEvent == BUTTON_SHORT_PRESS) {
    // Confirm the value and execute
    switch (activeSubMenu) {
      case MENU_SET_TARGET:
        if (setTargetTempCallback) {
          setTargetTempCallback((float)subMenuValue);
          #if DEBUG_ENABLED
            Serial.print(F("Tgt:"));
            Serial.println(subMenuValue);
          #endif
        }
        break;
        
      default:
        break;
    }
    exitSubMenu();
  } else if (buttonEvent == BUTTON_LONG_PRESS) {
    // Cancel without saving
    exitSubMenu();
  }
}

void MenuSystem::executeMenuItem(MenuId id) {
  switch (id) {
    case MENU_HEATER_TOGGLE:
      if (heaterEnabledCallback && setHeaterEnabledCallback) {
        bool currentState = heaterEnabledCallback();
        setHeaterEnabledCallback(!currentState);
        DEBUG_PRINT("Heater enabled: ");
        DEBUG_PRINTLN(!currentState);
      }
      closeMenu();
      break;
      
    case MENU_SET_TARGET:
      if (getTargetTempCallback) {
        inSubMenu = true;
        activeSubMenu = MENU_SET_TARGET;
        subMenuValue = (int)getTargetTempCallback();
        subMenuMin = MIN_TARGET_TEMP;
        subMenuMax = MAX_TARGET_TEMP;
        DEBUG_PRINTLN_F("TgtSubMenu");
      }
      break;
      
    case MENU_WAKEUP_TIMERS:
      // TODO: Implement wake-up timer main menu
      DEBUG_PRINTLN_F("WakeMenu TODO");
      closeMenu();
      break;
      
    case MENU_ADD_WAKEUP:
      if (addWakeupTimerCallback) {
        startWakeupTimerFlow();
        DEBUG_PRINTLN_F("WakeFlow+");
      } else {
        DEBUG_PRINTLN_F("No wake CB");
        closeMenu();
      }
      break;
      
    case MENU_VIEW_WAKEUPS:
      // TODO: Implement view wake-up timers
      DEBUG_PRINTLN_F("ViewWake TODO");
      closeMenu();
      break;
      
    case MENU_SET_TIME:
      if (enterTimeSetCallback) {
        enterTimeSetCallback();
        closeMenu();
      }
      break;
      
    case MENU_DEBUG_INFO:
      if (enterDebugCallback) {
        enterDebugCallback();
        closeMenu();
      }
      break;
      
    case MENU_POWER_SAVE:
      if (enterPowerSaveCallback) {
        enterPowerSaveCallback();
        closeMenu();
      }
      break;
      
    case MENU_EXIT:
      closeMenu();
      break;
      
    default:
      break;
  }
}

void MenuSystem::exitSubMenu() {
  inSubMenu = false;
  activeSubMenu = MENU_MAIN;
  
  #if DEBUG_INPUT
    DEBUG_PRINTLN("Exiting submenu");
  #endif
}

void MenuSystem::update() {
  if (menuActive && shouldTimeout()) {
    DEBUG_PRINTLN(F("Menu timeout"));
    closeMenu();
  }
}

bool MenuSystem::shouldTimeout() const {
  if (!menuActive) return false;
  
  const unsigned long now = millis();
  const unsigned long inactiveTime = now - lastActivity;
  
  return inactiveTime > MENU_TIMEOUT;
}

void MenuSystem::resetTimeout() {
  recordActivity();
}

void MenuSystem::recordActivity() {
  lastActivity = millis();
}

void MenuSystem::updateScrollPosition() {
  // Ensure the selected item is visible
  // If current index is below scroll window, scroll down
  if (currentIndex >= scrollOffset + MAX_VISIBLE_MENU_ITEMS) {
    scrollOffset = currentIndex - MAX_VISIBLE_MENU_ITEMS + 1;
  }
  // If current index is above scroll window, scroll up
  else if (currentIndex < scrollOffset) {
    scrollOffset = currentIndex;
  }
  
  // Keep scroll offset within bounds
  if (scrollOffset < 0) {
    scrollOffset = 0;
  }
  if (scrollOffset > menuItemCount - MAX_VISIBLE_MENU_ITEMS) {
    scrollOffset = max(0, menuItemCount - MAX_VISIBLE_MENU_ITEMS);
  }
}

const char* MenuSystem::getMenuItemText(int index) const {
  if (index >= 0 && index < menuItemCount) {
    return menuItems[index].text;
  }
  return "";
}

bool MenuSystem::isMenuItemEnabled(int index) const {
  if (index >= 0 && index < menuItemCount && menuItems[index].isEnabled) {
    return menuItems[index].isEnabled();
  }
  return false;
}

// Static callback functions
bool MenuSystem::alwaysEnabled() {
  return true;
}

bool MenuSystem::heaterToggleEnabled() {
  return menuSystemInstance && menuSystemInstance->heaterEnabledCallback;
}

bool MenuSystem::targetTempEnabled() {
  return menuSystemInstance && menuSystemInstance->getTargetTempCallback;
}

bool MenuSystem::timeSetEnabled() {
  return menuSystemInstance && menuSystemInstance->enterTimeSetCallback;
}

bool MenuSystem::debugEnabled() {
  return menuSystemInstance && menuSystemInstance->enterDebugCallback;
}

bool MenuSystem::powerSaveEnabled() {
  return menuSystemInstance && menuSystemInstance->enterPowerSaveCallback;
}

// Wake-up timer flow methods
void MenuSystem::startWakeupTimerFlow() {
  inWakeupTimerFlow = true;
  wakeupFlowStep = 0;  // Start with hour setting
  wakeupHour = 7;      // Default 7:00 AM
  wakeupMinute = 0;
  wakeupTemp = 20;     // Default 20°C
  wakeupDayMask = 0x3E; // Default Mon-Fri
  strcpy(wakeupName, "Wake-up");
  
  activeSubMenu = MENU_WAKEUP_SET_HOUR;
  inSubMenu = true;
  subMenuValue = wakeupHour;
  subMenuMin = 0;
  subMenuMax = 23;
  
  DEBUG_PRINTLN_F("WakeFlow+");
}

void MenuSystem::handleWakeupTimerFlow(RotaryEvent rotaryEvent, ButtonEvent buttonEvent) {
  // Handle rotary input for current step
  if (rotaryEvent == ROTARY_CW && subMenuValue < subMenuMax) {
    subMenuValue++;
  } else if (rotaryEvent == ROTARY_CCW && subMenuValue > subMenuMin) {
    subMenuValue--;
  }
  
  // Handle button input
  if (buttonEvent == BUTTON_SHORT_PRESS) {
    // Save current value and move to next step
    switch (activeSubMenu) {
      case MENU_WAKEUP_SET_HOUR:
        wakeupHour = subMenuValue;
        nextWakeupTimerStep();
        break;
        
      case MENU_WAKEUP_SET_MINUTE:
        wakeupMinute = subMenuValue;
        nextWakeupTimerStep();
        break;
        
      case MENU_WAKEUP_SET_TEMP:
        wakeupTemp = subMenuValue;
        nextWakeupTimerStep();
        break;
        
      case MENU_WAKEUP_SET_DAYS:
        // Convert simple selection to day mask
        if (subMenuValue == 0) {
          wakeupDayMask = 0x3E;  // Weekdays (Mon-Fri)
        } else if (subMenuValue == 1) {
          wakeupDayMask = 0x41;  // Weekend (Sat-Sun)
        } else {
          wakeupDayMask = 0x7F;  // Daily (all days)
        }
        nextWakeupTimerStep();
        break;
        
      case MENU_WAKEUP_CONFIRM:
        // Create the timer
        if (addWakeupTimerCallback && 
            addWakeupTimerCallback(wakeupHour, wakeupMinute, wakeupTemp, wakeupDayMask, wakeupName)) {
          DEBUG_PRINTLN_F("Timer+");
        } else {
          DEBUG_PRINTLN_F("Timer fail");
        }
        exitWakeupTimerFlow();
        break;
        
      default:
        exitWakeupTimerFlow();
        break;
    }
  } else if (buttonEvent == BUTTON_LONG_PRESS) {
    // Cancel the flow
    exitWakeupTimerFlow();
  }
}

void MenuSystem::nextWakeupTimerStep() {
  wakeupFlowStep++;
  
  switch (wakeupFlowStep) {
    case 1:  // Set minute
      activeSubMenu = MENU_WAKEUP_SET_MINUTE;
      subMenuValue = wakeupMinute;
      subMenuMin = 0;
      subMenuMax = 59;
      break;
      
    case 2:  // Set temperature
      activeSubMenu = MENU_WAKEUP_SET_TEMP;
      subMenuValue = wakeupTemp;
      subMenuMin = MIN_WAKEUP_TEMP;
      subMenuMax = MAX_WAKEUP_TEMP;
      break;
      
    case 3:  // Set days (simplified - just weekdays vs weekend for now)
      activeSubMenu = MENU_WAKEUP_SET_DAYS;
      subMenuValue = (wakeupDayMask == 0x3E) ? 0 : 1;  // 0 = weekdays, 1 = weekend
      subMenuMin = 0;
      subMenuMax = 2;  // 0=weekdays, 1=weekend, 2=daily
      break;
      
    case 4:  // Confirm
      activeSubMenu = MENU_WAKEUP_CONFIRM;
      subMenuValue = 1;  // Default to "Yes"
      subMenuMin = 0;    // 0 = No, 1 = Yes
      subMenuMax = 1;
      break;
      
    default:
      exitWakeupTimerFlow();
      break;
  }
}

void MenuSystem::exitWakeupTimerFlow() {
  inWakeupTimerFlow = false;
  inSubMenu = false;
  wakeupFlowStep = 0;
  closeMenu();
  DEBUG_PRINTLN_F("WakeFlow-");
}

void MenuSystem::printStatus() const {
  DEBUG_PRINT("MenuSystem Status - Active: ");
  DEBUG_PRINT(menuActive);
  DEBUG_PRINT(" Index: ");
  DEBUG_PRINT(currentIndex);
  DEBUG_PRINT(" InSubMenu: ");
  DEBUG_PRINT(inSubMenu);
  DEBUG_PRINT(" InWakeupFlow: ");
  DEBUG_PRINT(inWakeupTimerFlow);
  DEBUG_PRINT(" Timeout in: ");
  DEBUG_PRINT((MENU_TIMEOUT - (millis() - lastActivity)) / 1000);
  DEBUG_PRINTLN("s");
}