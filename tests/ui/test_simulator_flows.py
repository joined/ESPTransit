from __future__ import annotations

import pytest

from tests.ui.simulator_runner import is_two_view_mode, rotation_for_board


def test_flow_boot_without_config_reaches_wifi_setup(run_sim) -> None:
    with run_sim(fixture_name=None) as session:
        session.wait_for_log("=== ESPTransit Simulator Starting ===")
        session.wait_for_log("No config found, starting WiFi setup")
        session.wait_for_log("WiFi manager init (auto_connect=0)")
        session.wait_for_log("Transitioning from BOOT to WIFI_SETUP")
        session.wait_for_log("Handling WiFi scan request")
        session.wait_for_log("Found 5 networks")


def test_flow_boot_with_configured_wifi_but_no_stations_reaches_station_search(
    run_sim,
) -> None:
    with run_sim(fixture_name="configured_no_stations.json") as session:
        session.wait_for_log("Config found, auto-connecting...")
        session.wait_for_log("WiFi manager init (auto_connect=1)")
        session.wait_for_log("Auto-connect enabled, will connect to saved network")
        session.wait_for_log("WiFi connected to 'HomeWiFi'")
        session.wait_for_log("SNTP sync (simulated)")
        session.wait_for_log("Transitioning from BOOT to STATION_SEARCH")


