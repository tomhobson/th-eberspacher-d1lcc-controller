#include "Config.h"
#include "Display.h"

Display::Display(U8G2_SH1106_128X64_NONAME_F_HW_I2C* displayPtr)
  : u8g2(displayPtr), currentMode(DISPLAY_MAIN), displayOn(true), lastUpdate(0) {
}

bool Display::begin() {
  if (!u8g2->begin()) {
    DEBUG_PRINTLN_F("ERR: No display");
    return false;
  }
  
  u8g2->enableUTF8Print();
  u8g2->setFont(FONT_SMALL);
  
  // Show startup message briefly
  u8g2->clearBuffer();
  drawCenteredText("Eberspacher", 28);
  drawCenteredText("TempCtrl", 40);
  drawCenteredText("v1.0", 52);
  u8g2->sendBuffer();
  delay(500);
  
  DEBUG_PRINTLN_F("Display OK");
  return true;
}

void Display::setMode(DisplayMode mode) {
  if (currentMode != mode) {
    currentMode = mode;
    lastUpdate = 0; // Force immediate update
    
    if (mode == DISPLAY_POWER_SAVE) {
      displayOn = false;
      u8g2->setPowerSave(1);
    } else {
      displayOn = true;
      u8g2->setPowerSave(0);
    }
    
    #if DEBUG_DISPLAY
      DEBUG_PRINT_F("Display mode: ");
      DEBUG_PRINT(mode);
      DEBUG_PRINTLN_F("");
    #endif
  }
}

void Display::setPowerSave(bool enabled) {
  if (enabled) {
    setMode(DISPLAY_POWER_SAVE);
  } else if (currentMode == DISPLAY_POWER_SAVE) {
    setMode(DISPLAY_MAIN);
  }
}

void Display::update(const DisplayData& data) {
  // Throttle updates to reduce flicker
  const unsigned long now = millis();
  if (now - lastUpdate < DISPLAY_UPDATE_INTERVAL && currentMode != DISPLAY_MENU) {
    return;
  }
  
  if (!displayOn && currentMode != DISPLAY_POWER_SAVE) {
    return;
  }
  
  u8g2->clearBuffer();
  
  switch (currentMode) {
    case DISPLAY_MAIN:
      drawMainScreen(data);
      break;
      
    case DISPLAY_MENU:
      drawMenuScreen(data);
      break;
      
    case DISPLAY_DEBUG:
      drawDebugScreen(data);
      break;
      
    case DISPLAY_TIME_SET:
      drawTimeSetScreen(data);
      break;
      
    case DISPLAY_POWER_SAVE:
      drawPowerSaveScreen();
      break;
  }
  
  u8g2->sendBuffer();
  lastUpdate = now;
}

void Display::forceUpdate(const DisplayData& data) {
  lastUpdate = 0;
  update(data);
}

void Display::drawMainScreen(const DisplayData& data) {
  // Top row: Time and RTC status
  drawTimeInfo(data);
  
  // Middle section: Temperature info
  drawTemperatureInfo(data);
  
  // Bottom section: Heater status and delay info
  drawHeaterStatus(data);
  if (data.heaterDelayActive) {
    drawDelayInfo(data);
  }
}

void Display::drawTimeInfo(const DisplayData& data) {
  char timeStr[16];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", data.hour, data.minute);
  
  u8g2->setFont(FONT_SMALL);
  u8g2->drawStr(2, 12, timeStr);
  
  // RTC status indicator (temporary text for testing)
  if (data.rtcWorking) {
    u8g2->drawStr(45, 12, "T");  // T for Time
  } else {
    // Draw warning indicator for RTC issues
    u8g2->drawStr(45, 12, "!");
  }
  
  // Show menu indicator if menu active
  if (data.menuActive) {
    u8g2->drawStr(110, 12, "MENU");
  }
}

void Display::drawTemperatureInfo(const DisplayData& data) {
  // Thermometer icon (temporary text for testing)
  u8g2->setFont(FONT_SMALL);
  u8g2->drawStr(8, 30, "TEMP");
  
  // Current temperature - large font (debug with simpler format)
  u8g2->setFont(FONT_LARGE);
  char tempStr[16];
  
  // Debug: Check if we have valid temperature data
  if (data.cabinTemp > -50 && data.cabinTemp < 100) {
    // Try integer format first to isolate floating point issues
    int tempInt = (int)data.cabinTemp;
    snprintf(tempStr, sizeof(tempStr), "%dC", tempInt);
  } else {
    // Show error indicator
    snprintf(tempStr, sizeof(tempStr), "ERR");
  }
  
  u8g2->drawStr(35, 38, tempStr);
  
  // Target temperature - smaller font (simplified)
  u8g2->setFont(FONT_MEDIUM);
  int targetInt = (int)data.targetTemp;
  snprintf(tempStr, sizeof(tempStr), ">%dC", targetInt);
  u8g2->drawStr(90, 32, tempStr);
  
  // Temperature difference for debug
  if (data.showDebug) {
    float diff = data.targetTemp - data.cabinTemp;
    snprintf(tempStr, sizeof(tempStr), "D%.1fC", diff);
    u8g2->setFont(FONT_SMALL);
    u8g2->drawStr(90, 44, tempStr);
  }
}

