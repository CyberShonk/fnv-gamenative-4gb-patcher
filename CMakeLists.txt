cmake_minimum_required(VERSION 3.16)
project(FNVGameNativePatcher VERSION 0.1.3 LANGUAGES CXX)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_executable(FNVGameNativePatcher WIN32
        src/windows_gui.cpp
        src/patcher.cpp
    )
else()
    add_executable(FNVGameNativePatcher
        src/main.cpp
        src/patcher.cpp
    )
endif()

target_compile_features(FNVGameNativePatcher PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(FNVGameNativePatcher PRIVATE /W4 /permissive-)
else()
    target_compile_options(FNVGameNativePatcher PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

if(MINGW)
    target_link_options(FNVGameNativePatcher PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
endif()

install(TARGETS FNVGameNativePatcher RUNTIME DESTINATION .)
