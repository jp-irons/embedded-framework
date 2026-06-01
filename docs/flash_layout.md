# Flash layout

Partition table for the current ESP32-S3 target. Defined in `partitions.csv` and selected via `sdkconfig.defaults`. If support for additional targets is added, each target will maintain its own partition layout.

| Name       | Type | SubType  | Offset     | Size       | Notes                              |
|------------|------|----------|------------|------------|------------------------------------|
| nvs        | data | nvs      | 0x009000   | 24 KB      | NVS key-value store                |
| otadata    | data | ota      | 0x00F000   |  8 KB      | OTA boot-slot selection record     |
| factory    | app  | factory  | 0x020000   |  4 MB      | Factory image (USB flash only)     |
| ota_0      | app  | ota_0    | 0x420000   |  4 MB      | OTA slot 0                         |
| ota_1      | app  | ota_1    | 0x820000   |  4 MB      | OTA slot 1                         |
| assets_fs  | data | littlefs | 0xC20000   |  2 MB      | Reserved for downstream app use    |

## Notes

- **otadata** records which OTA slot to boot. Erasing it (`POST /framework/api/firmware/factoryReset`) causes the bootloader to fall back to the factory partition.
- **factory** can only be updated by flashing over USB — OTA updates never touch it.
- **ota_0 / ota_1** are used alternately for OTA updates. The inactive slot receives each new image; on success it becomes the next-boot slot.
- **assets_fs** is a LittleFS volume reserved for downstream app use. The framework itself does not use it — web assets are embedded directly in the firmware binary.

## Changing the partition layout

If you modify `partitions.csv`:

1. Update this file to match.
2. Update the partition table in `README.md`.
3. Run `idf.py fullclean && idf.py flash` to apply the new table via USB. OTA updates do not update the bootloader or partition table.

For the full checklist (sdkconfig, OtaWriter checks, and release implications) see [`maintainer-guide.md`](maintainer-guide.md).
