add_library(griz-client-commands STATIC
    CommandBridge.cpp)

target_include_directories(griz-client-commands
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(griz-client-commands
    PUBLIC
        griz-client-net
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets)
