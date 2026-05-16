# Creating an application using the framework

This guide walks through setting up a new ESP-IDF project that consumes the framework as a git submodule. Your application lives in its own repository and pulls in framework updates on demand, with full control over when to take them.

## Quick start — GitHub template

The fastest way to begin is the [embedded-app-template](https://github.com/jp-irons/embedded-app-template) repository. It contains all the boilerplate wired up and ready, with the embedded-framework already added as a submodule pinned to the latest release.

1. Open the template on GitHub and click **Use this template → Create a new repository**.
2. Clone your new repository with submodules:

```bash
git clone --recurse-submodules https://github.com/your-org/my_app.git
cd my_app
```
3. Espressif IDE (Eclipse)
From Espressif IDE: File -> Import... -> Espressif -> Existing IDF Project
Existing project Location -> Browse (to cloned directory) -> Finish
Open a terminal
idf.py build or idf.py build flash monitor

OR
3. Install managed dependencies and build:

```bash
idf.py update-dependencies
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Edit `version.txt`, `main/ApplicationContext.cpp`, and `main/app_files/` to start building your application. The manual steps below describe what the template contains and why, and are useful when you need to understand or customise the setup.

---

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
      framework_files/
      framework/
      http/
      logger/
      ota/
      wifi_manager/
    ...
```

## Manual setup

The steps below walk through creating the project structure by hand. This is useful if the template doesn't fit your situation, or if you want to understand what each piece does.

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
git commit -m "Initial project framework"
```

## Step 2 — Add the framework as a git submodule

```bash
git submodule add https://github.com/jp-irons/embedded-framework.git framework
git commit -m "Add embedded-framework as submodule"
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

## Step 5 — Copy build configuration files and create version.txt

The framework requires specific bootloader and partition settings. Copy all three config files from the framework repository as a starting point:

```bash
cp framework/sdkconfig .
cp framework/sdkconfig.defaults .
cp framework/partitions.csv .
```

`sdkconfig` is the live build configuration and the source of truth for your project — it should be committed to your repository and not gitignored. `sdkconfig.defaults` serves as a regeneration baseline if `sdkconfig` is ever recreated from scratch. The framework's copies give you a proven working starting point; use `idf.py menuconfig` to adjust settings for your application.

At minimum, `sdkconfig.defaults` must contain:

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

Create a `version.txt` at the repository root with your initial version string. The OTA update system reads this file at build time and embeds the version in the firmware binary:

```bash
echo "0.1.0" > version.txt
```

Commit all four files:

```bash
git add sdkconfig sdkconfig.defaults partitions.csv version.txt
git commit -m "Add build configuration"
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
file(GLOB_RECURSE APP_EMBED_FILES "app_files/files/*")

idf_component_register(
    SRCS
        "app_main.cpp"
        "ApplicationContext.cpp"
        "app_files/AppFileTable.cpp"
    INCLUDE_DIRS
        "."
        "app_files"
    EMBED_FILES
        ${APP_EMBED_FILES}
)
```

The `GLOB_RECURSE` collects every file under `main/app_files/files/` and embeds them directly in the firmware binary. See [Serving static files](#serving-static-files) below for how to register embedded files at their URL paths.

## Step 9 — Build and flash

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Keeping the framework up to date

The submodule in your app repo is a pointer to a specific commit SHA. Nothing changes in your build until you explicitly move that pointer and commit the result. You are always in control of when to take a framework update.

### Checking your current version and available releases

```bash
# What your app is currently on
cat framework/version.txt
git -C framework describe --tags

# Fetch new tags without changing anything
git -C framework fetch --tags

# List available releases
git -C framework tag --sort=-version:refname | head -10
```

### Updating to a new release

```bash
cd framework
git fetch --tags
git checkout v0.2.0          # the target release
cd ..
```

Before committing, work through the checklist below — the submodule move may need accompanying changes in your app.

### Update checklist

**1. sdkconfig.defaults** — diff the framework's copy against yours and pull in any new required settings:

```bash
diff framework/sdkconfig.defaults sdkconfig.defaults
```

**2. sdkconfig** — run `idf.py menuconfig` to absorb any new Kconfig symbols introduced by new or changed components. ESP-IDF will use Kconfig defaults for any symbol not already in `sdkconfig`; review new entries before saving. Commit the updated `sdkconfig`.

**3. idf_component.yml** — check whether the framework has added new managed component dependencies:

```bash
diff framework/main/idf_component.yml main/idf_component.yml
```

Add any new entries to your `main/idf_component.yml` and run `idf.py update-dependencies`.

**4. partitions.csv** — if `framework/partitions.csv` has changed, copy it across and treat this as a partition layout change: fullclean, rebuild, and USB-reflash. OTA cannot apply partition or bootloader changes.

```bash
diff framework/partitions.csv partitions.csv
```

**5. API changes** — build and fix any compile errors. Patch releases (`v0.1.1 → v0.1.2`) should not break the `ApplicationContext` or handler interfaces; minor version bumps (`v0.1.x → v0.2.0`) may do so and will be noted in the release changelog.

**6. Commit atomically** — record the submodule move and all accompanying changes together so your app history stays coherent:

```bash
git add framework sdkconfig sdkconfig.defaults partitions.csv main/idf_component.yml
git commit -m "Update framework to v0.2.0"
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

Static files are embedded directly in the firmware binary at build time. There are two parts to wiring this up: the CMake side (which handles embedding) and `AppFileTable` (which maps embedded symbols to URL paths).

### File layout

```
main/
  app_files/
    AppFileTable.hpp       ← class declaration
    AppFileTable.cpp       ← URL-to-symbol mapping (update for each new file)
    files/
      favicon.ico          → served at /favicon.ico
      app/ui/index.html    → served at /app/ui/index.html
      app/ui/styles.css    → served at /app/ui/styles.css
      app/ui/app.js        → served at /app/ui/app.js
```

The `GLOB_RECURSE` in `main/CMakeLists.txt` picks up every file under `app_files/files/` and embeds it in the binary. ESP-IDF generates a pair of linker symbols for each file — `_binary_<mangled_name>_start` and `_binary_<mangled_name>_end` — where the file path is flattened and non-alphanumeric characters become underscores.

`AppFileTable` maps those symbols to their URL paths. **This registration is not automatic** — you must add an `extern` declaration and a table entry for each file.

### AppFileTable.hpp

```cpp
#pragma once
#include "framework_files/EmbeddedFileTable.hpp"

class AppFileTable : public framework_files::EmbeddedFileTable {
public:
    static constexpr const char* TAG = "AppFileTable";

    const framework_files::EmbeddedFile* find(std::string_view path) const override;
    const uint8_t* find(const char* path, size_t& outSize) const override;
};
```

### AppFileTable.cpp

```cpp
#include "AppFileTable.hpp"
#include <cstring>

// Declare one pair of linker symbols per embedded file.
// Symbol name rule: flatten the file path relative to the component directory,
// replacing '/', '.', and '-' with '_', then wrap with _binary_ ... _start/_end.
// e.g. app_files/files/app/ui/index.html → _binary_app_ui_index_html_start

extern const uint8_t _binary_favicon_ico_start[]        asm("_binary_favicon_ico_start");
extern const uint8_t _binary_favicon_ico_end[]          asm("_binary_favicon_ico_end");
extern const uint8_t _binary_app_ui_index_html_start[]  asm("_binary_app_ui_index_html_start");
extern const uint8_t _binary_app_ui_index_html_end[]    asm("_binary_app_ui_index_html_end");
extern const uint8_t _binary_app_ui_styles_css_start[]  asm("_binary_app_ui_styles_css_start");
extern const uint8_t _binary_app_ui_styles_css_end[]    asm("_binary_app_ui_styles_css_end");
extern const uint8_t _binary_app_ui_app_js_start[]      asm("_binary_app_ui_app_js_start");
extern const uint8_t _binary_app_ui_app_js_end[]        asm("_binary_app_ui_app_js_end");

namespace {
struct FileEntry { const char *path; const uint8_t *start; const uint8_t *end; };

static const FileEntry files[] = {
    {"/favicon.ico",       _binary_favicon_ico_start,       _binary_favicon_ico_end},
    {"/app/ui/index.html", _binary_app_ui_index_html_start, _binary_app_ui_index_html_end},
    {"/app/ui/styles.css", _binary_app_ui_styles_css_start, _binary_app_ui_styles_css_end},
    {"/app/ui/app.js",     _binary_app_ui_app_js_start,     _binary_app_ui_app_js_end},
};
} // anonymous namespace

const framework_files::EmbeddedFile* AppFileTable::find(std::string_view path) const {
    for (const auto &e : files) {
        if (path == e.path) {
            static framework_files::EmbeddedFile result;
            result.data = e.start;
            result.size = e.end - e.start;
            return &result;
        }
    }
    return nullptr;
}

const uint8_t* AppFileTable::find(const char* path, size_t &outSize) const {
    for (const auto &e : files) {
        if (std::strcmp(e.path, path) == 0) {
            outSize = e.end - e.start;
            return e.start;
        }
    }
    return nullptr;
}
```

When you add a new file, add it to `app_files/files/`, then add the two `extern` lines and a table entry to `AppFileTable.cpp`. The symbol name mangling is straightforward — if you are unsure of a generated symbol name, run `nm build/main/libmain.a | grep _binary_` after building.

The app file handler is tried before the framework's own file handler, so app-provided files always take precedence over framework assets at the same path.

### Favicon

The framework does not provide a fallback `favicon.ico`. If your application does not include one at `main/app_files/files/favicon.ico`, browsers will receive a 404 for that request. Add your own favicon to suppress this.
