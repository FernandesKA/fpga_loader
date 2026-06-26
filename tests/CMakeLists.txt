include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(Catch2)

add_executable(fpga_loader_tests
    test_file_utils.cpp
    test_fpga_manager.cpp
    test_dt_overlay.cpp
)

target_link_libraries(fpga_loader_tests
    PRIVATE
    fpga::loader
    Catch2::Catch2WithMain
)

target_compile_options(fpga_loader_tests PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
    $<$<CONFIG:Debug>:-Werror>
)

include(CTest)
include(Catch)
catch_discover_tests(fpga_loader_tests)
