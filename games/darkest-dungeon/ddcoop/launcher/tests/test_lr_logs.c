#include "lr_logs.h"
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do { if (!(x)) {printf("FAIL log redaction line %d\n", __LINE__); failures++;} } while (0)
int main(void) {
    uint8_t data[] = "normal turn=7\r\nSteam API AuthenticateUserTicket error: FAKE-SECRET\r\n"
                     "fault code=C0000005\naccess_token=FAKE-SECRET\nlast line";
    const char want[] = "normal turn=7\r\n[removed]\r\nfault code=C0000005\n[removed]\nlast line";
    unsigned count = 0; size_t n = lr_logs_redact(data, sizeof data - 1, &count);
    CHECK(count == 2 && n == sizeof want - 1 && !memcmp(data, want, n));
    const char *cases[] = {"authentication ticket FAKE", "AUTH_TICKET=FAKE", "AuthTicket=FAKE", "password=", "Authorization: Bearer FAKE", "refresh_token=FAKE", "EncryptedAppTicket FAKE"};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        uint8_t buffer[96]; size_t len = strlen(cases[i]); memset(buffer, 0xa5, sizeof buffer); memcpy(buffer, cases[i], len);
        n = lr_logs_redact(buffer, len, &count);
        CHECK(count == 1 && n == 9 && !memcmp(buffer, "[removed]", 9) && buffer[len] == 0xa5);
        CHECK(lr_logs_redact(buffer, n, &count) == n && !count);
    }
    uint8_t clean[] = "Invalid ticket\r\nhero=7 item=123 rng=456\n\xef\xbb\xbf UTF-8";
    uint8_t saved[sizeof clean]; memcpy(saved, clean, sizeof clean);
    CHECK(lr_logs_redact(clean, sizeof clean - 1, &count) == sizeof clean - 1 && !count && !memcmp(clean, saved, sizeof clean));
    CHECK(!lr_logs_redact(NULL, 0, &count) && !count);
    printf("Exported logs: ticket/token/password redaction, case/CRLF/final line, bounds, idempotence and ordinary diagnostics: %s (%d failures)\n", failures ? "FAILED" : "PASS", failures);
    return failures != 0;
}
