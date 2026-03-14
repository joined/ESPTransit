# Desktop Simulator

The simulator lets you develop and test the UI without flashing to hardware. It uses SDL2 for rendering and a FreeRTOS POSIX port so that tasks, queues, and notifications work identically to the ESP target.

## Prerequisites

- CMake, a C++ compiler, SDL2, and libcurl development libraries
- [mise](https://mise.jdx.dev/) for tool management

On Ubuntu/Debian:

```bash
sudo apt-get install build-essential cmake libsdl2-dev libcurl4-openssl-dev
```

## Running

```bash
mise run sim                          # Default board (jc8012p4a1c, 800x1280)
mise run sim jc4880p443c              # Small board (480x800)
mise run sim jc1060p470c              # Medium board (1024x600, landscape)
```

## Options

| Flag | Description |
|------|-------------|
| `-m` | Mock HTTP mode — returns predefined data without network requests |
| `-z <factor>` | Zoom level (e.g. `-z 0.8` for 80%) |
| `-r <degrees>` | Display rotation: 0, 90, 180, or 270 |

Flags are passed after the board name:

```bash
mise run sim jc8012p4a1c -m           # Offline mode
mise run sim jc4880p443c -z 0.8       # 80% zoom, small board
mise run sim jc8012p4a1c -r 90        # Rotated 90 degrees
```

## UI Tests

The simulator doubles as a test harness. Tests are orchestrated by **pytest** and communicate with the simulator through an **HTTP control server** built into the simulator binary (`--control-http` flag). The control server exposes JSON endpoints for waiting on UI elements by test ID, clicking, typing text, taking screenshots, and more.

Tests run the simulator headlessly using SDL's dummy video driver, with mock HTTP enabled, across all three board variants and both orientations. pytest-xdist parallelises the matrix automatically.

There are two kinds of tests:

- **Flow tests** (`test_simulator_flows.py`) — verify state machine transitions and UI interactions by asserting on simulator log output (e.g. boot → WiFi setup → station search → departures).
- **Screenshot tests** (`test_simulator_screenshots.py`) — drive the UI to specific states and compare pixel-perfect screenshots against golden baselines stored in `tests/ui/golden/`.

```bash
mise run ui-test              # Run all tests (headless)
mise run ui-test-live         # Run with visible SDL window
mise run ui-test-regen        # Regenerate golden baselines (headless)
mise run ui-test-regen-live   # Regenerate baselines with visible window
```

### Golden Screenshot Viewer

To inspect the golden baselines at their physical display size, use the built-in viewer:

```bash
mise run ui-golden-view
```

This opens a local web page that renders each screenshot at the correct DPI-scaled size, making it easy to review how the UI will look on the actual hardware.

## Differences from Hardware

| Feature | Hardware | Simulator |
|---------|----------|-----------|
| Display | MIPI DSI panel | SDL2 window |
| Touch | I2C touch controller | Mouse input |
| WiFi | ESP32-C6 companion | Mock responses |
| Storage | NVS flash | JSON file (`~/.esptransit_config.json`) |
| HTTP | esp_http_client | libcurl (or mock with `-m`) |
