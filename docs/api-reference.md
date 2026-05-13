# URL Plan

All API routes share a configurable root URI. The default is `/framework/api`.
This is set in `FrameworkContext` and passed through `WiFiContext.rootUri` to `EmbeddedServer`.

## Static assets

Requests that do not match any API prefix are handled by `EmbeddedServer`, which serves files
from the `assets_fs` LittleFS flash partition via `StaticFileHandler`.

```
/                   → redirect to /runtime/index.html
/index.html         → redirect to /runtime/index.html
/runtime/*          → static files from assets_fs
```

## API routes

The `EmbeddedServer` dispatches requests by longest-prefix match against the following table.
Trailing slashes in the registered prefixes are significant.

### Networks  —  `/framework/api/networks/`

Handler: `network_store::NetworkApiHandler`

| Method | Sub-target    | Description                                      |
|--------|--------------|--------------------------------------------------|
| GET    | `list`       | Return all saved Wi-Fi networks                  |
| POST   | `submit`     | Add or update a network (SSID + password)        |
| DELETE | `delete`     | Remove a network by SSID                         |
| POST   | `clear`      | Remove all saved networks                        |
| POST   | `makeFirst`  | Move a named network to position 0               |

### Device  —  `/framework/api/device/`

Handler: `device::DeviceApiHandler`

| Method | Sub-target   | Description                                               |
|--------|-------------|-----------------------------------------------------------|
| GET    | `info`      | Chip model, IDF version, uptime, STA IP, free heap, temperature |
| POST   | `reboot`    | Restart the device immediately                            |
| POST   | `clearNvs`  | Erase all NVS namespaces and reboot                       |

### Wi-Fi  —  `/framework/api/wifi/`

Handler: `wifi_manager::WiFiApiHandler`

| Method | Sub-target     | Description                              |
|--------|---------------|------------------------------------------|
| GET    | `status`      | Current Wi-Fi mode, SSID, RSSI, IP       |
| GET    | `scan`        | Scan for nearby access points            |
| POST   | `connect`     | Connect to a specified SSID              |
| POST   | `disconnect`  | Disconnect from the current AP           |

### Firmware (OTA)  —  `/framework/api/firmware/`

Handler: `ota::OtaApiHandler`

| Method | Sub-target       | Description                                                                 |
|--------|-----------------|-----------------------------------------------------------------------------|
| GET    | `status`        | Partition table with label, state, OTA state, version, project, build date  |
| POST   | `upload`        | Stream a firmware binary (application/octet-stream) to the inactive OTA slot and reboot |
| POST   | `rollback`      | Set the previous VALID OTA slot as next-boot and reboot; 409 if none available |
| POST   | `factoryReset`  | Erase the otadata partition and reboot to the factory image                 |

#### `GET /framework/api/firmware/status` response shape

```json
{
  "partitions": [
    {
      "label":         "factory",
      "state":         "factory",
      "otaState":      "empty",
      "isRunning":     true,
      "isNextBoot":    true,
      "partitionSize": 2097152,
      "firmwareSize":  913408,
      "version":       "0.0.1",
      "project":       "embedded_framework",
      "buildDate":     "May  3 2026 10:00:00",
      "idfVersion":    "v6.0-..."
    },
    {
      "label":         "ota_0",
      "state":         "valid",
      "otaState":      "valid",
      "isRunning":     false,
      "isNextBoot":    false,
      "partitionSize": 2097152,
      "firmwareSize":  921600,
      "version":       "0.0.2",
      "project":       "embedded_framework",
      "buildDate":     "May  3 2026 12:00:00",
      "idfVersion":    "v6.0-..."
    },
    {
      "label":         "ota_1",
      "state":         "empty",
      "otaState":      "empty",
      "isRunning":     false,
      "isNextBoot":    false,
      "partitionSize": 2097152,
      "firmwareSize":  0,
      "version":       "",
      "project":       "",
      "buildDate":     "",
      "idfVersion":    ""
    }
  ]
}
```

State values:
- `"running"` — this partition is currently executing
- `"factory"` — factory partition (always `ESP_OTA_IMG_UNDEFINED` from the bootloader's perspective; this is normal)
- `"valid"` — OTA slot confirmed healthy by a previous `markValid()` call
- `"pending"` — new image waiting for application to call `markValid()`
- `"new"` — written but not yet booted
- `"invalid"` / `"aborted"` — failed verification or update was aborted
- `"empty"` — slot has never been written or otadata was erased

Version fields (`version`, `project`, `buildDate`, `idfVersion`) are omitted (empty strings) for OTA slots with state `"empty"` to avoid showing stale flash content after a factory reset.

## Notes

- Routes are matched by prefix. The `EmbeddedServer` tries each prefix in registration order and uses the first handler that does not return `Result::NotFound`.
- The root URI prefix is configurable at construction time. All example paths above assume the default `/framework/api`.
- The `POST /firmware/upload` endpoint bypasses the normal body pre-read. The request body is streamed directly from the socket in 4 KB chunks. Do not attempt to pre-read the body before calling this endpoint from client code.
