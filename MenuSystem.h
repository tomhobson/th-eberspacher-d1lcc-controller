#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>
#include "Config.h"
#include "InputHandler.h"

enum MenuId {
  MENU_MAIN = 0,
  MENU_HEATER_TOGGLE,
  MENU_SET_TARGET,
  MENU_WAKEUP_TIMERS,     // Main wake-up timer menu
  MENU_ADD_WAKEUP,        // Add new wake-up timer
  MENU_VIEW_WAKEUPS,      // View/edit existing timers
  MENU_SET_TIME,
  MENU_DEBUG_INFO,
  MENU_POWER_SAVE,
  MENU_EXIT,
  
  // Wake-up timer sub-menu states
  MENU_WAKEUP_SET_HOUR,   // Set wake-up hour
  MENU_WAKEUP_SET_MINUTE, // Set wake-up minute
  MENU_WAKEUP_SET_TEMP,   // Set target temperature
  MENU_WAKEUP_SET_DAYS,   // Set days of week
  MENU_WAKEUP_SET_NAME,   // Set timer name
  MENU_WAKEUP_CONFIRM,    // Confirm and save timer
  
  MENU_COUNT
};

struct MenuItem {
  MenuId id;
  const char* text;
  bool (*isEnabled)();     // Function to check if menu item should be enabled
  void (*onSelect)();      // Function to call when item is selected
};

class MenuSystem {
private:
  // State
  bool menuActive;
  int currentIndex;
  int scrollOffset;  // First visible menu item index
  unsigned long menuOpenTime;
  unsigned long lastActivity;
  
  // Menu items
  MenuItem menuItems[MENU_COUNT];
  int menuItemCount;
  
  // Navigation state for sub-menus
  bool inSubMenu;
  MenuId activeSubMenu;
  int subMenuValue;
  int subMenuMin;
  int subMenuMax;
  
  // Wake-up timer creation state
  bool inWakeupTimerFlow;
  uint8_t wakeupHour;
  uint8_t wakeupMinute;
  uint8_t wakeupTemp;
  uint8_t wakeupDayMask;
  char wakeupName[16];
  int wakeupFlowStep;  // Current step in wake-up timer creation
  
  // Callbacks for external state
  bool (*heaterEnabledCallback)();
  void (*setHeaterEnabledCallback)(bool enabled);
  float (*getTargetTempCallback)();
  void (*setTargetTempCallback)(float temp);
  void (*enterTimeSetCallback)();
  void (*enterDebugCallback)();
  void (*enterPowerSaveCallback)();
  
  // Wake-up timer callbacks
  bool (*addWakeupTimerCallback)(uint8_t hour, uint8_t minute, uint8_t temp, uint8_t dayMask, const char* name);
  uint8_t (*getWakeupTimerCountCallback)();
  void* (*getWakeupTimerCallback)(uint8_t index);  // Returns WakeupTimerData*
  bool (*removeWakeupTimerCallback)(uint8_t index);
  
  // Internal helper methods
  void initializeMenuItems();
  void handleMainMenuNavigation(RotaryEvent rotaryEvent, ButtonEvent buttonEvent);
  void handleSubMenuNavigation(RotaryEvent rotaryEvent, ButtonEvent buttonEvent);
  void handleWakeupTimerFlow(RotaryEvent rotaryEvent, ButtonEvent buttonEvent);
  void executeMenuItem(MenuId id);
  void exitSubMenu();
  void startWakeupTimerFlow();
  void nextWakeupTimerStep();
  void exitWakeupTimerFlow();
  void updateScrollPosition();  // Update scroll offset based on current index
  void recordActivity();
  
  // Menu item static callbacks
  static bool alwaysEnabled();
  static bool heaterToggleEnabled();
  static bool targetTempEnabled();
  static bool timeSetEnabled();
  static bool debugEnabled();
  static bool powerSaveEnabled();
  
public:
  MenuSystem();
  
  // Initialization
  void begin();
  
  // Callback registration
  void setHeaterCallbacks(bool (*getEnabled)(), void (*setEnabled)(bool));
  void setTargetTempCallbacks(float (*getTemp)(), void (*setTemp)(float));
  void setTimeSetCallback(void (*enterTimeSet)());
  void setDebugCallback(void (*enterDebug)());
  void setPowerSaveCallback(void (*enterPowerSave)());
  void setWakeupTimerCallbacks(
    bool (*addTimer)(uint8_t, uint8_t, uint8_t, uint8_t, const char*),
    uint8_t (*getCount)(),
    void* (*getTimer)(uint8_t),
    bool (*removeTimer)(uint8_t)
  );
  
  // Menu control
  void openMenu();
  void closeMenu();
  bool isActive() const { return menuActive; }
  bool isInSubMenu() const { return inSubMenu; }
  MenuId getActiveSubMenu() const { return activeSubMenu; }
  
  // Input handling
  void handleInput(RotaryEvent rotaryEvent, ButtonEvent buttonEvent);
  
  // Menu data for display
  int getCurrentIndex() const { return currentIndex; }
  int getScrollOffset() const { return scrollOffset; }
  int getMenuItemCount() const { return menuItemCount; }
  const char* getMenuItemText(int index) const;
  bool isMenuItemEnabled(int index) const;
  
  // Sub-menu data for display
  int getSubMenuValue() const { return subMenuValue; }
  int getSubMenuMin() const { return subMenuMin; }
  int getSubMenuMax() const { return subMenuMax; }
  
  // Wake-up timer flow data for display
  bool isInWakeupFlow() const { return inWakeupTimerFlow; }
  int getWakeupFlowStep() const { return wakeupFlowStep; }
  uint8_t getWakeupHour() const { return wakeupHour; }
  uint8_t getWakeupMinute() const { return wakeupMinute; }
  uint8_t getWakeupTemp() const { return wakeupTemp; }
  uint8_t getWakeupDayMask() const { return wakeupDayMask; }
  
  // Timeout handling
  void update();
  bool shouldTimeout() const;
  void resetTimeout();
  
  // Debug
  void printStatus() const;
};

#endif // MENU_SYSTEM_H