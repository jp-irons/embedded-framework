# Flash layout

Partition table for the current ESP32-S3 target. Defined in `partitions.csv` and selected via `sdkconfig.defaults`. If support for additional targets is added, each target will maintain its own partition layout.

| Name       | Type | SubType  | Offset     | Size       | Notes                              |
|------------|------|----------|------------|------------|------------------------------------|
| nvs        | data | nvs      | 0x009000   | 24 KB      | NVS key-value store                |
| otadata    | data | ota      | 0x00F000   |  8 KB      | OTA boot-slot selection record     |
| factory    | app  | factory  | 0x020000   |  2 MB      | Factory image (USB flash only)     |
| ota_0      | app  | ota_0    | 0x220000   |  2 MB      | OTA slot 0                         |
| ota_1      | app  | ota_1    | 0x420000   |  2 MB      | OTA slot 1                         |
| assets_fs  | data | littlefs | 0x620000   |  1.875 MB  | Embedded web UI and static assets  |

## Notes

- **otadata** records which OTA slot to boot. Erasing it (`POST /framework/api/firmware/factoryReset`) causes the bootloader to fall back to the factory partition.
- **factory** can only be updated by flashing over USB — OTA updates never touch it.
- **ota_0 / ota_1** are used alternately for OTA updates. The inactive slot receives each new image; on success it becomes the next-boot slot.
- **assets_fs** is a LittleFS volume flashed separately. It holds the web UI files served by `EmbeddedServer`.

## Changing the partition layout

If you modify `partitions.csv`:

1. Update this file to match.
2. Update the partition table in `README.md`.
3. Delete `sdkconfig`, then do a full clean build and re-flash via USB (`idf.py fullclean && idf.py flash`). OTA updates do not update the bootloader or partition table.
