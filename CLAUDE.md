# ESP32 App Framework — Claude Notes

## Project overview

ESP32-S3 application framework built on ESP-IDF 6.x. Single hardware target: ESP32-S3 with 16 MB flash.
The framework is designed to be consumed as a git submodule by downstream application projects.

## Before making changes

Always explain what you plan to do and ask for confirmation before editing any file.

## Key facts

- `sdkconfig` is committed and is the source of truth for the build configuration. Do not suggest deleting it as a routine step.
- `sdkconfig.defaults` exists as a regeneration baseline but may not be current. Treat it as supplementary, not authoritative.
- Component directory is `components/framework_files/` (not `_framework_files/`).
- The `auth` component exists alongside the other framework components.

## File verification

The bash sandbox can show stale cached content for files that have been modified on the Windows host since the mount was established. Before concluding that a file is truncated, malformed, or missing content, always cross-check using the Read tool, which reads directly from the Windows filesystem. Do not draw conclusions about file content from bash `wc`, `cat`, or `stat` output alone.

## Documentation files

- `README.md` — main project doc; API reference is in `docs/api-reference.md`
- `docs/creating-an-app.md` — guide for consuming the framework as a submodule
- `docs/flash_layout.md` — partition table reference (source of truth is `partitions.csv`)
- `docs/api-reference.md` — full HTTP API route table with response shapes
- `CONTRIBUTING.md` — internal development conventions
