add_executable(
    SmoothEverything.CoreTests
    core/json_settings_tests.cpp
    core/motion_engine_tests.cpp
)

target_link_libraries(
    SmoothEverything.CoreTests
    PRIVATE
        SmoothEverything.Core
)

target_include_directories(
    SmoothEverything.CoreTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/engine/include
)

if(MSVC)
    target_compile_options(SmoothEverything.CoreTests PRIVATE /W4 /permissive-)
else()
    target_compile_options(
        SmoothEverything.CoreTests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
    target_link_options(
        SmoothEverything.CoreTests
        PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
    )
endif()

add_test(NAME core.motion COMMAND SmoothEverything.CoreTests)

if(WIN32 AND TARGET SmoothEverything.EngineLib)
    add_executable(
        SmoothEverything.EngineTests
        engine/engine_component_tests.cpp
    )
    target_link_libraries(
        SmoothEverything.EngineTests
        PRIVATE
            SmoothEverything.EngineLib
    )
    if(MSVC)
        target_compile_options(SmoothEverything.EngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(
            SmoothEverything.EngineTests
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
        )
        target_link_options(
            SmoothEverything.EngineTests
            PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
        )
    endif()
    add_test(NAME engine.components COMMAND SmoothEverything.EngineTests)
endif()
