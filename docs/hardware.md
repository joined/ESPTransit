# Supported Hardware

All supported boards use the **ESP32-P4** MCU with an **ESP32-C6** companion chip for WiFi, 16 MB flash, and PSRAM. They are manufactured by [Guition](https://www.guition.com/){target=_blank} and available on AliExpress.

## Board Comparison

| | JC8012P4A1C | JC4880P443C | JC1060P470C |
|---|---|---|---|
| **Screen size** | 10.1" | 4.3" | 7.0" |
| **Resolution** | 800 x 1280 | 480 x 800 | 1024 x 600 |
| **Native orientation** | Portrait | Portrait | Landscape |
| **LCD controller** | JD9365 | ST7701 | JD9165 |
| **Touch controller** | GSL3680 | GT911 | GT911 |
| **Buy** | [AliExpress](https://aliexpress.com/item/1005008789890066.html){target=_blank} | [AliExpress](https://aliexpress.com/item/1005009834924470.html){target=_blank} | [AliExpress](https://aliexpress.com/item/1005010023056143.html){target=_blank} |

## Which Board Should I Get?

- **JC8012P4A1C (10.1")** — Best for wall-mounted displays. The large portrait screen fits many departures at once and is readable from across a room. The USB-C port is on the side, making it easy to cable-manage when wall-mounted.
- **JC1060P470C (7")** — Good middle ground. Landscape orientation works well on a desk or shelf. The USB-C port is on the back, which can make wall mounting trickier.
- **JC4880P443C (4.3")** — Most compact option. Good for a desk or nightstand where space is tight. The USB-C port is on the back. Note that this board seems to have noticeably weaker WiFi reception compared to the other two.

All three boards support [display rotation](#display-rotation) (0/90/180/270), so you can mount them in any orientation regardless of the native one.

## What's in the Box

These boards typically ship with:

- The ESP32-P4 development board with integrated touchscreen
- A USB-C cable for power and flashing

No soldering or additional components are required — just plug in the USB cable, [flash the firmware](flash.md), and you're ready to go.

## Display Rotation

Rotation is configurable from the Settings screen. The setting is saved and applied automatically on boot.

- **0/180 degrees** use hardware rotation for best performance
- **90/270 degrees** use software rotation via LVGL

Switching between hardware and software rotation modes requires a reboot, which the device will prompt you for.

## Screenshots by Board

Screenshots are from the desktop simulator and automatically kept up to date via [golden screenshot tests](simulator.md#ui-tests).

### JC8012P4A1C (10.1" portrait)

<div style="display: flex; gap: 1rem; flex-wrap: wrap; align-items: flex-start;" markdown>

<figure markdown>
  ![Departures — single station](screenshots/jc8012p4a1c/departures_1_station_portrait.png){ width="200" }
  <figcaption>Single station</figcaption>
</figure>

<figure markdown>
  ![Departures — 4 stations split](screenshots/jc8012p4a1c/departures_4_stations_split_mode_portrait.png){ width="200" }
  <figcaption>4 stations (split mode)</figcaption>
</figure>

<figure markdown>
  ![Settings](screenshots/jc8012p4a1c/settings_opened_portrait.png){ width="200" }
  <figcaption>Settings</figcaption>
</figure>

</div>

### JC1060P470C (7" landscape)

<div style="display: flex; gap: 1rem; flex-wrap: wrap; align-items: flex-start;" markdown>

<figure markdown>
  ![Departures — single station](screenshots/jc1060p470c/departures_1_station_landscape.png){ width="280" }
  <figcaption>Single station</figcaption>
</figure>

<figure markdown>
  ![Departures — 4 stations split](screenshots/jc1060p470c/departures_4_stations_split_mode_landscape.png){ width="280" }
  <figcaption>4 stations (split mode)</figcaption>
</figure>

</div>

### JC4880P443C (4.3" portrait)

<div style="display: flex; gap: 1rem; flex-wrap: wrap; align-items: flex-start;" markdown>

<figure markdown>
  ![Departures — single station](screenshots/jc4880p443c/departures_1_station_portrait.png){ width="150" }
  <figcaption>Single station</figcaption>
</figure>

<figure markdown>
  ![Departures — 2 stations split](screenshots/jc4880p443c/departures_2_stations_split_mode_portrait.png){ width="150" }
  <figcaption>2 stations (split mode)</figcaption>
</figure>

</div>
