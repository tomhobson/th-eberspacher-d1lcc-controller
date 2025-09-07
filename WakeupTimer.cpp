#include "WakeupTimer.h"
#include <limits.h>

WakeupTimer::WakeupTimer(RTCManager* rtcMgr) 
  : rtcManager(rtcMgr), timerCount(0), activeTimerIndex(-1), lastUpdateTime(0),
    alarm1InUse(false), alarm2InUse(false), alarm1TimerIndex(-1), alarm2TimerIndex(-1) {
  // Initialize all timers as disabled
  for (uint8_t i = 0; i < MAX_WAKEUP_TIMERS; i++) {
    timers[i].enabled = false;
    timers[i].hour = 7;  // Default 7:00 AM
    timers[i].minute = 0;
    timers[i].targetTemp = 20;  // Default 20°C
    timers[i].dayMask = 0x3E;   // Monday-Friday (bits 1-5)
    timers[i].state = WAKEUP_DISABLED;
    strcpy(timers[i].name, "Timer");
  }
}

bool WakeupTimer::begin() {
  if (!rtcManager) {
    DEBUG_PRINTLN_F("ERR: No RTCMgr");
    return false;
  }
  
  DEBUG_PRINTLN_F("WakeupTimer OK");
  return true;
}

void WakeupTimer::update(float currentTemp) {
  const unsigned long now = millis();
  if (now - lastUpdateTime < 5000) {  // Update every 5 seconds
    return;
  }
  lastUpdateTime = now;
  
  if (!rtcManager->hasValidTime()) {
    return;  // Can't update timers without valid time
  }
  
  DateTime currentTime = rtcManager->getStableTime();
  
  // Update all timer states
  updateTimerStates(currentTime, currentTemp);
  
  // Check for new active timer
  checkForNewActiveTimer(currentTime);
  
  // Reset expired timers for next day
  resetExpiredTimers(currentTime);
}

bool WakeupTimer::addTimer(uint8_t hour, uint8_t minute, uint8_t targetTemp, uint8_t dayMask, const char* name) {
  if (timerCount >= MAX_WAKEUP_TIMERS) {
    DEBUG_PRINTLN_F("ERR: Max timers");
    return false;
  }
  
  if (!isValidTime(hour, minute) || !isValidTemp(targetTemp)) {
    DEBUG_PRINTLN_F("ERR: Bad params");
    return false;
  }
  
  // Find first available slot
  for (uint8_t i = 0; i < MAX_WAKEUP_TIMERS; i++) {
    if (!timers[i].enabled) {
      timers[i].enabled = true;
      timers[i].hour = hour;
      timers[i].minute = minute;
      timers[i].targetTemp = targetTemp;
      timers[i].dayMask = dayMask;
      timers[i].state = WAKEUP_ARMED;
      
      // Set name
      if (name && strlen(name) > 0) {
        strncpy(timers[i].name, name, 15);
        timers[i].name[15] = '\0';
      } else {
        snprintf(timers[i].name, sizeof(timers[i].name), "Timer %d", i + 1);
      }
      
      timerCount++;
      #if DEBUG_ENABLED
        Serial.print(F("T+:"));
        Serial.println(timers[i].name);
      #endif
      return true;
    }
  }
  
  return false;
}

bool WakeupTimer::removeTimer(uint8_t index) {
  if (!isValidTimerIndex(index) || !timers[index].enabled) {
    return false;
  }
  
  timers[index].enabled = false;
  timers[index].state = WAKEUP_DISABLED;
  
  if (activeTimerIndex == index) {
    activeTimerIndex = -1;  // Clear active timer
  }
  
  timerCount--;
  #if DEBUG_ENABLED
    Serial.print(F("T-:"));
    Serial.println(index);
  #endif
  return true;
}

bool WakeupTimer::enableTimer(uint8_t index, bool enabled) {
  if (!isValidTimerIndex(index)) {
    return false;
  }
  
  if (timers[index].enabled != enabled) {
    timers[index].enabled = enabled;
    timers[index].state = enabled ? WAKEUP_ARMED : WAKEUP_DISABLED;
    
    if (!enabled && activeTimerIndex == index) {
      activeTimerIndex = -1;  // Clear active timer
    }
    
    if (enabled) {
      timerCount++;
    } else {
      timerCount--;
    }
  }
  
  return true;
}

void WakeupTimer::clearAllTimers() {
  for (uint8_t i = 0; i < MAX_WAKEUP_TIMERS; i++) {
    timers[i].enabled = false;
    timers[i].state = WAKEUP_DISABLED;
  }
  timerCount = 0;
  activeTimerIndex = -1;
  DEBUG_PRINTLN_F("Timers cleared");
}

