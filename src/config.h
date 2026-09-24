#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// Pin definitions
constexpr int PIN_BACKLIGHT = 5;
constexpr int PIN_BUTTON = 4;

// Font size definitions
constexpr int FONT_MICRO   = 1;  // Original Adafruit 8px font, ~1820 bytes flash
constexpr int FONT_SMALL   = 2;  // Small 16px font, ~3534 bytes flash, 96 chars
constexpr int FONT_DEFAULT = 4;  // Medium 26px font, ~5848 bytes flash, 96 chars
constexpr int FONT_DIGIT   = 7;  // 7-segment 48px font, ~2438 bytes flash, digits only

// WiFi settings
constexpr char WIFI_AP_NAME[]     = "SmartClock-Setup";
constexpr char WIFI_AP_PASSWORD[] = "smartclock123";
constexpr int  WIFI_RETRY_ATTEMPTS        = 5;
constexpr int  WIFI_RETRY_DELAY_MS        = 2000;
constexpr unsigned long WIFI_CONNECTION_TIMEOUT = 30000UL; // 30 s per attempt

// OTA settings
constexpr char OTA_HOSTNAME[] = "smartclock";
constexpr char OTA_PASSWORD[] = "admin";

// Web server
constexpr int WEB_SERVER_PORT = 80;

// Update intervals
constexpr unsigned long DISPLAY_UPDATE_INTERVAL = 1000UL;
constexpr unsigned long WEATHER_UPDATE_INTERVAL = 900000UL;

// Button settings
constexpr unsigned long BUTTON_DEBOUNCE_MS      = 50UL;
constexpr unsigned long BUTTON_SHORT_PRESS_MAX_MS = 800UL;
constexpr unsigned long BUTTON_LONG_PRESS_MIN_MS  = 2000UL;

// Defaults
constexpr char DEFAULT_TIMEZONE[] = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
constexpr int  DEFAULT_BRIGHTNESS = 50;

// Animation settings
constexpr int ANIMATION_STEPS      = 20;
constexpr int ANIMATION_STEP_DELAY = 20;

// Display size (pixels)
constexpr int DISPLAY_SIZE   = 240;
constexpr int DISPLAY_CENTER = DISPLAY_SIZE / 2;

// Theme identifiers
enum class Theme : int8_t {
    SERVICE_AP   = -1, // AP setup mode
    NONE         =  0, // No active theme (transition state)
    CLOCK        =  1, // Analog/digital clock with optional weather & note
    NOTIFICATION =  2, // Full-screen text notification / gauge
    IMAGE        =  3, // JPEG image viewer
    COUNTDOWN    =  4, // Countdown to a datetime
    BIG_CLOCK    =  5, // Large digital clock
    ANALOG       =  6, // Analog clock
};

constexpr auto DEFAULT_THEME = Theme::CLOCK;
constexpr int  THEME_COUNT   = 6; // Highest valid theme ID

#endif
