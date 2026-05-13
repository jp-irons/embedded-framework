# Setting up on a new machine

This guide covers getting the framework built and running from a fresh clone on a machine that already has the **Espressif IDE (Eclipse-based) with ESP-IDF v6.0** installed.

## Prerequisites

- Espressif IDE (Eclipse-based) with ESP-IDF v6.0 installed
- Git

No other tooling is required — the IDE bundles CMake, Python, and the compiler toolchain.

## 1. Clone the repository

Use the ESP-IDF Terminal inside the Espressif IDE so the environment variables are already configured:

```bash
git clone https://github.com/jp-irons/embedded-framework
```

The project has no git submodules, so no further init step is needed.

## 2. Import into Eclipse

Open **File → Import → Espressif → Existing IDF Project**, then browse to the `embedded-framework` folder. Eclipse will detect the `CMakeLists.txt` automatically.

## 3. Confirm the target

The project targets **ESP32-S3**. Verify the active target in the IDE matches. If it does not, set it from the target selector or run:

```bash
idf.py set-target esp32s3
```

The committed `sdkconfig` already encodes the correct target, so this should be a no-op on a clean clone.

## 4. Build

Use the IDE's build button, or from the ESP-IDF Terminal:

```bash
idf.py build
```

`sdkconfig` is committed and is the source of truth for the build configuration — no menuconfig step is required before building. Do not delete `sdkconfig` before building.

## 5. Flash and monitor

Connect the ESP32-S3 board via USB, then:

```bash
idf.py -p COMx flash monitor
```

Replace `COMx` with the correct port for your machine (e.g. `COM3` on Windows, `/dev/ttyUSB0` on Linux). The IDE's flash/monitor toolbar button will handle port detection automatically if you prefer.

## sdkconfig and sdkconfig.defaults

`sdkconfig` is the authoritative build configuration and is committed to the repository. `sdkconfig.defaults` captures the critical non-default settings as a regeneration baseline — if `sdkconfig` is ever deleted or corrupted, running `idf.py build` will regenerate it from `sdkconfig.defaults`.

The key settings captured in `sdkconfig.defaults` are:

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_MBEDTLS_X509_CREATE_C=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="16MB"
```

If a build fails with a partition table or flash size error, verify these settings are present in `sdkconfig`. If they are missing, they can be restored by running menuconfig or by deleting `sdkconfig` and rebuilding from `sdkconfig.defaults` — but only after confirming that `sdkconfig.defaults` is up to date, as any settings in `sdkconfig` that are absent from `sdkconfig.defaults` will be lost.
