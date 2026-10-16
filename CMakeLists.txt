# Debug: console subsystem + BRIDGE_CONSOLE=1. Release: Windows subsystem, no console, LTCG.
cmake_minimum_required(VERSION 3.24)
project(cursor-gd-bridge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cursor-gd-bridge
    src/main.cpp
    src/app.cpp
    src/net/winhttp_client.cpp
    src/cursor/agent_client.cpp
    src/openai/chat_completions.cpp
    src/openai/models.cpp
    src/util/log.cpp
)

target_include_directories(cursor-gd-bridge PRIVATE src)
target_include_directories(cursor-gd-bridge SYSTEM PRIVATE third_party)

target_compile_definitions(cursor-gd-bridge PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:BRIDGE_CONSOLE=1>
    $<$<NOT:$<CONFIG:Debug>>:BRIDGE_CONSOLE=0>
)

if(MSVC)
    target_compile_options(cursor-gd-bridge PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
        /Zc:preprocessor
        /external:W0
        $<$<CONFIG:Release>:/O2 /GL>
    )
    target_link_options(cursor-gd-bridge PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup>
        $<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup>
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
endif()

target_link_libraries(cursor-gd-bridge PRIVATE winhttp ws2_32 shell32 ole32)
