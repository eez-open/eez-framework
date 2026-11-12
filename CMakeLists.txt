# ESP_PLATFORM: Set to 1 when the CMake file is processed within the ESP-IDF build system.
if(ESP_PLATFORM)

    file(GLOB_RECURSE app_sources src/eez/*.c*)
    idf_component_register(SRCS ${app_sources}
        INCLUDE_DIRS "src" "src/eez/platform/simulator/"
        PRIV_REQUIRES lvgl
    )
else()
    cmake_minimum_required(VERSION 3.12)

    include_directories(
        ./src
    )

    file(GLOB_RECURSE SOURCES
        ./src/eez/*.cpp
        ./src/eez/*.c
    )

    ADD_LIBRARY(eez-framework STATIC ${SOURCES})

    target_include_directories(eez-framework SYSTEM PUBLIC ./src)

    # vector fonts (EEZ_OPTION_GUI_VECTOR_FONTS) with FreeType and HarfBuzz
    option(EEZ_FRAMEWORK_VECTOR_FONTS "Vector fonts: build and link FreeType and HarfBuzz (third_party)" OFF)
    if(EEZ_FRAMEWORK_VECTOR_FONTS)
        add_subdirectory(third_party ${CMAKE_CURRENT_BINARY_DIR}/third_party)
        target_link_libraries(eez-framework PUBLIC eez-harfbuzz eez-freetype)
        target_compile_definitions(eez-framework PUBLIC EEZ_OPTION_GUI_VECTOR_FONTS=1)
    endif()
endif()

