cmake_minimum_required(VERSION 3.10)
project(esptransit_simulator)

# === ArduinoJson (shared parsing with ESP) ===
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(
  arduinojson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(arduinojson)

# === argparse (CLI parser for simulator main) ===
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.2
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(argparse)
set(ARGPARSE_INCLUDE_DIR "${argparse_SOURCE_DIR}/include")

# === cpp-httplib (HTTP control server for simulator automation) ===
FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.32.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(cpp_httplib)

# === FreeRTOS Configuration ===
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/config)
target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

set(FREERTOS_PORT
    "GCC_POSIX"
    CACHE STRING "FreeRTOS port")
set(FREERTOS_HEAP
    "4"
    CACHE STRING "FreeRTOS heap implementation")

FetchContent_Declare(
  freertos_kernel_src
  GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
  GIT_TAG V11.2.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(freertos_kernel_src)
set(FREERTOS_SOURCE_DIR "${freertos_kernel_src_SOURCE_DIR}")

FetchContent_Declare(
  lvgl_src
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v9.4.0
  GIT_SHALLOW TRUE
  # Populate source now, but defer adding lvgl's CMakeLists until after
  # lv_conf.h is generated.
  SOURCE_SUBDIR __skip_add_subdirectory__)
FetchContent_MakeAvailable(lvgl_src)
set(LVGL_SOURCE_DIR "${lvgl_src_SOURCE_DIR}")

# === Generate lv_conf.h from lv_conf.defaults ===
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(LV_CONF_DEFAULTS ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.defaults)
set(LV_CONF_TEMPLATE ${LVGL_SOURCE_DIR}/lv_conf_template.h)
set(LV_CONF_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h)
set(LV_CONF_GENERATOR ${LVGL_SOURCE_DIR}/scripts/generate_lv_conf.py)

# Regenerate lv_conf.h if defaults or template changed
if(EXISTS ${LV_CONF_DEFAULTS} AND EXISTS ${LV_CONF_TEMPLATE})
  # Check if regeneration is needed (IS_NEWER_THAN works on file paths, not
  # timestamps)
  set(NEEDS_REGEN FALSE)
  if(NOT EXISTS ${LV_CONF_GENERATED})
    set(NEEDS_REGEN TRUE)
  elseif(${LV_CONF_DEFAULTS} IS_NEWER_THAN ${LV_CONF_GENERATED})
    set(NEEDS_REGEN TRUE)
  elseif(${LV_CONF_TEMPLATE} IS_NEWER_THAN ${LV_CONF_GENERATED})
    set(NEEDS_REGEN TRUE)
  endif()

  if(NEEDS_REGEN)
    message(STATUS "Generating lv_conf.h from lv_conf.defaults...")
    execute_process(
      COMMAND
        ${Python3_EXECUTABLE} ${LV_CONF_GENERATOR} --template
        ${LV_CONF_TEMPLATE} --defaults ${LV_CONF_DEFAULTS} --config
        ${LV_CONF_GENERATED}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      RESULT_VARIABLE GEN_RESULT)
    if(NOT GEN_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to generate lv_conf.h")
    endif()
  endif()
endif()

# === LVGL ===
set(LV_CONF_PATH
    ${LV_CONF_GENERATED}
    CACHE STRING "LVGL config file path" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES
    OFF
    CACHE BOOL "Disable LVGL examples")
set(CONFIG_LV_BUILD_DEMOS
    OFF
    CACHE BOOL "Disable LVGL demos")
set(CONFIG_LV_USE_THORVG_INTERNAL
    OFF
    CACHE BOOL "Disable ThorVG")
add_subdirectory("${LVGL_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/lvgl")

# Add FreeRTOS includes to LVGL
target_include_directories(
  lvgl
  PUBLIC ${FREERTOS_SOURCE_DIR}/include
         ${FREERTOS_SOURCE_DIR}/portable/ThirdParty/GCC/Posix
         ${CMAKE_CURRENT_SOURCE_DIR}/config)

# === SDL2 ===
find_package(SDL2 REQUIRED)
find_package(CURL REQUIRED)

# === Source Files ===
set(SIMULATOR_SOURCES
    src/freertos/freertos_posix_port.c
    src/freertos_hooks.c
    src/mouse_cursor_icon.c
    src/main.cpp
    src/sim_cli.cpp
    src/sim_control.cpp
    src/app_platform_sim.cpp
    src/sim_timing.cpp
    src/sim_log.cpp
    src/mock/bsp/display.cpp
    src/mock/app_state.cpp
    src/mock/http_client.cpp
    src/mock/wifi_manager.cpp)

# Shared directory
set(SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared")

# Shared core sources
set(SHARED_CORE_SOURCES
    ${SHARED_DIR}/app_manager.cpp ${SHARED_DIR}/http_fetcher.cpp
    ${SHARED_DIR}/http_parse.cpp ${SHARED_DIR}/http_util.cpp
    ${SHARED_DIR}/station_config_utils.cpp ${SHARED_DIR}/station_products.cpp)

# UI sources (from shared/ui)
file(GLOB UI_SOURCES ${SHARED_DIR}/ui/*.cpp ${SHARED_DIR}/ui/components/*.cpp
     ${SHARED_DIR}/ui/screens/*.cpp ${SHARED_DIR}/ui/fonts/*.c)

# === Executable ===
add_executable(esptransit_sim ${SIMULATOR_SOURCES} ${SHARED_CORE_SOURCES}
                              ${UI_SOURCES})

target_include_directories(
  esptransit_sim
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/src/mock
          ${CMAKE_CURRENT_SOURCE_DIR}/include
          ${SHARED_DIR}
          ${SHARED_DIR}/ui/fonts
          ${arduinojson_SOURCE_DIR}/src
          ${ARGPARSE_INCLUDE_DIR}
          ${cpp_httplib_SOURCE_DIR}
          ${LVGL_SOURCE_DIR}/src/libs/thorvg
          ${LVGL_SOURCE_DIR}/src/libs/lodepng
          ${FREERTOS_SOURCE_DIR}/include
          ${FREERTOS_SOURCE_DIR}/portable/ThirdParty/GCC/Posix
          ${CMAKE_CURRENT_SOURCE_DIR}/config
          ${SDL2_INCLUDE_DIRS})

target_link_libraries(
  esptransit_sim
  PRIVATE lvgl
          freertos_kernel
          freertos_config
          ${SDL2_LIBRARIES}
          CURL::libcurl
          pthread
          m)

# === Git version ===
execute_process(
  COMMAND git describe --tags --always --dirty
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(GIT_VERSION STREQUAL "")
  set(GIT_VERSION "unknown")
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../boards.json" BOARDS_JSON_CONTENT)
# Escape backslashes and quotes for safe embedding as a C++ raw string isn't
# needed since we use a raw string literal, but we do need to ensure the
# delimiter doesn't appear in the content.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/boards_json.cpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/boards_json.cpp" @ONLY)

target_sources(esptransit_sim
               PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/boards_json.cpp")

target_compile_definitions(esptransit_sim PRIVATE APP_VERSION="${GIT_VERSION}")

target_compile_options(esptransit_sim PRIVATE -Wall -Wextra -pedantic
                                              -Wno-unused-parameter)

set_target_properties(
  esptransit_sim
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
             CXX_STANDARD 20
             C_STANDARD 11)
