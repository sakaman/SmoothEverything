set(SE_ENGINE_SOURCES
    src/autostart.cpp
    src/engine_app.cpp
    src/input_hook.cpp
    src/motion_worker.cpp
    src/pipe_server.cpp
    src/settings_store.cpp
    src/target_policy.cpp
)

add_library(SmoothEverything.EngineLib STATIC ${SE_ENGINE_SOURCES})

target_include_directories(
    SmoothEverything.EngineLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    SmoothEverything.EngineLib
    PUBLIC
        SmoothEverything.Core
        advapi32
        ole32
        shell32
        user32
)

target_compile_definitions(
    SmoothEverything.EngineLib
    PUBLIC
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(SmoothEverything.EngineLib PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(
        SmoothEverything.EngineLib
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
endif()

add_executable(
    SmoothEverything.Engine
    WIN32
    engine.rc
    src/main.cpp
)

target_include_directories(
    SmoothEverything.Engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    SmoothEverything.Engine
    PRIVATE
        SmoothEverything.EngineLib
)

if(MSVC)
    target_compile_options(SmoothEverything.Engine PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(
        SmoothEverything.Engine
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
    target_link_options(
        SmoothEverything.Engine
        PRIVATE
            -municode
            -static
            -static-libgcc
            -static-libstdc++
    )
endif()

set_target_properties(
    SmoothEverything.Engine
    PROPERTIES
        OUTPUT_NAME "SmoothEverything.Engine"
)
