#ifndef CONFIG_H
#define CONFIG_H

// Pin definitions
#define PIN_BACKLIGHT 5
#define PIN_BUTTON 4

// Font size definitions
#define FONT_MICRO 1     // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define FONT_SMALL 2     // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define FONT_DEFAULT 4   // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define FONT_DIGIT 7     // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.

// WiFi settings
#define WIFI_AP_NAME "SmartClock-Setup"
#define WIFI_AP_PASSWORD "smartclock123"
#define WIFI_TIMEOUT 180
#define WIFI_RETRY_ATTEMPTS 5
#define WIFI_RETRY_DELAY_MS 2000
#define WIFI_CONNECTION_TIMEOUT 30000  // 30 seconds per attempt

// OTA settings
#define OTA_HOSTNAME "smartclock"
#define OTA_PASSWORD "admin"

// Web server
#define WEB_SERVER_PORT 80

// Update intervals
#define DISPLAY_UPDATE_INTERVAL 1000UL
#define WEATHER_UPDATE_INTERVAL 900000UL

// Button settings
#define BUTTON_DEBOUNCE_MS 50UL
#define BUTTON_SHORT_PRESS_MAX_MS 800UL
#define BUTTON_LONG_PRESS_MIN_MS 2000UL

// Defaults
#define DEFAULT_TIMEZONE "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"
#define DEFAULT_BRIGHTNESS 50

// Animation settings
#define ANIMATION_STEPS 20 // Higher number - smoother animation
#define ANIMATION_STEP_DELAY 20 // Lower number - faster animation

#endif
