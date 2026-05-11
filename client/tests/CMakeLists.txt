find_package(Qt5 5.15 REQUIRED COMPONENTS Test)

get_filename_component(GRIZ_REPO_ROOT "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)

add_subdirectory(net)
