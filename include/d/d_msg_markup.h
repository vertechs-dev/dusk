#ifndef D_MSG_MARKUP_H
#define D_MSG_MARKUP_H

// Dependency-free markup -> in-memory BMG encoder for upgrade descriptions.
// Pure logic (no engine headers) so it is host-unit-testable. See
// docs/superpowers/specs/2026-06-26-upgrade-description-glyphs-design.md.

#include <cstdint>
#include <cstddef>

namespace dMsgMarkup {

// JMessage tag values mirror d/d_msg_class.h (kept duplicated here to stay
// header-light; values are asserted against the engine in d_msg_markup.cpp).
enum Glyph : uint16_t {
    GLYPH_A = 10, GLYPH_B = 11, GLYPH_STICK = 12, GLYPH_L = 13, GLYPH_R = 14,
    GLYPH_X = 15, GLYPH_Y = 16, GLYPH_Z = 17, GLYPH_DPAD = 18,
    GLYPH_ARROW_LEFT = 20, GLYPH_ARROW_RIGHT = 21, GLYPH_ARROW_UP = 22, GLYPH_ARROW_DOWN = 23,
    GLYPH_STICK_UP = 24, GLYPH_STICK_DOWN = 25, GLYPH_STICK_LEFT = 26, GLYPH_STICK_RIGHT = 27,
    // STICK_CROSS: the animated rotating control stick (font_07 family, same as
    // the directional {stick.left/right} above). NOT RED_TARGET (35), which is
    // the red targeting reticle.
    GLYPH_STICK_ROTATE = 19,
};

// Color indices into getFontCCColorTable (d_msg_class.cpp:216).
enum Color : uint8_t {
    COLOR_DEFAULT = 0, COLOR_RED = 1, COLOR_GREEN = 2, COLOR_BLUE = 3,
    COLOR_YELLOW = 4, COLOR_ORANGE = 8,
};

enum TokenKind : uint8_t {
    TOK_TEXT, TOK_GLYPH, TOK_COLOR_PUSH, TOK_COLOR_POP, TOK_BULLET, TOK_NEWLINE,
};

struct Token {
    TokenKind kind;
    uint16_t  value;     // GLYPH: Glyph code; COLOR_PUSH: Color index; else 0
    const char* text;    // TEXT: pointer into the source string
    uint16_t  textLen;   // TEXT: byte length
};

// Group/index our synthesized message is stored under (arbitrary but stable).
static const uint16_t kGroupID = 0;
static const uint16_t kIndex   = 0;

// Tokenize `src` into `out` (cap entries). Returns token count, or 0 on overflow.
size_t tokenize(const char* src, Token* out, size_t cap);

// Encode tokens into a JMessage message-text byte stream (CP1252 text + 0x1A
// tag escapes), NUL-terminated. Returns byte length written (excluding the NUL),
// or 0 on overflow.
size_t encodeMessage(const Token* toks, size_t n, uint8_t* out, size_t cap);

// Wrap a complete THeader+INF1+DAT1 BMG around `msg` (msgLen bytes, NUL-terminated
// content). Returns total BMG size written to `out`, or 0 on overflow.
size_t buildBmg(const uint8_t* msg, size_t msgLen, uint8_t* out, size_t cap);

} // namespace dMsgMarkup

#endif // D_MSG_MARKUP_H
