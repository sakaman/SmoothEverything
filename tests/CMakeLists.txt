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

if(WIN32)
    add_executable(
        SmoothEverything.LocalizationTests
        control_panel/localization_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/control_panel/src/localization.cpp
    )
    target_include_directories(
        SmoothEverything.LocalizationTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src/control_panel/include
    )
    target_compile_definitions(
        SmoothEverything.LocalizationTests
        PRIVATE
            UNICODE
            _UNICODE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
    if(MSVC)
        target_compile_options(SmoothEverything.LocalizationTests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(
            SmoothEverything.LocalizationTests
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
        )
        target_link_options(
            SmoothEverything.LocalizationTests
            PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
        )
    endif()
    add_test(NAME control_panel.localization COMMAND SmoothEverything.LocalizationTests)

    add_executable(
        SmoothEverything.SingleInstanceTests
        control_panel/single_instance_tests.cpp
        ${PROJECT_SOURCE_DIR}/src/control_panel/src/single_instance.cpp
    )
    target_include_directories(
        SmoothEverything.SingleInstanceTests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src/control_panel/include
            ${PROJECT_SOURCE_DIR}/src/core/include
    )
    target_compile_definitions(
        SmoothEverything.SingleInstanceTests
        PRIVATE
            UNICODE
            _UNICODE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(SmoothEverything.SingleInstanceTests PRIVATE user32)
    if(MSVC)
        target_compile_options(SmoothEverything.SingleInstanceTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(
            SmoothEverything.SingleInstanceTests
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
        )
        target_link_options(
            SmoothEverything.SingleInstanceTests
            PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
        )
    endif()
    add_test(NAME control_panel.single_instance COMMAND SmoothEverything.SingleInstanceTests)
endif()

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
