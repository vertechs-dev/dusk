// dusk/test/d_msg_markup_test.cpp
#include "d/d_msg_markup.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>
using namespace dMsgMarkup;

static int g_fail = 0;
#define CHECK(c) do { if(!(c)){ printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while(0)

static void test_tokenize_basic() {
    Token t[32];
    size_t n = tokenize("Press {B} now", t, 32);
    CHECK(n == 3);
    CHECK(t[0].kind == TOK_TEXT && t[0].textLen == 6 && strncmp(t[0].text, "Press ", 6) == 0);
    CHECK(t[1].kind == TOK_GLYPH && t[1].value == GLYPH_B);
    CHECK(t[2].kind == TOK_TEXT && t[2].textLen == 4 && strncmp(t[2].text, " now", 4) == 0);
}

static void test_tokenize_color_bullet_newline() {
    Token t[32];
    size_t n = tokenize("Deals {red}20%{/} more.\n- Note here", t, 32);
    CHECK(t[0].kind == TOK_TEXT);                       // "Deals "
    CHECK(t[1].kind == TOK_COLOR_PUSH && t[1].value == COLOR_RED);
    CHECK(t[2].kind == TOK_TEXT);                       // "20%"
    CHECK(t[3].kind == TOK_COLOR_POP);
    CHECK(t[4].kind == TOK_TEXT);                       // " more."
    CHECK(t[5].kind == TOK_NEWLINE);
    CHECK(t[6].kind == TOK_BULLET);                     // leading "- " on the new line
    CHECK(t[7].kind == TOK_TEXT);                       // "Note here"
    (void)n;
}

static void test_tokenize_stick_rotate() {
    Token t[8];
    size_t n = tokenize("{stick.rotate}{stick.left}", t, 8);
    CHECK(n == 2);
    CHECK(t[0].kind == TOK_GLYPH && t[0].value == GLYPH_STICK_ROTATE);
    CHECK(t[1].kind == TOK_GLYPH && t[1].value == GLYPH_STICK_LEFT);
}

static void test_encode_button() {
    Token t[8]; size_t n = tokenize("Hit {B}!", t, 8);
    uint8_t buf[64];
    size_t len = encodeMessage(t, n, buf, sizeof(buf));
    // "Hit " + (1A 05 00 00 0B) + "!"
    const uint8_t expect[] = {'H','i','t',' ', 0x1A,0x05,0x00,0x00,0x0B, '!'};
    CHECK(len == sizeof(expect));
    CHECK(memcmp(buf, expect, sizeof(expect)) == 0);
    CHECK(buf[len] == 0x00); // NUL terminator written
}
static void test_encode_color() {
    Token t[8]; size_t n = tokenize("{red}50{/}", t, 8);
    uint8_t buf[64];
    size_t len = encodeMessage(t, n, buf, sizeof(buf));
    // COLOR red (1A 06 FF 00 00 01) + "50" + COLOR default (1A 06 FF 00 00 00)
    const uint8_t expect[] = {0x1A,0x06,0xFF,0x00,0x00,0x01, '5','0', 0x1A,0x06,0xFF,0x00,0x00,0x00};
    CHECK(len == sizeof(expect));
    CHECK(memcmp(buf, expect, sizeof(expect)) == 0);
}
static void test_encode_bullet() {
    Token t[8]; size_t n = tokenize("- Item", t, 8);
    uint8_t buf[64];
    size_t len = encodeMessage(t, n, buf, sizeof(buf));
    const uint8_t expect[] = {0x1A,0x05,0x06,0x00,0x0A, 'I','t','e','m'};
    CHECK(len == sizeof(expect));
    CHECK(memcmp(buf, expect, sizeof(expect)) == 0);
}

static uint32_t beRead32(const uint8_t* p){ return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]; }
static void test_build_bmg() {
    Token t[8]; size_t n = tokenize("{B}", t, 8);
    uint8_t msg[32]; size_t mlen = encodeMessage(t, n, msg, sizeof(msg));   // 5 bytes + NUL
    uint8_t bmg[256]; size_t blen = buildBmg(msg, mlen, bmg, sizeof(bmg));
    CHECK(blen > 0);
    CHECK(memcmp(bmg, "MESGbmg1", 8) == 0);
    CHECK(beRead32(bmg + 0xC) == 2);                 // block count
    CHECK(bmg[0x10] == 1);                            // encoding
    CHECK(memcmp(bmg + 0x20, "INF1", 4) == 0);       // first block
    // INF1 size is at 0x20+4; DAT1 follows.
    uint32_t inf1Size = beRead32(bmg + 0x24);
    CHECK(memcmp(bmg + 0x20 + inf1Size, "DAT1", 4) == 0);
    CHECK(blen == beRead32(bmg + 0x8));              // header fileSize == total
}

int main() {
    test_tokenize_basic();
    test_tokenize_color_bullet_newline();
    test_tokenize_stick_rotate();
    test_encode_button();
    test_encode_color();
    test_encode_bullet();
    test_build_bmg();
    printf(g_fail ? "TESTS FAILED (%d)\n" : "ALL TESTS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
