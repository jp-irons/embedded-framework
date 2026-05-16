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

## Template repository

The [embedded-app-template](https://github.com/jp-irons/embedded-app-template) repository is a GitHub template that gives app developers a ready-to-build starting point. Its submodule is pinned to a specific framework release, so new apps always start from a known-good baseline.

**The template is not updated automatically** — it must be kept in sync manually each time a framework release is cut. Add this to the release checklist.

### Updating the template after a release

Once the framework release is published and verified:

```bash
cd embedded-app-template
git -C framework fetch --tags
git -C framework checkout v0.2.0     # the new release tag

# Copy updated config baselines from the framework
cp framework/sdkconfig sdkconfig
cp framework/sdkconfig.defaults sdkconfig.defaults
cp framework/partitions.csv partitions.csv

# Update the version placeholder
echo "0.1.0" > version.txt           # reset to a generic app starting point — not the framework version
```

Review and commit:

```bash
git add framework sdkconfig sdkconfig.defaults partitions.csv
git commit -m "Update framework submodule to v0.2.0"
git push
```

Do not update `version.txt` in the template to match the framework version — it is an app version placeholder and should stay at a generic starting value (e.g. `0.1.0`).

### What the template must always contain

- `framework/` — submodule pinned to the latest release
- `sdkconfig`, `sdkconfig.defaults`, `partitions.csv` — copied from that release
- `version.txt` — generic starting version (`0.1.0`)
- `main/CMakeLists.txt` — with `GLOB_RECURSE` / `EMBED_FILES` wired up
- `main/idf_component.yml` — managed dependencies matching the framework
- `main/app_main.cpp`, `main/ApplicationContext.h/.cpp` — minimal working stubs
- `main/app_files/AppFileTable.hpp/.cpp` — empty file table ready to extend
- `main/app_files/files/favicon.ico` — placeholder favicon

## Partition layout changes

The source of truth is `partitions.csv`. When you modify it, work through this checklist before pushing:

1. **Update `docs/flash_layout.md`** — keep the offset/size table in sync with `partitions.csv`.
2. **Update `README.md`** — if it contains a partition table, update it to match.
3. **Update `sdkconfig`** — only needed if the path to the custom partition CSV has changed; `sdkconfig` is committed and is the source of truth for build configuration, so do not delete it as a routine step.
4. **Check `OtaWriter`** — verify there are no hardcoded partition size assumptions (currently none; it reads `OTA_WITH_SEQUENTIAL_WRITES`).
5. **Fullclean, build and USB-flash** — run `idf.py fullclean && idf.py flash`. OTA updates do not touch the bootloader or partition table; the only way to apply partition changes to a device is over USB.

> Partition table changes are not OTA-compatible. All devices must be reflashed by hand after a layout change.
