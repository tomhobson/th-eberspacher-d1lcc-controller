#include "RTCManager.h"

RTCManager::RTCManager(RTC_DS3231* rtcPtr)
  : rtc(rtcPtr), rtcInitialized(false), rtcWorking(true),
    lastGoodYear(2024), lastGoodMonth(1), lastGoodDay(1),
    lastGoodHour(12), lastGoodMinute(0), lastRtcRead(0) {
}

bool RTCManager::begin() {
  if (!rtc->begin()) {
    DEBUG_PRINTLN(F("WARN: No RTC"));
    rtcInitialized = false;
    rtcWorking = false;
    return false;
  }
  
  rtcInitialized = true;
  
  if (rtc->lostPower()) {
    DEBUG_PRINTLN(F("RTC lost pwr"));
    setTimeFromCompile();
  }
  
  // Wait a moment for RTC to stabilize
  delay(100);
  
  // Test RTC by getting initial time
  DateTime now = rtc->now();
  if (isValidTime(now)) {
    storeGoodTime(now);
    rtcWorking = true;
    DEBUG_PRINTLN(F("RTC OK"));
  } else {
    rtcWorking = false;
    DEBUG_PRINTLN(F("RTC bad time"));
  }
  
  return rtcInitialized;
}

bool RTCManager::isValidTime(DateTime dt) const {
  // Check if year is reasonable
  if (dt.year() < RTC_VALID_YEAR_MIN || dt.year() > RTC_VALID_YEAR_MAX) {
    return false;
  }
  
  // Check month (1-12)
  if (dt.month() < 1 || dt.month() > 12) {
    return false;
  }
  
  // Check day (1-31)
  if (dt.day() < 1 || dt.day() > 31) {
    return false;
  }
  
  // Check hour (0-23)
  if (dt.hour() > 23) {
    return false;
  }
  
  // Check minute (0-59)
  if (dt.minute() > 59) {
    return false;
  }
  
  return true;
}

bool RTCManager::isReasonableTimeChange(DateTime newTime) const {
  // If we don't have a reference, accept first valid time
  if (lastRtcRead == 0) return true;
  
  // Calculate expected time progression
  unsigned long elapsedMs = millis() - lastRtcRead;
  unsigned long expectedMinuteChange = elapsedMs / 60000; // minutes
  
  // Create expected time based on last good time + elapsed
  DateTime expected(lastGoodYear, lastGoodMonth, lastGoodDay,
                    lastGoodHour, lastGoodMinute, 0);
  expected = expected + TimeSpan(0, 0, expectedMinuteChange, 0);
  
  // Check if new time is within reasonable range
  long timeDiff = abs((long)newTime.unixtime() - (long)expected.unixtime());
  bool reasonable = (timeDiff < RTC_TIME_JUMP_THRESHOLD);
  
  #if DEBUG_RTC
    if (!reasonable) {
      DEBUG_PRINT("Time jump detected: ");
      DEBUG_PRINT(timeDiff);
      DEBUG_PRINTLN(" seconds");
    }
  #endif
  
  return reasonable;
}

void RTCManager::storeGoodTime(DateTime dt) {
  lastGoodYear = dt.year();
  lastGoodMonth = dt.month();
  lastGoodDay = dt.day();
  lastGoodHour = dt.hour();
  lastGoodMinute = dt.minute();
  lastRtcRead = millis();
}

DateTime RTCManager::getFallbackTime() const {
  return DateTime(lastGoodYear, lastGoodMonth, lastGoodDay,
                  lastGoodHour, lastGoodMinute, 0);
}

DateTime RTCManager::getStableTime() {
  if (!rtcInitialized) {
    // RTC not available, use stored fallback
    rtcWorking = false;
    return getFallbackTime();
  }
  
  DateTime now = rtc->now();
  
  if (isValidTime(now) && isReasonableTimeChange(now)) {
    // Time is valid and reasonable - store it
    storeGoodTime(now);
    rtcWorking = true;
    return now;
  } else {
    // Time is invalid or jumping - use stored time
    rtcWorking = false;
    
    #if DEBUG_RTC
      DEBUG_PRINTLN(F("RTC bad"));
    #endif
    
    return getFallbackTime();
  }
}

DateTime RTCManager::getCurrentTime() {
  if (!rtcInitialized) {
    return getFallbackTime();
  }
  return rtc->now();
}

bool RTCManager::hasValidTime() const {
  if (!rtcInitialized) return false;
  
  DateTime now = rtc->now();
  return isValidTime(now);
}

bool RTCManager::setTime(DateTime newTime) {
  if (!rtcInitialized) {
    DEBUG_PRINTLN(F("No RTC init"));
    return false;
  }
  
  if (!isValidTime(newTime)) {
    DEBUG_PRINTLN(F("Bad time"));
    return false;
  }
  
  rtc->adjust(newTime);
  storeGoodTime(newTime);
  rtcWorking = true;
  
  DEBUG_PRINTLN(F("RTC set"));
  return true;
}

bool RTCManager::setTimeFromCompile() {
  DateTime compileTime(F(__DATE__), F(__TIME__));
  return setTime(compileTime);
}

