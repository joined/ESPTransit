set(SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../shared")

file(
  GLOB_RECURSE
  SRCS
  "*.cpp"
  "${SHARED_DIR}/ui/*.cpp"
  "${SHARED_DIR}/ui/components/*.cpp"
  "${SHARED_DIR}/ui/screens/*.cpp"
  "${SHARED_DIR}/ui/fonts/*.c"
  "${SHARED_DIR}/app_manager.cpp"
  "${SHARED_DIR}/station_config_utils.cpp"
  "${SHARED_DIR}/station_products.cpp"
  "${SHARED_DIR}/http_fetcher.cpp"
  "${SHARED_DIR}/http_parse.cpp"
  "${SHARED_DIR}/http_util.cpp")

idf_component_register(
  SRCS
  ${SRCS}
  INCLUDE_DIRS
  .
  ${SHARED_DIR}
  ${SHARED_DIR}/ui/fonts
  PRIV_REQUIRES
  esp_app_format
  nvs_flash
  esp_wifi
  esp_http_client
  esp_netif
  esp_event
  lwip
  mbedtls)

if(BOARD STREQUAL "jc4880p443c")
  idf_component_optional_requires(PRIVATE bsp_jc4880p443c)
elseif(BOARD STREQUAL "jc1060p470c")
  idf_component_optional_requires(PRIVATE bsp_jc1060p470c)
else()
  idf_component_optional_requires(PRIVATE bsp_jc8012p4a1c)
endif()

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(${LVGL_LIB} PRIVATE -DLV_LVGL_H_INCLUDE_SIMPLE)
target_compile_definitions(${LVGL_LIB}
                           PRIVATE LV_SYSMON_GET_IDLE=lv_timer_get_idle)
