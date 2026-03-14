# The following lines of boilerplate have to be in your project's CMakeLists in
# this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Board selection: pass -DBOARD=jc4880p443c for the small board. Default is
# jc8012p4a1c (large board, 800x1280).
if(BOARD STREQUAL "jc4880p443c")
  set(COMPONENTS main bsp_jc4880p443c)
elseif(BOARD STREQUAL "jc1060p470c")
  set(COMPONENTS main bsp_jc1060p470c)
else()
  set(COMPONENTS main bsp_jc8012p4a1c)
endif()

project(ESPTransit)
