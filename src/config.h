#ifndef CONFIG_H
#define CONFIG_H

// Pin definitions
#define PIN_BACKLIGHT 5
#define PIN_BUTTON 4

// Display settings
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

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
#define DISPLAY_UPDATE_INTERVAL 60000

// Button settings
#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_SHORT_PRESS_MAX_MS 800
#define BUTTON_LONG_PRESS_MIN_MS 2000

// Filesystem
#define IMAGE_DIR "/image/"

#endif
