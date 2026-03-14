# TODO List

## Features & Improvements

- [x] Implement multiple stations support
   - Split screen mode
- [x] Implement rotation of the screen (from settings page)
  - Need to remove hardcoded height/width values or derived constants
  - Use HW rotation for 0+180 and sw for 90deg/270deg
- [x] Version number (commit) in settings page
- [x] User starts setup and configures wifi but not station -> on next boot you have to select wifi again -> probably fixed already
- [x] Station limit reached title is cut
- [x] Smart update of departures like in suntransit storing dictionary as reference
- [x] Search station list moves when clicking due to border appearing
- [x] On boot, turn on backlight only after screen is configured
- [x] Narrower product badges, scrolling in worst case
- [x] In landscape mode we should switch to numbers for tabswitch later, there is more space, calculate that
- [x] Improve split mode 4 stations landscape
- [x] Improve manage station layout for landscape (two columns?)
- [x] Horizontal split view: headers should have same height
- [x] Do not show wrong rotation on boot
- [x] Debug dashboard with task list in settings page
- [x] Brightness setting
- [x] Add boot screen
- [x] Add `update_every` to `StateConfig` which you can set so that the app manager will call update every so often on that screen
- [ ] Adapt mock data for multiple stations -> it already works but only for select stops.
- [ ] Add "hide departures sooner than X min" setting
- [ ] Add hide cancelled departures setting
- [ ] Animations using lottie
- [ ] Choose product types (subway / tram / train / ...) to show for each station you configured
- [ ] Clear departure cache smarter: only clear cache of removed stations
- [ ] Click on trip to visualize details
- [ ] Condensed bold font for product badge?
- [ ] Custom button map for keyboard, remove ok button or let it do something
- [ ] For initial setup flow just show a variant of the settings page with hints
- [ ] LV_USE_PPA
- [ ] Network connection indicator for departures page and no spinner but error message if we are not connected
- [ ] Scroll reset for departures list after x time of inactivity
- [ ] Special split mode for one single station: split mode by product type
- [ ] Sync time should happen on first dep screen enter i guess, currently happens on every wifi connect I think?
- [ ] Translations (lv_i18n?)
- [ ] UI style overhaul
- [ ] When exiting manage stations page (back button), we should ask if user wants to save unsaved changes, if any

## Code / Dev QoL improvements

- [x] Use C++ bindings? https://github.com/aptumfr/lv -> Won't do
- [x] Test framework using python
- [x] Remove cppcheck from prek, it's too messy to run locally due to different builds etc
- [x] Prek
- [x] Move s_last_departures to appmanager or somewhere else, not the screen
- [x] Is there a point in names for timers? I dont think so
- [x] Improve component inclusion - each board should include only its own BSP component -> ESP-IDF limitations
- [x] Get rid of uinetworkblabla type?
- [x] Do not paint the screen in the screen constructor, create a `.create` function specifically for that in the abstract base class which we can call automatically in transitionTo -> WONTDO
- [x] Check dependencies.lock is up to date in CI
- [x] Add boot test: store a "fixture" config and run the simulator until boot, check logs, so that we know config parsing works
- [ ] Review FreeRTOS debug configs (`GENERATE_RUN_TIME_STATS`, `VTASKLIST_INCLUDE_COREID`) — are they needed? Can the debug screen expose more data (e.g. per-task CPU%) when they're enabled?
- [ ] Merge app state with app platform? Not sure
- [ ] Migrate to esp_lv_adapter? What about SW/HW rotation?
- [ ] Recursive mutex
- [ ] Review structure. e.g. dedupe app_state. At which level should we mock?
- [ ] Should `.simulator-config.json` have 3 variants, one for board?
- [ ] Use observer pattern? https://docs.lvgl.io/9.4/details/auxiliary-modules/observer/observer.html#overview
- [ ] Write storage compatibility check workflow. For PRs load new build with storage sample from old one (need to store fixture)

## Bug Fixes

- [x] Wifi network credentials are stored when wifi is changed with wrong passworg, should store them only if connection succeeds? Or better said: reset if connection fails?
- [x] Support open wifi networks
- [x] Station search in portrait mode looks weird, list does not fill height
- [x] Core dump when changing wifi network and setting the same one - should we disallow it?
- [x] Connect to *checkmark* for current wifi network is shown
- [x] Allow to go back from station selection UI if one is already configured
- [x] Allow changing password for current network (= we should not disable current selected network)
- [x] Allow 8 characters wifi password
- [x] Settings gear button is unresponsive at times (before first deps are shown)
- [x] Keyboard is too narrow in portrait mode e.g. wifi password input
- [x] Fix station search typing unresponsiveness
- [x] Boot with saved wifi credentials that fail to connect -> we should show an error for that
- [x] Add some padding to the right of station search list - currently the product icons touch the right border & scrollbar
- [x] Do not show cached data after station change when going back to departures
- [x] Station search list, have keyboard separate from list currently keyboard overlaps it which means probably we render more result stations than are visible
- [x] Show overlay behind wifi password popup so that clicks dont get through?
- [x] Scroll when opening settings page in landscape mode -> only for simulator? still broken after changing rotation?
- [ ] Two networks with same name, one of which you're connected to -> double selected network
- [ ] Show message in place of departures list when fetching fails for a while e.g. 1 min
