# Getting Started

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/) v5.5+
- [mise](https://mise.jdx.dev/) for tool management
- One of the supported boards (see [Home](index.md#supported-boards))

## Quick Flash (No Build Required)

If you just want to flash pre-built firmware, use the [Web Flasher](flash.md) — no tools or build environment needed, just a Chromium browser and a USB cable.

## Build and Flash

The project uses `scripts/idf.sh` to handle board selection and build directories. Mise tasks provide convenient shortcuts:

```bash
# JC8012P4A1C (large, 800x1280) — default board
mise run esp build flash monitor

# JC4880P443C (small, 480x800)
mise run esp-small build flash monitor

# JC1060P470C (medium, 1024x600)
mise run esp-medium build flash monitor
```

Or use the script directly:

```bash
scripts/idf.sh jc8012p4a1c build flash monitor
```

## First Boot

On first boot, the device will:

1. Show the **WiFi Setup** screen — select your network and enter the password
2. After connecting, show the **Station Search** screen — search for your departure station
3. Display the **Departures** board with real-time data

Configuration is saved to flash and persists across reboots.

## Development Workflow

1. Make changes to shared code in `shared/`
2. Test quickly with the [desktop simulator](simulator.md)
3. Build and flash to hardware when ready
