add_library(
    SmoothEverything.Core
    STATIC
    src/json.cpp
    src/motion_engine.cpp
    src/settings.cpp
)

target_include_directories(
    SmoothEverything.Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(SmoothEverything.Core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(SmoothEverything.Core PRIVATE /W4 /permissive-)
else()
    target_compile_options(
        SmoothEverything.Core
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
endif()
