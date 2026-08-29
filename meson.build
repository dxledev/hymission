# Alternate plugin build retained for Hyprland plugin tooling. CMake is the
# repository's primary development/test path and also builds the helper tools.
project('hymission', 'cpp',
  version: '0.3.2',
  default_options: ['buildtype=release'],
)

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.has_argument('-std=c++23')
  add_global_arguments('-std=c++23', language: 'cpp')
elif cpp_compiler.has_argument('-std=c++2b')
  add_global_arguments('-std=c++2b', language: 'cpp')
else
  error('Could not configure current C++ compiler with C++23 support')
endif

hyprland_dep = dependency('hyprland', version: ['>=0.55.2', '<0.57.0'])

add_project_arguments(
  [
    '-DWLR_USE_UNSTABLE',
    '-g',
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter',
    '-Wno-unused-value',
    '-Wno-missing-field-initializers',
    '-Wno-narrowing',
    '-Wno-pointer-arith',
  ],
  language: 'cpp')

src = [
  'src/main.cpp',
  'src/mission_layout.cpp',
  'src/overview_logic.cpp',
  'src/overview_drag_logic.cpp',
  'src/overview_controller.cpp',
  'src/overview_controller_niri_scrolling.cpp',
  'src/overview_controller_niri_drag.cpp',
  'src/overview_controller_niri_resize.cpp',
  'src/overview_controller_niri_dnd.cpp',
]

shared_module(meson.project_name(), src,
  dependencies: [
    hyprland_dep,
    dependency('lua'),
  ],
  install: true,
)

executable('hymission-layout-demo',
  ['tools/layout_demo.cpp', 'src/mission_layout.cpp'],
  include_directories: include_directories('src'),
  install: false,
)

mission_layout_test = executable('hymission-mission-layout-test',
  ['tools/mission_layout_test.cpp', 'src/mission_layout.cpp'],
  include_directories: include_directories('src'),
  install: false,
)

test('hymission-mission-layout-test', mission_layout_test)

overview_logic_test = executable('hymission-overview-logic-test',
  ['tools/overview_logic_test.cpp', 'src/overview_logic.cpp'],
  include_directories: include_directories('src'),
  install: false,
)

test('hymission-overview-logic-test', overview_logic_test)

overview_drag_logic_test = executable('hymission-overview-drag-logic-test',
  ['tools/overview_drag_logic_test.cpp', 'src/overview_drag_logic.cpp'],
  include_directories: include_directories('src'),
  install: false,
)

test('hymission-overview-drag-logic-test', overview_drag_logic_test)
