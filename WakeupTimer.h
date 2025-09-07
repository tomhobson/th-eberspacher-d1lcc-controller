#ifndef WAKEUPTIMER_H
#define WAKEUPTIMER_H

#include "Config.h"
#include "RTCManager.h"

// Structure for a single wake-up timer
struct WakeupTimerData {
  bool enabled;
  uint8_t hour;           // Target wake-up hour (0-23)
  uint8_t minute;         // Target wake-up minute (0-59)
  uint8_t targetTemp;     // Target temperature to reach
  uint8_t dayMask;        // Bit mask for days of week (bit 0 = Sunday, bit 1 = Monday, etc.)
  WakeupState state;      // Current state of this timer
  char name[16];          // User-friendly name for the timer
};

class WakeupTimer {
  public:
    WakeupTimer(RTCManager* rtcMgr);
    
    // Core functionality
    bool begin();
    void update(float currentTemp);
    void handleAlarmInterrupt(uint8_t alarmNumber);  // Called when RTC alarm triggers
    
    // Timer management
    bool addTimer(uint8_t hour, uint8_t minute, uint8_t targetTemp, uint8_t dayMask, const char* name = "");
    bool removeTimer(uint8_t index);
    bool enableTimer(uint8_t index, bool enabled);
    void clearAllTimers();
    
    // Timer configuration
    bool setTimerTime(uint8_t index, uint8_t hour, uint8_t minute);
    bool setTimerTemp(uint8_t index, uint8_t targetTemp);
    bool setTimerDays(uint8_t index, uint8_t dayMask);
    bool setTimerName(uint8_t index, const char* name);
    
    // Status queries
    uint8_t getTimerCount() const { return timerCount; }
    WakeupTimerData* getTimer(uint8_t index);
    int8_t getActiveTimerIndex() const;
    WakeupState getActiveState() const;
    
    // Check if system should be heating for wake-up timer
    bool shouldHeat() const;
    uint8_t getActiveTargetTemp() const;
    
    // RTC Alarm management
    void scheduleNextAlarm();  // Program next alarm into RTC
    void clearRTCAlarms();     // Clear all RTC alarms
    
    // Time calculations
    bool isTimeToStart(const WakeupTimerData& timer, const DateTime& now) const;
    bool isTimeToStop(const WakeupTimerData& timer, const DateTime& now) const;
    unsigned long getMinutesUntilNextTimer() const;
    
    // Day of week utilities
    static bool isDayEnabled(uint8_t dayMask, WakeupDay day);
    static uint8_t setDayEnabled(uint8_t dayMask, WakeupDay day, bool enabled);
    static WakeupDay getCurrentDay(const DateTime& dt);
    
    // Debug and status
    void printStatus() const;
    void printTimer(uint8_t index) const;
    
  private:
    RTCManager* rtcManager;
    WakeupTimerData timers[MAX_WAKEUP_TIMERS];
    uint8_t timerCount;
    int8_t activeTimerIndex;
    unsigned long lastUpdateTime;
    
    // RTC alarm tracking
    bool alarm1InUse;          // Is Alarm 1 being used for wake-up?
    bool alarm2InUse;          // Is Alarm 2 being used for wake-up?
    int8_t alarm1TimerIndex;   // Which timer is using Alarm 1
    int8_t alarm2TimerIndex;   // Which timer is using Alarm 2
    
    // Internal helper methods
    void updateTimerStates(const DateTime& now, float currentTemp);
    void checkForNewActiveTimer(const DateTime& now);
    void resetExpiredTimers(const DateTime& now);
    bool isTimerDayActive(const WakeupTimerData& timer, const DateTime& now) const;
    DateTime calculateStartTime(const WakeupTimerData& timer, const DateTime& now) const;
    
    // RTC alarm helper methods
    bool setRTCAlarm(uint8_t alarmNumber, const DateTime& alarmTime, int8_t timerIndex);
    void clearRTCAlarm(uint8_t alarmNumber);
    DateTime findNextAlarmTime(int8_t& timerIndex) const;
    bool isAlarmAvailable() const;
    
    // Validation helpers
    bool isValidTimerIndex(uint8_t index) const;
    bool isValidTime(uint8_t hour, uint8_t minute) const;
    bool isValidTemp(uint8_t temp) const;
};

#endif // WAKEUPTIMER_H