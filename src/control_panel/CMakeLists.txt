set(SE_CONTROL_PANEL_SOURCES
    src/main.cpp
    src/main_window.cpp
    src/settings_client.cpp
)

add_executable(
    SmoothEverything.ControlPanel
    WIN32
    control_panel.rc
    ${SE_CONTROL_PANEL_SOURCES}
)

target_include_directories(
    SmoothEverything.ControlPanel
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    SmoothEverything.ControlPanel
    PRIVATE
        SmoothEverything.Core
        comctl32
        comdlg32
        dwmapi
        shell32
        user32
        uxtheme
)

target_compile_definitions(
    SmoothEverything.ControlPanel
    PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
        SE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        SE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        SE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(MSVC)
    target_compile_options(SmoothEverything.ControlPanel PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(
        SmoothEverything.ControlPanel
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
    target_link_options(
        SmoothEverything.ControlPanel
        PRIVATE
            -municode
            -static
            -static-libgcc
            -static-libstdc++
    )
endif()

set_target_properties(
    SmoothEverything.ControlPanel
    PROPERTIES
        OUTPUT_NAME "SmoothEverything.ControlPanel"
)
