#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include "Config.h"

class RTCManager {
private:
  // Hardware
  RTC_DS3231* rtc;
  
  // State tracking
  bool rtcInitialized;
  bool rtcWorking;
  
  // Anti-jump time validation
  uint16_t lastGoodYear;
  uint8_t lastGoodMonth;
  uint8_t lastGoodDay;
  uint8_t lastGoodHour;
  uint8_t lastGoodMinute;
  unsigned long lastRtcRead;
  
  // Internal helper methods
  bool isValidTime(DateTime dt) const;
  bool isReasonableTimeChange(DateTime newTime) const;
  void storeGoodTime(DateTime dt);
  DateTime getFallbackTime() const;
  
public:
  RTCManager(RTC_DS3231* rtcPtr);
  
  // Initialization
  bool begin();
  
  // Time retrieval
  DateTime getStableTime();
  DateTime getCurrentTime();  // Raw time without validation
  
  // Status
  bool isInitialized() const { return rtcInitialized; }
  bool isWorking() const { return rtcWorking; }
  bool hasValidTime() const;
  
  // Time setting
  bool setTime(DateTime newTime);
  bool setTimeFromCompile();
  
  // Alarm functionality
  bool setAlarm1(const DateTime& alarmTime, bool enableInterrupt = true);
  bool setAlarm2(const DateTime& alarmTime, bool enableInterrupt = true);
  void clearAlarm1();
  void clearAlarm2();
  bool isAlarm1Triggered();
  bool isAlarm2Triggered();
  void clearAlarmFlags();
  
  // Alarm interrupt control
  bool enableAlarmInterrupt(uint8_t alarmNumber, bool enable = true);
  bool isAlarmInterruptEnabled(uint8_t alarmNumber) const;
  
  // Diagnostics
  void printStatus() const;
  void printTimeInfo(DateTime dt) const;
};

#endif // RTC_MANAGER_H