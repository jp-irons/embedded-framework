# Contributing

This project is under active development and is not currently accepting external contributions. The architecture may change significantly between versions.

Once the framework stabilises, contribution guidelines and a public roadmap will be published here.

---

The remainder of this document captures internal development conventions for use by the project maintainers.

## Development environment

- ESP-IDF v6.0
- Target: ESP32-S3
- C++17 (`-std=gnu++17`)
- CMake 3.16+
- Espressif Eclipse IDE or VS Code with the ESP-IDF extension

## Component conventions

### Structure

Each component lives under `components/<name>/` and follows the standard ESP-IDF layout:

```
components/<name>/
  CMakeLists.txt
  include/<name>/       Public headers (one subdirectory matching component name)
  *.cpp                 Implementations
```

Components expose their public API only through `include/<name>/`. Internal helpers that are not part of the API stay in `.cpp` files or in private headers not listed in `INCLUDE_DIRS`.

### Naming

- Namespaces match the component name (e.g. `namespace ota`, `namespace wifi_manager`).
- Classes use `PascalCase`. Files use the class name exactly.
- Member variables use a trailing underscore for owned values (`rootUri_`) and no underscore for injected references (`wifiCtx`).
- Static logger instance at file scope: `static logger::Logger log{"ComponentName"};`

### Result type

All handler and service methods return `common::Result`. Use `Result::Ok` for success and `Result::NotFound` to signal that the handler does not own the route (allows the dispatcher to try the next handler).

```cpp
common::Result MyHandler::handle(HttpRequest &req, HttpResponse &res) {
    // ...
    res.sendJson(json);
    return Result::Ok;
}
```

### HTTP handlers

Implement `http::HttpHandler` and override `handle()`. Use `HttpHandler::extractTarget(req.path())` to obtain the last path segment for sub-route dispatch. Prefer a private `handleGet` / `handlePost` split at the top level, then per-target methods.

```cpp
common::Result MyApiHandler::handle(HttpRequest &req, HttpResponse &res) {
    switch (req.method()) {
        case HttpMethod::Get:  return handleGet (req, res);
        case HttpMethod::Post: return handlePost(req, res);
        default:
            res.sendJson(405, "Method not allowed");
            return Result::Ok;
    }
}
```

Register new handlers in `EmbeddedServer` by adding a `{prefix, &handler}` entry to the `routes` vector in `EmbeddedServer::start()`, and wire the handler instance in `FrameworkContext`.

### Large request bodies (OTA uploads)

`HttpRequest` pre-reads the body into memory for normal requests. It skips pre-read when `content_len` exceeds `MAX_PRELOAD_BYTES` (64 KB). For streaming handlers (e.g. firmware upload) access the raw `httpd_req_t*` via `req.raw()` and use `httpd_req_recv()` directly. Do not pre-read large bodies — the ESP32-S3 internal SRAM will not hold a 1–2 MB firmware image.

### Logging

Add a per-component log level in `app_main.cpp → setupLogging()`:

```cpp
LogSinkRegistry::setLevelForTag("MyComponent", LogLevel::Debug);
```

Use `log.debug()`, `log.info()`, `log.warn()`, `log.error()` throughout. The tag is set in the constructor: `Logger log{"MyComponent"}`.

## OTA system

The OTA system has three classes with distinct responsibilities:

- **`OtaManager`** — boot-time guardian and validity marking. Call `checkOnBoot()` at the very top of `app_main` before any tasks. Call `markValid()` once the system is healthy (currently done in `EmbeddedServer::start()`).
- **`OtaWriter`** — streams a binary from an HTTP socket to the inactive OTA partition. Returns false on error (response already sent); does not return on success (calls `esp_restart()`).
- **`OtaApiHandler`** — HTTP front-end. Delegates upload to `OtaWriter`; implements rollback and factory-reset logic directly.

When `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` is not compiled in, `esp_ota_get_state_partition()` always returns `ESP_OTA_IMG_UNDEFINED` for all partitions. 
If OTA state queries return unexpected results, verify that `sdkconfig` has `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`, run a fullclean and rebuild.

## Versioning

Bump `version.txt` before building a release. The build system copies `build/esp32_app_framework.bin` to `build/esp32_app_framework-<version>.bin` after each successful build. 
This versioned binary is the file to distribute for OTA updates.

## Partition layout changes

If you modify `partitions.csv`, you must also update:

1. `sdkconfig` if the custom partition file path changes.
2. The partition offsets table in `README.md`.
3. Any hardcoded size assumptions in `OtaWriter` (currently none — it reads `OTA_WITH_SEQUENTIAL_WRITES`).

After any partition change, fullclean, build and re-flash the bootloader via USB (`idf.py flash`). OTA updates do not update the bootloader or partition table.

## Static assets (embedded UI)

The web UI source files live in `components/_framework_files/files/`. They are bundled into the `assets_fs` LittleFS partition at flash time. The JavaScript stack is vanilla ES2020 — no build step, no bundler. The three main files are:

- `_framework_api.js` — fetch wrappers for every API endpoint
- `_framework_app.js` — route definitions and HTML templates
- `_framework_ui.js` — DOM wiring, state, event handlers

When adding a new API endpoint, add the corresponding fetch wrapper to `_framework_api.js` first, then wire up any UI in `_framework_app.js` / `_framework_ui.js`.
