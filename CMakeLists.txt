cmake_minimum_required(VERSION 3.16)
project(fpga-loader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Cross-compilation (Buildroot SDK):
#   source /path/to/sdk/environment-setup-<tuple>
#   cmake -B build -DCMAKE_BUILD_TYPE=Release

include(GNUInstallDirs)

# When used as a submodule the executable is not built by default
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_default_exe ON)
else()
    set(_default_exe OFF)
endif()
option(FPGA_LOADER_BUILD_EXECUTABLE
    "Build the fpga-loader CLI executable" ${_default_exe})

# ── Library ───────────────────────────────────────────────────────────────────

add_library(fpga_loader
    src/fpga_manager.cpp
    src/dt_overlay.cpp
    src/file_utils.cpp
)
add_library(fpga::loader ALIAS fpga_loader)

target_include_directories(fpga_loader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fpga_loader>
)

target_compile_options(fpga_loader PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
    $<$<CONFIG:Debug>:-Werror>
)

# ── Executable ────────────────────────────────────────────────────────────────

if(FPGA_LOADER_BUILD_EXECUTABLE)
    add_executable(fpga-loader src/main.cpp)
    target_link_libraries(fpga-loader PRIVATE fpga::loader)
    target_compile_options(fpga-loader PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
        $<$<CONFIG:Debug>:-Werror>
    )
    install(TARGETS fpga-loader DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(PROGRAMS scripts/mount-configfs.sh DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# ── Install ───────────────────────────────────────────────────────────────────

install(TARGETS fpga_loader
    EXPORT fpga_loaderTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY inc/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fpga_loader
)
install(EXPORT fpga_loaderTargets
    FILE fpga_loaderTargets.cmake
    NAMESPACE fpga::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fpga_loader
)
