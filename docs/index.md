# ESPTransit

Open-source firmware that turns ESP32-P4 touchscreen boards into real-time departure displays for German rail and transit systems.

<video autoplay loop muted playsinline>
  <source src="assets/demo.webm" type="video/webm">
</video>

## Features

- Real-time departure data from BVG (Berlin public transit) via [ESPTransit-Server](https://github.com/joined/ESPTransit-Server){target=_blank}
- Station search with autocomplete
- WiFi setup via on-screen keyboard
- Display rotation support (0/90/180/270)
- Desktop simulator for rapid development

## Supported Boards

Three ESP32-P4 touchscreen boards are supported — see [Supported Hardware](hardware.md) for details, buying links, and a comparison.

| Board | Size | Resolution | Orientation |
|-------|------|-----------|-------------|
| JC8012P4A1C | 10.1" | 800x1280 | Portrait |
| JC4880P443C | 4.3" | 480x800 | Portrait |
| JC1060P470C | 7" | 1024x600 | Landscape |

## Screenshots

Screenshots are from the desktop simulator and are automatically kept up to date via [golden screenshot tests](simulator.md#ui-tests).

### Departures (single station)

<div class="grid" style="display: flex; gap: 1rem; flex-wrap: wrap; align-items: flex-end;" markdown>

<figure markdown>
  ![Single station — 10" portrait](screenshots/jc8012p4a1c/departures_1_station_portrait.png){ width="200" }
  <figcaption>10" portrait</figcaption>
</figure>

<figure markdown>
  ![Single station — 7" landscape](screenshots/jc1060p470c/departures_1_station_landscape.png){ width="280" }
  <figcaption>7" landscape</figcaption>
</figure>

<figure markdown>
  ![Single station — 4.3" portrait](screenshots/jc4880p443c/departures_1_station_portrait.png){ width="150" }
  <figcaption>4.3" portrait</figcaption>
</figure>

</div>

### Departures (multi-station split mode)

<div class="grid" style="display: flex; gap: 1rem; flex-wrap: wrap; align-items: flex-end;" markdown>

<figure markdown>
  ![4 stations split mode — 10" portrait](screenshots/jc8012p4a1c/departures_4_stations_split_mode_portrait.png){ width="200" }
  <figcaption>10" portrait</figcaption>
</figure>

<figure markdown>
  ![4 stations split mode — 7" landscape](screenshots/jc1060p470c/departures_4_stations_split_mode_landscape.png){ width="280" }
  <figcaption>7" landscape</figcaption>
</figure>

</div>

### WiFi Setup & Station Search

<div class="grid" style="display: flex; gap: 1rem; flex-wrap: wrap; align-items: flex-end;" markdown>

<figure markdown>
  ![WiFi network selection](screenshots/jc8012p4a1c/wifi_setup_screen_portrait.png){ width="200" }
  <figcaption>WiFi setup</figcaption>
</figure>

<figure markdown>
  ![Station search with keyboard](screenshots/jc8012p4a1c/station_search_screen_portrait.png){ width="200" }
  <figcaption>Station search</figcaption>
</figure>

</div>
