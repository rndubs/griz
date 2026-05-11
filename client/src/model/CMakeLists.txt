add_library(griz-client-model STATIC
    SessionState.cpp)

target_include_directories(griz-client-model
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(griz-client-model
    PUBLIC
        Qt5::Core)
