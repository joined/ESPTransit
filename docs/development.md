# Development

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/){target=_blank} v5.5+
- [mise](https://mise.jdx.dev/){target=_blank} for tool management
- One of the [supported boards](hardware.md)

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

## Development Workflow

1. Make changes to shared code in `shared/`
2. Test quickly with the [desktop simulator](simulator.md)
3. Build and flash to hardware when ready
