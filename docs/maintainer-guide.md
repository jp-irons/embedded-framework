# Maintainer guide

Operational procedures for framework maintainers: versioning, cutting releases, and partition table changes. For coding conventions and component architecture see `CONTRIBUTING.md`.

## Versioning

The version string lives in `version.txt` at the repository root. Bump it before building a release — the build system embeds it in the firmware binary, where it is accessible at runtime via `esp_app_get_description()->version`, and the OTA update logic uses it to decide whether a download is needed.

After a successful build, `build/embedded_framework.bin` is copied to `build/embedded_framework-<version>.bin` automatically.

## Release process

Releases are built and published automatically by GitHub Actions when a version tag is pushed. The workflow (`.github/workflows/release.yml`) triggers on tags matching `v*`, builds the firmware, and uploads `firmware.bin` and `version.txt` as release assets. Devices polling GitHub will pick up the update on their next OTA check once the release is published.

**Pre-flight checklist**

1. Bump `version.txt` to the new version string (e.g. `0.0.3`).
2. Commit and push `version.txt` to `development`.
3. Verify CI is green on the `development` branch.

**Tagging and publishing**

```bash
git push origin development:main    # bring main up to date without switching branches
git tag v0.0.3                      # tag must match version.txt exactly (without the v prefix)
git push origin v0.0.3              # triggers the Actions build and release
```

The workflow validates that the tag version matches `version.txt` before building — if they are out of sync it fails immediately.

> **Do not push a tag until `version.txt` has been committed and pushed.**

## Partition layout changes

The source of truth is `partitions.csv`. When you modify it, work through this checklist before pushing:

1. **Update `docs/flash_layout.md`** — keep the offset/size table in sync with `partitions.csv`.
2. **Update `README.md`** — if it contains a partition table, update it to match.
3. **Update `sdkconfig`** — only needed if the path to the custom partition CSV has changed; `sdkconfig` is committed and is the source of truth for build configuration, so do not delete it as a routine step.
4. **Check `OtaWriter`** — verify there are no hardcoded partition size assumptions (currently none; it reads `OTA_WITH_SEQUENTIAL_WRITES`).
5. **Fullclean, build and USB-flash** — run `idf.py fullclean && idf.py flash`. OTA updates do not touch the bootloader or partition table; the only way to apply partition changes to a device is over USB.

> Partition table changes are not OTA-compatible. All devices must be reflashed by hand after a layout change.
