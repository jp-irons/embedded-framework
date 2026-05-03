# ESP32 Application Framework

A clean, deterministic, commercial-grade application framework for ESP32 devices built on ESP-IDF 6.x.

The framework is aimed at engineers who need predictable behaviour, maintainable architecture, and a reproducible build pipeline. Provisioning, AP/STA transitions, OTA firmware updates, and UI delivery are all driven by explicit state machines — no globals, no hidden behaviour, no monolithic codebase.

## Features

- Wi-Fi provisioning with explicit AP → STA state machine
- Embedded web UI served from a LittleFS flash partition
- Modular, transport-agnostic HTTP API handlers
- Credential storage backed by NVS
- OTA firmware update with rollback protection and boot-attempt guardian
- Per-device TLS certificate generated on first boot and persisted in NVS
- mDNS advertising with MAC-derived hostname
- Versioned OTA binaries produced automatically at build time
- Modern C++17 design with clear component boundaries and private internals

## Hardware target

ESP32-S3. The framework uses `RTC_DATA_ATTR` for boot-attempt tracking and references ESP32-S3-specific partition offsets. Porting to other ESP32 variants requires adjusting `partitions.csv` and reviewing PSRAM usage in the `device` component.

## Prerequisites

- ESP-IDF v6.0 installed and on `PATH` (`idf.py` must be accessible)
- Python 3.8+
- CMake 3.16+
- Espressif Eclipse IDE or VS Code with the ESP-IDF extension (optional but recommended)

## Getting started

Clone the repository then configure, build, and flash:

```bash
# If sdkconfig does not yet exist, or after changing sdkconfig.defaults:
rm -f sdkconfig

idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

The first build generates `sdkconfig` from `sdkconfig.defaults`. If you change `sdkconfig.defaults` (e.g. to enable or reconfigure OTA rollback) you must delete `sdkconfig` and rebuild so the bootloader picks up the changes.

### Building a flashable OTA binary

The post-build step in `CMakeLists.txt` produces a versioned copy of the application binary alongside the normal build output:

```
build/esp32_app_framework-<version>.bin
```

The version comes from `version.txt`. Upload this file via the firmware page in the web UI or via any HTTP client.

## Partition layout

```
nvs        data  nvs      0x9000    24 KB   NVS key-value store
otadata    data  ota      0xF000     8 KB   OTA boot selection
factory    app   factory  0x20000    2 MB   Factory image (USB flash only)
ota_0      app   ota_0    0x220000   2 MB   OTA slot 0
ota_1      app   ota_1    0x420000   2 MB   OTA slot 1
assets_fs  data  littlefs 0x620000   1.9 MB Embedded web UI and static assets
```

A custom `partitions.csv` is selected via `sdkconfig.defaults`:

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

## OTA update and rollback

### How it works

1. Upload a new `.bin` via `POST /framework/api/firmware/upload`.
2. The binary is streamed in 4 KB chunks directly from the socket into the inactive OTA slot — no large heap allocation required.
3. On success the new partition is set as next-boot and the device restarts.
4. The bootloader marks the new image `PENDING_VERIFY`.
5. `OtaManager::checkOnBoot()` runs at the very top of `app_main` before any tasks start. If the image exceeds its boot-attempt budget (default 3) it rolls back automatically.
6. Once the HTTP server is up, `OtaManager::markValid()` is called — this cancels the rollback timer and marks the image `VALID`.

### Rollback

The **Rollback** button in the firmware UI is enabled only when a non-running OTA slot has state `VALID`. This means rollback requires that at least two successful OTA upgrades have been performed (so there is a previous VALID slot to return to).

`POST /framework/api/firmware/rollback` finds the VALID non-running slot, sets it as next-boot, and reboots.

### Factory reset

`POST /framework/api/firmware/factoryReset` erases the `otadata` partition. With no OTA selection record the bootloader falls back to the factory partition on next boot.

## Component architecture

```
main/
  app_main.cpp          Entry point: boot guardian, logging setup, FrameworkContext, ApplicationContext
  ApplicationContext     Application-specific startup and main loop hook

