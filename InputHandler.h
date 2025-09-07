#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>
#include <ezButton.h>
#include "Config.h"

enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_SHORT_PRESS,
  BUTTON_LONG_PRESS,
  BUTTON_DOUBLE_CLICK
};

enum RotaryEvent {
  ROTARY_NONE,
  ROTARY_CW,     // Clockwise
  ROTARY_CCW     // Counter-clockwise
};

class InputHandler {
private:
  // Hardware
  ezButton* button;
  volatile int rotaryDelta;
  volatile unsigned long lastRotaryTime;
  
  // Button state tracking
  unsigned long pressStartTime;
  unsigned long lastReleaseTime;
  bool longPressTriggered;
  bool waitingForDoubleClick;
  
  // Activity tracking
  unsigned long lastActivityTime;
  
  // Internal helper methods
  ButtonEvent checkButtonEvent();
  RotaryEvent checkRotaryEvent();
  
public:
  InputHandler(ezButton* buttonPtr);
  
  // Initialization
  void begin();
  
  // Main update method (call frequently)
  void update();
  
  // Event checking
  ButtonEvent getButtonEvent();
  RotaryEvent getRotaryEvent();
  bool hasActivity();
  
  // Activity tracking
  unsigned long getLastActivityTime() const { return lastActivityTime; }
  void recordActivity();
  
  // ISR handlers (called from main sketch)
  void handleRotaryInterrupt(int direction);
  
  // Debug
  void printStatus() const;
};

#endif // INPUT_HANDLER_H