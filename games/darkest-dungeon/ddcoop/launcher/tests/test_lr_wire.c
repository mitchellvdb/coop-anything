/* Launcher wire format: round trips, strict refusals, and 1,000,000 fuzzed inputs. */
#include "lr_wire.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                         \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static void round_trip(const lr_msg *m) {
    uint8_t buf[LR_MAX_MSG], again[LR_MAX_MSG];
    size_t n = lr_encode(m, buf);
    CHECK(n > 0);
    lr_msg d;
    CHECK(lr_decode(buf, n, lr_channel_of(m->type), &d));
    CHECK(d.type == m->type && d.id == m->id && d.a == m->a && d.b == m->b && d.len == m->len);
    CHECK(strcmp(d.text, m->text) == 0);
    if (m->len) CHECK(memcmp(d.data, m->data, m->len) == 0);
    CHECK(lr_encode(&d, again) == n && memcmp(buf, again, n) == 0);
    lr_msg scratch;
    CHECK(!lr_decode(buf, n, 1 - lr_channel_of(m->type), &scratch)); /* wrong channel */
    if (m->type != LR_STREAM_DATA && m->type != LR_SAVE_CHUNK && m->type != LR_ROSTER && m->type != LR_KIT_OFFER &&
        m->type != LR_KIT_CHUNK && (m->type != LR_MODS || !m->len)) /* variable length */
        CHECK(!lr_decode(buf, n - 1, lr_channel_of(m->type), &scratch));
}

