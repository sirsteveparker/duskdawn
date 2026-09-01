# duskdawn

duskdawn is a lightweight ESP8266 firmware to control an outdoor/light relay based on sunrise/sunset (uses nautical sunset) with an optional OLED display, a minimal REST API for status and override, and support for HTTP-based OTA firmware updates. It’s intended for NodeMCU / Wemos D1 mini style boards to automate lighting at dusk/dawn on a local network.

## Key features

- Automatic on/off based on nautical sunset + configurable rise/bed times.
- Optional OLED (SSD1306) status display.
- Minimal REST UI: `GET /status` (shows current state, override form), `POST /status` (force ON/OFF).
- HTTP OTA updates: checks a JSON `.ver` file on a local host and downloads a `.bin` when the version differs or when the server requests a forced update.
- Optional syslog output (send logs to a local syslog server).
- Weekly self-reboot to reduce long‑running instability.

## Quick repo map

- `OTAduskdawn.ino` — main firmware (control loop, sunrise/sunset calculations, OTA check).
- `API.hpp` — REST API handlers (`/status` and basic `/item` handlers).
- `Server.hpp` — HTTP server setup and route wiring.
- `ESP8266_Utils_APIREST.hpp` — small helpers (payload parsing, URL id extraction).
- `config-example.hpp` — example configuration (WiFi, lat/long, pins, timezones, OTA host info).

## Dependencies

Install these Arduino/ESP8266 libraries (used by the sketch):

- ESP8266WiFi
- ESP8266WiFiMulti (optional)
- ESPAsyncTCP
- ESPAsyncWebServer
- ArduinoJson
- SSD1306Wire (ThingPulse / SSD1306 library)
- sunset.h (sunrise/sunset calculations)
- TLog (logging wrapper)
- ESP8266HTTPClient
- ESP8266httpUpdate
- (optional) SyslogStream if you set `SYSLOG_HOST`

## Hardware

- ESP8266 board: NodeMCU 1.0 or Wemos D1 mini recommended.
- Relay module connected to the pin defined by `RELAY` in `config.hpp`.
  - If `SCREEN` is `1` (OLED enabled), `OTAduskdawn.ino` defines `RELAY` as `D3`.
  - Otherwise `RELAY` defaults to `D1` unless overridden in your config.
- Optional SSD1306 OLED (I2C) on D1 (SCL) and D2 (SDA) when `SCREEN == 1`.
- Optional local syslog server for remote logging.

**Security note:** OTA and REST endpoints are unauthenticated and intended for local/LAN use only. Do not expose the device directly to the internet without additional protections.

## Configuration

1. Copy `config-example.hpp` to `config.hpp` in the sketch folder and edit values:
   - `WIFI_SSID`, `WIFI_PASSWORD` — your WiFi credentials
   - `LATITUDE`, `LONGITUDE` — device location for sunrise/sunset calculations
   - `TIMEZONE` — timezone offset (hours)
   - `DST` — DST offset (hours)
   - `BEDTIME` — minutes past midnight after which lights won't turn on (e.g. `1350` = 22:30)
   - `RISETIME` — minutes past midnight before which lights won't turn on (e.g. `360` = 06:00)
   - `LOOPWAIT` — default loop sleep time in minutes between checks
   - `SYSLOG_HOST` and `SYSLOG_PORT` — optional syslog server
   - `MODEL`, `VERSION` — firmware model and version strings used for OTA checks
   - `host`, `fwURLLoc`, `httpPort` — host and path where OTA metadata/images are served

## Sample `.ver` JSON expected by OTA checker (NEW)

The firmware checks a URL constructed as: `http://<host><fwURLLoc><devID>.ver`.

The `.ver` file now uses a small schema that supports server-driven forced updates. The expected JSON keys are:

- `Force` (boolean) — optional; when true the device will download and apply the `.bin` even if the `Version` matches the running firmware. Default: `false`.
- `Version` (string) — the firmware version label. Devices still update when this differs from the running `VERSION` constant.

Examples:

Default (no forced update):

```json
{
  "Force": false,
  "Version": "1.0.11"
}
```

Force an update for a specific device (server-side):

```json
{
  "Force": true,
  "Version": "1.0.11"
}
```

