# Power Management / Dynamic Frequency Scaling — Notes

## Current state (as of May 2026)

- `CONFIG_PM_ENABLE=y` — ESP-IDF PM framework is **active**
- DFS running: CPU scales between **80 and 160 MHz** based on load
- `WIFI_PS_MIN_MODEM` active — DTIM-based modem sleep enabled
- `light_sleep_enable = false` — automatic light sleep **not yet wired up**

## What was done

### sdkconfig / sdkconfig.defaults
- `CONFIG_PM_ENABLE` flipped to `y`
- Light-sleep infrastructure configs (`PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP`,
  `PM_RESTORE_CACHE_TAGMEM_AFTER_LIGHT_SLEEP`, `PM_ESP_SLEEP_POWER_DOWN_CPU`)
  were already present in sdkconfig and are now active.
- `sdkconfig.defaults` updated with `CONFIG_PM_ENABLE=y` to keep it usable as
  a regeneration baseline.

### EspDeviceInterface.cpp
`esp_pm_configure()` added as step 4 of `EspDeviceInterface::init()`, after NVS,
event loop, and netif initialisation:

```cpp
esp_pm_config_t pmCfg = {
    .max_freq_mhz       = 160,
    .min_freq_mhz       = 80,
    .light_sleep_enable = false,   // reserved for future idle-mode hook
};
esp_pm_configure(&pmCfg);
```

Placement matters: the WiFi driver registers a PM lock when it starts; it must
see PM already configured so it uses the correct APB floor frequency. Keeping it
in `EspDeviceInterface::init()` ensures this sequencing holds regardless of what
the consuming framework or app does after device init.

`esp_pm` added to `REQUIRES` in `components/esp_platform/CMakeLists.txt`.

### EspWiFiInterface.cpp
Both `WIFI_PS_NONE` calls replaced with `WIFI_PS_MIN_MODEM`:
- `startDriver()` — initial set after `esp_wifi_start()`
- `connectSta()` — re-applied after each stop/start cycle (PS mode resets to
  default after `esp_wifi_stop()`)

## What remains

### Idle-mode light sleep (app-level)
When the van monitor app implements its display timeout:

1. On timeout, turn off / dim the display backlight
2. Stretch sensor polling intervals
3. Call `esp_light_sleep_start()` — the PM infrastructure is already in place
4. Configure a wakeup source before sleeping (touch interrupt on `TP_INT` GPIO 17
   is the natural choice; `esp_sleep_enable_gpio_wakeup()`)
5. On wakeup, restore display and polling rate

No framework changes are needed for this — the app calls `esp_light_sleep_start()`
directly. To enable automatic light sleep (CPU enters sleep during FreeRTOS idle
ticks), flip `light_sleep_enable = true` in the `esp_pm_config_t` in
`EspDeviceInterface::init()` — but validate MQTT keepalive and any
time-sensitive peripherals before doing so.

### MQTT keepalive / DTIM interaction
With `WIFI_PS_MIN_MODEM`, the modem wakes on each DTIM beacon. If the AP's DTIM
interval is high (some routers default to DTIM 3 = ~300 ms), MQTT publish latency
will increase by up to one DTIM interval on the transmit side. A keepalive of 30–60s
is well within any reasonable DTIM window. Monitor for unexpected disconnects on
first deployment.

## Background: why DFS rather than fixed 80 MHz

Fixed 80 MHz (Option A, considered and rejected) would save power but slows
mbedTLS/HTTPS and LVGL at all times. DFS gives the same idle savings while
keeping full 160 MHz available for TLS handshakes (OTA), LVGL frame renders, and
any compute-heavy sensor processing. The cost is a few lines of init code.
