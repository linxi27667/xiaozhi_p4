# Project Instructions

## Firmware Build Artifacts

- Use only the repository-root `build/` directory for ESP-IDF builds.
- Do not create parallel build directories such as `build_face_demo/` or `build_ninja/`.
- Keep exactly one application firmware image: `build/xiaozhi.bin`.
- Keep the bootloader, partition table, and generated assets required for a complete flash.
- Before handing off a firmware image, report its absolute path, build time, size, and SHA256.
- The user performs device flashing. Do not flash or occupy the serial port unless explicitly requested.
