add_executable(lapm_tests
    test_version_cmp.cpp
    test_manifest.cpp
    test_package_naming.cpp
    test_safe_extract.cpp
    test_paths.cpp
    test_config.cpp
)

target_link_libraries(lapm_tests PRIVATE
    lapm_core
    Catch2::Catch2WithMain
)

# ASan exige ser la primera librería cargada. Al enlazar estáticamente
# evitamos el conflicto con Catch2 al descubrir los tests.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(lapm_tests PRIVATE -static-libasan)
endif()

target_compile_definitions(lapm_tests PRIVATE LAPM_VERSION="${PROJECT_VERSION}")

include(Catch)
catch_discover_tests(lapm_tests)