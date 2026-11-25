add_library(lapm_warnings INTERFACE)
add_library(lapm::warnings ALIAS lapm_warnings)

target_compile_options(lapm_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    -Wnon-virtual-dtor -Wcast-align
    -Wunused -Woverloaded-virtual -Wnull-dereference
    -Wdouble-promotion -Wformat=2
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(lapm_warnings INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer
        -g3
    )
    target_link_options(lapm_warnings INTERFACE
        -fsanitize=address,undefined
    )
endif()