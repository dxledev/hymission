# Primary build definition. Produces the Hyprland plugin plus standalone layout,
# overview-logic, and drag-logic verification tools.
cmake_minimum_required(VERSION 3.19)
project(hymission VERSION "0.3.3")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

add_compile_definitions(WLR_USE_UNSTABLE)
add_compile_options(
  -g
  -Wall
  -Wextra
  -Wno-unused-parameter
  -Wno-unused-value
  -Wno-missing-field-initializers
  -Wno-narrowing
  -Wno-pointer-arith
  --no-gnu-unique
)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

find_package(PkgConfig REQUIRED)
set(HYM_HYPRLAND_MIN_VERSION "0.55.2" CACHE STRING "Oldest supported Hyprland version")
set(HYM_HYPRLAND_MAX_VERSION "0.57.0" CACHE STRING "First unsupported Hyprland version")
pkg_check_modules(HYPR_DEPS REQUIRED "hyprland>=${HYM_HYPRLAND_MIN_VERSION}")
if(HYPR_DEPS_VERSION VERSION_GREATER_EQUAL HYM_HYPRLAND_MAX_VERSION)
  message(FATAL_ERROR
    "Hyprland ${HYPR_DEPS_VERSION} is newer than the tested compatibility range "
    "${HYM_HYPRLAND_MIN_VERSION} through 0.56.x")
endif()
message(STATUS "Building hymission for Hyprland ${HYPR_DEPS_VERSION}")
pkg_check_modules(LUA REQUIRED lua)

add_library(hymission SHARED
  src/main.cpp
  src/mission_layout.cpp
  src/overview_logic.cpp
  src/overview_drag_logic.cpp
  src/overview_controller.cpp
  src/overview_controller_niri_scrolling.cpp
  src/overview_controller_niri_drag.cpp
  src/overview_controller_niri_resize.cpp
  src/overview_controller_niri_dnd.cpp
)

target_include_directories(hymission PRIVATE ${HYPR_DEPS_INCLUDE_DIRS} ${LUA_INCLUDE_DIRS})
target_link_directories(hymission PRIVATE ${HYPR_DEPS_LIBRARY_DIRS} ${LUA_LIBRARY_DIRS})
target_link_libraries(hymission PRIVATE ${HYPR_DEPS_LIBRARIES} ${LUA_LIBRARIES})

add_executable(hymission-layout-demo
  tools/layout_demo.cpp
  src/mission_layout.cpp
)

target_include_directories(hymission-layout-demo PRIVATE src)

if(BUILD_TESTING)
  add_executable(hymission-mission-layout-test
    tools/mission_layout_test.cpp
    src/mission_layout.cpp
  )

  target_include_directories(hymission-mission-layout-test PRIVATE src)
  add_test(NAME hymission-mission-layout-test COMMAND hymission-mission-layout-test)

  add_executable(hymission-overview-logic-test
    tools/overview_logic_test.cpp
    src/overview_logic.cpp
  )

  target_include_directories(hymission-overview-logic-test PRIVATE src)
  add_test(NAME hymission-overview-logic-test COMMAND hymission-overview-logic-test)

  add_executable(hymission-overview-drag-logic-test
    tools/overview_drag_logic_test.cpp
    src/overview_drag_logic.cpp
  )

  target_include_directories(hymission-overview-drag-logic-test PRIVATE src)
  add_test(NAME hymission-overview-drag-logic-test COMMAND hymission-overview-drag-logic-test)
endif()

install(TARGETS hymission LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
