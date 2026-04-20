cmake_minimum_required(VERSION 3.16)

# TODO: once planning/ui-design/09-build-packaging-ci.md §4.3 lands the shared
# repo-root VERSION file, read it here instead of hardcoding.
project(griz-client
    VERSION 23.1.4
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 5.15 bootstrap. Qt 6 is the design target (see planning/ui-design/04-client.md
# §2.2, planning/ui-design/09-build-packaging-ci.md §3.2) but TOSS ships only Qt 5.
# Port is localized to a handful of includes; see planning/UI.md Phase 5 notes.
find_package(Qt5 5.15 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    OpenGL
    Concurrent)

add_subdirectory(src)
