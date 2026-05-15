# API Reference

All API routes share a configurable root URI. The default is `/framework/api`.
This is set in `FrameworkContext` and passed through `WiFiContext.rootUri` to `EmbeddedServer`.

## Static assets

Web assets (HTML, JS, CSS) are embedded directly in the firmware binary via `EMBED_FILES`.
Requests that do not match any API prefix are served by `EmbeddedServer`:

```
/                        → redirect to /framework/ui/index.html
/framework/ui/*          → framework assets (embedded in firmware)
/app/ui/*                → application assets (embedded in firmware)
```

## Authentication

All `/framework/api/` endpoints require authentication.

**Session tokens** — obtain a token via `POST /framework/api/auth/login`, then include it as
`Authorization: Bearer <token>` on all subsequent requests. Tokens are invalidated on logout or
device reboot. They are stored in `sessionStorage` by the browser client (tab-scoped, cleared on
tab close).

**API keys** — machine-to-machine clients use a persistent API key generated on the Security page,
presented identically as a `Bearer` token. The server accepts both session tokens and API keys on
every framework endpoint.

A `401` response means the token is invalid or absent. The browser client clears the token and
re-shows the login overlay on any `401`.

## API routes

### Auth  —  `/framework/api/auth/`

Handler: `auth::AuthApiHandler`

| Method | Sub-target  | Description                                                         |
|--------|-------------|---------------------------------------------------------------------|
| POST   | `login`     | Exchange `Authorization: Basic admin:<password>` for a session token |
| POST   | `logout`    | Invalidate the current session token                                |
| GET    | `status`    | Return `{"authenticated":true}` if the token is valid; 401 otherwise |
| POST   | `password`  | Change the admin password                                           |
| GET    | `apikey`    | Return API key status (`{"exists":true/false}`)                     |
| POST   | `apikey`    | Generate (or rotate) the device API key                             |
| DELETE | `apikey`    | Revoke the current API key                                          |

#### `POST /framework/api/auth/login` request / response

Request header: `Authorization: Basic <base64(admin:password)>`

```json
{ "token": "<64-hex-chars>" }
```

#### `POST /framework/api/auth/apikey` response

```json
{ "key": "<64-hex-chars>" }
```

---

### Networks  —  `/framework/api/networks/`

Handler: `network_store::NetworkApiHandler`

| Method | Sub-target   | Description                                       |
|--------|--------------|---------------------------------------------------|
| GET    | `list`       | Return all saved Wi-Fi networks                   |
| POST   | `submit`     | Add or update a network (SSID + password)         |
| DELETE | `<ssid>`     | Remove a network by SSID                          |
| POST   | `clear`      | Remove all saved networks                         |
| POST   | `makeFirst`  | Move a named network to position 0                |

---

### Device  —  `/framework/api/device/`

Handler: `device::DeviceApiHandler`

| Method | Sub-target  | Description                                                              |
|--------|-------------|--------------------------------------------------------------------------|
| GET    | `info`      | Chip model, IDF version, uptime, STA IP, free heap, temperature          |
| POST   | `reboot`    | Restart the device immediately                                           |
| POST   | `clearNvs`  | Erase all NVS namespaces and reboot                                      |

---

### Wi-Fi  —  `/framework/api/wifi/`

Handler: `wifi_manager::WiFiApiHandler`

| Method | Sub-target    | Description                               |
|--------|---------------|-------------------------------------------|
| GET    | `status`      | Current Wi-Fi mode, SSID, RSSI, IP        |
| GET    | `scan`        | Scan for nearby access points             |

---

### Firmware (OTA)  —  `/framework/api/firmware/`

Handler: `ota::OtaApiHandler`

| Method | Sub-target        | Description                                                                          |
|--------|-------------------|--------------------------------------------------------------------------------------|
| GET    | `status`          | Partition table: label, state, OTA state, version, project, build date, sizes        |
| POST   | `upload`          | Stream a firmware binary (`application/octet-stream`) to the inactive OTA slot and reboot |
| POST   | `rollback`        | Set the previous `VALID` OTA slot as next-boot and reboot; `409` if none available   |
| POST   | `factoryReset`    | Erase the otadata partition and reboot to the factory image                          |
| GET    | `pullStatus`      | Return the currently configured pull-OTA base URL                                    |
| POST   | `pullConfig`      | Save a new pull-OTA base URL to NVS (plain-text body)                                |
| POST   | `checkUpdate`     | Trigger an immediate pull-OTA version check (spawns background task)                 |
| GET    | `pullCheckStatus` | Return the current state of an in-progress (or recently completed) pull-OTA check    |

