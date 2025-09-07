/*
 * Eberspächer Temperature Controller v1.0
 * 
 * A comprehensive temperature control system for Eberspächer D1LC heaters
 * featuring OOP architecture, menu system, power management, and RTC support.
 * 
 * Hardware:
 * - Arduino (Uno/Nano/Pro Mini)
 * - SH1106 128x64 OLED Display (I2C)
 * - DS18B20 Temperature Sensor
 * - DS3231 RTC Module
 * - DS3502 Digital Potentiometer
 * - Rotary Encoder with Button
 * - Eberspächer D1LC Heater Interface
 * 
 * Features:
 * - Temperature-based heater control with hysteresis
 * - Menu system for configuration
 * - Power management with sleep modes
 * - RTC time management with anti-jump protection
 * - Comprehensive error handling and diagnostics
 */

#include "EberspracherController.h"

// Global system controller instance
EberspracherController controller;

void setup() {
  // Initialize the main controller
  if (!controller.begin()) {
    // System initialization failed - enter error mode
    Serial.println("FATAL: System initialization failed!");
    while (true) {
      // Blink built-in LED to indicate error
      digitalWrite(LED_BUILTIN, HIGH);
      delay(250);
      digitalWrite(LED_BUILTIN, LOW);
      delay(250);
    }
  }
  
  // Setup rotary encoder interrupt
  pinMode(ENCODER_CLK_PIN, INPUT);
  pinMode(ENCODER_DT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), rotaryISR, CHANGE);
  
  Serial.println("System ready!");
  
  // Optional: Run initial diagnostics
  #if DEBUG_ENABLED
    controller.runDiagnostics();
  #endif
}

void loop() {
  // Main system loop - all logic is handled by the controller
  controller.loop();
  
  // Small delay to prevent excessive CPU usage
  delay(10);
}

// Interrupt Service Routine for rotary encoder
void rotaryISR() {
  if (g_controller) {
    g_controller->handleRotaryISR();
  }
}