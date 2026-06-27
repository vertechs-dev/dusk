// dusk/src/d/d_msg_markup.cpp
#include "d/d_msg_markup.h"
#include <cstring>

namespace dMsgMarkup {
namespace {

struct NamedToken { const char* name; TokenKind kind; uint16_t value; };

// {token} table. Color pushes use {red}…{/}.  "-" bullets are handled by line scan.
const NamedToken kTokens[] = {
    {"A", TOK_GLYPH, GLYPH_A}, {"B", TOK_GLYPH, GLYPH_B}, {"X", TOK_GLYPH, GLYPH_X},
    {"Y", TOK_GLYPH, GLYPH_Y}, {"L", TOK_GLYPH, GLYPH_L}, {"R", TOK_GLYPH, GLYPH_R},
    {"Z", TOK_GLYPH, GLYPH_Z}, {"dpad", TOK_GLYPH, GLYPH_DPAD},
    {"stick", TOK_GLYPH, GLYPH_STICK}, {"stick.rotate", TOK_GLYPH, GLYPH_STICK_ROTATE},
    {"stick.left", TOK_GLYPH, GLYPH_STICK_LEFT}, {"stick.right", TOK_GLYPH, GLYPH_STICK_RIGHT},
    {"stick.up", TOK_GLYPH, GLYPH_STICK_UP}, {"stick.down", TOK_GLYPH, GLYPH_STICK_DOWN},
    {"arrow.left", TOK_GLYPH, GLYPH_ARROW_LEFT}, {"arrow.right", TOK_GLYPH, GLYPH_ARROW_RIGHT},
    {"arrow.up", TOK_GLYPH, GLYPH_ARROW_UP}, {"arrow.down", TOK_GLYPH, GLYPH_ARROW_DOWN},
    {"red", TOK_COLOR_PUSH, COLOR_RED}, {"green", TOK_COLOR_PUSH, COLOR_GREEN},
    {"blue", TOK_COLOR_PUSH, COLOR_BLUE}, {"yellow", TOK_COLOR_PUSH, COLOR_YELLOW},
    {"orange", TOK_COLOR_PUSH, COLOR_ORANGE}, {"/", TOK_COLOR_POP, 0},
};

bool matchBrace(const char* p, const NamedToken** out, size_t* consumed) {
    // p points just past '{'. Find closing '}'.
    const char* end = strchr(p, '}');
    if (!end) return false;
    size_t len = (size_t)(end - p);
    for (const NamedToken& nt : kTokens) {
        if (strlen(nt.name) == len && strncmp(nt.name, p, len) == 0) {
            *out = &nt; *consumed = len + 1 /*'}'*/; return true;
        }
    }
    return false;
}

} // namespace

size_t tokenize(const char* src, Token* out, size_t cap) {
    size_t n = 0;
    const char* p = src;
    bool atLineStart = true;
    auto push = [&](TokenKind k, uint16_t v, const char* t, uint16_t tl) -> bool {
        if (n >= cap) return false;
        out[n].kind = k; out[n].value = v; out[n].text = t; out[n].textLen = tl; n++; return true;
    };
    while (*p) {
        // Bullet: a line that begins with "- " becomes TOK_BULLET (the "- " is consumed).
        if (atLineStart && p[0] == '-' && p[1] == ' ') {
            if (!push(TOK_BULLET, 0, nullptr, 0)) return 0;
            p += 2; atLineStart = false; continue;
        }
        atLineStart = false;
        if (*p == '\n') { if (!push(TOK_NEWLINE, 0, nullptr, 0)) return 0; p++; atLineStart = true; continue; }
        if (*p == '{') {
            const NamedToken* nt; size_t consumed;
            if (matchBrace(p + 1, &nt, &consumed)) {
                if (!push(nt->kind, nt->value, nullptr, 0)) return 0;
                p += 1 + consumed; continue;
            }
            // Unknown {...}: fall through and treat '{' as literal text.
        }
        // Text run until the next special char.
        const char* start = p;
        while (*p && *p != '{' && *p != '\n') p++;
        if (p > start) { if (!push(TOK_TEXT, 0, start, (uint16_t)(p - start))) return 0; }
        else if (*p == '{') { // lone '{' that didn't match — emit one literal char to avoid stalling
            if (!push(TOK_TEXT, 0, p, 1)) return 0; p++;
        }
    }
    return n;
}

namespace {
bool putByte(uint8_t* out, size_t cap, size_t& o, uint8_t b) {
    if (o >= cap) return false; out[o++] = b; return true;
}
// Emit a tag: 0x1A, size(=5+dataLen), group, typeHi, typeLo, data...
bool putTag(uint8_t* out, size_t cap, size_t& o, uint8_t group, uint16_t type,
            const uint8_t* data, uint8_t dataLen) {
    if (!putByte(out, cap, o, 0x1A)) return false;
    if (!putByte(out, cap, o, (uint8_t)(5 + dataLen))) return false;
    if (!putByte(out, cap, o, group)) return false;
    if (!putByte(out, cap, o, (uint8_t)(type >> 8))) return false;
    if (!putByte(out, cap, o, (uint8_t)(type & 0xFF))) return false;
    for (uint8_t i = 0; i < dataLen; i++) if (!putByte(out, cap, o, data[i])) return false;
    return true;
}
} // namespace

size_t encodeMessage(const Token* toks, size_t n, uint8_t* out, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        const Token& t = toks[i];
        switch (t.kind) {
        case TOK_TEXT:
            for (uint16_t j = 0; j < t.textLen; j++) if (!putByte(out, cap, o, (uint8_t)t.text[j])) return 0;
            break;
        case TOK_GLYPH:        // group 0, no data
            if (!putTag(out, cap, o, 0, t.value, nullptr, 0)) return 0;
            break;
        case TOK_BULLET:       // group 6, type 10 (BULLET)
            if (!putTag(out, cap, o, 6, 10, nullptr, 0)) return 0;
            break;
        case TOK_COLOR_PUSH: { uint8_t idx = (uint8_t)t.value;
            if (!putTag(out, cap, o, 255, 0, &idx, 1)) return 0; break; }
        case TOK_COLOR_POP:  { uint8_t idx = COLOR_DEFAULT;
            if (!putTag(out, cap, o, 255, 0, &idx, 1)) return 0; break; }
        case TOK_NEWLINE:
            if (!putByte(out, cap, o, (uint8_t)'\n')) return 0;
            break;
        }
    }
    if (o >= cap) return 0;
    out[o] = 0x00;   // NUL terminate (not counted in return)
    return o;
}

