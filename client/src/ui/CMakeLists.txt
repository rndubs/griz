add_library(griz-client-ui STATIC
    MainWindow.cpp
    Viewport.cpp
    Console.cpp
    MaterialsDock.cpp
    ResultsDock.cpp
    SelectionDock.cpp
    TimeSlider.cpp)

target_include_directories(griz-client-ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(griz-client-ui
    PUBLIC
        griz-client-model
        griz-client-net
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::OpenGL)
