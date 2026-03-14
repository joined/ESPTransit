# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### ESP32-P4 Hardware

Use `scripts/idf.sh <board> [idf.py args...]` instead of `idf.py` directly — it handles board selection (`-DBOARD=<board>`), per-board build directories (`-B build_<board>`), and IDF environment setup automatically. Mise tasks `esp`, `esp-small`, and `esp-medium` are shortcuts for the three boards.

```bash
# JC8012P4A1C — large board, 800x1280 (default)
scripts/idf.sh jc8012p4a1c build flash monitor
# Same via mise task
mise run esp build flash monitor

# JC4880P443C — small board, 480x800
scripts/idf.sh jc4880p443c build flash monitor
# Same via mise task
mise run esp-small build flash monitor

# JC1060P470C — medium board, 1024x600 native landscape
scripts/idf.sh jc1060p470c build flash monitor
# Same via mise task
mise run esp-medium build flash monitor

# Flash/monitor only (no rebuild)
scripts/idf.sh jc8012p4a1c flash monitor
scripts/idf.sh jc4880p443c flash monitor
scripts/idf.sh jc1060p470c flash monitor

# Set target (already configured for esp32p4)
scripts/idf.sh jc8012p4a1c set-target esp32p4

# Menuconfig for SDK settings
scripts/idf.sh jc8012p4a1c menuconfig
# Same via mise task
mise run esp menuconfig
```

### Desktop Simulator

```bash
# Build and run the simulator (from project root)
mise run sim

# Build only (no run) — use this to verify compilation
mise run sim-build
```

### Python Scripts

The project venv (`.venv`) is **not** auto-activated. Python scripts that depend on project packages must be run via `uv run` (or their corresponding mise task), not `python3` directly:

```bash
uv run scripts/ui_golden_viewer.py    # or: mise run ui-golden-view
uv run scripts/regenerate_fonts.py    # or: mise run fonts-regen
```

This keeps the project venv separate from ESP-IDF's own Python venv.

## Architecture

This is a departure board display application for German rail/transit systems running on ESP32-P4 with a touchscreen. Three board variants are supported with different native resolutions (800x1280, 480x800, 1024x600).

### Code Sharing Design

The codebase is structured to share as much code as possible between the ESP32 hardware target and a desktop simulator. The goal is to make the simulator behave identically to the ESP while minimizing code duplication.

```
esptransit/
├── shared/              # Platform-agnostic code (compiles for both targets)
│   ├── app_manager.cpp  # Core state machine and command handling
│   ├── app_platform.h   # Platform abstraction interface
│   ├── http_client.h    # HTTP interface definition
│   └── ui/
│       ├── common.cpp/h        # Shared UI utilities, colors, fonts
│       └── screens/            # All LVGL screen classes
│           ├── screen_base.h   # Abstract base class for screens
│           ├── departures.*    # Departure board screen
│           ├── settings.*      # Settings screen
│           ├── station_search.*# Station search screen
│           └── wifi_setup.*    # WiFi setup screen
├── esp/                 # ESP32-P4 specific code
│   └── main/
│       ├── main.cpp           # ESP entry point
│       ├── app_platform_esp.cpp  # Platform impl (WiFi, NVS, SNTP)
│       └── http_client.cpp    # HTTP via esp_http_client
└── simulator/           # Desktop simulator
    └── src/
        ├── main.cpp              # SDL2/LVGL setup
        ├── app_platform_sim.cpp  # Platform impl (mock WiFi, JSON storage)
        └── mock/                 # Mock hardware abstractions
```

**Key patterns:**
- `AppManager` (shared) contains all state machine logic, screen transitions, and command handling
- `state_configs_` in `shared/app_manager.cpp` defines per-state screen lifecycle (`init`/`on_enter`/`on_exit`) and state-scoped timers
- Each screen is a class inheriting from `ScreenBase` (in `shared/ui/screens/screen_base.h`), constructed in the state config's `init` callback
- `AppPlatform` (interface in shared, impl per-target) abstracts WiFi, storage, and hardware operations
- All UI code lives in `shared/ui/screens/` and compiles identically for both targets
- The simulator uses FreeRTOS POSIX port so task/queue/notification code works unchanged

### Target Hardware

Three boards are supported, selectable via board Kconfig symbols (`CONFIG_ESPTRANSIT_BOARD_*`), typically through `SDKCONFIG_DEFAULTS` overlays:

