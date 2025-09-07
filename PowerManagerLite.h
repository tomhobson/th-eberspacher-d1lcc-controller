#ifndef POWER_MANAGER_LITE_H
#define POWER_MANAGER_LITE_H

#include <Arduino.h>
#include "Config.h"

// Simplified power manager with minimal features
class PowerManagerLite {
private:
  unsigned long lastActivityTime;
  bool displayShouldBeOff;
  
public:
  PowerManagerLite() : lastActivityTime(0), displayShouldBeOff(false) {}
  
  void begin() { 
    lastActivityTime = millis(); 
  }
  
  void recordActivity() { 
    lastActivityTime = millis();
    displayShouldBeOff = false;
  }
  
  void update() {
    displayShouldBeOff = (millis() - lastActivityTime) > POWER_SAVE_TIMEOUT;
  }
  
  bool shouldDisplayBeOff() const { 
    return displayShouldBeOff; 
  }
  
  void setHeaterRunning(bool running) { 
    // Minimal implementation - just record activity if heater is running
    if (running) recordActivity();
  }
};

#endif // POWER_MANAGER_LITE_H