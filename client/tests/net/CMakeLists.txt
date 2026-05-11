add_executable(test_worker_smoke test_worker_smoke.cpp)

target_compile_definitions(test_worker_smoke
    PRIVATE
        GRIZ_REPO_ROOT="${GRIZ_REPO_ROOT}")

target_link_libraries(test_worker_smoke
    PRIVATE
        griz-client-net
        Qt5::Core
        Qt5::Network
        Qt5::Test)

add_test(NAME worker_smoke COMMAND test_worker_smoke)