| Feature | JC8012P4A1C (default) | JC4880P443C | JC1060P470C |
|---------|----------------------|-------------|-------------|
| Native resolution | 800x1280 (portrait) | 480x800 (portrait) | 1024x600 (landscape) |
| Native orientation | Portrait | Portrait | Landscape |
| LCD controller | JD9365 | ST7701 | JD9165 |
| Touch controller | GSL3680 (custom) | GT911 (standard) | GT911 (standard) |
| Build command | `mise run esp build` | `mise run esp-small build` | `mise run esp-medium build` |

All boards share:
- **MCU**: ESP32-P4 with companion ESP32-C6 for WiFi (via esp_wifi_remote)
- **Memory**: 16MB Flash, PSRAM enabled (XIP mode)

### State Machine
The app uses a simple state machine in `shared/app_manager.cpp`:
- **BOOT** → Storage version check → **WIFI_SETUP** (if no saved credentials) or **WIFI_CONNECTING**
- **WIFI_CONNECTING** → **STATION_SEARCH** (if no saved station) or **DEPARTURES**
- **DEPARTURES** ↔ **SETTINGS**
- **SETTINGS** can transition to **WIFI_SETUP** or **STATION_SEARCH** to change configuration

### Key Components

| File | Purpose |
|------|---------|
| `shared/app_manager.cpp` | State machine, screen transitions, command dispatcher |
| `shared/app_manager.h` | AppManager class, AppGlobalState, StateConfig definitions |
| `shared/app_platform.h` | Platform abstraction interface, UiWifiNetwork struct |
| `shared/http_client.h` | HTTP request/response interface |
| `shared/ui/screens/screen_base.h` | Abstract base class for all screens |
| `shared/ui/screens/*.cpp` | Screen classes (departures, settings, wifi_setup, station_search) |
| `shared/ui/common.cpp/h` | Shared UI utilities, colors, fonts, common widgets |
| `esp/main/main.cpp` | ESP entry point, hardware init |
| `esp/main/app_platform_esp.cpp` | ESP platform impl (WiFi, NVS, SNTP) |
| `esp/main/http_client.cpp` | HTTP via esp_http_client with ArduinoJson |
| `simulator/src/main.cpp` | Simulator entry, SDL2/LVGL setup |
| `simulator/src/app_platform_sim.cpp` | Simulator platform impl |
| `simulator/src/mock/` | Mock implementations (WiFi, storage, HTTP) |

### Custom Board Support
- `esp/components/bsp_jc8012p4a1c/` - JC8012P4A1C board (800x1280, JD9365 + GSL3680)
- `esp/components/bsp_jc4880p443c/` - JC4880P443C board (480x800, ST7701 + GT911)
- `esp/components/bsp_jc1060p470c/` - JC1060P470C board (1024x600 native landscape, JD9165 + GT911)
- `esp/components/esp_lcd_touch_gsl3680/` - Custom GSL3680 touch driver

### Communication Pattern
UI screens communicate with AppManager via `std::function` callbacks passed at construction time. These callbacks use `postCommand()` to queue lambdas for execution on the main task:
```cpp
// Screen constructor receives callbacks that queue commands
auto screen = std::make_unique<DeparturesScreen>(
    [this]() { postCommand([this]() { onDeparturesRefresh(); }); },
    ...
);
```
The main task processes commands from a FreeRTOS queue in `runMainLoop()`. A separate HTTP fetcher task handles network requests via `http_request_queue`.

Each screen is a class inheriting from `ScreenBase` with RAII lifecycle — constructed in `StateConfig::init`, destroyed automatically on state transition.

### Display Operations
Always wrap LVGL calls with the display lock:
```cpp
bsp_display_lock(0);
// LVGL operations here
bsp_display_unlock();
```

### UI Test Determinism
- Use `ui_textarea_create(parent)` from `shared/ui/common.h` instead of calling
  `lv_textarea_create(parent)` directly.
- The wrapper auto-calls `ui_stabilize_textarea_for_tests()` so textarea cursor rendering
  stays deterministic in screenshot tests.

