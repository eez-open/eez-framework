# eez-framework tests (Linux / WSL, g++). Use run.sh, which configures, builds
# and runs them.

cmake_minimum_required(VERSION 3.16)

project(eez_framework_tests C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FW_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src" CACHE PATH "eez-framework src folder to test")
option(EEZ_TESTS_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" ON)
option(EEZ_TESTS_TSAN "Build with ThreadSanitizer (instead of EEZ_TESTS_SANITIZE)" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_image)

if(EEZ_TESTS_TSAN)
    set(SANITIZE_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
elseif(EEZ_TESTS_SANITIZE)
    set(SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

################################################################################
# the framework, configured like the BB3+ on CM5 (conf/eez-framework-conf.h)

file(GLOB_RECURSE FW_SOURCES "${FW_SRC}/eez/*.cpp" "${FW_SRC}/eez/*.c")
list(FILTER FW_SOURCES EXCLUDE REGEX "/eez/libs/libscpi/")

add_library(eez_framework STATIC ${FW_SOURCES})
target_include_directories(eez_framework PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/conf" "${FW_SRC}")
target_compile_definitions(eez_framework PUBLIC EEZ_PLATFORM_CM5_RASPI)
# the tests are about behaviour: the framework's own warnings are left out
target_compile_options(eez_framework PRIVATE -w ${SANITIZE_FLAGS})
target_link_libraries(eez_framework PUBLIC PkgConfig::SDL2 pthread)

# FreeType and HarfBuzz of the vector fonts (EEZ_OPTION_GUI_VECTOR_FONTS)
add_subdirectory("${FW_SRC}/../third_party" "${CMAKE_BINARY_DIR}/eez-third-party")
target_link_libraries(eez_framework PUBLIC eez-harfbuzz eez-freetype)

################################################################################
# the tests

file(GLOB_RECURSE TEST_SOURCES
    harness/*.cpp
    core/*.cpp
    expressions/*.cpp
    flow/*.cpp
    gui/*.cpp
    widgets/*.cpp
    render/*.cpp
)

add_executable(eez-framework-tests ${TEST_SOURCES} projects/fwtest/generated/document.cpp)
target_include_directories(eez-framework-tests PRIVATE harness projects/fwtest/generated)
# -Wno-unused-variable: TITLE and ICON of eez-framework-conf.h
target_compile_options(eez-framework-tests PRIVATE -Wall -Wno-unused-variable ${SANITIZE_FLAGS})
# eez::millis() is the fake clock of harness/clock.cpp
target_link_options(eez-framework-tests PRIVATE ${SANITIZE_FLAGS} -Wl,--wrap=_ZN3eez6millisEv)
target_link_libraries(eez-framework-tests PRIVATE eez_framework)
