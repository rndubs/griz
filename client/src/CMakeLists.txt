add_subdirectory(net)
add_subdirectory(model)
add_subdirectory(ui)

add_executable(griz-client
    main.cpp
    App.cpp)

target_link_libraries(griz-client
    PRIVATE
        griz-client-ui
        griz-client-model
        griz-client-net
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui)

target_compile_definitions(griz-client
    PRIVATE
        GRIZ_CLIENT_VERSION="${PROJECT_VERSION}"
        GRIZ_REPO_ROOT="${GRIZ_REPO_ROOT}")