void Display::drawHeaterStatus(const DisplayData& data) {
  const int iconX = 8;
  const int iconY = 45;
  
  // Draw heater icon based on state
  drawHeaterIcon(iconX, iconY, data.heaterState);
  
  // Status text
  u8g2->setFont(FONT_MEDIUM);
  const char* statusText = "OFF";
  
  if (data.heaterEnabled) {
    switch (data.heaterState) {
      case HS_LOW:  statusText = "LOW";  break;
      case HS_MED:  statusText = "MED";  break;
      case HS_HIGH: statusText = "HIGH"; break;
      case HS_OFF:  
        statusText = data.heaterDelayActive ? "WAIT" : "OFF";
        break;
    }
  } else {
    statusText = "DISABLED";
  }
  
  u8g2->drawStr(35, 58, statusText);
}

void Display::drawHeaterIcon(int x, int y, HeatState state) {
  // Temporary text icons for testing
  u8g2->setFont(FONT_SMALL);
  switch (state) {
    case HS_OFF:
      u8g2->drawStr(x, y+8, "OFF");
      break;
    case HS_LOW:
      u8g2->drawStr(x, y+8, "LO");
      break;
    case HS_MED:
      u8g2->drawStr(x, y+8, "MED");
      break;
    case HS_HIGH:
      u8g2->drawStr(x, y+8, "HI");
      break;
  }
}

void Display::drawDelayInfo(const DisplayData& data) {
  u8g2->setFont(FONT_SMALL);
  
  if (data.delayRemaining > 0) {
    char delayStr[16];
    unsigned long seconds = data.delayRemaining / 1000;
    
    if (seconds > 60) {
      snprintf(delayStr, sizeof(delayStr), "%lum%lus", seconds / 60, seconds % 60);
    } else {
      snprintf(delayStr, sizeof(delayStr), "%lus", seconds);
    }
    
    drawRightAlignedText(delayStr, 125, 58);
  }
}

void Display::drawMenuScreen(const DisplayData& data) {
  if (data.inWakeupFlow) {
    // Draw wake-up timer creation flow
    drawWakeupTimerFlow(data);
  } else if (data.inSubMenu) {
    // Draw sub-menu for value adjustment
    u8g2->setFont(FONT_MEDIUM);
    drawCenteredText("SET TARGET", 16);
    
    // Show current value being adjusted
    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%d°C", data.subMenuValue);
    
    u8g2->setFont(FONT_LARGE);
    drawCenteredText(valueStr, 40);
    
    // Show range
    u8g2->setFont(FONT_SMALL);
    char rangeStr[32];
    snprintf(rangeStr, sizeof(rangeStr), "Range: %d-%d°C", data.subMenuMin, data.subMenuMax);
    drawCenteredText(rangeStr, 52);
    
    drawCenteredText("Rotate: adjust, Press: save", 62);
  } else {
    // Draw main menu
    u8g2->setFont(FONT_MEDIUM);
    drawCenteredText("MENU", 16);
    
    // Draw menu items (only visible ones)
    u8g2->setFont(FONT_SMALL);
    const int startY = 28;
    const int lineHeight = 10;
    
    // Calculate visible items
    int visibleStart = data.menuScrollOffset;
    int visibleEnd = min(visibleStart + MAX_VISIBLE_MENU_ITEMS, data.menuCount);
    
    for (int i = visibleStart; i < visibleEnd; i++) {
      int displayIndex = i - visibleStart;  // 0-based index for display
      int y = startY + (displayIndex * lineHeight);
      
      // Highlight selected item
      if (i == data.menuIndex) {
        u8g2->drawStr(2, y, ">");
        u8g2->drawStr(10, y, data.menuItems[i]);
      } else {
        u8g2->drawStr(10, y, data.menuItems[i]);
      }
    }
    
    // Draw scroll indicators
    if (data.menuCount > MAX_VISIBLE_MENU_ITEMS) {
      // Show up arrow if not at top
      if (data.menuScrollOffset > 0) {
        u8g2->drawStr(120, 25, "^");
      }
      
      // Show down arrow if not at bottom
      if (data.menuScrollOffset < data.menuCount - MAX_VISIBLE_MENU_ITEMS) {
        u8g2->drawStr(120, 60, "v");
      }
      
      // Show scroll position indicator
      char scrollInfo[8];
      snprintf(scrollInfo, sizeof(scrollInfo), "%d/%d", data.menuIndex + 1, data.menuCount);
      u8g2->setFont(FONT_SMALL);
      u8g2->drawStr(85, 16, scrollInfo);
    }
  }
}

