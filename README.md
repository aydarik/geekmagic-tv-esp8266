# GeekMagic SmallTV - Custom Firmware

[![License](https://img.shields.io/github/license/aydarik/geekmagic-tv-esp8266)](/LICENSE) [![Release](https://img.shields.io/github/v/release/aydarik/geekmagic-tv-esp8266)](https://github.com/aydarik/geekmagic-tv-esp8266/releases) [![Downloads](https://img.shields.io/github/downloads/aydarik/geekmagic-tv-esp8266/latest/firmware.bin?displayAssetName=false)](https://github.com/aydarik/geekmagic-tv-esp8266/releases) [![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-Donate-orange?logo=buy-me-a-coffee)](https://www.buymeacoffee.com/aydarik)

ESP8266 firmware compatible with the GeekMagic API, designed for GeekMagic SmallTV devices.

> This project is a fork of https://github.com/bvweerd/geekmagic-tv-esp8266, huge thanks to [@bvweerd](https://github.com/bvweerd) for the original work ❤️
>
> It started as a personal learning/experimentation project. Due to significant refactoring and changes, it is **not intended to stay in sync** with the upstream repository.

![Clock](/assets/photo_clock.jpg) ![Message](/assets/photo_message.jpg)

> [!WARNING]
> The Smalltv and Smalltv-Ultra are based on an ESP8266, while the Smalltv-Pro uses an ESP32 with more memory and processing power.
>
> This code has only been tested on the Smalltv Ultra (ESP8266) and may require modifications for other variants.
>
> **Firmware updates are at your own risk!**

## Compatibility

Works with:
- https://github.com/aydarik/hass-geekmagic
- https://github.com/adrienbrault/geekmagic-hacs


## First Installation (UART Required)

The first flash cannot be done via OTA.
You must **flash over UART** ⚠

Recommended tool: https://web.esphome.io/

## Features

- Web-based User Interface (UI) for settings and control
- WiFi configuration via captive portal
- ArduinoOTA updates
- Web-based OTA updates (/update)
- LittleFS filesystem
- Persistent settings
- NTP time synchronization with configurable TZ
- Simplified clock display (time and date only)
- Custom messages
- Image upload and rendering

## First boot

1. Device starts in AP mode
2. **Look at the device display** to see the AP password
3. Connect to the WiFi network using the displayed credentials
4. Captive portal opens automatically, if not - navigate to the IP address displayed on the screen
5. Configure WiFi credentials
6. Device reboots and connects to the network

## Failsafe AP Mode

When in failsafe mode:
- Device runs as Access Point
- **AP credentials displayed on device screen** (SSID, password, and IP address)
- Web interface remains accessible via AP IP (typically 192.168.4.1)

## Factory Reset

The firmware provides a **manual factory reset mechanism** that users can trigger without needing the web interface or serial console:

**How it works:**
1. Power cycle the device **5 times in quick succession** (within ~10 seconds total)
2. On the 5th boot, the device automatically performs a complete factory reset
3. All settings, WiFi credentials, and filesystem data are erased
4. Device restarts in AP mode ready for initial setup

## Web Control Panel

Access the comprehensive web-based control panel by navigating to your device's IP address in a web browser.

![WEB UI](/assets/web_ui.png)

### HTTP API (for advanced users/integrations)

```bash
# Upload image
curl -F "file=@image.jpg" http://192.168.0.193/doUpload?dir=/image/

# Show image (after upload)
curl http://192.168.0.193/set?img=/image/image.jpg

# Set brightness (0-100)
curl http://192.168.0.193/set?brt=50

# Set timezone
curl "http://192.168.0.193/set?tz=UTC-1"

# Custom message
curl "http://192.168.0.193/set?msg=Hello\nworld!"

# Device status
curl http://192.168.0.193/app.json
```

## OTA Updates

### Via Web

1. Open `http://192.168.0.193/update` (or your device's IP)
2. Select `firmware.bin`
3. Upload

## License

This project is licensed under the MIT License - see the [LICENSE](/LICENSE) file for details.
