# ESP32 App Framework — Claude Notes

## Project overview

ESP32-S3 application framework built on ESP-IDF 6.x. Single hardware target: ESP32-S3 with 16 MB flash.
The framework is designed to be consumed as a git submodule by downstream application projects.

## Before making changes

Always explain what you plan to do and ask for confirmation before editing any file.

## Key facts

- `sdkconfig` is committed and is the source of truth for the build configuration. Do not suggest deleting it as a routine step.
- `sdkconfig.defaults` exists as a regeneration baseline but may not be current. Treat it as supplementary, not authoritative.
- Component directory is `components/framework_files/` (not `_framework_files/`).
- The `auth` component exists alongside the other framework components.

## File verification

The bash sandbox can show stale cached content for files that have been modified on the Windows host since the mount was established. Before concluding that a file is truncated, malformed, or missing content, always cross-check using the Read tool, which reads directly from the Windows filesystem. Do not draw conclusions about file content from bash `wc`, `cat`, or `stat` output alone.

## Documentation files

- `README.md` — main project doc; API reference is in `docs/api-reference.md`
- `docs/creating-an-app.md` — guide for consuming the framework as a submodule
- `docs/flash_layout.md` — partition table reference (source of truth is `partitions.csv`)
- `docs/api-reference.md` — full HTTP API route table with response shapes
- `CONTRIBUTING.md` — internal development conventions

## Target hardware

SpotPear ESP32-S3-Touch-LCD-2 (SpotPear/Waveshare product — demos hosted on waveshare.com).
- **SoC:** ESP32-S3R8 — 8MB Octal PSRAM embedded (AP Memory, gen 3, 64Mbit, confirmed working). Use `CONFIG_SPIRAM_MODE_OCT`.
- **Flash:** 16MB external
- **Display:** ST7789T3, 240×320 IPS, SPI
- **Touch:** CST816D, I2C
- **IMU:** QMI8658 6-axis (accelerometer + gyroscope)
- **Battery management:** onboard lithium charge controller

### GPIO assignments (verify against integrated board schematic before use)
These are from the SpotPear standalone display wiki and may differ on the integrated board.

| Signal   | GPIO |
|----------|------|
| LCD MOSI | 2    |
| LCD SCLK | 4    |
| LCD MISO | 42   |
| LCD_CS   | 39   |
| LCD_DC   | 41   |
| LCD_RST  | 40   |
| LCD_BL   | 6    |
| TP_SDA   | 15   |
| TP_SCL   | 7    |
| TP_INT   | 17   |
| TP_RST   | 16   |
| SD_CS    | 38   |

**Note:** Current dev work is on a different board. sdkconfig will need migration
when moving to the target board. Treat committed sdkconfig as dev-board config
until that migration is done.

## Partition layout

Current layout (partitions.csv):
- nvs:       24KB  at 0x9000
- otadata:   8KB   at 0xF000
- factory:   4MB   at 0x20000  (recovery fallback — do not remove)
- ota_0:     4MB   at 0x420000
- ota_1:     4MB   at 0x820000
- assets_fs: 2MB   at 0xC20000 (littlefs — reserved for app use)
- ~1.9MB unallocated at end of flash

All three app partitions are identical in size so the same firmware.bin can be
flashed to any of them. Partition table changes require a full USB reflash —
cannot be updated via OTA.

Current firmware size: ~1.10MB. 4MB partitions give comfortable headroom for
LVGL, HTTPS OTA, MQTT, embedded web assets, and future growth.

## Asset strategy

Web assets (HTML, JS, CSS) are bundled with the firmware binary via `EMBED_FILES`
in component CMakeLists.txt — not stored in LittleFS. This ensures OTA updates
are atomic: assets and firmware are always consistent.

- Pre-compress assets with gzip at build time; serve with `Content-Encoding: gzip`
- Framework assets served from `/framework/ui/***`
- App assets served from `/app/ui/***`
- Both served under a shared EmbeddedServer; each component registers its own routes
- LittleFS is reserved for downstream app use if needed; framework itself does not use it

## OTA strategy

Pull from GitHub Releases — not push. Two-step process:
1. Fetch `version.txt` from latest release (tiny, avoids unnecessary binary download)
2. Compare with `esp_app_get_description()->version`; download and flash `firmware.bin` only if different

URLs follow the pattern:
```
https://github.com/<user>/<repo>/releases/latest/download/version.txt
https://github.com/<user>/<repo>/releases/latest/download/firmware.bin
```

- Use `esp_crt_bundle_attach` for TLS — covers GitHub's CA chain without managing individual certs
- Call `esp_ota_mark_app_valid_cancel_rollback()` once the new firmware is confirmed healthy
- MQTT topic (on Venus broker or other) used as an out-of-band trigger for immediate OTA check
- Automate release asset uploads via GitHub Actions

## Power management

`CONFIG_PM_ENABLE=y` is active. `esp_pm_configure()` is called in
`EspDeviceInterface::init()` with `max_freq_mhz=160, min_freq_mhz=80`
as step 4 of device init, after NVS, event loop, and netif. The CPU scales
between 80 and 160 MHz based on load. `light_sleep_enable` is `false` —
automatic light sleep is reserved for the idle-mode hook below.

Wi-Fi uses `WIFI_PS_MIN_MODEM` (DTIM-based modem sleep). This is set in
`EspWiFiInterface::startDriver()` and re-applied after every stop/start cycle
in `connectSta()`. Persistent MQTT requires periodic keepalive; duty-cycling
Wi-Fi off is not appropriate once Venus integration is active.

Two operating modes:
- **Active:** display on, fast sensor polling, Wi-Fi awake
- **Idle:** triggered by display timeout after last touch event; display off/dim,
  CPU in light sleep, sensor polling stretched. The app calls
  `esp_light_sleep_start()` directly — no framework hook required. PM
  infrastructure is already in place (`CONFIG_PM_ENABLE`, light-sleep power-down
  configs in sdkconfig).

LVGL frame buffers should be allocated in PSRAM. DMA transfer buffers must remain
in internal SRAM.

## Van monitoring — first application

Replaces a Pi Pico (MicroPython) implementation. The ESP32-S3 reads sensors
directly and drives the 2" touch display as its primary UI.

### Water level sensor
- Model: 2-wire loop-powered 4-20mA, 0–2m range, 12–32V supply
- Power: regulated 12V (within sensor range, lower stress than 24V)
- Shunt: 150Ω in series with loop → 0.6V (empty/4mA) to 3.0V (full/20mA)
- Add 100nF cap across shunt to filter switching noise from MPPT/inverter
- Average multiple ADC readings; discard outliers
- Calibration: 2-point (user presses Empty/Full button at known states)

```c
float level_m = (voltage - 0.6f) / 2.4f * 2.0f;  // 0.0–2.0m
```

### Venus OS integration (planned)
Venus OS runs on RPi in the van. Built-in Mosquitto broker on port 1883 (no auth
on local network).

- Topic hierarchy: `N/<portal_id>/<service_type>/<device_instance>/<path>`
- Write topics: `W/<portal_id>/...`
- **Keepalive required:** publish to `R/<portal_id>/keepalive` every ~60s or Venus
  stops publishing
- ESP32 publishes water level on a timer; subscribes to battery SOC and solar yield
  for display
- Portal ID visible in Venus → Remote Console → Device List
- Venus broker also used for OTA trigger topic
