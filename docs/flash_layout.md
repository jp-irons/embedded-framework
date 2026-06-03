# Flash layout

Three pre-built partition layouts are provided in `partitions/`. The consuming app selects one in its `sdkconfig`. All layouts share the same NVS and otadata placement; only the app and data partitions differ.

## Layout comparison

| Layout file                  | Flash | Factory | OTA slots      | LittleFS | Use when                                      |
|------------------------------|-------|---------|----------------|----------|-----------------------------------------------|
| `factory_ota0_ota1.csv`      | 16 MB | 4 MB    | 2 × 4 MB       | 2 MB     | Default. Full recovery fallback via USB.      |
| `ota0_ota1.csv`              |  8 MB | —       | 2 × ~1.875 MB  | 2 MB     | No factory fallback needed; app needs LittleFS|
| `ota0_ota1_4MB.csv`          |  4 MB | —       | 2 × ~1.875 MB  | —        | 4 MB flash; no factory fallback or LittleFS   |

---

## factory_ota0_ota1.csv (default, 16 MB)

| Name       | Type | SubType  | Offset     | Size       | Notes                              |
|------------|------|----------|------------|------------|------------------------------------|
| nvs        | data | nvs      | 0x009000   | 24 KB      | NVS key-value store                |
| otadata    | data | ota      | 0x00F000   |  8 KB      | OTA boot-slot selection record     |
| factory    | app  | factory  | 0x020000   |  4 MB      | Factory image (USB flash only)     |
| ota_0      | app  | ota_0    | 0x420000   |  4 MB      | OTA slot 0                         |
| ota_1      | app  | ota_1    | 0x820000   |  4 MB      | OTA slot 1                         |
| assets_fs  | data | littlefs | 0xC20000   |  2 MB      | Reserved for downstream app use    |

Required `sdkconfig` entries:

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/factory_ota0_ota1.csv"
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_FRAMEWORK_HAS_FACTORY_PARTITION=y
```

---

## ota0_ota1.csv (8 MB, no factory)

| Name       | Type | SubType  | Offset     | Size         | Notes                              |
|------------|------|----------|------------|--------------|------------------------------------|
| nvs        | data | nvs      | 0x009000   | 24 KB        | NVS key-value store                |
| otadata    | data | ota      | 0x00F000   |  8 KB        | OTA boot-slot selection record     |
| ota_0      | app  | ota_0    | 0x020000   | ~1.875 MB    | OTA slot 0                         |
| ota_1      | app  | ota_1    | 0x210000   | ~1.875 MB    | OTA slot 1                         |
| assets_fs  | data | littlefs | 0x400000   |  2 MB        | Reserved for downstream app use    |

Required `sdkconfig` entries:

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/ota0_ota1.csv"
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_FRAMEWORK_HAS_FACTORY_PARTITION=n
```

---

## ota0_ota1_4MB.csv (4 MB, no factory, no LittleFS)

| Name       | Type | SubType  | Offset     | Size         | Notes                              |
|------------|------|----------|------------|--------------|------------------------------------|
| nvs        | data | nvs      | 0x009000   | 24 KB        | NVS key-value store                |
| otadata    | data | ota      | 0x00F000   |  8 KB        | OTA boot-slot selection record     |
| ota_0      | app  | ota_0    | 0x020000   | ~1.875 MB    | OTA slot 0                         |
| ota_1      | app  | ota_1    | 0x210000   | ~1.875 MB    | OTA slot 1                         |

Required `sdkconfig` entries:

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/ota0_ota1_4MB.csv"
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_FRAMEWORK_HAS_FACTORY_PARTITION=n
```

---

## Notes

**otadata** records which OTA slot to boot. Erasing it (`POST /framework/api/firmware/factoryReset`) causes the bootloader to fall back to the factory partition — only meaningful with the `factory_ota0_ota1` layout. On two-slot layouts the factory reset endpoint returns 501 and the button is hidden in the web UI.

**factory** can only be updated by flashing over USB — OTA updates never touch it.

**ota_0 / ota_1** are used alternately for OTA updates. The inactive slot receives each new image; on success it becomes the next-boot slot.

**assets_fs** is a LittleFS volume reserved for downstream app use. The framework itself does not use it — web assets are embedded directly in the firmware binary. It is absent from the 4 MB layout.

**Boot-failure escalation** differs by layout. With a factory partition, exceeding the boot-attempt limit erases otadata and reboots to factory. Without one, the framework logs the failure and continues — the hardware watchdog will reset the device if the image truly cannot boot.

## Changing the partition layout

If you switch layouts or modify a CSV:

1. Update this file to match.
2. Update the partition table in `README.md`.
3. Update `sdkconfig` — set `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` to the new path and set `CONFIG_FRAMEWORK_HAS_FACTORY_PARTITION` accordingly.
4. Run `idf.py fullclean && idf.py flash` to apply the new table via USB. OTA updates do not update the bootloader or partition table.

For the full checklist (OtaWriter checks, release implications) see [`maintainer-guide.md`](maintainer-guide.md).
