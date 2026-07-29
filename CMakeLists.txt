# ESP_PLATFORM: Set to 1 when the CMake file is processed within the ESP-IDF build system.
if(ESP_PLATFORM)

    file(GLOB_RECURSE app_sources src/eez/*.c*)
    idf_component_register(SRCS ${app_sources}
        INCLUDE_DIRS "src" "src/eez/libs/agg" "src/eez/platform/simulator/"
        PRIV_REQUIRES lvgl
    )
else()
    cmake_minimum_required(VERSION 3.12)

    include_directories(
        ./src
        ./src/eez/libs/agg
    )

    file(GLOB_RECURSE SOURCES
        ./src/eez/*.cpp
        ./src/eez/*.c
    )

    ADD_LIBRARY(eez-framework STATIC ${SOURCES})

    target_include_directories(eez-framework SYSTEM PUBLIC ./src ./src/eez/libs/agg)
endif()

