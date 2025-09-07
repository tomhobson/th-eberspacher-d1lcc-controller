#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "Config.h"
#include "Icons.h"

enum DisplayMode {
  DISPLAY_MAIN,
  DISPLAY_MENU,
  DISPLAY_DEBUG,
  DISPLAY_TIME_SET,
  DISPLAY_POWER_SAVE
};

// Use the HeatState enum from HeaterController.h

struct DisplayData {
  // Temperature data
  float cabinTemp;
  float targetTemp;
  
  // Time data
  uint8_t hour;
  uint8_t minute;
  bool rtcWorking;
  
  // Heater data
  HeatState heaterState;
  bool heaterEnabled;
  bool heaterDelayActive;
  unsigned long delayRemaining;
  
  // System status
  bool menuActive;
  int menuIndex;
  int menuScrollOffset;
  int menuCount;
  char menuItems[MAX_MENU_ITEMS][16];
  
  // Sub-menu data
  bool inSubMenu;
  int subMenuValue;
  int subMenuMin;
  int subMenuMax;
  
  // Wake-up timer flow data
  bool inWakeupFlow;
  int wakeupFlowStep;
  uint8_t wakeupHour;
  uint8_t wakeupMinute;
  uint8_t wakeupTemp;
  uint8_t wakeupDayMask;
  
  // Debug info
  bool showDebug;
  char debugLine1[32];
  char debugLine2[32];
  char debugLine3[32];
};

class Display {
private:
  // Hardware
  U8G2_SH1106_128X64_NONAME_F_HW_I2C* u8g2;
  
  // State
  DisplayMode currentMode;
  bool displayOn;
  unsigned long lastUpdate;
  
  // Internal helper methods
  void drawMainScreen(const DisplayData& data);
  void drawMenuScreen(const DisplayData& data);
  void drawWakeupTimerFlow(const DisplayData& data);
  void drawDebugScreen(const DisplayData& data);
  void drawTimeSetScreen(const DisplayData& data);
  void drawPowerSaveScreen();
  
  void drawTemperatureInfo(const DisplayData& data);
  void drawTimeInfo(const DisplayData& data);
  void drawHeaterStatus(const DisplayData& data);
  void drawHeaterIcon(int x, int y, HeatState state);
  void drawDelayInfo(const DisplayData& data);
  
  void drawCenteredText(const char* text, int y);
  void drawRightAlignedText(const char* text, int x, int y);
  
public:
  Display(U8G2_SH1106_128X64_NONAME_F_HW_I2C* displayPtr);
  
  // Initialization
  bool begin();
  
  // Display control
  void setMode(DisplayMode mode);
  DisplayMode getMode() const { return currentMode; }
  void setPowerSave(bool enabled);
  bool isPowerSave() const { return currentMode == DISPLAY_POWER_SAVE; }
  
  // Main update method
  void update(const DisplayData& data);
  void forceUpdate(const DisplayData& data);
  
  // Utility
  void clear();
  void setBrightness(uint8_t level);
  
  // Debug
  void printStatus() const;
};

#endif // DISPLAY_H