// Alarm functionality
bool RTCManager::setAlarm1(const DateTime& alarmTime, bool enableInterrupt) {
  if (!rtcInitialized) {
    DEBUG_PRINTLN_F("No RTC");
    return false;
  }
  
  // Set Alarm 1 to match hour, minute, and second
  if (!rtc->setAlarm1(alarmTime, DS3231_A1_Hour)) {
    DEBUG_PRINTLN_F("A1 fail");
    return false;
  }
  
  if (enableInterrupt) {
    rtc->writeSqwPinMode(DS3231_OFF);  // Disable square wave output
    // Enable alarm interrupt - this is typically done at the hardware level
  }
  
  #if DEBUG_RTC
    Serial.print(F("A1:"));
    Serial.print(alarmTime.hour());
    Serial.print(alarmTime.minute());
  #endif
  
  return true;
}

bool RTCManager::setAlarm2(const DateTime& alarmTime, bool enableInterrupt) {
  if (!rtcInitialized) {
    DEBUG_PRINTLN_F("No RTC");
    return false;
  }
  
  // Set Alarm 2 to match hour and minute (no seconds on Alarm 2)
  if (!rtc->setAlarm2(alarmTime, DS3231_A2_Hour)) {
    DEBUG_PRINTLN_F("A2 fail");
    return false;
  }
  
  if (enableInterrupt) {
    rtc->writeSqwPinMode(DS3231_OFF);  // Disable square wave output  
    // Enable alarm interrupt - this is typically done at the hardware level
  }
  
  #if DEBUG_RTC
    Serial.print(F("A2:"));
    Serial.print(alarmTime.hour());
    Serial.print(alarmTime.minute());
  #endif
  
  return true;
}

void RTCManager::clearAlarm1() {
  if (rtcInitialized) {
    rtc->disableAlarm(1);
    rtc->clearAlarm(1);
    DEBUG_PRINTLN_F("A1 clear");
  }
}

void RTCManager::clearAlarm2() {
  if (rtcInitialized) {
    rtc->disableAlarm(2);
    rtc->clearAlarm(2);
    DEBUG_PRINTLN_F("A2 clear");
  }
}

bool RTCManager::isAlarm1Triggered() {
  if (!rtcInitialized) return false;
  return rtc->alarmFired(1);
}

bool RTCManager::isAlarm2Triggered() {
  if (!rtcInitialized) return false;
  return rtc->alarmFired(2);
}

void RTCManager::clearAlarmFlags() {
  if (rtcInitialized) {
    rtc->clearAlarm(1);
    rtc->clearAlarm(2);
  }
}

bool RTCManager::enableAlarmInterrupt(uint8_t alarmNumber, bool enable) {
  if (!rtcInitialized || (alarmNumber != 1 && alarmNumber != 2)) {
    return false;
  }
  
  // For RTClib, alarm interrupts are enabled by setting the alarm
  // and configuring the INT/SQW pin mode
  if (enable) {
    rtc->writeSqwPinMode(DS3231_OFF);  // Disable square wave, enable alarms
  } else {
    rtc->disableAlarm(alarmNumber);
  }
  
  return true;
}

bool RTCManager::isAlarmInterruptEnabled(uint8_t alarmNumber) const {
  if (!rtcInitialized || (alarmNumber != 1 && alarmNumber != 2)) {
    return false;
  }
  
  // Read control register to check interrupt enable bits
  // This is implementation-specific to RTCLib
  return true; // Simplified for now
}

void RTCManager::printStatus() const {
  DEBUG_PRINT("RTCManager Status - Initialized: ");
  DEBUG_PRINT(rtcInitialized);
  DEBUG_PRINT(" Working: ");
  DEBUG_PRINT(rtcWorking);
  DEBUG_PRINT(" Last good: ");
  DEBUG_PRINT(lastGoodYear);
  DEBUG_PRINT("/");
  DEBUG_PRINT(lastGoodMonth);
  DEBUG_PRINT("/");
  DEBUG_PRINT(lastGoodDay);
  DEBUG_PRINT(" ");
  DEBUG_PRINT(lastGoodHour);
  DEBUG_PRINT(":");
  DEBUG_PRINTLN(lastGoodMinute);
}

void RTCManager::printTimeInfo(DateTime dt) const {
  DEBUG_PRINT("Time: ");
  DEBUG_PRINT(dt.year());
  DEBUG_PRINT("/");
  DEBUG_PRINT(dt.month());
  DEBUG_PRINT("/");
  DEBUG_PRINT(dt.day());
  DEBUG_PRINT(" ");
  DEBUG_PRINT(dt.hour());
  DEBUG_PRINT(":");
  DEBUG_PRINT(dt.minute());
  DEBUG_PRINT(":");
  DEBUG_PRINT(dt.second());
  DEBUG_PRINT(" Valid: ");
  DEBUG_PRINT(isValidTime(dt));
  DEBUG_PRINT(" Reasonable: ");
  DEBUG_PRINTLN(isReasonableTimeChange(dt));
}