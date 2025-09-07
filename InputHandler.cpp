#include "InputHandler.h"

InputHandler::InputHandler(ezButton* buttonPtr)
  : button(buttonPtr), rotaryDelta(0), lastRotaryTime(0),
    pressStartTime(0), lastReleaseTime(0), longPressTriggered(false),
    waitingForDoubleClick(false), lastActivityTime(0) {
}

void InputHandler::begin() {
  // Initialize button
  button->setDebounceTime(DEBOUNCE_TIME);
  
  // Initialize rotary encoder pins
  pinMode(ENCODER_CLK_PIN, INPUT);
  pinMode(ENCODER_DT_PIN, INPUT);
  
  recordActivity();
  DEBUG_PRINTLN("InputHandler initialized");
}

void InputHandler::update() {
  button->loop();  // Update ezButton state
}

void InputHandler::handleRotaryInterrupt(int direction) {
  const unsigned long now = millis();
  
  // Debounce rotary encoder
  if ((now - lastRotaryTime) < DEBOUNCE_TIME) return;
  
  rotaryDelta += direction;
  lastRotaryTime = now;
  recordActivity();
  
  #if DEBUG_INPUT
    DEBUG_PRINT("Rotary: ");
    DEBUG_PRINTLN(direction > 0 ? "CW" : "CCW");
  #endif
}

ButtonEvent InputHandler::checkButtonEvent() {
  if (button->isPressed()) {
    pressStartTime = millis();
    longPressTriggered = false;
    recordActivity();
    
    #if DEBUG_INPUT
      DEBUG_PRINTLN("Button pressed");
    #endif
  }
  
  if (button->isReleased()) {
    unsigned long pressDuration = millis() - pressStartTime;
    lastReleaseTime = millis();
    recordActivity();
    
    #if DEBUG_INPUT
      DEBUG_PRINT("Button released after ");
      DEBUG_PRINT(pressDuration);
      DEBUG_PRINTLN("ms");
    #endif
    
    if (longPressTriggered) {
      // Long press already handled during press
      return BUTTON_NONE;
    }
    
    if (pressDuration >= BUTTON_LONG_PRESS_TIME) {
      return BUTTON_LONG_PRESS;
    }
    
    // Check for double click
    if (waitingForDoubleClick) {
      waitingForDoubleClick = false;
      return BUTTON_DOUBLE_CLICK;
    } else {
      waitingForDoubleClick = true;
      return BUTTON_NONE; // Wait to see if double click comes
    }
  }
  
  // Check for long press during hold
  if (button->getState() == LOW && !longPressTriggered) {
    unsigned long pressDuration = millis() - pressStartTime;
    if (pressDuration >= BUTTON_LONG_PRESS_TIME) {
      longPressTriggered = true;
      return BUTTON_LONG_PRESS;
    }
  }
  
  // Handle double click timeout
  if (waitingForDoubleClick && 
      (millis() - lastReleaseTime) > BUTTON_DOUBLE_CLICK_TIME) {
    waitingForDoubleClick = false;
    return BUTTON_SHORT_PRESS;
  }
  
  return BUTTON_NONE;
}

RotaryEvent InputHandler::checkRotaryEvent() {
  if (rotaryDelta > 0) {
    rotaryDelta--;
    return ROTARY_CW;
  } else if (rotaryDelta < 0) {
    rotaryDelta++;
    return ROTARY_CCW;
  }
  return ROTARY_NONE;
}

ButtonEvent InputHandler::getButtonEvent() {
  return checkButtonEvent();
}

RotaryEvent InputHandler::getRotaryEvent() {
  return checkRotaryEvent();
}

bool InputHandler::hasActivity() {
  // Check if there's any pending input
  return (rotaryDelta != 0) || (button->getState() == LOW) || 
         waitingForDoubleClick;
}

void InputHandler::recordActivity() {
  lastActivityTime = millis();
}

void InputHandler::printStatus() const {
  DEBUG_PRINT("InputHandler - Button: ");
  DEBUG_PRINT(button->getState());
  DEBUG_PRINT(" Rotary: ");
  DEBUG_PRINT(rotaryDelta);
  DEBUG_PRINT(" Activity: ");
  DEBUG_PRINT(millis() - lastActivityTime);
  DEBUG_PRINTLN("ms ago");
}