#include "lr_logs.h"
#include <string.h>
static unsigned lower(unsigned c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
static int secret_line(const uint8_t *s, size_t n) {
    static const char *markers[] = {"authentication ticket", "authenticateuserticket", "encryptedappticket",
        "auth ticket", "auth_ticket", "authticket", "auth_token", "access_token", "refresh_token",
        "authorization: bearer", "access token", "refresh token", "password="};
    for (size_t k = 0; k < sizeof markers / sizeof *markers; k++) {
        size_t m = strlen(markers[k]);
        for (size_t i = 0; m <= n && i <= n - m; i++) {
            if (lower(s[i]) != (unsigned char)markers[k][0]) continue;
            size_t j = 1;
            while (j < m && lower(s[i + j]) == (unsigned char)markers[k][j]) j++;
            if (j == m) return 1;
        }
    }
    return 0;
}
size_t lr_logs_redact(uint8_t *data, size_t len, unsigned *removed) {
    size_t out = 0, pos = 0;
    if (removed) *removed = 0;
    if (!data) return 0;
    while (pos < len) {
        size_t end = pos;
        while (end < len && data[end] != '\n') end++;
        size_t text_end = end;
        if (text_end > pos && data[text_end - 1] == '\r') text_end--;
        if (secret_line(data + pos, text_end - pos)) {
            /* Every marker has >=9 bytes: this replacement never expands. */
            memcpy(data + out, "[removed]", 9); out += 9;
            if (removed) (*removed)++;
            if (text_end < end) data[out++] = '\r';
        } else {
            memmove(data + out, data + pos, end - pos); out += end - pos;
        }
        if (end < len) data[out++] = '\n';
        pos = end + (end < len);
    }
    return out;
}
