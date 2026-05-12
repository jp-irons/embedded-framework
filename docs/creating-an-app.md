# Creating an application using the framework

This guide walks through setting up a new ESP-IDF project that consumes the framework as a git submodule. Your application lives in its own repository and pulls in framework updates on demand, with full control over when to take them.

## Overview

The framework's `components/` directory contains all reusable modules. Your application provides its own `main/` and wires in the framework components via ESP-IDF's `EXTRA_COMPONENT_DIRS` mechanism. The framework repository's own `main/` (the demo application) is ignored by your build.

```
my_app/                          ← your git repository
  main/
    CMakeLists.txt
    idf_component.yml            ← declares managed dependencies
    app_main.cpp                 ← calls FrameworkContext + your ApplicationContext
    ApplicationContext.h/.cpp    ← your application logic
  CMakeLists.txt                 ← sets EXTRA_COMPONENT_DIRS, calls project()
  sdkconfig.defaults
  partitions.csv
  framework/                     ← git submodule (this repo)
    components/
      auth/
      common/
      network_store/
      device/
      device_cert/
      _framework_files/
      framework/
      http/
      logger/
      ota/
      wifi_manager/
    ...
```

## Prerequisites

- ESP-IDF v6.0 installed and on `PATH`
- Python 3.8+
- CMake 3.16+
- Git

## Step 1 — Create a new ESP-IDF project

```bash
idf.py create-project my_app
cd my_app
git init
git add .
git commit -m "Initial project scaffold"
```

## Step 2 — Add the framework as a git submodule

```bash
git submodule add https://github.com/jp-irons/esp32-app-framework.git framework
git commit -m "Add esp32-app-framework as submodule"
```

When cloning your repository on a new machine, the submodule must be initialised:

```bash
git clone --recurse-submodules https://github.com/your-org/my_app.git
# or, if already cloned without --recurse-submodules:
git submodule update --init --recursive
```

## Step 3 — Configure CMakeLists.txt

Replace the generated `CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.16)

# Pull in all framework components
set(EXTRA_COMPONENT_DIRS framework/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

The framework's `main/` component is not in `components/` so it is never included in your build.

## Step 4 — Declare managed dependencies

The framework components depend on two IDF-managed libraries. Declare them in your application's `main/idf_component.yml`:

```yaml
dependencies:
  espressif/cjson: "^1.7.0"
  espressif/mdns: "^1.4.0"
```

Run the component manager to download them:

```bash
idf.py update-dependencies
```

## Step 5 — Copy sdkconfig.defaults and partitions.csv

The framework requires specific bootloader and partition settings. Copy the files from the framework repository as a starting point:

```bash
cp framework/sdkconfig.defaults .
cp framework/partitions.csv .
```

At minimum, `sdkconfig.defaults` must contain:

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

## Step 6 — Implement ApplicationContext

The framework calls into your application through `ApplicationContext`. Create the following two files in `main/`.

**main/ApplicationContext.h**

```cpp
#pragma once

namespace app {

class ApplicationContext {
public:
    void start();   // called once after the framework is fully up
    void loop();    // called every 50 ms from app_main
};

} // namespace app
```

**main/ApplicationContext.cpp**

```cpp
#include "ApplicationContext.h"

namespace app {

void ApplicationContext::start() {
    // one-time application startup — register your handlers, start tasks, etc.
}

void ApplicationContext::loop() {
    // periodic work, or leave empty
}

} // namespace app
```

## Step 7 — Write app_main.cpp

```cpp
#include "framework/FrameworkContext.h"
#include "ApplicationContext.h"

extern "C" void app_main() {
    // Boot guardian must run before anything else
    framework::FrameworkContext fw{};

    app::ApplicationContext app{};
    app.start();

    for (;;) {
        app.loop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

To customise the AP name and mDNS prefix:

```cpp
wifi_manager::ApConfig apConfig = {
    .ssid        = "MyDevice",
    .password    = "password",
    .channel     = 1,
    .maxConnections = 4
};
framework::FrameworkContext fw{apConfig, "/api", "mydevice"};
```

The device will advertise itself as `mydevice-<last3MacBytes>.local`.

## Step 8 — Configure main/CMakeLists.txt

```cmake
idf_component_register(
    SRCS
        "app_main.cpp"
        "ApplicationContext.cpp"
    INCLUDE_DIRS "."
)
```

## Step 9 — Build and flash

```bash
rm -f sdkconfig          # ensure sdkconfig is generated fresh from sdkconfig.defaults
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Keeping the framework up to date

### Updating to the latest commit

```bash
git submodule update --remote framework
git add framework
git commit -m "Update framework to latest"
```

### Pinning to a specific release

```bash
cd framework
git checkout v1.0.0
cd ..
git add framework
git commit -m "Pin framework to v1.0.0"
```

Pinning is recommended for production projects. You control exactly when to take framework changes and can review the changelog before updating.

### Checking what version you are on

```bash
git -C framework describe --tags
# or
cat framework/version.txt
```

---

## Adding your own HTTP API handlers

To extend the framework's HTTP API, implement `http::HttpHandler` in your application and register it with `FrameworkContext`:

```cpp
// In ApplicationContext.cpp or a dedicated handler file
#include "http/HttpHandler.h"

class MyApiHandler : public http::HttpHandler {
public:
    common::Result handle(http::HttpRequest &req, http::HttpResponse &res) override {
        res.sendJson(200, "{\"status\":\"ok\"}");
        return common::Result::Ok;
    }
};
```

Refer to `CONTRIBUTING.md` in the framework repository for the full handler conventions.

---

## Serving static files

The framework serves embedded static files through `AppFileTable`. Any file placed under `main/app_files/files/` is picked up automatically by the `GLOB_RECURSE` in `main/CMakeLists.txt`, embedded into the firmware image, and registered in the file table with its URL path.

```
main/
  app_files/
    files/
      favicon.ico          → served at /favicon.ico
      app/ui/index.html    → served at /app/ui/index.html
      app/ui/styles.css    → served at /app/ui/styles.css
      app/ui/app.js        → served at /app/ui/app.js
```

The app file handler is tried before the framework's own file handler, so app-provided files always take precedence over framework assets at the same path.

### Favicon

The framework does not provide a fallback `favicon.ico`. If your application does not include one at `main/app_files/files/favicon.ico`, browsers will receive a 404 for that request. Add your own favicon to suppress this.
