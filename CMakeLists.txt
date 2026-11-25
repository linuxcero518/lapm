cmake_minimum_required(VERSION 3.28)
project(lapm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(LAPM_BUILD_TESTS "Build the test suite" ON)

include(cmake/warnings.cmake)
include(cmake/deps.cmake)

# ---------- liblapm_core ----------
add_library(lapm_core STATIC
    src/lib/logger.cpp
    src/lib/config.cpp
    src/lib/fs_utils.cpp
    src/lib/version_cmp.cpp
    src/lib/manifest.cpp
    src/lib/package.cpp
    src/lib/repository.cpp
    src/lib/database.cpp
    src/lib/archive.cpp
    src/lib/fetcher.cpp
    src/lib/gpg.cpp
    src/lib/resolver.cpp
    src/lib/installer.cpp
    src/lib/paths.cpp
)

target_include_directories(lapm_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(lapm_core PUBLIC LAPM_VERSION="${PROJECT_VERSION}")

target_link_libraries(lapm_core PUBLIC
    lapm::warnings
    CLI11::CLI11
    CURL::libcurl
    LibArchive::LibArchive
)

# ---------- executables ----------
add_executable(lapm
    src/lapm/main.cpp
    src/commands/dispatch_stub.cpp
    src/commands/update.cpp
    src/commands/upgrade.cpp
)

add_executable(lapm-mk
    src/lapm-mk/main.cpp
)

add_executable(lapm-rep
    src/lapm-rep/main.cpp
)

foreach(exe lapm lapm-mk lapm-rep)
    target_link_libraries(${exe} PRIVATE lapm_core)
endforeach()

# ---------- tests ----------
if(LAPM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()