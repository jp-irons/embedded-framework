# Embedded Application Framework

A clean, deterministic, commercial-grade embedded application framework built on ESP-IDF 6.x. The current hardware target is ESP32-S3; the architecture is designed to accommodate additional targets as the framework evolves.

The framework is aimed at engineers who need predictable behaviour, maintainable architecture, and a reproducible build pipeline. Provisioning, AP/STA transitions, OTA firmware updates, and UI delivery are all driven by explicit state machines — no globals, no hidden behaviour, no monolithic codebase.

## Features

- Wi-Fi provisioning with explicit AP → STA state machine
- Embedded web UI served from a LittleFS flash partition
- Modular, transport-agnostic HTTP API handlers
- Wi-Fi network storage backed by NVS
- OTA firmware update with rollback protection and boot-attempt guardian
- Per-device TLS certificate generated on first boot and persisted in NVS
- mDNS advertising with MAC-derived hostname
- Versioned OTA binaries produced automatically at build time
- Modern C++17 design with clear component boundaries and private internals

## Hardware targets

The current supported target is **ESP32-S3**. The framework uses `RTC_DATA_ATTR` for boot-attempt tracking and references ESP32-S3-specific partition offsets.

Additional targets may be added in future. Porting to another ESP32 variant requires adjusting `partitions.csv` and reviewing PSRAM usage in the `device` component; porting to a different chipset family would additionally require replacing or abstracting the ESP-IDF HAL calls in the `wifi_manager`, `device_cert`, and `ota` components.

## Using this framework in your own project

The framework is designed to be consumed as a git submodule. Your application lives in its own repository, provides its own `main/`, and pulls in the framework's `components/` via ESP-IDF's `EXTRA_COMPONENT_DIRS` mechanism. You control when to take framework updates, and can pin to a specific release for production stability.

See [Creating an application](docs/creating-an-app.md) for a complete step-by-step guide.

The remainder of this document covers building and running the **demo application** included in this repository.

---

## Getting started

See [docs/new-machine-setup.md](docs/new-machine-setup.md) for a complete step-by-step guide to cloning, importing into the Espressif IDE, building, and flashing on a new machine.

### Building a flashable OTA binary

The post-build step in `CMakeLists.txt` produces a versioned copy of the application binary alongside the normal build output:

```
build/embedded_framework-<version>.bin
```

The version comes from `version.txt`. Upload this file via the firmware page in the web UI or via any HTTP client.

## Partition layout

See [docs/flash_layout.md](docs/flash_layout.md) for full details and guidance on changing the layout.

```
nvs        data  nvs      0x009000   24 KB     NVS key-value store
otadata    data  ota      0x00F000    8 KB     OTA boot-slot selection record
factory    app   factory  0x020000    2 MB     Factory image (USB flash only)
ota_0      app   ota_0    0x220000    2 MB     OTA slot 0
ota_1      app   ota_1    0x420000    2 MB     OTA slot 1
assets_fs  data  littlefs 0x620000    1.875 MB Embedded web UI and static assets
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
  auth/                 API key store, session store + AuthApiHandler
  common/               Result type, shared utilities
  logger/               Tag-based log sink with per-tag level filtering
  http/                 HttpServer (esp_http_server wrapper), HttpRequest, HttpResponse, HttpHandler base
  network_store/        NVS-backed Wi-Fi network store + NetworkApiHandler
  device/               Device info, reboot, NVS clear + DeviceApiHandler
  device_cert/          Per-device TLS cert (generated on first boot, stored in NVS)
  framework_files/      LittleFS-backed static asset server + EmbeddedAssetTable
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

See [docs/api-reference.md](docs/api-reference.md) for the full route table, response shapes, and state value definitions.

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
