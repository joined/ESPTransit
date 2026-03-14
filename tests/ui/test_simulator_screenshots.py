from __future__ import annotations

import pytest

from tests.ui.simulator_runner import is_two_view_mode, rotation_for_board


def test_wifi_setup_screen(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.settings_button", 7.0)
        client.click_id("departures.settings_button")
        client.wait_for_id("settings.change_wifi_button", 4.0)
        client.click_id("settings.change_wifi_button")
        client.wait_for_id("wifi_setup.network.homewifi", 9.0)
        client.sleep_ms(300)
        client.compare_and_save_screenshot("wifi_setup_screen")


def test_wifi_connect_failed(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.settings_button", 7.0)
        client.click_id("departures.settings_button")
        client.wait_for_id("settings.change_wifi_button", 4.0)
        client.click_id("settings.change_wifi_button")
        client.wait_for_id("wifi_setup.network.errornetwork", 9.0)
        client.click_id("wifi_setup.network.errornetwork")
        client.wait_for_id("wifi_setup.password_popup.keyboard", 5.0)
        client.type_text("badpass")
        client.click_id("wifi_setup.password_popup.connect_button")
        client.wait_for_log("Connection failed", 10.0)
        client.sleep_ms(300)
        client.compare_and_save_screenshot("wifi_connect_failed")


def test_station_search_screen(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.settings_button", 7.0)
        client.click_id("departures.settings_button")
        client.wait_for_id("settings.manage_stations_button", 4.0)
        client.click_id("settings.manage_stations_button")
        client.wait_for_id("station_search.screen", 7.0)
        client.sleep_ms(300)
        client.compare_and_save_screenshot("station_search_screen")


def test_station_search_search_mode_screen(
    run_sim, board: str, orientation: str
) -> None:
    if not is_two_view_mode(board, orientation):
        pytest.skip("two-view mode not active for this board/orientation")

    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.settings_button", 7.0)
        client.click_id("departures.settings_button")
        client.wait_for_id("settings.manage_stations_button", 4.0)
        client.click_id("settings.manage_stations_button")
        client.wait_for_id("station_search.add_button", 4.0)
        client.click_id("station_search.add_button")
        client.wait_for_id("station_search.cancel_button", 4.0)
        client.type_text("Ber")
        client.wait_for_log("Starting search for: Ber")
        client.wait_for_log("[MOCK] Station search request")
        client.wait_for_log("Station search completed: 10 results")
        client.sleep_ms(300)
        client.compare_and_save_screenshot("station_search_search_mode_screen")


def test_departures_1_station(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.screen", 7.0)
        client.sleep_ms(1000)
        client.compare_and_save_screenshot("departures_1_station")


def test_settings_opened(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.settings_button", 7.0)
        client.click_id("departures.settings_button")
        client.wait_for_id("settings.back_button", 4.0)
        client.compare_and_save_screenshot("settings_opened")


def test_wifi_password_keyboard_opened(run_sim) -> None:
    with run_sim() as client:
        client.wait_for_id("wifi_setup.network.homewifi", 9.0)
        client.click_id("wifi_setup.network.homewifi")
        client.wait_for_id("wifi_setup.password_popup.keyboard", 5.0)
        client.type_text("Test1234")
        client.sleep_ms(250)
        client.compare_and_save_screenshot("wifi_password_keyboard_opened")


def test_rotation_dropdown_opened(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.settings_button", 7.0)
        client.click_id("departures.settings_button")
        client.wait_for_id("settings.rotation_dropdown", 4.0)
        client.scroll_to_view_id("settings.rotation_dropdown")
        client.click_id("settings.rotation_dropdown")
        client.sleep_ms(250)
        client.compare_and_save_screenshot("rotation_dropdown_opened")


def test_debug_screen_opened(run_sim) -> None:
    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.settings_button", 7.0)
        client.click_id("departures.settings_button")
        client.wait_for_id("settings.version_hitbox", 4.0)
        client.scroll_to_view_id("settings.version_hitbox")
        client.click_id("settings.version_hitbox")
        client.click_id("settings.version_hitbox")
        client.click_id("settings.version_hitbox")
        client.wait_for_id("debug.screen", 4.0)
        client.sleep_ms(250)
        client.compare_and_save_screenshot("debug_screen_opened")


def test_rotation_confirm_popup_opened(run_sim, board: str, orientation: str) -> None:
    with run_sim(fixture_name="configured_with_station.json") as client:
        client.wait_for_id("departures.settings_button", 7.0)
        client.click_id("departures.settings_button")
        client.wait_for_id("settings.rotation_dropdown", 4.0)
        client.scroll_to_view_id("settings.rotation_dropdown")
        current_index = rotation_for_board(board, orientation) // 90
        client.set_dropdown_id("settings.rotation_dropdown", 1 - current_index)
        client.wait_for_id("settings.rotation_confirm_popup", 4.0)
        client.sleep_ms(250)
        client.compare_and_save_screenshot("rotation_confirm_popup_opened")


def test_departures_2_stations_split_mode(run_sim) -> None:
    with run_sim(fixture_name="configured_with_2_stations.json") as client:
        client.wait_for_id("departures.screen", 7.0)
        client.sleep_ms(1000)
        client.compare_and_save_screenshot("departures_2_stations_split_mode")


def test_departures_4_stations_split_mode(run_sim) -> None:
    with run_sim(fixture_name="configured_with_4_stations.json") as client:
        client.wait_for_id("departures.screen", 7.0)
        client.sleep_ms(1000)
        client.compare_and_save_screenshot("departures_4_stations_split_mode")


def test_departures_4_stations_no_split_mode(run_sim) -> None:
    with run_sim(fixture_name="configured_with_4_stations_nosplit.json") as client:
        client.wait_for_id("departures.screen", 7.0)
        client.sleep_ms(1000)
        client.compare_and_save_screenshot("departures_4_stations_no_split_mode")