Notes:
- The device accepts `Force` as a boolean or string (`true`/`false` or `"1"`/`"0"`) for compatibility with simple servers.
- The device constructs the firmware image URL as `http://<host><fwURLLoc><devID>.bin` and attempts an HTTP OTA update when `Force` is `true` or when `Version` differs.
- After a forced update, clear `Force` on the server (set to `false` or serve a new `.ver`) to avoid repeated forced downloads.

## How it works (brief)

- On boot: connects to WiFi, initializes logging, optional OLED, sets current time via NTP.
- Every loop: calculates today's sunrise/sunset (civil and nautical values via `sunset.h`). Uses nautical sunset and configured `RISETIME`/`BEDTIME` to decide whether the relay should be ON or OFF.
- Override: `requiredState` (set via `POST /status`) forces the relay ON/OFF until cleared.
- The device checks the OTA version URL each loop; if a newer version exists or the server sets `Force: true` it downloads the `.bin` and updates itself.
- The device calls `reset()` once per 7 days (weekly reboot).

## Build & upload

Using Arduino IDE:

1. Install the ESP8266 board package (Boards Manager: "esp8266 by ESP8266 Community").
2. Select "NodeMCU 1.0 (ESP-12E Module)" or Wemos D1 mini in Tools > Board.
3. Install the libraries listed in Dependencies via Library Manager (or manually).
4. Copy `config-example.hpp` → `config.hpp` and set your values.
5. Open `OTAduskdawn.ino`, compile, and upload.

Using PlatformIO:

- Create a new project for `nodemcuv2` or Wemos D1 mini, add required libraries to `platformio.ini`, add `config.hpp`, and build/upload with `pio run -t upload`.

## REST API examples

- Get a plain status:

  ```bash
  curl http://<device-ip>/
  ```
  returns `Outside Lights: ON` or `OFF`

- Interactive status page:
  Visit `http://<device-ip>/status` in your browser — a minimal form lets you force ON/OFF.

- Force ON via POST (JSON):
  ```bash
  curl -X POST http://<device-ip>/status -H "Content-Type: application/json" -d '{"force":"on"}'
  ```

- Force OFF:
  ```bash
  curl -X POST http://<device-ip>/status -H "Content-Type: application/json" -d '{"force":"off"}'
  ```

## Notes & gotchas

- Timezones: `configTime()` and `configTzTime()` are used. Ensure `TIMEZONE` and `DST` in `config.hpp` are correct for accurate scheduling.
- Screen burn-in: the code has a comment and a flag to optionally turn the OLED off after a period; review and adapt if required.
- If you enable `SYSLOG_HOST` you must ensure the syslog C++ object is available in your build environment and network reachable.
- The OTA check uses plain HTTP and lacks authentication — host the firmware on a trusted LAN server.

## Development / customization

- `API.hpp` contains the REST handlers. `/item` endpoints are stubbed for examples and can be extended.
- `Server.hpp` wires the routes; you can add routes or change behavior here.
- `OTAduskdawn.ino` is the single-file main sketch and is suitable for small customizations (pin changes, different sunset algorithm).
- To change between civil and nautical sunset rules, inspect `OTAduskdawn.ino` and swap which `calc*` functions are used.

## Author & license

Author: Steve Parker
Repository description: dusk & dawn light control for esp32 / esp8266

_No LICENSE file is included in the repository._ Before using or redistributing this code, check the repository for an explicit license or contact the author for permission.

## Troubleshooting

- Device not connecting to WiFi: double-check `WIFI_SSID` and `WIFI_PASSWORD` in `config.hpp`, reboot device, and monitor Serial at `115200` for connection output.
- Time not correct: ensure NTP servers are reachable and `TIMEZONE`/`DST` are set correctly.
- OTA failing: verify the `.ver` JSON and `.bin` are reachable from the device (try HTTP from another LAN host); check device logs for HTTP error codes.
- Debug logs: watch the Serial output at `115200` and enable `SYSLOG_HOST` to forward logs to a syslog server.

## Want more?

If you want, I can:
- Provide a sample `config.hpp` based on your location and hardware.
- Draft a minimal `.ver`/`.bin` hosting setup (simple Python HTTP server and file naming).
- Add basic authentication to the REST/OTA endpoints or suggest secure OTA alternatives.

## Acknowledgements

Thanks to various Arduino/ESP8266 library authors used by this project (ESPAsyncWebServer, ArduinoJson, SSD1306Wire, sunset.h, etc.).