---

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
      "partitionSize": 4194304,
      "firmwareSize":  1130496,
      "version":       "0.1.0",
      "project":       "embedded_framework",
      "buildDate":     "May 15 2026 11:01:00",
      "idfVersion":    "v6.0-..."
    },
    {
      "label":         "ota_0",
      "state":         "valid",
      "otaState":      "valid",
      "isRunning":     false,
      "isNextBoot":    false,
      "partitionSize": 4194304,
      "firmwareSize":  1134592,
      "version":       "0.0.5",
      "project":       "embedded_framework",
      "buildDate":     "May 14 2026 09:00:00",
      "idfVersion":    "v6.0-..."
    },
    {
      "label":         "ota_1",
      "state":         "empty",
      "otaState":      "empty",
      "isRunning":     false,
      "isNextBoot":    false,
      "partitionSize": 4194304,
      "firmwareSize":  0,
      "version":       "",
      "project":       "",
      "buildDate":     "",
      "idfVersion":    ""
    }
  ]
}
```

`state` values:

| Value       | Meaning                                                                              |
|-------------|--------------------------------------------------------------------------------------|
| `running`   | This partition is currently executing                                                |
| `factory`   | Factory partition (always `ESP_OTA_IMG_UNDEFINED` from the bootloader; this is normal) |
| `valid`     | OTA slot confirmed healthy by a previous `markValid()` call                          |
| `pending`   | New image waiting for the application to call `markValid()`                          |
| `new`       | Written but not yet booted                                                           |
| `invalid`   | Failed verification                                                                  |
| `aborted`   | Update was aborted                                                                   |
| `empty`     | Slot has never been written, or otadata was erased                                   |

`otaState` reflects the raw ESP-IDF OTA state of the partition. For the running partition, `state`
is always `"running"` while `otaState` shows the underlying ESP-IDF state (e.g. `"valid"`,
`"pending"`). Version fields are empty strings for slots with `state == "empty"` to avoid showing
stale flash content after a factory reset.

---

#### `GET /framework/api/firmware/pullStatus` response shape

```json
{ "url": "https://github.com/user/repo/releases/latest/download" }
```

Returns an empty string for `url` if no URL has been configured.

---

#### `POST /framework/api/firmware/pullConfig` request

Body: plain text (`Content-Type: text/plain`), containing the base URL. Trailing whitespace is
stripped. The URL is persisted to NVS and survives reboots.

```
https://github.com/user/repo/releases/latest/download
```

Response:

```json
{ "status": "ok", "url": "https://github.com/user/repo/releases/latest/download" }
```

---

#### `POST /framework/api/firmware/checkUpdate` response

Marks state as `checking` and spawns a background task. The client should begin polling
`pullCheckStatus` after receiving this response.

```json
{ "status": "ok", "message": "OTA check initiated" }
```

---

#### `GET /framework/api/firmware/pullCheckStatus` response shape

```json
{
  "state":      "downloading",
  "message":    "0.1.0",
  "downloaded": 458752,
  "total":      1130496
}
```

`state` values:

| Value         | Meaning                                                                         |
|---------------|---------------------------------------------------------------------------------|
| `idle`        | No check in progress                                                            |
| `checking`    | Fetching `version.txt` from the configured URL                                  |
| `up_to_date`  | Remote version matches local; `message` contains the remote version string      |
| `downloading` | Newer version found; `esp_https_ota` in progress; `downloaded`/`total` in bytes |
| `error`       | Any failure; `message` contains a short description                             |

`downloaded` and `total` are meaningful only when `state == "downloading"`. `total` is zero if the
server did not provide a `Content-Length` header. The device reboots automatically on a successful
download; the client detects this as a network error on the next poll.

---

## Notes

- Routes are matched by prefix. `EmbeddedServer` tries each prefix in registration order and uses
  the first handler whose `handle()` call does not return `Result::NotFound`.
- The root URI prefix (`/framework/api`) is configurable at construction time via `FrameworkContext`.
- `POST /firmware/upload` bypasses normal body pre-reading. The request body is streamed directly
  from the socket in chunks. Do not pre-read the body before calling this endpoint.
- All endpoints return errors as `{"error": "<message>"}` with an appropriate HTTP status code.