void Display::drawWakeupTimerFlow(const DisplayData& data) {
  // Step indicators at top
  u8g2->setFont(FONT_SMALL);
  char stepStr[16];
  snprintf(stepStr, sizeof(stepStr), "Step %d/4", data.wakeupFlowStep + 1);
  drawCenteredText(stepStr, 12);
  
  // Main content based on current step
  u8g2->setFont(FONT_MEDIUM);
  char titleStr[20];
  char valueStr[16];
  char helpStr[32];
  
  switch (data.wakeupFlowStep) {
    case 0:  // Set hour
      strcpy(titleStr, "Set Hour");
      snprintf(valueStr, sizeof(valueStr), "%02d:xx", data.subMenuValue);
      strcpy(helpStr, "Range: 0-23");
      break;
      
    case 1:  // Set minute
      strcpy(titleStr, "Set Minute");
      snprintf(valueStr, sizeof(valueStr), "%02d:%02d", data.wakeupHour, data.subMenuValue);
      strcpy(helpStr, "Range: 0-59");
      break;
      
    case 2:  // Set temperature
      strcpy(titleStr, "Target Temp");
      snprintf(valueStr, sizeof(valueStr), "%d°C", data.subMenuValue);
      snprintf(helpStr, sizeof(helpStr), "Range: %d-%d°C", data.subMenuMin, data.subMenuMax);
      break;
      
    case 3:  // Set days
      strcpy(titleStr, "Schedule");
      if (data.subMenuValue == 0) {
        strcpy(valueStr, "Weekdays");
      } else if (data.subMenuValue == 1) {
        strcpy(valueStr, "Weekend");
      } else {
        strcpy(valueStr, "Daily");
      }
      strcpy(helpStr, "0=Week 1=End 2=Daily");
      break;
      
    case 4:  // Confirm
      strcpy(titleStr, "Create Timer?");
      strcpy(valueStr, data.subMenuValue ? "YES" : "NO");
      snprintf(helpStr, sizeof(helpStr), "%02d:%02d %d°C", 
               data.wakeupHour, data.wakeupMinute, data.wakeupTemp);
      break;
      
    default:
      strcpy(titleStr, "Error");
      strcpy(valueStr, "---");
      strcpy(helpStr, "");
      break;
  }
  
  // Draw the screen
  drawCenteredText(titleStr, 24);
  
  u8g2->setFont(FONT_LARGE);
  drawCenteredText(valueStr, 42);
  
  u8g2->setFont(FONT_SMALL);
  drawCenteredText(helpStr, 54);
  drawCenteredText("Press: Next, Long: Cancel", 62);
}

void Display::drawDebugScreen(const DisplayData& data) {
  u8g2->setFont(FONT_SMALL);
  drawCenteredText("DEBUG INFO", 12);
  
  u8g2->drawStr(2, 24, data.debugLine1);
  u8g2->drawStr(2, 36, data.debugLine2);
  u8g2->drawStr(2, 48, data.debugLine3);
  
  // Instructions
  u8g2->drawStr(2, 60, "Long press to exit");
}

void Display::drawTimeSetScreen(const DisplayData& data) {
  u8g2->setFont(FONT_MEDIUM);
  drawCenteredText("SET TIME", 16);
  
  // Show current time being set
  char timeStr[16];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", data.hour, data.minute);
  
  u8g2->setFont(FONT_LARGE);
  drawCenteredText(timeStr, 40);
  
  u8g2->setFont(FONT_SMALL);
  drawCenteredText("Rotate to adjust", 52);
  drawCenteredText("Press to confirm", 62);
}

void Display::drawPowerSaveScreen() {
  // Blank screen for power saving
  // u8g2 buffer is already cleared, so just send empty buffer
}

void Display::drawCenteredText(const char* text, int y) {
  int width = u8g2->getStrWidth(text);
  int x = (SCREEN_WIDTH - width) / 2;
  u8g2->drawStr(x, y, text);
}

void Display::drawRightAlignedText(const char* text, int x, int y) {
  int width = u8g2->getStrWidth(text);
  u8g2->drawStr(x - width, y, text);
}

void Display::clear() {
  u8g2->clearBuffer();
  u8g2->sendBuffer();
}

void Display::setBrightness(uint8_t level) {
  u8g2->setContrast(level);
}

void Display::printStatus() const {
  DEBUG_PRINT_F("Display Status - Mode: ");
  DEBUG_PRINT(currentMode);
  DEBUG_PRINT_F(" On: ");
  DEBUG_PRINT(displayOn);
  DEBUG_PRINT_F(" Last update: ");
  DEBUG_PRINT(millis() - lastUpdate);
  DEBUG_PRINTLN_F("ms ago");
}