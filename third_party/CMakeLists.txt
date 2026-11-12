# FreeType and HarfBuzz for the vector fonts of eez-framework
# (EEZ_OPTION_GUI_VECTOR_FONTS), as the static libraries eez-freetype and
# eez-harfbuzz. The same sources (see README.md) are built for every target:
# the device, the simulator, Studio's WASM and the framework tests draw the
# same pixels.
#
#   add_subdirectory(<eez-framework>/third_party ${CMAKE_BINARY_DIR}/eez-third-party)
#   target_link_libraries(<target> eez-harfbuzz eez-freetype)

cmake_minimum_required(VERSION 3.12)

################################################################################
# FreeType: TrueType and CFF (OpenType) fonts, antialiased rendering; the
# configuration is in freetype-config/ (modules, no zlib, bzip2, PNG, Brotli)

set(EEZ_FREETYPE_DIR ${CMAKE_CURRENT_LIST_DIR}/freetype)

add_library(eez-freetype STATIC
    ${EEZ_FREETYPE_DIR}/src/base/ftsystem.c
    ${EEZ_FREETYPE_DIR}/src/base/ftinit.c
    ${EEZ_FREETYPE_DIR}/src/base/ftdebug.c
    ${EEZ_FREETYPE_DIR}/src/base/ftbase.c
    ${EEZ_FREETYPE_DIR}/src/base/ftbbox.c
    ${EEZ_FREETYPE_DIR}/src/base/ftglyph.c
    ${EEZ_FREETYPE_DIR}/src/base/ftbitmap.c
    ${EEZ_FREETYPE_DIR}/src/base/ftmm.c
    ${EEZ_FREETYPE_DIR}/src/sfnt/sfnt.c
    ${EEZ_FREETYPE_DIR}/src/truetype/truetype.c
    ${EEZ_FREETYPE_DIR}/src/cff/cff.c
    ${EEZ_FREETYPE_DIR}/src/psaux/psaux.c
    ${EEZ_FREETYPE_DIR}/src/pshinter/pshinter.c
    ${EEZ_FREETYPE_DIR}/src/psnames/psnames.c
    ${EEZ_FREETYPE_DIR}/src/smooth/smooth.c
)

# freetype-config first: its freetype/config/ftoption.h and ftmodule.h replace
# the default ones
target_include_directories(eez-freetype SYSTEM PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/freetype-config
    ${EEZ_FREETYPE_DIR}/include
)

target_compile_definitions(eez-freetype PRIVATE FT2_BUILD_LIBRARY)

################################################################################
# HarfBuzz: the text shaping, built from its amalgamated source, without
# external libraries (the fonts are read through its own OpenType code)

set(EEZ_HARFBUZZ_DIR ${CMAKE_CURRENT_LIST_DIR}/harfbuzz)

add_library(eez-harfbuzz STATIC
    ${EEZ_HARFBUZZ_DIR}/src/harfbuzz.cc
)

target_include_directories(eez-harfbuzz SYSTEM PUBLIC ${EEZ_HARFBUZZ_DIR}/src)

set_target_properties(eez-harfbuzz PROPERTIES CXX_STANDARD 17)

################################################################################
# third-party code: no warnings

foreach(target eez-freetype eez-harfbuzz)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W0)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
endforeach()

if(MSVC)
    target_compile_options(eez-harfbuzz PRIVATE /bigobj)
endif()
