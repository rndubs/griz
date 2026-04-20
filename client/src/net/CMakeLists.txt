add_library(griz-client-net STATIC
    Framing.cpp
    Rendezvous.cpp
    Worker.cpp)

target_include_directories(griz-client-net
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(griz-client-net
    PUBLIC
        Qt5::Core
        Qt5::Network)