bool WakeupTimer::setTimerTime(uint8_t index, uint8_t hour, uint8_t minute) {
  if (!isValidTimerIndex(index) || !isValidTime(hour, minute)) {
    return false;
  }
  
  timers[index].hour = hour;
  timers[index].minute = minute;
  return true;
}

bool WakeupTimer::setTimerTemp(uint8_t index, uint8_t targetTemp) {
  if (!isValidTimerIndex(index) || !isValidTemp(targetTemp)) {
    return false;
  }
  
  timers[index].targetTemp = targetTemp;
  return true;
}

bool WakeupTimer::setTimerDays(uint8_t index, uint8_t dayMask) {
  if (!isValidTimerIndex(index)) {
    return false;
  }
  
  timers[index].dayMask = dayMask;
  return true;
}

bool WakeupTimer::setTimerName(uint8_t index, const char* name) {
  if (!isValidTimerIndex(index) || !name) {
    return false;
  }
  
  strncpy(timers[index].name, name, 15);
  timers[index].name[15] = '\0';
  return true;
}

WakeupTimerData* WakeupTimer::getTimer(uint8_t index) {
  if (!isValidTimerIndex(index)) {
    return nullptr;
  }
  return &timers[index];
}

int8_t WakeupTimer::getActiveTimerIndex() const {
  return activeTimerIndex;
}

WakeupState WakeupTimer::getActiveState() const {
  if (activeTimerIndex >= 0 && activeTimerIndex < MAX_WAKEUP_TIMERS) {
    return timers[activeTimerIndex].state;
  }
  return WAKEUP_DISABLED;
}

bool WakeupTimer::shouldHeat() const {
  if (activeTimerIndex < 0) {
    return false;
  }
  
  WakeupState state = timers[activeTimerIndex].state;
  return (state == WAKEUP_PREHEATING || state == WAKEUP_READY);
}

uint8_t WakeupTimer::getActiveTargetTemp() const {
  if (activeTimerIndex >= 0 && activeTimerIndex < MAX_WAKEUP_TIMERS) {
    return timers[activeTimerIndex].targetTemp;
  }
  return 20;  // Default fallback
}

bool WakeupTimer::isTimeToStart(const WakeupTimerData& timer, const DateTime& now) const {
  if (!timer.enabled || !isTimerDayActive(timer, now)) {
    return false;
  }
  
  // Calculate start time (preheat time before target)
  DateTime startTime = calculateStartTime(timer, now);
  
  // Check if current time is at or after start time but before target time
  return (now.hour() > startTime.hour() || 
          (now.hour() == startTime.hour() && now.minute() >= startTime.minute())) &&
         (now.hour() < timer.hour || 
          (now.hour() == timer.hour && now.minute() < timer.minute));
}

bool WakeupTimer::isTimeToStop(const WakeupTimerData& timer, const DateTime& now) const {
  // Stop 1 hour after target time
  uint8_t stopHour = timer.hour + 1;
  if (stopHour >= 24) stopHour = 0;
  
  return (now.hour() > stopHour || 
          (now.hour() == stopHour && now.minute() >= timer.minute));
}

unsigned long WakeupTimer::getMinutesUntilNextTimer() const {
  if (timerCount == 0 || !rtcManager->hasValidTime()) {
    return 0;
  }
  
  DateTime now = rtcManager->getStableTime();
  unsigned long minMinutes = 0xFFFFFFFF;  // Maximum unsigned long value
  
  for (uint8_t i = 0; i < MAX_WAKEUP_TIMERS; i++) {
    if (!timers[i].enabled) continue;
    
    // Calculate next occurrence of this timer
    DateTime nextTime = calculateStartTime(timers[i], now);
    
    // Calculate minutes until next occurrence
    unsigned long minutes = (nextTime.unixtime() - now.unixtime()) / 60;
    
    if (minutes < minMinutes) {
      minMinutes = minutes;
    }
  }
  
  return (minMinutes == 0xFFFFFFFF) ? 0 : minMinutes;
}

// Static utility methods
bool WakeupTimer::isDayEnabled(uint8_t dayMask, WakeupDay day) {
  return (dayMask & (1 << day)) != 0;
}

uint8_t WakeupTimer::setDayEnabled(uint8_t dayMask, WakeupDay day, bool enabled) {
  if (enabled) {
    return dayMask | (1 << day);
  } else {
    return dayMask & ~(1 << day);
  }
}

WakeupDay WakeupTimer::getCurrentDay(const DateTime& dt) {
  return static_cast<WakeupDay>(dt.dayOfTheWeek());
}

