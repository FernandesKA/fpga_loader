cmake_minimum_required(VERSION 3.16)
project(fpga-loader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Cross-compilation (Buildroot SDK):
#   source /path/to/sdk/environment-setup-<tuple>
#   cmake -B build -DCMAKE_BUILD_TYPE=Release

add_executable(fpga-loader
    src/main.cpp
    src/fpga_manager.cpp
    src/dt_overlay.cpp
    src/file_utils.cpp
)

target_include_directories(fpga-loader PRIVATE inc)

target_compile_options(fpga-loader PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

install(TARGETS fpga-loader DESTINATION bin)
install(PROGRAMS scripts/mount-configfs.sh DESTINATION bin)
