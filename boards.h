/*
 * boards.h - Board definitions for P1-LoRa-receiver MVP
 * Target: M5Stack Atom Lite (ESP32-PICO-D4, built-in RGB LED, no display, no LoRa on board)
 * See: https://docs.m5stack.com/en/core/ATOM%20Lite
 */
#pragma once

#define M5STACK_ATOM_LITE

// M5Stack Atom Lite pinout
#define BOARD_LED           27   // SK6812 RGB LED data pin
#define BOARD_LED_PIN       BOARD_LED
#define BOARD_BUTTON_PIN    39   // Built-in button

// No OLED on Atom Lite
#undef HAS_DISPLAY

// Placeholder for compatibility with original code (setLCD is no-op on Atom Lite)
inline void setLCD(int /*lcdState*/, unsigned long /*displayULong*/, int /*displayInt*/) {
  // No display on Atom Lite
}