// Private methods
void WakeupTimer::updateTimerStates(const DateTime& now, float currentTemp) {
  for (uint8_t i = 0; i < MAX_WAKEUP_TIMERS; i++) {
    if (!timers[i].enabled) continue;
    
    switch (timers[i].state) {
      case WAKEUP_ARMED:
        if (isTimeToStart(timers[i], now)) {
          timers[i].state = WAKEUP_PREHEATING;
          #if DEBUG_ENABLED
            Serial.print(F("T"));
            Serial.print(i);
            Serial.println(F(" heat"));
          #endif
        }
        break;
        
      case WAKEUP_PREHEATING:
        if (currentTemp >= timers[i].targetTemp - 1.0) {  // Within 1°C
          timers[i].state = WAKEUP_READY;
          #if DEBUG_ENABLED
            Serial.print(F("T"));
            Serial.print(i);
            Serial.println(F(" ready"));
          #endif
        } else if (isTimeToStop(timers[i], now)) {
          timers[i].state = WAKEUP_EXPIRED;
        }
        break;
        
      case WAKEUP_READY:
        if (isTimeToStop(timers[i], now)) {
          timers[i].state = WAKEUP_EXPIRED;
          #if DEBUG_ENABLED
            Serial.print(F("T"));
            Serial.print(i);
            Serial.println(F(" exp"));
          #endif
        }
        break;
        
      case WAKEUP_EXPIRED:
        // Will be reset by resetExpiredTimers
        break;
        
      default:
        break;
    }
  }
}

void WakeupTimer::checkForNewActiveTimer(const DateTime& now) {
  // Find highest priority active timer
  int8_t newActiveIndex = -1;
  
  for (uint8_t i = 0; i < MAX_WAKEUP_TIMERS; i++) {
    if (!timers[i].enabled) continue;
    
    if (timers[i].state == WAKEUP_PREHEATING || timers[i].state == WAKEUP_READY) {
      if (newActiveIndex == -1 || timers[i].hour < timers[newActiveIndex].hour ||
          (timers[i].hour == timers[newActiveIndex].hour && timers[i].minute < timers[newActiveIndex].minute)) {
        newActiveIndex = i;
      }
    }
  }
  
  if (newActiveIndex != activeTimerIndex) {
    activeTimerIndex = newActiveIndex;
    if (activeTimerIndex >= 0) {
      #if DEBUG_ENABLED
        Serial.print(F("Act:"));
        Serial.println(timers[activeTimerIndex].name);
      #endif
    }
  }
}

void WakeupTimer::resetExpiredTimers(const DateTime& now) {
  for (uint8_t i = 0; i < MAX_WAKEUP_TIMERS; i++) {
    if (timers[i].enabled && timers[i].state == WAKEUP_EXPIRED) {
      // Reset for next occurrence
      timers[i].state = WAKEUP_ARMED;
    }
  }
}

bool WakeupTimer::isTimerDayActive(const WakeupTimerData& timer, const DateTime& now) const {
  WakeupDay today = getCurrentDay(now);
  return isDayEnabled(timer.dayMask, today);
}

DateTime WakeupTimer::calculateStartTime(const WakeupTimerData& timer, const DateTime& now) const {
  // Start heating WAKEUP_PREHEAT_MINUTES before target time
  uint8_t startHour = timer.hour;
  uint8_t startMinute = timer.minute;
  
  if (startMinute >= WAKEUP_PREHEAT_MINUTES) {
    startMinute -= WAKEUP_PREHEAT_MINUTES;
  } else {
    startMinute = 60 - (WAKEUP_PREHEAT_MINUTES - startMinute);
    if (startHour == 0) {
      startHour = 23;
    } else {
      startHour--;
    }
  }
  
  return DateTime(now.year(), now.month(), now.day(), startHour, startMinute, 0);
}

bool WakeupTimer::isValidTimerIndex(uint8_t index) const {
  return index < MAX_WAKEUP_TIMERS;
}

bool WakeupTimer::isValidTime(uint8_t hour, uint8_t minute) const {
  return hour < 24 && minute < 60;
}

bool WakeupTimer::isValidTemp(uint8_t temp) const {
  return temp >= MIN_WAKEUP_TEMP && temp <= MAX_WAKEUP_TEMP;
}

void WakeupTimer::printStatus() const {
  #if DEBUG_ENABLED
    Serial.print(F("Timers: "));
    Serial.print(timerCount);
    Serial.print(F(" Active: "));
    Serial.println(activeTimerIndex);
  #endif
}

void WakeupTimer::printTimer(uint8_t index) const {
  #if DEBUG_ENABLED
    if (!isValidTimerIndex(index)) return;
    const WakeupTimerData& timer = timers[index];
    Serial.print(F("T"));
    Serial.print(index);
    Serial.print(F(":"));
    Serial.print(timer.hour);
    Serial.print(F(":"));
    if (timer.minute < 10) Serial.print(F("0"));
    Serial.print(timer.minute);
    Serial.print(F("->"));
    Serial.print(timer.targetTemp);
    Serial.println(F("C"));
  #endif
}