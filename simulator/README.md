# ESPTransit Simulator

Desktop simulator for the ESPTransit departure board application. Runs the same UI code as the ESP32-P4 hardware target using LVGL with SDL2 backend and FreeRTOS POSIX port.

## Purpose

The simulator allows rapid UI development and testing without flashing to hardware. The UI code in `shared/ui/` is shared between the ESP32 target and this simulator.

## Prerequisites

### Linux (Debian/Ubuntu)
```bash
sudo apt install build-essential cmake libsdl2-dev libcurl4-openssl-dev
```

### Linux (Arch)
```bash
sudo pacman -S base-devel cmake sdl2 curl
```

### Linux (Fedora)
```bash
sudo dnf install @development-tools cmake SDL2-devel libcurl-devel
```

### macOS (Homebrew)
```bash
brew install sdl2 cmake make curl
```

### Verify Installation
```bash
sdl2-config --version
cmake --version
gcc --version
curl --version
```

## Building and Running

From the project root directory:

```bash
mise run sim
```

This task will:
1. Configure CMake with FreeRTOS enabled
2. Build the simulator
3. Launch the application

On first configure, CMake fetches third-party dependencies (LVGL, FreeRTOS, argparse).
This requires internet access.

### Manual Build

If you prefer to build manually:

```bash
cd simulator
mkdir -p build && cd build
cmake .. -DUSE_FREERTOS=ON
make -j$(nproc)
./bin/esptransit_sim
```

## Architecture

### FreeRTOS Integration

This simulator uses FreeRTOS with the POSIX port running on top of your native OS. FreeRTOS is **mandatory** for this simulator (not optional) because the application depends on FreeRTOS tasks, queues, and notifications for its architecture.

The FreeRTOS heap is configured to 512 MB in `config/FreeRTOSConfig.h` to accommodate SDL2 window rendering.

### Mock Components

The simulator includes mock implementations in `src/mock/` for hardware-specific functionality:

- `bsp_display.cpp` - Display initialization and locking (wraps SDL2)
- `app_state.cpp` - NVS storage simulation (JSON file persisted at `simulator/.simulator-config.json`)
- `http_client.cpp` - HTTP client (returns mock departure data)

### Shared UI Code

All UI screens in `shared/ui/` are compiled identically for both ESP32 and simulator targets. Changes made in the simulator immediately apply to the hardware build.

## Display Configuration

Board definitions from `boards.json` are embedded into the simulator binary at compile time. Each board's `width` and `height` represent its native 0° orientation — portrait-native boards have `width < height`, landscape-native boards have `width > height`. Select a board with `-b/--board` or the `mise run sim [board]` task argument.

## Differences from Hardware

| Feature | Hardware (ESP32-P4) | Simulator |
|---------|-------------------|-----------|
| Display | MIPI DSI (JD9365) | SDL2 window |
| Touch | GSL3680 I2C | Mouse input |
| WiFi | ESP32-C6 companion | Mock responses |
| Storage | NVS flash | JSON file (`simulator/.simulator-config.json`) |
| HTTP | esp_http_client | Mock data |
| Memory | PSRAM (XIP mode) | Native heap |

## Runtime Options

`mise run sim` accepts an optional board task argument first, then optional simulator CLI args:

- `mise run sim` (default board `jc8012p4a1c`, 800x1280 portrait)
- `mise run sim jc4880p443c` (small board, 480x800 portrait)
- `mise run sim jc1060p470c` (medium board, 1024x600 landscape)

After the optional board argument, remaining args are passed to the simulator binary:

- `-z <zoom>` Set initial window zoom
- `-m` Enable mock HTTP mode
- `-r <degrees>` Override display rotation (`0`, `90`, `180`, `270`)
- `--config-file <path>` Use custom simulator config file (default: `simulator/.simulator-config.json`)
- `--control-http` Enable live test control server on `127.0.0.1`
- `--control-http-port <port>` Control HTTP server port (`1-65535`, required with `--control-http`)

To clear the local simulator config file:

```bash
mise run sim-clear-config
```

## Development Workflow

1. Make changes to UI code in `shared/ui/`
2. Run `mise run sim` to test changes
3. Once satisfied, build for ESP32 with `idf.py build flash monitor`

## Log-Based UI Smoke Tests (Pytest)

The repository includes a starter pytest harness for simulator UI smoke tests:

```bash
mise run py-sync
mise run ui-test
# run with a real SDL window:
mise run ui-test-live
# regenerate committed screenshot baselines:
mise run ui-test-regen
# override parallel worker count if needed:
PYTEST_XDIST_WORKERS=2 mise run ui-test
# cap auto worker fan-out if needed:
PYTEST_XDIST_MAXPROCESSES=2 mise run ui-test
# if simulator binary is already prebuilt (CI/artifacts), run pytest only:
SDL_VIDEODRIVER=dummy mise run ui-test-runner
```