def test_flow_boot_with_station_config_reaches_departures(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as session:
        session.wait_for_log("Config found, auto-connecting...")
        session.wait_for_log("WiFi connected to 'HomeWiFi'")
        session.wait_for_log("Transitioning from BOOT to DEPARTURES")
        session.wait_for_log("Refreshing departures")
        session.wait_for_log("[MOCK] Departures request")
        session.wait_for_log("Departures updated successfully")


def test_flow_wifi_open_network_connects_to_station_search(run_sim) -> None:
    with run_sim(fixture_name=None) as session:
        session.wait_for_id("wifi_setup.network.publicwifi")
        session.click_id("wifi_setup.network.publicwifi")
        session.wait_for_log("Connecting to WiFi: PublicWiFi")
        session.wait_for_log("Connected to 'PublicWiFi'")
        session.wait_for_log("Transitioning from WIFI_SETUP to STATION_SEARCH")


def test_flow_wifi_secured_network_failure_keeps_user_on_wifi_setup(run_sim) -> None:
    with run_sim(fixture_name=None) as session:
        session.wait_for_id("wifi_setup.network.errornetwork")
        session.click_id("wifi_setup.network.errornetwork")
        session.wait_for_id("wifi_setup.password_popup.keyboard", 5.0)
        session.type_text("badpass123")
        session.click_id("wifi_setup.password_popup.connect_button")
        session.wait_for_log("Connecting to WiFi: ErrorNetwork")
        session.wait_for_log(
            "Connection failed: authentication timeout (simulated failure)"
        )
        session.assert_no_logs(
            [
                "Transitioning from WIFI_SETUP to STATION_SEARCH",
                "Transitioning from WIFI_SETUP to DEPARTURES",
            ]
        )
        session.wait_for_id("wifi_setup.network_list")


def test_flow_change_wifi_and_back_from_settings(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as session:
        session.wait_for_id("departures.settings_button")
        session.click_id("departures.settings_button")
        session.wait_for_log("Transitioning from DEPARTURES to SETTINGS")

        session.wait_for_id("settings.change_wifi_button", 4.0)
        session.click_id("settings.change_wifi_button")
        session.wait_for_log("Transitioning from SETTINGS to WIFI_SETUP")

        session.wait_for_id("wifi_setup.back_button")
        session.click_id("wifi_setup.back_button")
        session.wait_for_log("WiFi back pressed")
        session.wait_for_log("Transitioning from WIFI_SETUP to SETTINGS")


def _enter_search_mode_if_two_view(session, board: str, orientation: str) -> None:
    """In two-view mode, click the Add button to reveal the search input."""
    if is_two_view_mode(board, orientation):
        session.wait_for_id("station_search.add_button", 4.0)
        session.click_id("station_search.add_button")
        session.wait_for_id("station_search.cancel_button", 4.0)


def test_flow_manage_stations_search_from_settings(
    run_sim, board: str, orientation: str
) -> None:
    with run_sim(fixture_name="configured_with_station.json") as session:
        session.wait_for_id("departures.settings_button")
        session.click_id("departures.settings_button")
        session.wait_for_log("Transitioning from DEPARTURES to SETTINGS")

        session.wait_for_id("settings.manage_stations_button", 4.0)
        session.click_id("settings.manage_stations_button")
        session.wait_for_log("Transitioning from SETTINGS to STATION_SEARCH")

        _enter_search_mode_if_two_view(session, board, orientation)
        session.wait_for_id("station_search.search_input")
        session.type_text("Ber")
        session.wait_for_log("Starting search for: Ber")
        session.wait_for_log("[MOCK] Station search request")
        session.wait_for_log("Station search completed: 10 results")


def test_flow_station_search_select_result_exits_search_mode_without_crash(
    run_sim, board: str, orientation: str
) -> None:
    if not is_two_view_mode(board, orientation):
        pytest.skip("two-view mode not active for this board/orientation")

    with run_sim(fixture_name="configured_no_stations.json") as session:
        session.wait_for_id("station_search.add_button", 7.0)
        session.click_id("station_search.add_button")
        session.wait_for_id("station_search.cancel_button", 4.0)

        session.type_text("Ber")
        session.wait_for_log("Starting search for: Ber")
        session.wait_for_log("[MOCK] Station search request")
        session.wait_for_log("Station search completed: 10 results")

        session.wait_for_id("station_search.result_item.900000003201", 4.0)
        session.click_id("station_search.result_item.900000003201")

        session.wait_for_log("Selected station: Berlin Hauptbahnhof (id=900000003201)")
        session.wait_for_id("station_search.configured_item.900000003201", 4.0)
        session.wait_for_id("station_search.add_button", 4.0)
        session.sleep_ms(200)
        session.assert_no_log("Guru Meditation Error", since_last_match=False)


def test_flow_rotation_change_confirm_yes_applies(
    run_sim, board: str, orientation: str
) -> None:
    current_idx = rotation_for_board(board, orientation) // 90
    new_idx = (current_idx + 1) % 4

    with run_sim(fixture_name="configured_with_station.json") as session:
        session.wait_for_id("departures.settings_button")
        session.click_id("departures.settings_button")
        session.wait_for_id("settings.rotation_dropdown", 4.0)
        session.set_dropdown_id("settings.rotation_dropdown", new_idx)
        session.wait_for_log(
            f"Rotation change requested from {current_idx} to {new_idx}"
        )

        session.wait_for_id("settings.rotation_confirm_popup.yes_button", 4.0)
        session.click_id("settings.rotation_confirm_popup.yes_button")
        session.wait_for_log(f"Rotation confirmed: {new_idx}")
        session.wait_for_log(f"Rotation changed to {new_idx}")
        session.wait_for_log(f"Display rotation changed: {new_idx * 90}")


def test_flow_rotation_change_confirm_no_cancels(
    run_sim, board: str, orientation: str
) -> None:
    current_idx = rotation_for_board(board, orientation) // 90
    new_idx = (current_idx + 1) % 4

    with run_sim(fixture_name="configured_with_station.json") as session:
        session.wait_for_id("departures.settings_button")
        session.click_id("departures.settings_button")
        session.wait_for_id("settings.rotation_dropdown", 4.0)
        session.set_dropdown_id("settings.rotation_dropdown", new_idx)
        session.wait_for_log(
            f"Rotation change requested from {current_idx} to {new_idx}"
        )

        session.wait_for_id("settings.rotation_confirm_popup.no_button", 4.0)
        session.click_id("settings.rotation_confirm_popup.no_button")
        session.wait_for_log("Rotation cancelled")
        session.assert_no_log(f"Rotation changed to {new_idx}")
        session.wait_for_id("settings.rotation_dropdown")


def test_flow_debug_screen_can_be_opened_from_settings(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as session:
        session.wait_for_id("departures.settings_button")
        session.click_id("departures.settings_button")
        session.wait_for_log("Transitioning from DEPARTURES to SETTINGS")

        session.wait_for_id("settings.version_hitbox", 4.0)
        session.scroll_to_view_id("settings.version_hitbox")
        session.click_id("settings.version_hitbox")
        session.click_id("settings.version_hitbox")
        session.click_id("settings.version_hitbox")
        session.wait_for_log("Transitioning from SETTINGS to DEBUG")
