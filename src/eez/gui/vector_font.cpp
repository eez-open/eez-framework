/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI && EEZ_OPTION_GUI_VECTOR_FONTS

#include <string.h>

#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ot.h>

#include <eez/gui/vector_font.h>

namespace eez {
namespace gui {
namespace vector_font {

// A font file is identified by its address in the assets, checked with a
// fingerprint of its content: assets are freed and loaded again (Studio's
// editor, external assets), so the same address can hold another file.
struct FileKey {
    const FontFile *file;
    uint32_t size;
    uint32_t fingerprint;

    bool operator==(const FileKey &other) const {
        return file == other.file && size == other.size && fingerprint == other.fingerprint;
    }
};

static FileKey makeFileKey(const FontFile *file) {
    FileKey key;
    key.file = file;
    key.size = file->size;
    // FNV-1a of up to 64 bytes from the start, the middle and the end
    uint32_t hash = 2166136261u;
    auto add = [&hash, file](uint32_t from) {
        for (uint32_t i = from; i < from + 64 && i < file->size; i++) {
            hash = (hash ^ file->data[i]) * 16777619u;
        }
    };
    add(0);
    add(file->size / 2);
    add(file->size > 64 ? file->size - 64 : 0);
    key.fingerprint = hash;
    return key;
}

// a font: the file, the size and the features
struct FontKey {
    FileKey file;
    uint32_t size;
    int32_t letterSpacing;
    std::string features;

    bool operator==(const FontKey &other) const {
        return file == other.file && size == other.size && letterSpacing == other.letterSpacing && features == other.features;
    }
};

struct FontKeyHash {
    size_t operator()(const FontKey &key) const {
        size_t h = std::hash<const void *>()(key.file.file);
        h = h * 31 + key.file.fingerprint;
        h = h * 31 + key.size;
        h = h * 31 + (uint32_t)key.letterSpacing;
        h = h * 31 + std::hash<std::string>()(key.features);
        return h;
    }
};

struct Glyph {
    GlyphImage image;
    std::vector<uint8_t> pixels;
};

struct Font {
    FT_Face ftFace = nullptr;
    hb_blob_t *hbBlob = nullptr;
    hb_face_t *hbFace = nullptr;
    hb_font_t *hbFont = nullptr;
    int32_t letterSpacing = 0;
    std::vector<hb_feature_t> features;
    // with ligatures off
    std::vector<hb_feature_t> featuresNoLigatures;
    std::unordered_map<uint32_t, Glyph> glyphs;

    ~Font() {
        if (hbFont) {
            hb_font_destroy(hbFont);
        }
        if (hbFace) {
            hb_face_destroy(hbFace);
        }
        if (hbBlob) {
            hb_blob_destroy(hbBlob);
        }
        if (ftFace) {
            FT_Done_Face(ftFace);
        }
    }
};

// the most glyph images of a font, then they are dropped
static const size_t MAX_GLYPHS = 1024;

// shaped texts
struct ShapeKey {
    const Font *font;
    bool ligatures;
    std::string text;

