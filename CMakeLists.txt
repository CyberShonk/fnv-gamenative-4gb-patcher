cmake_minimum_required(VERSION 3.16)
project(FNVGameNativePatcher VERSION 0.1.1 LANGUAGES CXX)

add_executable(FNVGameNativePatcher src/main.cpp)
target_compile_features(FNVGameNativePatcher PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(FNVGameNativePatcher PRIVATE /W4 /permissive-)
else()
    target_compile_options(FNVGameNativePatcher PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

install(TARGETS FNVGameNativePatcher RUNTIME DESTINATION .)
