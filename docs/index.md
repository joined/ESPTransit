# ESPTransit

A departure board display for German rail and transit systems, running on ESP32-P4 with a touchscreen.

![Departures board showing 4 stations in split mode](screenshots/jc1060p470c/departures_4_stations_split_mode_landscape.png)

## Features

- Real-time departure data from BVG (Berlin public transit) via [ESPTransit-Server](https://github.com/joined/ESPTransit-Server)
- Station search with autocomplete
- WiFi setup via on-screen keyboard
- Display rotation support (0/90/180/270)
- Desktop simulator for rapid development

## Supported Boards

| Board | Size | Resolution | Orientation | Buy |
|-------|------|-----------|-------------|-----|
| JC8012P4A1C | 10" | 800x1280 | Portrait | [AliExpress](https://aliexpress.com/item/1005008789890066.html) |
| JC4880P443C | 4.3" | 480x800 | Portrait | [AliExpress](https://aliexpress.com/item/1005009834924470.html) |
| JC1060P470C | 7" | 1024x600 | Landscape | [AliExpress](https://aliexpress.com/item/1005010023056143.html) |

All boards use an ESP32-P4 MCU with an ESP32-C6 companion chip for WiFi.

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