    bool operator==(const ShapeKey &other) const {
        return font == other.font && ligatures == other.ligatures && text == other.text;
    }
};

struct ShapeKeyHash {
    size_t operator()(const ShapeKey &key) const {
        return std::hash<const void *>()(key.font) * 31 + std::hash<std::string>()(key.text) + key.ligatures;
    }
};

typedef std::list<std::pair<ShapeKey, std::vector<ShapedGlyph>>> ShapeList;

static const size_t MAX_SHAPED_TEXTS = 512;

static std::recursive_mutex g_mutex;
static FT_Library g_library;
// nullptr for a font that can't be used (asked for once)
static std::unordered_map<FontKey, Font *, FontKeyHash> g_fonts;
// the most recently used first
static ShapeList g_shapedTexts;
static std::unordered_map<ShapeKey, ShapeList::iterator, ShapeKeyHash> g_shapedTextsMap;
static hb_buffer_t *g_buffer;

Lock::Lock() {
    g_mutex.lock();
}

Lock::~Lock() {
    g_mutex.unlock();
}

static void parseFeatures(const char *features, std::vector<hb_feature_t> &result) {
    if (!features) {
        return;
    }
    const char *p = features;
    while (*p) {
        const char *end = strchr(p, ',');
        int length = end ? (int)(end - p) : (int)strlen(p);
        hb_feature_t feature;
        if (length > 0 && hb_feature_from_string(p, length, &feature)) {
            result.push_back(feature);
        }
        if (!end) {
            break;
        }
        p = end + 1;
    }
}

static Font *createFont(const VectorFontData *fontData) {
    const FontFile *file = fontData->file;

    if (!g_library && FT_Init_FreeType(&g_library) != 0) {
        g_library = nullptr;
        return nullptr;
    }

    auto font = new Font();

    if (FT_New_Memory_Face(g_library, file->data, (FT_Long)file->size, 0, &font->ftFace) != 0) {
        font->ftFace = nullptr;
        delete font;
        return nullptr;
    }

    // the size in 1/64 points at 72 dpi is the size in 1/64 pixels
    if (FT_Set_Char_Size(font->ftFace, 0, fontData->size, 72, 72) != 0) {
        delete font;
        return nullptr;
    }

    // the shaping reads the font with HarfBuzz's own OpenType code, at the same
    // size (the scale is in 1/64 pixels too)
    font->hbBlob = hb_blob_create((const char *)file->data, file->size, HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    font->hbFace = hb_face_create(font->hbBlob, 0);
    font->hbFont = hb_font_create(font->hbFace);
    hb_ot_font_set_funcs(font->hbFont);
    hb_font_set_scale(font->hbFont, (int)fontData->size, (int)fontData->size);

    font->letterSpacing = fontData->letterSpacing;

    parseFeatures(fontData->features, font->features);
    font->featuresNoLigatures = font->features;
    parseFeatures("-liga,-clig,-dlig", font->featuresNoLigatures);

    return font;
}

static Font *getFont(const VectorFontData *fontData) {
    const FontFile *file = fontData->file;
    if (!file || file->size == 0) {
        return nullptr;
    }

    FontKey key;
    key.file = makeFileKey(file);
    key.size = fontData->size;
    key.letterSpacing = fontData->letterSpacing;
    const char *features = fontData->features;
    if (features) {
        key.features = features;
    }

    auto it = g_fonts.find(key);
    if (it != g_fonts.end()) {
        return it->second;
    }

    auto font = createFont(fontData);
    g_fonts[key] = font;
    return font;
}

const ShapedGlyph *shape(const VectorFontData *fontData, const char *text, int textLength, bool ligatures, uint32_t &numGlyphs) {
    numGlyphs = 0;

    auto font = getFont(fontData);
    if (!font) {
        return nullptr;
    }

    ShapeKey key;
    key.font = font;
    key.ligatures = ligatures;
    key.text.assign(text, textLength);

    auto it = g_shapedTextsMap.find(key);
    if (it != g_shapedTextsMap.end()) {
        // the most recently used first
        g_shapedTexts.splice(g_shapedTexts.begin(), g_shapedTexts, it->second);
        auto &glyphs = it->second->second;
        numGlyphs = (uint32_t)glyphs.size();
        return glyphs.data();
    }

    if (!g_buffer) {
        g_buffer = hb_buffer_create();
    }
    hb_buffer_clear_contents(g_buffer);
    hb_buffer_add_utf8(g_buffer, text, textLength, 0, textLength);
    hb_buffer_guess_segment_properties(g_buffer);

    auto &features = ligatures ? font->features : font->featuresNoLigatures;
    hb_shape(font->hbFont, g_buffer, features.data(), (unsigned)features.size());

    unsigned count;
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(g_buffer, &count);
    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(g_buffer, &count);

    std::vector<ShapedGlyph> glyphs(count);
    for (unsigned i = 0; i < count; i++) {
        glyphs[i].glyphIndex = infos[i].codepoint;
        glyphs[i].cluster = infos[i].cluster;
        glyphs[i].xOffset = positions[i].x_offset;
        glyphs[i].yOffset = positions[i].y_offset;
        glyphs[i].advance = positions[i].x_advance + font->letterSpacing;
    }

    if (g_shapedTexts.size() >= MAX_SHAPED_TEXTS) {
        g_shapedTextsMap.erase(g_shapedTexts.back().first);
        g_shapedTexts.pop_back();
    }
    g_shapedTexts.emplace_front(key, std::move(glyphs));
    g_shapedTextsMap[g_shapedTexts.front().first] = g_shapedTexts.begin();

    auto &result = g_shapedTexts.front().second;
    numGlyphs = (uint32_t)result.size();
    return result.data();
}

const GlyphImage *getGlyphImage(const VectorFontData *fontData, uint32_t glyphIndex) {
    auto font = getFont(fontData);
    if (!font) {
        return nullptr;
    }

    auto it = font->glyphs.find(glyphIndex);
    if (it != font->glyphs.end()) {
        return it->second.image.width > 0 ? &it->second.image : nullptr;
    }

    if (font->glyphs.size() >= MAX_GLYPHS) {
        font->glyphs.clear();
    }

    auto &glyph = font->glyphs[glyphIndex];
    glyph.image.width = 0;
    glyph.image.height = 0;
    glyph.image.left = 0;
    glyph.image.top = 0;
    glyph.image.pixels = nullptr;

    // not hinted: the same shapes at every size and on every target
    if (FT_Load_Glyph(font->ftFace, glyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_RENDER) != 0) {
        return nullptr;
    }

    FT_GlyphSlot slot = font->ftFace->glyph;
    FT_Bitmap &bitmap = slot->bitmap;
    if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.width == 0 || bitmap.rows == 0) {
        return nullptr;
    }

    glyph.pixels.resize((size_t)bitmap.width * bitmap.rows);
    for (unsigned y = 0; y < bitmap.rows; y++) {
        memcpy(glyph.pixels.data() + y * bitmap.width, bitmap.buffer + (int)y * bitmap.pitch, bitmap.width);
    }

    glyph.image.width = (int)bitmap.width;
    glyph.image.height = (int)bitmap.rows;
    glyph.image.left = slot->bitmap_left;
    glyph.image.top = slot->bitmap_top;
    glyph.image.pixels = glyph.pixels.data();

    return &glyph.image;
}

void reset() {
    Lock lock;

    g_shapedTexts.clear();
    g_shapedTextsMap.clear();

    for (auto &it : g_fonts) {
        delete it.second;
    }
    g_fonts.clear();
}

} // namespace vector_font
} // namespace gui
} // namespace eez

#endif