namespace {
inline void be32(uint8_t* p, uint32_t v){ p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
inline void be16(uint8_t* p, uint16_t v){ p[0]=v>>8; p[1]=v; }
inline size_t pad32(size_t v){ return (v + 0x1F) & ~size_t(0x1F); }
} // namespace

size_t buildBmg(const uint8_t* msg, size_t msgLen, uint8_t* out, size_t cap) {
    const size_t headerSize = 0x20;
    const size_t inf1Size = pad32(0x10 + 4);                 // header + one 4-byte entry
    const size_t dat1Raw  = 0x8 + 1 /*reserved NUL*/ + msgLen + 1 /*msg NUL*/;
    const size_t dat1Size = pad32(dat1Raw);
    const size_t total    = headerSize + inf1Size + dat1Size;
    if (total > cap) return 0;
    memset(out, 0, total);

    // THeader
    memcpy(out, "MESGbmg1", 8);
    be32(out + 0x8, (uint32_t)total);
    be32(out + 0xC, 2);            // blocks
    out[0x10] = 1;                 // encoding (1 = single-byte / CP1252)

    // INF1
    uint8_t* inf = out + headerSize;
    memcpy(inf, "INF1", 4);
    be32(inf + 0x4, (uint32_t)inf1Size);
    be16(inf + 0x8, 1);            // entry count
    be16(inf + 0xA, 4);            // entry size
    be16(inf + 0xC, 0);            // group id
    be32(inf + 0x10, 1);           // entry[0] text offset = 1 (skip reserved byte 0)

    // DAT1
    uint8_t* dat = out + headerSize + inf1Size;
    memcpy(dat, "DAT1", 4);
    be32(dat + 0x4, (uint32_t)dat1Size);
    dat[0x8] = 0x00;               // reserved empty string at offset 0
    memcpy(dat + 0x8 + 1, msg, msgLen);
    dat[0x8 + 1 + msgLen] = 0x00;  // message NUL
    return total;
}

} // namespace dMsgMarkup
