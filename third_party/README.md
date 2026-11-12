# Third-party libraries of the vector fonts

FreeType and HarfBuzz draw the vector fonts (`EEZ_OPTION_GUI_VECTOR_FONTS`,
`src/eez/gui/vector_font.*`): HarfBuzz shapes the text (kerning, ligatures,
OpenType features), FreeType renders the glyphs. They are kept here as pinned
sources, so the device, the simulator, Studio's WASM and the framework tests
build the same versions and draw the same pixels. They are outside `src/eez`,
which the builds compile by globbing.

`CMakeLists.txt` builds them as the static libraries `eez-freetype` and
`eez-harfbuzz`:

```cmake
add_subdirectory(<eez-framework>/third_party ${CMAKE_BINARY_DIR}/eez-third-party)
target_link_libraries(<target> eez-harfbuzz eez-freetype)
```

(or `EEZ_FRAMEWORK_VECTOR_FONTS=ON` with the framework's own `CMakeLists.txt`),
and the configuration of the project defines `EEZ_OPTION_GUI_VECTOR_FONTS 1`.

| Library | Version | Source | SHA-256 of the archive | License |
|---|---|---|---|---|
| FreeType | 2.13.3 | https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz | `0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289` | FreeType License (`freetype/docs/FTL.TXT`) |
| HarfBuzz | 10.1.0 | https://github.com/harfbuzz/harfbuzz/releases/download/10.1.0/harfbuzz-10.1.0.tar.xz | `6ce3520f2d089a33cef0fc48321334b8e0b72141f6a763719aaaecd2779ecb82` | "Old MIT" (`harfbuzz/COPYING`) |

The credit lines and the license texts are in `../THIRD-PARTY-LICENSES.TXT`.

## What is kept

- FreeType: `include/`, the modules `base`, `sfnt`, `truetype`, `cff`, `psaux`,
  `pshinter`, `psnames` and `smooth` of `src/` (TrueType and CFF/OpenType fonts,
  antialiased rendering), `LICENSE.TXT`, `docs/FTL.TXT`, `README`. The build
  files of the modules (`*.mk`) are removed.
- `freetype-config/freetype/config/`: `ftmodule.h` (the modules above) and
  `ftoption.h`, the default one without zlib, LZW, the `FREETYPE_PROPERTIES`
  environment variable, Mac font files and OT-SVG glyphs. This folder comes
  first in the include path, so these two files replace the default ones.
- HarfBuzz: `src/` without the tests, the fuzzer helper and the generator
  scripts, `COPYING`, `README.md`. Only `src/harfbuzz.cc` (the amalgamated
  library) is compiled, without external libraries: HarfBuzz reads the fonts
  with its own OpenType code.

## Updating

Unpack the new archives, copy the same files over (keep `freetype-config/`,
compare `ftoption.h` with the new default one), update the table above, build
and run the framework tests (`tests/run.sh`), the vector font tests are
`render/vector_font_test.cpp`.