int main(void) {
    uint8_t data[LR_CHUNK];
    for (size_t i = 0; i < sizeof data; i++) data[i] = (uint8_t)(i * 7);
    lr_msg m;

    memset(&m, 0, sizeof m); m.type = LR_STREAM_OPEN; m.id = 7; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_STREAM_CLOSE; m.id = 9; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_STREAM_DATA; m.id = 1; m.data = data; m.len = LR_CHUNK; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_STREAM_DATA; m.id = 1; m.data = data; m.len = 1; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_HELLO; m.a = LR_ROLE_GUEST; m.b = 0x4d78fbfa; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_HELLO; m.a = LR_ROLE_GUEST; m.b = 0x4d78fbfa; m.id = 0xaff98e68; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_KEEPALIVE; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_BYE; m.a = LR_BYE_FULL; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_SESSION; m.a = 3; strcpy(m.text, "abcdefghijkmnpqrstuvwxyzABCDEFGH"); round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_SAVE_BEGIN; m.a = 26; m.b = 123456; strcpy(m.text, "profile_2"); round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_SAVE_FILE; m.id = 3; m.a = 1000; m.b = 0xdeadbeef; strcpy(m.text, "persist.roster.json"); round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_SAVE_CHUNK; m.id = 3; m.a = 16000; m.data = data; m.len = 500; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_SAVE_END; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_SAVE_RESULT; m.a = 0; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_ROSTER; m.id = 12; m.data = data; m.len = 300; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_ROSTER; m.id = 1; m.data = data; m.len = LR_CHUNK; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_CLAIM; m.id = 16; m.a = 1; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_CLAIM; m.id = 16; m.a = 0; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_MODS; m.id = 0; round_trip(&m);                         /* no mods */
    memset(&m, 0, sizeof m); m.type = LR_MODS; m.id = 2; m.data = data; m.len = 400; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_MODS; m.id = 32; m.data = data; m.len = LR_CHUNK; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_BYE; m.a = LR_BYE_MODS; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_KIT_OFFER; m.id = 77; m.data = data; m.len = 600; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_KIT_CHUNK; m.id = 2; m.a = 32000; m.data = data; m.len = LR_CHUNK; round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_KIT_END; round_trip(&m);
    {
        uint8_t w[LR_MAX_MSG];
        lr_msg d;
        memset(&m, 0, sizeof m); m.type = LR_KIT_OFFER; m.id = 0; m.data = data; m.len = 10; CHECK(lr_encode(&m, w) == 0); /* serial 0 */
        m.id = 5; m.len = 0; CHECK(lr_encode(&m, w) == 0);
        memset(&m, 0, sizeof m); m.type = LR_KIT_CHUNK; m.id = 3; m.data = data; m.len = 10; CHECK(lr_encode(&m, w) == 0); /* file 3 */
        m.id = 2; size_t n = lr_encode(&m, w);
        w[4] = 3; CHECK(n && !lr_decode(w, n, LR_CH_CONTROL, &d));
        w[4] = 2; CHECK(lr_decode(w, n, LR_CH_CONTROL, &d) && !lr_decode(w, n, LR_CH_STREAM, &d));
    }
    CHECK(strstr(lr_bye_text(LR_BYE_MODS), "mod") != NULL);
    {
        uint8_t w[LR_MAX_MSG];
        lr_msg d;
        memset(&m, 0, sizeof m); m.type = LR_MODS; m.id = 33; CHECK(lr_encode(&m, w) == 0);  /* more than a campaign holds */
        m.id = 1; m.len = 1; m.data = NULL; CHECK(lr_encode(&m, w) == 0);
        m.data = data; m.len = LR_CHUNK + 1; CHECK(lr_encode(&m, w) == 0);
        memset(&m, 0, sizeof m); m.type = LR_MODS; m.id = 1; size_t n = lr_encode(&m, w);
        w[4] = 33; CHECK(n == 8 && !lr_decode(w, n, LR_CH_CONTROL, &d));                     /* count checked on decode */
        w[4] = 1; CHECK(lr_decode(w, n, LR_CH_CONTROL, &d) && d.id == 1 && d.len == 0 && !d.data);
        CHECK(!lr_decode(w, 7, LR_CH_CONTROL, &d) && !lr_decode(w, n, LR_CH_STREAM, &d));
    }

    /* refusals on the encode side */
    uint8_t buf[LR_MAX_MSG];
    memset(&m, 0, sizeof m); m.type = LR_STREAM_DATA; m.data = data; m.len = 0; CHECK(lr_encode(&m, buf) == 0);
    m.len = LR_CHUNK + 1; CHECK(lr_encode(&m, buf) == 0);
    memset(&m, 0, sizeof m); m.type = LR_SESSION; m.a = 3; strcpy(m.text, "short"); CHECK(lr_encode(&m, buf) == 0);
    strcpy(m.text, "abcdefghijkmnpqrstuvwxyzABCDEF;="); CHECK(lr_encode(&m, buf) == 0);
    memset(&m, 0, sizeof m); m.type = LR_SAVE_FILE; strcpy(m.text, "..\\evil"); CHECK(lr_encode(&m, buf) == 0);
    strcpy(m.text, "../evil"); CHECK(lr_encode(&m, buf) == 0);
    strcpy(m.text, "a/b"); CHECK(lr_encode(&m, buf) == 0);
    strcpy(m.text, ".hidden"); CHECK(lr_encode(&m, buf) == 0);
    strcpy(m.text, "C:x"); CHECK(lr_encode(&m, buf) == 0);
    memset(&m, 0, sizeof m); m.type = LR_ROSTER; m.data = data; m.len = 0; CHECK(lr_encode(&m, buf) == 0);
    m.len = LR_CHUNK + 1; CHECK(lr_encode(&m, buf) == 0);
    memset(&m, 0, sizeof m); m.type = LR_CLAIM; m.id = 1; m.a = 2; CHECK(lr_encode(&m, buf) == 0);
    memset(&m, 0, sizeof m); m.type = (lr_type)99; CHECK(lr_encode(&m, buf) == 0);

    /* refusals on the decode side: path tricks smuggled past an encoder */
    memset(&m, 0, sizeof m); m.type = LR_SAVE_FILE; m.id = 1; m.a = 5; strcpy(m.text, "abcdefg");
    size_t n = lr_encode(&m, buf);
    lr_msg d;
    memcpy(buf + LR_HEADER + 13, "..\\abcd", 7); CHECK(!lr_decode(buf, n, LR_CH_CONTROL, &d));
    memcpy(buf + LR_HEADER + 13, "ab\0defg", 7); CHECK(!lr_decode(buf, n, LR_CH_CONTROL, &d));
    memcpy(buf + LR_HEADER + 13, "abcdefg", 7); CHECK(lr_decode(buf, n, LR_CH_CONTROL, &d));
    buf[2] = LR_VERSION + 1; CHECK(!lr_decode(buf, n, LR_CH_CONTROL, &d));
    memset(&m, 0, sizeof m); m.type = LR_SAVE_FILE; m.id = 1; m.a = LR_MAX_FILE_SIZE; strcpy(m.text, "a");
    n = lr_encode(&m, buf); CHECK(lr_decode(buf, n, LR_CH_CONTROL, &d));
    buf[LR_HEADER + 4] = 1; buf[LR_HEADER + 7] = 0x7f; CHECK(!lr_decode(buf, n, LR_CH_CONTROL, &d)); /* too big */
    memset(&m, 0, sizeof m); m.type = LR_CLAIM; m.id = 16; m.a = 1;
    n = lr_encode(&m, buf); buf[LR_HEADER + 4] = 2; CHECK(!lr_decode(buf, n, LR_CH_CONTROL, &d)); /* tick 2 */
    memset(&m, 0, sizeof m); m.type = LR_SESSION; m.a = 3; strcpy(m.text, "abcdefghijkmnpqrstuvwxyzABCDEFGH");
    n = lr_encode(&m, buf); buf[LR_HEADER] = 10; CHECK(!lr_decode(buf, n, LR_CH_CONTROL, &d)); /* slot 10 */

    /* player names (launcher/NAMES.md) */
    CHECK(lr_player_name_ok("") && lr_player_name_ok("Robin") && lr_player_name_ok("J\xc3\xb6rg van D"));
    CHECK(!lr_player_name_ok(" Robin") && !lr_player_name_ok("Robin ") && !lr_player_name_ok("a\tb"));
    CHECK(!lr_player_name_ok("a\x7f") && !lr_player_name_ok("a\xc2\x85") && !lr_player_name_ok("\xc3"));
    CHECK(!lr_player_name_ok("\xc0\xaf") && !lr_player_name_ok("\xed\xa0\x80")); /* overlong, surrogate */
    CHECK(lr_player_name_ok("1234567890123456789012345678901") && !lr_player_name_ok("12345678901234567890123456789012"));
    {
        char clean[LR_PLAYER_NAME_MAX + 1];
        lr_player_name_clean("  Ro\tbin \xff ", clean); CHECK(!strcmp(clean, "Ro bin"));
        lr_player_name_clean(NULL, clean); CHECK(!strcmp(clean, ""));
        /* 15 two-byte characters = 30 bytes, the 16th does not fit: cut on a character boundary */
        lr_player_name_clean("\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9", clean);
        CHECK(strlen(clean) == 30 && lr_player_name_ok(clean));
    }
    memset(&m, 0, sizeof m); m.type = LR_NAME; strcpy(m.text, "Robin"); round_trip(&m);
    memset(&m, 0, sizeof m); m.type = LR_NAME; round_trip(&m); /* no name */
    memset(&m, 0, sizeof m); m.type = LR_NAME; strcpy(m.text, " x"); CHECK(lr_encode(&m, buf) == 0);
    {
        lr_players pl, back;
        uint8_t body[LR_PLAYERS_MAX_BODY];
        memset(&pl, 0, sizeof pl);
        pl.total = 3; pl.present[0] = pl.present[2] = 1;
        strcpy(pl.name[0], "Host"); strcpy(pl.name[2], "Bob");
        size_t bn = lr_players_encode(&pl, body, sizeof body);
        CHECK(bn == 1 + 4 * 2 + 4 + 3);
        CHECK(lr_players_decode(body, bn, &back) && !memcmp(&pl, &back, sizeof pl));
        memset(&m, 0, sizeof m); m.type = LR_PLAYERS; m.id = 5; m.data = body; m.len = bn; round_trip(&m);
        m.id = 0; CHECK(lr_encode(&m, buf) == 0);
        CHECK(!lr_players_decode(body, bn - 1, &back));
        body[0] = 5; CHECK(!lr_players_decode(body, bn, &back)); body[0] = 3; /* total 5 */
        body[9] = 2; CHECK(!lr_players_decode(body, bn, &back)); body[9] = 1; /* player 3 present = 2 */
        CHECK(lr_players_decode(body, bn, &back));
        pl.present[3] = 1; CHECK(lr_players_encode(&pl, body, sizeof body) == 0); /* player 4 in a 3-player session */
        pl.present[3] = 0; strcpy(pl.name[1], "x\n"); CHECK(lr_players_encode(&pl, body, sizeof body) == 0);
    }

    CHECK(lr_crc32(0, (const uint8_t *)"123456789", 9) == 0xCBF43926u);

    /* fuzz: anything accepted must re-encode to exactly the same bytes */
    srand(12345);
    uint8_t seed[LR_MAX_MSG];
    memset(&m, 0, sizeof m); m.type = LR_SAVE_FILE; m.id = 1; m.a = 5; strcpy(m.text, "persist.town.json");
    size_t seed_len = lr_encode(&m, seed);
    long accepted = 0;
    for (long i = 0; i < 1000000; i++) {
        uint8_t f[LR_MAX_MSG];
        size_t len;
        if (i % 2) {
            memcpy(f, seed, seed_len);
            len = seed_len;
            int flips = 1 + rand() % 4;
            for (int k = 0; k < flips; k++) f[rand() % len] = (uint8_t)rand();
            if (rand() % 8 == 0) len = (size_t)(rand() % (int)len);
        } else {
            len = (size_t)(rand() % 64);
            for (size_t k = 0; k < len; k++) f[k] = (uint8_t)rand();
            if (len >= 4) { f[0] = 'D'; f[1] = 'L'; f[2] = LR_VERSION; f[3] = (uint8_t)(rand() % 34); }
        }
        for (int ch = 0; ch < 2; ch++) {
            if (!lr_decode(f, len, ch, &d)) continue;
            accepted++;
            size_t back = lr_encode(&d, buf);
            if (back != len || memcmp(buf, f, len) != 0) {
                printf("FAIL fuzz input %ld re-encodes differently\n", i);
                failures++;
            }
        }
    }
    printf("fuzz: 1000000 inputs, %ld accepted, all re-encode exactly\n", accepted);
    printf(failures ? "test_lr_wire: %d FAILURES\n" : "test_lr_wire: all passed\n", failures);
    return failures != 0;
}
