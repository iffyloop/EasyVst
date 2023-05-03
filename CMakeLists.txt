cmake_minimum_required(VERSION 3.8)

project(EasyVst)

include(cmake/get_cpm.cmake)
include(cmake/Dependencies.cmake)

add_executable(EasyVst
    examples/SimpleMidiExample.cpp
    include/EasyVst.h
    src/EasyVst.cpp
)

target_compile_features(EasyVst
    PUBLIC
        cxx_std_17
)

target_compile_definitions(EasyVst
    PUBLIC
        "-DSDL_MAIN_HANDLED"
)

target_link_libraries(EasyVst
    PRIVATE
        VST_SDK
        SDL2-static
        portaudio
        RtMidi
        concurrentqueue
)

target_include_directories(EasyVst
    PRIVATE
        include
)