components/
  common/               Result type, shared utilities
  logger/               Tag-based log sink with per-tag level filtering
  http/                 HttpServer (esp_http_server wrapper), HttpRequest, HttpResponse, HttpHandler base
  credential_store/     NVS-backed Wi-Fi credential store + CredentialApiHandler
  device/               Device info, reboot, NVS clear + DeviceApiHandler
  device_cert/          Per-device TLS cert (generated on first boot, stored in NVS)
  embedded_files/       LittleFS-backed static asset server + EmbeddedAssetTable
  framework/            FrameworkContext: owns and wires all components
  ota/                  OtaManager (boot guardian), OtaWriter (streaming flash), OtaApiHandler
  wifi_manager/         WiFiInterface, WiFiManager, WiFiStateMachine, EmbeddedServer, WiFiApiHandler
```

### FrameworkContext

`FrameworkContext` is the top-level owner. It constructs, wires, and starts every subsystem. The default constructor uses sensible defaults:

```cpp
framework::FrameworkContext fw{};
```

Or provide explicit configuration:

```cpp
wifi_manager::ApConfig apConfig = {
    .ssid = "MyDevice", .password = "password", .channel = 1, .maxConnections = 4};
framework::FrameworkContext fw{apConfig, "/framework/api", "mydevice"};
```

The third argument is the mDNS prefix; the device advertises itself as `<prefix>-<last3MacBytes>.local`.

### ApplicationContext

Implement `ApplicationContext` (in `main/`) for your application-specific logic. `start()` is called once after the framework is up; `loop()` is called every 50 ms from `app_main`.

## API reference

All routes are relative to the configured `rootUri` (default `/framework/api`). The framework's `EmbeddedServer` dispatches by prefix match, so routes are registered as shown below.

### Credentials — `/framework/api/credentials/`

| Method | Target | Description |
|--------|--------|-------------|
| GET    | `list`      | List stored Wi-Fi credentials |
| POST   | `submit`    | Add or update a credential |
| DELETE | `delete`    | Remove a credential by SSID |
| POST   | `clear`     | Remove all stored credentials |
| POST   | `makeFirst` | Promote a credential to first position |

### Device — `/framework/api/device/`

| Method | Target     | Description |
|--------|-----------|-------------|
| GET    | `info`    | Chip info, IDF version, uptime, IP, heap, temperature |
| POST   | `reboot`  | Restart the device |
| POST   | `clearNvs`| Erase all NVS partitions and reboot |

### Wi-Fi — `/framework/api/wifi/`

| Method | Target       | Description |
|--------|-------------|-------------|
| GET    | `status`    | Current Wi-Fi state and connection info |
| GET    | `scan`      | Scan for nearby access points |
| POST   | `connect`   | Connect to an SSID |
| POST   | `disconnect`| Disconnect from current AP |

### Firmware (OTA) — `/framework/api/firmware/`

| Method | Target         | Description |
|--------|---------------|-------------|
| GET    | `status`      | Partition table with state, version, and build metadata for factory, ota_0, ota_1 |
| POST   | `upload`      | Stream a firmware binary to the inactive OTA slot and reboot |
| POST   | `rollback`    | Boot the previous VALID OTA slot (409 if none available) |
| POST   | `factoryReset`| Erase OTA data and reboot to the factory partition |

### Static assets

All other requests are handled by `EmbeddedServer`, which serves files from the `assets_fs` LittleFS partition. Requests to `/` and `/index.html` redirect to `/runtime/index.html`.

## Configuration

`sdkconfig.defaults` contains the minimum required settings. Key values:

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

Log levels are set at runtime in `app_main.cpp → setupLogging()`. The `logger` component supports per-tag filtering independent of the ESP-IDF log level system.

## Versioning

The project version is stored in `version.txt`. ESP-IDF's `project.cmake` reads it into `PROJECT_VER`. The root `CMakeLists.txt` uses a post-build command to copy `build/<project>.bin` to `build/<project>-<version>.bin` after each successful build.

Bump `version.txt` before building a release binary.

## Licence

This project is not currently open for external contributions or distribution. See `CONTRIBUTING.md`.