Python test dependencies are declared in `pyproject.toml` (dependency group `test`).
`mise run py-sync` creates/updates the local `.venv` via `uv sync`.
`ui-test*` tasks depend on `sim-build`; `ui-test-runner` intentionally does not build.

Some things that this currently checks:
- Simulator boots with clean config and reaches WiFi setup flow
- Simulator boots with configured WiFi but no stations and reaches station search
- Simulator boots with configured WiFi + station and reaches departures
- Rotation override (`-r 90`) is applied and reflected in startup logs
- Live HTTP-control flow can click `departures.settings_button`, reach Settings, and match a golden screenshot
- Live HTTP-control flow can open the WiFi password popup keyboard for a secured network
- Live HTTP-control flow can open the Settings rotation dropdown
- Live HTTP-control flow can trigger and capture the rotation confirmation popup (change to 90°)
- Each scenario writes an actual PNG screenshot to `.artifacts/ui-smoke-screenshots/` (git-ignored)
- Each actual screenshot is compared against committed baselines in `tests/ui/golden/<resolution>/`
- Pixel diffs for mismatches are written to `.artifacts/ui-smoke-screenshots/diff/<resolution>/`

These tests run the real simulator binary for a short time and assert on captured logs (including expected order for key lifecycle events).
They isolate scenarios by passing per-test config fixtures with `--config-file` (without touching your default simulator config),
then drive screenshots and shutdown through `--control-http`.
The default `mise` tasks run pytest with `pytest-xdist` (`-n auto`).
Each simulator process allocates an ephemeral localhost control port and retries startup with a new port if a bind collision occurs.
When running with xdist, simulator build is synchronized so it runs once per pytest invocation (not once per worker).
Worker fan-out is capped by default via `--maxprocesses=4` for simulator stability.
Set `SIMULATOR_TEST_TIMEOUT` (seconds) to tune per-test wait/teardown limits in pytest.
Set `SIMULATOR_DELAY_SCALE` (test harness default `0.1`) to scale artificial simulator delays in mock WiFi/HTTP flows.
For example, `SIMULATOR_DELAY_SCALE=0.1` keeps async ordering while running much faster.
For headless CI (GitHub Actions), set `SDL_VIDEODRIVER=dummy` (or run `mise run ui-test`).
Set `UPDATE_GOLDEN=1` (or run `mise run ui-test-regen`) when intentional UI changes require baseline updates.

### Golden screenshot real-size viewer

You can open a local webpage to inspect committed screenshots in `tests/ui/golden/` at physical size.
The viewer groups screenshots by name and can display two resolution sets side-by-side (`show both`, `show only X`, `show only Y`):

```bash
mise run ui-golden-view
```

Then open the printed URL (`http://127.0.0.1:8765` by default).

For monitor-agnostic sizing, switch the viewer to **Ruler calibration** mode, measure the blue calibration bar on your current monitor with a physical ruler, and enter the measured width in millimeters.
That gives accurate physical scaling on any monitor without hardcoding laptop specs.

### Live HTTP control for UI tests

For interactive pytest-style UI automation, run the simulator with `--control-http` and keep the process alive.
Use JSON over HTTP:
- Health check: `GET /health` -> `{"ok":true,"service":"sim_control"}`
- Command: `POST /control` with:

```json
{"cmd":"wait_for_id","args":{"id":"departures.settings_button","timeout_ms":7000}}
```

- Success response:

```json
{"ok":true,"result":{"cmd":"wait_for_id","code":"ok","message":"wait_for_id departures.settings_button"}}
```

- Error response:

```json
{"ok":false,"error":{"cmd":"wait_for_id","code":"timeout","message":"wait_for_id timeout: departures.settings_button"}}
```

Supported `cmd` values and args:
- `wait_for_id`: `{"id": "...", "timeout_ms": 5000}` (`timeout_ms` optional)
- `click_id`: `{"id": "..."}`
- `set_dropdown_id`: `{"id": "...", "index": 1}`
- `type_text`: `{"text": "..."}`
- `screenshot`: `{"path": "/tmp/capture.png"}`
- `sleep_ms`: `{"milliseconds": 250}`
- `quit`: no args

Widget IDs are LVGL object names assigned in shared UI code (for example `departures.settings_button` and `settings.back_button`).

## Troubleshooting

**SDL2 window doesn't appear:**
- Increase FreeRTOS heap size in `config/FreeRTOSConfig.h`
- Check that SDL2 is properly installed

**Dependency download errors:**
- Ensure internet access is available on first configure
- If your environment blocks outbound Git, dependency fetch will fail

**Build errors:**
- Ensure all prerequisites are installed
- Try a clean build: `rm -rf simulator/build && mise run sim`
