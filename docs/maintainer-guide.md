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
#git push origin development:main    # bring main up to date without switching branches
git push origin
git tag -a v0.2.10 -m "chunked http transfers"  # tag must match version.txt exactly (without the v prefix)
git push origin v0.2.10              # triggers the Actions build and release

# in target repo root

cd framework
git fetch --tags
git -C . describe --tags
git checkout v0.2.10       # e.g. v1.2.0
cd ..

git add framework
git commit -m "Update framework submodule to v0.2.10"
git push

```


The workflow validates that the tag version matches `version.txt` before building — if they are out of sync it fails immediately.

> **Do not push a tag until `version.txt` has been committed and pushed.**

## Delete a release

In GitHub delete the release, then delete the tags
```bash
git tag -d <tagname>  
git push origin --delete <tagname>
```



## Template repository

The [embedded-app-template](https://github.com/jp-irons/embedded-app-template) repository is a GitHub template that gives app developers a ready-to-build starting point. Its submodule is pinned to a specific framework release, so new apps always start from a known-good baseline.

**The template is not updated automatically** — it must be kept in sync manually each time a framework release is cut. Add this to the release checklist.

### Updating the template after a release

Once the framework release is published and verified:

all this from directory above where embedded-app-template should go.

```bash
git clone --recurse-submodules https://github.com/jp-irons/embedded-app-template.git
cd embedded-app-template
git submodule update --init --recursive

git -C framework describe --tags
cd framework
git -C . describe --tags
git fetch --tags
git -C . describe --tags
git checkout <tagname>       # e.g. v1.2.0
cd ..

# Copy updated config baselines from the framework
cp framework/sdkconfig sdkconfig
cp framework/sdkconfig.defaults sdkconfig.defaults
cp framework/main .
# Update the version placeholder
echo "0.1.0" > version.txt           # reset to a generic app starting point — not the framework version
# Note: the template uses framework/partitions/factory_ota0_ota1.csv by default.
# sdkconfig already contains CONFIG_PARTITION_TABLE_CUSTOM_FILENAME pointing into
# the framework submodule — no separate partitions.csv copy is needed.
```

Review and commit:

```bash
git add framework sdkconfig sdkconfig.defaults
git commit -m "Update framework submodule to v0.2.0"
git push
```


Do not update `version.txt` in the template to match the framework version — it is an app version placeholder and should stay at a generic starting value (e.g. `0.1.0`).

Updating a project that uses the framework submodule:

# From the project root, checkout vx.y.z in the submodule

```bash
cd framework
git fetch --tags origin
git checkout vx.y.z
cd ..

# Stage and commit
git add framework
git commit -m "update framework submodule to vx.y.z"
git push
```
consider whether this needs to be merged with the main branch.
git push origin development:main    # bring main up to date without switching branches

### What the template must always contain

- `framework/` — submodule pinned to the latest release
- `sdkconfig`, `sdkconfig.defaults` — copied from that release; `sdkconfig` points `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` at the chosen layout inside the submodule
- `version.txt` — generic starting version (`0.1.0`)
- `main/CMakeLists.txt` — with `GLOB_RECURSE` / `EMBED_FILES` wired up
- `main/idf_component.yml` — managed dependencies matching the framework
- `main/app_main.cpp`, `main/ApplicationContext.h/.cpp` — minimal working stubs
- `main/app_files/AppFileTable.hpp/.cpp` — empty file table ready to extend
- `main/app_files/files/favicon.ico` — placeholder favicon

## Partition layout changes

The pre-built layouts live in `partitions/`. When adding or modifying a layout, work through this checklist before pushing:

1. **Update `docs/flash_layout.md`** — keep the offset/size table and layout comparison in sync.
2. **Update `README.md`** — update the default layout table if `factory_ota0_ota1.csv` changed; update the `sdkconfig` snippet if the default filename changed.
3. **Update `sdkconfig`** — only needed if the path to the custom partition CSV has changed; `sdkconfig` is committed and is the source of truth for build configuration, so do not delete it as a routine step.
4. **Check `OtaWriter`** — verify there are no hardcoded partition size assumptions (currently none; it uses `OTA_WITH_SEQUENTIAL_WRITES`).
5. **Fullclean, build and USB-flash** — run `idf.py fullclean && idf.py flash`. OTA updates do not touch the bootloader or partition table; the only way to apply partition changes to a device is over USB.

> Partition table changes are not OTA-compatible. All devices must be reflashed by hand after a layout change.
