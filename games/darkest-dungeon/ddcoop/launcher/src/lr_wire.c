#include "lr_wire.h"

#include <string.h>

int lr_channel_of(lr_type t) {
    return (t == LR_STREAM_OPEN || t == LR_STREAM_DATA || t == LR_STREAM_CLOSE) ? LR_CH_STREAM : LR_CH_CONTROL;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int lr_password_ok(const char *p) {
    size_t n = 0;
    for (; p[n]; n++) {
        char c = p[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return 0;
    }
    return n == LR_PASSWORD_LEN;
}

int lr_file_name_ok(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || n > LR_NAME_MAX || name[0] == '.') return 0;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
              c == '-'))
            return 0;
    }
    return strstr(name, "..") == NULL;
}

uint32_t lr_crc32(uint32_t crc, const uint8_t *p, size_t len) {
    crc = ~crc;
    while (len--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

/* Layout after the 4-byte header, per type:
 *   OPEN, CLOSE      id
 *   DATA             id, bytes (1..LR_CHUNK)
 *   HELLO            role, build[, kit] (kit: first 4 bytes of the runtime's SHA-256; absent before 1.1.1)
 *   KEEPALIVE, END   (nothing)
 *   BYE              reason
 *   SESSION          slot, password (LR_PASSWORD_LEN bytes)
 *   SAVE_BEGIN       count, total, folder name length (1 byte), folder name ("profile_2")
 *   SAVE_FILE        index, size, crc, name length (1 byte), name
 *   SAVE_CHUNK       index, offset, bytes (1..LR_CHUNK)
 *   SAVE_RESULT      result
 *   ROSTER           revision, table (1..LR_CHUNK bytes; checked by lr_owners_decode)
 *   CLAIM            hero id, tick (0 or 1)
 *   MODS             count, entries (0..LR_CHUNK bytes; checked by lr_mods_decode)
 *   KIT_OFFER        serial, manifest (1..LR_CHUNK bytes; checked by lr_kit_parse)
 *   KIT_CHUNK        file index (0..2), offset, bytes (1..LR_CHUNK)
 *   KIT_END          (nothing)
 *   NAME             name length (1 byte, 0..LR_PLAYER_NAME_MAX), name
 *   PLAYERS          revision (not 0), body (lr_players_encode) */
size_t lr_encode(const lr_msg *m, uint8_t *out) {
    uint8_t *p = out + LR_HEADER;
    out[0] = 'D';
    out[1] = 'L';
    out[2] = LR_VERSION;
    out[3] = (uint8_t)m->type;
    switch (m->type) {
    case LR_STREAM_OPEN:
    case LR_STREAM_CLOSE:
        put32(p,m->id);put32(p+4,m->b);p+=8;
        break;
    case LR_STREAM_DATA:
    case LR_SAVE_CHUNK:
    case LR_ROSTER:
    case LR_KIT_OFFER:
    case LR_KIT_CHUNK:
    case LR_DIAG_CHUNK:
        if (m->type==LR_DIAG_CHUNK && !m->id) return 0;
        if (m->len == 0 || m->len > LR_CHUNK || !m->data) return 0;
        if ((m->type == LR_KIT_OFFER && !m->id) || (m->type == LR_KIT_CHUNK && m->id > 2)) return 0;
        put32(p, m->id);
        p += 4;
        if(m->type==LR_STREAM_DATA){put32(p,m->b);p+=4;}
        if (m->type == LR_SAVE_CHUNK || m->type == LR_KIT_CHUNK || m->type == LR_DIAG_CHUNK) {
            put32(p, m->a);
            p += 4;
        }
        memcpy(p, m->data, m->len);
        p += m->len;
        break;
    case LR_HELLO:
        put32(p, m->a);
        put32(p + 4, m->b);
        p += 8;
        if (m->id) { /* kit id; 0 = the 8-byte form of launchers 1.0.x */
            put32(p, m->id);
            p += 4;
        }
        break;
    case LR_SAVE_BEGIN: {
        size_t n = strlen(m->text);
        if (!lr_file_name_ok(m->text)) return 0;
        put32(p, m->a);
        put32(p + 4, m->b);
        p[8] = (uint8_t)n;
        memcpy(p + 9, m->text, n);
        p += 9 + n;
        break;
    }
    case LR_KEEPALIVE:
    case LR_SAVE_END:
    case LR_KIT_END:
        break;
    case LR_BYE:
    case LR_SAVE_RESULT:
        put32(p, m->a);
        p += 4;
        break;
    case LR_DIAG_REQUEST:
    case LR_DIAG_BEGIN:
        if(!m->id || (m->type==LR_DIAG_REQUEST ? (!m->b || m->a<1 || m->a>4) : (!m->a || m->a>16u*1024*1024)))return 0;
        put32(p,m->id);put32(p+4,m->a);put32(p+8,m->b);p+=12;break;
    case LR_DIAG_NOTICE:
        if(!m->b || m->a<1 || m->a>4)return 0;
        put32(p,m->a);put32(p+4,m->b);p+=8;break;
    case LR_DIAG_END:
    case LR_DIAG_ACK:
        if(!m->id || (m->type==LR_DIAG_ACK && m->a>1))return 0;
        put32(p,m->id);p+=4;if(m->type==LR_DIAG_ACK){put32(p,m->a);p+=4;}break;
    case LR_RESTART:
        if (!m->id || !m->b || m->a<1 || m->a>5) return 0;
        put32(p,m->id);put32(p+4,m->a);put32(p+8,m->b);p+=12;break;
    case LR_MODS:
        if (m->len > LR_CHUNK || (m->len && !m->data) || m->id > 32) return 0;
        put32(p, m->id);
        if (m->len) memcpy(p + 4, m->data, m->len);
        p += 4 + m->len;
        break;
    case LR_CLAIM:
        if (m->a > 1) return 0;
        put32(p, m->id);
        put32(p + 4, m->a);
        p += 8;
        break;
    case LR_SESSION:
        if (!lr_password_ok(m->text)) return 0;
        put32(p, m->a);
        memcpy(p + 4, m->text, LR_PASSWORD_LEN);
        p += 4 + LR_PASSWORD_LEN;
        break;
    case LR_NAME: {
        size_t n = strlen(m->text);
        if (!lr_player_name_ok(m->text)) return 0;
        p[0] = (uint8_t)n;
        memcpy(p + 1, m->text, n);
        p += 1 + n;
        break;
    }
    case LR_PLAYERS: {
        lr_players check;
        if (!m->id || !m->data || !lr_players_decode(m->data, m->len, &check)) return 0;
        put32(p, m->id);
        memcpy(p + 4, m->data, m->len);
        p += 4 + m->len;
        break;
    }
    case LR_SAVE_FILE: {
        size_t n = strlen(m->text);
        if (!lr_file_name_ok(m->text)) return 0;
        put32(p, m->id);
        put32(p + 4, m->a);
        put32(p + 8, m->b);
        p[12] = (uint8_t)n;
        memcpy(p + 13, m->text, n);
        p += 13 + n;
        break;
    }
    default:
        return 0;
    }
    return (size_t)(p - out);
}

int lr_decode(const uint8_t *buf, size_t len, int channel, lr_msg *out) {
    memset(out, 0, sizeof *out);
    if (len < LR_HEADER || len > LR_MAX_MSG || buf[0] != 'D' || buf[1] != 'L' || buf[2] != LR_VERSION) return 0;
    lr_type t = (lr_type)buf[3];
    const uint8_t *p = buf + LR_HEADER;
    size_t n = len - LR_HEADER;
    out->type = t;
    switch (t) {
    case LR_STREAM_OPEN:
    case LR_STREAM_CLOSE:
        if(n!=8)return 0;
        out->id=get32(p);out->b=get32(p+4);break;
    case LR_STREAM_DATA:
        if(n<9 || n>8+LR_CHUNK)return 0;
        out->id=get32(p);out->b=get32(p+4);out->data=p+8;out->len=n-8;break;
    case LR_ROSTER:
    case LR_KIT_OFFER:
        if (n < 5 || n > 4 + LR_CHUNK) return 0;
        out->id = get32(p);
        out->data = p + 4;
        out->len = n - 4;
        if (t == LR_KIT_OFFER && !out->id) return 0;
        break;
    case LR_SAVE_CHUNK:
    case LR_KIT_CHUNK:
    case LR_DIAG_CHUNK:
        if (n < 9 || n > 8 + LR_CHUNK) return 0;
        out->id = get32(p);
        out->a = get32(p + 4);
        out->data = p + 8;
        out->len = n - 8;
        if (t == LR_KIT_CHUNK && out->id > 2) return 0;
        if(t==LR_DIAG_CHUNK && !out->id)return 0;
        break;
    case LR_HELLO:
        if (n != 8 && n != 12) return 0;
        out->a = get32(p);
        out->b = get32(p + 4);
        if (n == 12 && (out->id = get32(p + 8)) == 0) return 0; /* kit 0 is only ever the short form */
        if (out->a != LR_ROLE_HOST && out->a != LR_ROLE_GUEST) return 0;
        break;
    case LR_SAVE_BEGIN: {
        if (n < 10) return 0;
        size_t name_len = p[8];
        if (name_len == 0 || name_len > LR_NAME_MAX || n != 9 + name_len) return 0;
        out->a = get32(p);
        out->b = get32(p + 4);
        memcpy(out->text, p + 9, name_len);
        out->text[name_len] = 0;
        if (memchr(p + 9, 0, name_len) || !lr_file_name_ok(out->text)) return 0;
        if (out->a == 0 || out->a > LR_MAX_FILES || out->b > LR_MAX_SAVE_TOTAL) return 0;
        break;
    }
    case LR_KEEPALIVE:
    case LR_SAVE_END:
    case LR_KIT_END:
        if (n != 0) return 0;
        break;
    case LR_BYE:
    case LR_SAVE_RESULT:
        if (n != 4) return 0;
        out->a = get32(p);
        break;
    case LR_DIAG_REQUEST:
    case LR_DIAG_BEGIN:
        if(n!=12)return 0;
        out->id=get32(p);out->a=get32(p+4);out->b=get32(p+8);
        if(!out->id || (t==LR_DIAG_REQUEST ? (!out->b || out->a<1 || out->a>4) : (!out->a || out->a>16u*1024*1024)))return 0;
        break;
    case LR_DIAG_NOTICE:
        if(n!=8)return 0;
        out->a=get32(p);out->b=get32(p+4);
        if(!out->b || out->a<1 || out->a>4)return 0;
        break;
    case LR_DIAG_END:
    case LR_DIAG_ACK:
        if(n!=(t==LR_DIAG_END?4u:8u))return 0;
        out->id=get32(p);
        if(t==LR_DIAG_ACK)out->a=get32(p+4);
        if(!out->id || out->a>1)return 0;
        break;
    case LR_RESTART:
        if(n!=12)return 0;
        out->id=get32(p);out->a=get32(p+4);out->b=get32(p+8);
        if(!out->id || !out->b || out->a<1 || out->a>5)return 0;
        break;
    case LR_MODS:
        if (n < 4 || n > 4 + LR_CHUNK) return 0;
        out->id = get32(p);
        if (out->id > 32) return 0;
        out->data = n > 4 ? p + 4 : NULL;
        out->len = n - 4;
        break;
    case LR_CLAIM:
        if (n != 8) return 0;
        out->id = get32(p);
        out->a = get32(p + 4);
        if (out->a > 1) return 0;
        break;
    case LR_SESSION:
        if (n != 4 + LR_PASSWORD_LEN) return 0;
        out->a = get32(p);
        memcpy(out->text, p + 4, LR_PASSWORD_LEN);
        out->text[LR_PASSWORD_LEN] = 0;
        /* a: the co-op slot (1..9) in the low byte, the guest's player (0 = 1, else 1..3) in the next. */
        if (!lr_password_ok(out->text) || (out->a & 0xffu) < 1 || (out->a & 0xffu) > 9 || (out->a >> 8) > 3) return 0;
        break;
    case LR_NAME: {
        if (n < 1 || n != 1u + p[0] || p[0] > LR_PLAYER_NAME_MAX || memchr(p + 1, 0, p[0])) return 0;
        memcpy(out->text, p + 1, p[0]);
        out->text[p[0]] = 0;
        if (!lr_player_name_ok(out->text)) return 0;
        break;
    }
    case LR_PLAYERS: {
        lr_players check;
        if (n < 5 || n > 4 + LR_PLAYERS_MAX_BODY) return 0;
        out->id = get32(p);
        out->data = p + 4;
        out->len = n - 4;
        if (!out->id || !lr_players_decode(out->data, out->len, &check)) return 0;
        break;
    }
    case LR_SAVE_FILE: {
        if (n < 14) return 0;
        size_t name_len = p[12];
        if (name_len == 0 || name_len > LR_NAME_MAX || n != 13 + name_len) return 0;
        out->id = get32(p);
        out->a = get32(p + 4);
        out->b = get32(p + 8);
        memcpy(out->text, p + 13, name_len);
        out->text[name_len] = 0;
        if (memchr(p + 13, 0, name_len) || !lr_file_name_ok(out->text)) return 0;
        if (out->id >= LR_MAX_FILES || out->a > LR_MAX_FILE_SIZE) return 0;
        break;
    }
    default:
        return 0;
    }
    return lr_channel_of(t) == channel;
}

/* Length of the UTF-8 character at s (1..4), 0 if it is not well formed (overlong, surrogate, > U+10FFFF). */
static size_t utf8_char(const unsigned char *s) {
    unsigned c = s[0];
    if (c < 0x80) return 1;
    size_t n = c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
    for (size_t i = 1; i < n; i++)
        if ((s[i] & 0xc0) != 0x80) return 0;
    if (n == 3 && ((c == 0xe0 && s[1] < 0xa0) || (c == 0xed && s[1] >= 0xa0))) return 0;
    if (n == 4 && ((c == 0xf0 && s[1] < 0x90) || (c == 0xf4 && s[1] >= 0x90))) return 0;
    return n;
}

int lr_player_name_ok(const char *name) {
    const unsigned char *s = (const unsigned char *)name;
    size_t len = strlen(name);
    if (len > LR_PLAYER_NAME_MAX) return 0;
    if (len && (s[0] == ' ' || s[len - 1] == ' ')) return 0;
    for (size_t i = 0; i < len;) {
        size_t k = utf8_char(s + i);
        if (!k || (k == 1 && (s[i] < 0x20 || s[i] == 0x7f))) return 0;
        if (k == 2 && s[i] == 0xc2 && s[i + 1] < 0xa0) return 0; /* C1 controls */
        i += k;
    }
    return 1;
}

void lr_player_name_clean(const char *in, char *out) {
    const unsigned char *s = (const unsigned char *)(in ? in : "");
    size_t o = 0;
    while (*s == ' ' || (*s && *s < 0x20)) s++;
    while (*s) {
        size_t k = utf8_char(s);
        if (!k) {
            s++;
            continue;
        }
        if (o + k > LR_PLAYER_NAME_MAX) break;
        if ((k == 1 && (*s < 0x20 || *s == 0x7f)) || (k == 2 && s[0] == 0xc2 && s[1] < 0xa0)) out[o++] = ' ';
        else {
            memcpy(out + o, s, k);
            o += k;
        }
        s += k;
    }
    while (o && out[o - 1] == ' ') o--;
    out[o] = 0;
}

size_t lr_players_encode(const lr_players *p, uint8_t *out, size_t cap) {
    size_t k = 1;
    if (p->total < 2 || p->total > 4 || cap < LR_PLAYERS_MAX_BODY) return 0;
    out[0] = (uint8_t)p->total;
    for (int i = 0; i < 4; i++) {
        size_t n = strlen(p->name[i]);
        if (p->present[i] > 1 || !lr_player_name_ok(p->name[i]) || (i >= p->total && (p->present[i] || n))) return 0;
        out[k] = p->present[i];
        out[k + 1] = (uint8_t)n;
        memcpy(out + k + 2, p->name[i], n);
        k += 2 + n;
    }
    return k;
}

int lr_players_decode(const uint8_t *buf, size_t len, lr_players *out) {
    lr_players r;
    size_t k = 1;
    memset(&r, 0, sizeof r);
    if (len < 9 || buf[0] < 2 || buf[0] > 4) return 0;
    r.total = buf[0];
    for (int i = 0; i < 4; i++) {
        if (k + 2 > len) return 0;
        size_t n = buf[k + 1];
        if (buf[k] > 1 || n > LR_PLAYER_NAME_MAX || k + 2 + n > len || memchr(buf + k + 2, 0, n)) return 0;
        r.present[i] = buf[k];
        memcpy(r.name[i], buf + k + 2, n);
        r.name[i][n] = 0;
        if (!lr_player_name_ok(r.name[i]) || (i >= r.total && (r.present[i] || n))) return 0;
        k += 2 + n;
    }
    if (k != len) return 0;
    *out = r;
    return 1;
}

const char *lr_bye_text(uint32_t reason) {
    switch (reason) {
    case LR_BYE_RESYNC: return "The host requested a resync. The games will close normally and guest backups recover. Then the host clicks Host and everyone joins again from the saved checkpoint.";
    case LR_BYE_QUIT: return "The other player closed the session.";
    case LR_BYE_FULL: return "The host already has a player (this version supports one friend at a time).";
    case LR_BYE_VERSION: return "Your DDCoop versions differ; install the same ZIP on both PCs.";
    case LR_BYE_BUILD: return "Your Darkest Dungeon versions differ; update the game through Steam.";
    case LR_BYE_SAVE: return "The campaign could not be transferred.";
    case LR_BYE_GAME_RUNNING: return "Close Darkest Dungeon first, then join again.";
    case LR_BYE_PROTOCOL: return "Unexpected message (different versions?)";
    case LR_BYE_RESTART: return "The host must close the game and click Host again first; then join the new session.";
    case LR_BYE_STARTED: return "The host's game has already loaded the campaign. Everyone has to join before that: ask the host to close the game, choose the number of players and click Host again.";
    case LR_BYE_MODS: return "Your friend is missing a mod this campaign uses, or has a different version of it (see their launcher).";
    case LR_BYE_DLC: return "Your friend is missing DLC required by this campaign (see their launcher).";
    default: return "unknown reason";
    }
}