### Memory Considerations
- Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` for large allocations
- HTTP responses buffer to PSRAM (max 128KB)
- ArduinoJson uses PSRAM-backed allocator

### Display Rotation
The app supports 4 rotation modes (0°, 90°, 180°, 270°) configurable from the Settings screen:
- Rotation preference saved to NVS (`AppConfig.rotation` field)
- Applied at boot time in `app_main()` before LVGL initialization
- **Hardware rotation** (0°/180°): Uses `esp_lcd_panel_mirror()` and `esp_lcd_panel_swap_xy()` for better performance
- **Software rotation** (90°/270°): Uses LVGL's `lv_display_set_rotation()` for compatibility
- Switching between HW and SW rotation modes requires a reboot

**Native orientation**: Rotation 0° corresponds to the panel's native orientation. For portrait-native boards (JC8012P4A1C, JC4880P443C), 0° is portrait. For the landscape-native board (JC1060P470C), 0° is landscape. `boards.json` stores native dimensions (`width x height`), and a board is landscape-native when `width > height`. The settings screen labels ("Landscape"/"Portrait") are derived at runtime via `ui_is_native_landscape()` which queries `bsp_display_is_native_landscape()`.

### Storage Version Control
- `STORAGE_VERSION` constant in `app_state.h` controls config schema version
- Incrementing this version forces a full reset of both P4 app config and C6 WiFi credentials
- Checked at boot via `storage_check_version()`
- Useful for testing WiFi setup flow or after breaking config changes

### Font System
Fonts are generated using `scripts/regenerate_fonts.py`:
- Requires `lv_font_conv` (install via `npm install -g lv_font_conv`)
- Generates Fira Sans in multiple sizes (14pt, 16pt, 20pt, 24pt regular, 24pt bold)
- Includes Nerd Font symbols (arrows, circles, checkmarks) and Font Awesome icons
- Character ranges: Basic Latin, extended punctuation, common European characters (ÄÖÜäöüß)
- Generated fonts stored in `shared/ui/fonts/` and compiled into both targets
- Font references available as `FONT_*` macros in `shared/ui/common.h`

```bash
./scripts/regenerate_fonts.py
# or
mise run fonts-regen
```

## Simulator

The desktop simulator enables rapid UI development without flashing to hardware. It uses:
- **LVGL** with SDL2 backend for rendering
- **FreeRTOS POSIX port** so tasks, queues, and notifications work identically to ESP
- **Mock implementations** for WiFi (returns fake networks), storage (JSON file), and optionally HTTP

### Simulator Command-Line Options

```bash
mise run sim                                 # Run with default board (jc8012p4a1c, 800x1280)
mise run sim jc4880p443c                     # Run with JC4880P443C board (480x800)
mise run sim jc1060p470c                     # Run with JC1060P470C board (1024x600 landscape)
mise run sim jc8012p4a1c -m                  # Enable mock HTTP mode (offline development)
mise run sim jc8012p4a1c -z 0.8              # Set 80% zoom level
mise run sim jc8012p4a1c -r 90               # Set display rotation (0, 90, 180, or 270 degrees)
mise run sim jc4880p443c -m                  # Combine flags (mock mode + small board)
mise run sim --help                          # Show task argument help
```

**Board selection**: `mise run sim [board]` maps the task argument to simulator `-b/--board`, which determines display resolution and native orientation. Board definitions are in `boards.json` (native 0° dimensions). Default board is `jc8012p4a1c`.

**Mock HTTP mode (`-m` flag)**: Returns predefined station search and departure data without making real network requests. Useful for offline development or when the API is unavailable.

**Rotation override (`-r` flag)**: Sets the initial display rotation, overriding the saved config. Useful for testing different orientations during development.

### Simulator vs Hardware Differences

| Feature | Hardware (ESP32-P4) | Simulator |
|---------|---------------------|-----------|
| Display | MIPI DSI (JD9365/ST7701/JD9165) | SDL2 window |
| Touch | GSL3680/GT911 I2C | Mouse input |
| WiFi | ESP32-C6 companion | Mock responses |
| Storage | NVS flash | JSON file (`~/.esptransit_config.json`) |
| HTTP | esp_http_client | libcurl (or mock data with `-m` flag) |
| Memory | PSRAM (XIP mode) | Native heap |

### Development Workflow

1. Make changes to shared code (`shared/`)
2. Run `mise run sim` to test changes quickly (add `jc8012p4a1c -m` for offline work)
3. Once satisfied, build for hardware with `mise run esp build flash monitor` (or `mise run esp-small build flash monitor` for JC4880P443C, `mise run esp-medium build flash monitor` for JC1060P470C)

## Code Formatting

The project uses [prek](https://prek.j178.dev/) for git hooks and formatting enforcement, with tools managed via [mise](https://mise.jdx.dev/).

### Setup

```bash
mise install        # Install prek, biome, ruff, shfmt, uv, cmakelang
prek install        # Set up git pre-commit hook
```

`clang-format-20` and `cppcheck` must be installed separately via system package manager (`apt install clang-format-20 cppcheck` on Ubuntu). The formatting hooks invoke the `clang-format-20` binary directly.

### Formatters

| Tool | Scope | Config |
|------|-------|--------|
| biome | Viewer web assets + UI fixture JSON (`scripts/ui_golden_viewer_assets/*.{html,css,js}`, `tests/ui/fixtures/*.json`) | `biome.json` |
| ruff | Python lint/format (`*.py`) | `pyproject.toml` |
| clang-format | C/C++ (`*.c`, `*.h`, `*.cpp`, `*.hpp`) | `.clang-format` |
| shfmt | Shell scripts (`*.sh`) | — |
| cmake-format | CMake files (`CMakeLists.txt`, `*.cmake`) | — |
| trailing-whitespace | All files | — |
| end-of-file-fixer | All files (ensure final newline) | — |

### Commands

```bash
mise run biome-lint  # Lint + auto-format Biome-managed files
mise run biome-lint-check  # Check Biome-managed files without writing changes
prek run --all-files  # Auto-format everything
prek run              # Run on staged files only
```

### Excluded from formatting
- Simulator dependency fetch cache/build outputs under `simulator/build`
- Managed components: `esp/managed_components`
- Vendor board components: `esp/components/bsp_jc8012p4a1c`, `esp/components/bsp_jc4880p443c`, `esp/components/bsp_jc1060p470c`, `esp/components/esp_lcd_touch_gsl3680`
- Generated fonts: `shared/ui/fonts`
- Build directories: `build`

## CI/CD

### Build Workflow (`.github/workflows/build.yml`)

Runs on every push to any branch, and is also called by the release workflow. Jobs:
1. **format** — runs `prek run --all-files` (formatting/lint check)
2. **build-simulator** — builds the desktop simulator, runs cppcheck, uploads binary artifact
3. **ui-tests** — downloads simulator binary, runs headless UI smoke tests (`mise run ui-test-runner`), uploads screenshot artifacts
4. **build-esp** — matrix build across all boards from `boards.json`, runs cppcheck per board, uploads firmware artifacts

When called from the release workflow, accepts `ref` (git tag) and `release` (applies `sdkconfig.release` overrides) inputs.

### Release Workflow (`.github/workflows/release.yml`)

Manually triggered via `workflow_dispatch` with a version input (e.g. `v0.1.0`). Steps:
1. Validates version format and creates a git tag
2. Calls the build workflow with `release: true`
3. Downloads firmware artifacts for all boards and creates a GitHub Release with board-prefixed binaries
4. On failure, automatically cleans up the tag

### Docs Workflow (`.github/workflows/docs.yml`)

Manually triggered via `workflow_dispatch`. Builds and deploys the documentation site to GitHub Pages:
1. Downloads firmware binaries from all GitHub Releases, organizing them by tag and board
2. Generates `docs/firmware/index.json` manifest for the web flasher
3. Builds the site with `uv run --group docs mkdocs build --strict`
4. Deploys to GitHub Pages

### Documentation Site

The docs site is built with [MkDocs Material](https://squidfun.github.io/mkdocs-material/) and hosted at **https://esptrans.it** (custom domain over GitHub Pages). Source files are in `docs/`, configured by `mkdocs.yml`.

Pages:
- **Home** (`docs/index.md`) — project overview
- **Getting Started** (`docs/getting-started.md`) — setup instructions
- **Web Flasher** (`docs/flash.md`) — browser-based ESP flashing using esptool.js (firmware served from `docs/firmware/`)
- **Simulator** (`docs/simulator.md`) — simulator usage guide

To build docs locally:
```bash
uv run --group docs mkdocs serve
```

## Dependencies

### ESP32 (managed components)
- **lvgl/lvgl** v9.4.0 - Graphics library
- **espressif/esp_lvgl_port** v2.7.0 - LVGL porting
- **bblanchon/arduinojson** v7.4.2 - JSON parsing
- **espressif/esp_wifi_remote** v1.3.1 - WiFi via companion chip

### Simulator (CMake FetchContent)
- **lvgl/lvgl** - Same version as ESP
- **FreeRTOS-Kernel** - POSIX port for desktop
- **p-ranav/argparse** - CLI argument parsing for simulator runtime options
- **SDL2** - System dependency for display/input
