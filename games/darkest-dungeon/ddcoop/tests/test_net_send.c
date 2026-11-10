/* Windows-only localhost send-budget regression; no Steam/game/user data.
 * The source is included so the actual private send_all path is exercised.
 * The shorter fixture budget keeps nonreading/dribbling cases quick. */
#define WIN32_LEAN_AND_MEAN
#define DD_NET_SEND_TIMEOUT_MS 1500u
#include "../src/dd_net.c"
static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
#define PAYLOAD (8u * 1024u * 1024u)
typedef struct {
    dd_socket socket;
    volatile LONG done;
    int mode, mismatch;
    size_t received;
} reader_context;
static DWORD WINAPI reader_main(void *ptr) {
    reader_context *c = ptr;
    uint64_t start = GetTickCount64();
    uint8_t buf[65536];
    while (!InterlockedCompareExchange(&c->done, 0, 0) && c->received < PAYLOAD && GetTickCount64() - start < DD_NET_SEND_TIMEOUT_MS * 3) {
        if (!c->mode) { Sleep(10); continue; }
        if (!wait_ready(c->socket, 0, 100)) continue;
        int n = recv(c->socket, (char *)buf, c->mode == 1 ? 1 : sizeof buf, 0);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) if (buf[i] != (uint8_t)((c->received + (size_t)i) % 251u)) c->mismatch = 1;
        c->received += (size_t)n;
        if (c->mode == 1) Sleep(20);
    }
    /* Watchdog makes the pre-fix unbounded send reproducible without hanging. */
    if (!InterlockedCompareExchange(&c->done, 0, 0) && c->received < PAYLOAD) shutdown(c->socket, SD_BOTH);
    return 0;
}
static int pair_open(dd_socket *writer, dd_socket *reader, int small) {
    dd_socket listener = dd_net_listen(0, 0);
    struct sockaddr_in addr = {0}; int n = sizeof addr;
    *writer = *reader = DD_NO_SOCKET;
    if (listener == DD_NO_SOCKET || getsockname(listener, (struct sockaddr *)&addr, &n)) {dd_net_close(listener);return 0;}
    *writer = dd_net_connect("127.0.0.1", ntohs(addr.sin_port), 1000);
    *reader = dd_net_accept(listener, 1000); dd_net_close(listener);
    if (*writer == DD_NO_SOCKET || *reader == DD_NO_SOCKET) return 0;
    if (small) {
        int bytes = 8192;
        if (setsockopt(*writer, SOL_SOCKET, SO_SNDBUF, (const char *)&bytes, sizeof bytes) ||
            setsockopt(*reader, SOL_SOCKET, SO_RCVBUF, (const char *)&bytes, sizeof bytes)) return 0;
    }
    return 1;
}
static void run_case(int mode, const uint8_t *payload) {
    dd_socket writer = DD_NO_SOCKET, reader = DD_NO_SOCKET;
    if (!pair_open(&writer, &reader, mode < 2)) {CHECK(0);dd_net_close(writer);dd_net_close(reader);return;}
    reader_context context = {0};context.socket = reader;context.mode = mode;
    HANDLE thread = CreateThread(NULL, 0, reader_main, &context, 0, NULL);
    if (!thread) {CHECK(0);dd_net_close(writer);dd_net_close(reader);return;}
    uint64_t start = GetTickCount64();
    int sent = send_all(writer, payload, PAYLOAD);
    uint64_t elapsed = GetTickCount64() - start;
    if (mode < 2 || !sent) InterlockedExchange(&context.done, 1);
    DWORD waited = WaitForSingleObject(thread, DD_NET_SEND_TIMEOUT_MS * 4);
    CHECK(waited == WAIT_OBJECT_0);
    if (waited != WAIT_OBJECT_0) {
        InterlockedExchange(&context.done, 1);shutdown(reader, SD_BOTH);
        CHECK(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0);
    }
    if (mode < 2) {
        CHECK(!sent);
        CHECK(elapsed >= DD_NET_SEND_TIMEOUT_MS - 300 && elapsed < DD_NET_SEND_TIMEOUT_MS + 1500);
        if (mode == 1) CHECK(context.received >= 10);
    } else CHECK(sent && context.received == PAYLOAD && !context.mismatch);
    printf("TCP send budget: mode=%s sent=%d elapsed=%llu ms read=%zu bytes\n",mode == 0 ? "nonreader" : mode == 1 ? "dribbling reader" : "fast reader",sent,(unsigned long long)elapsed,context.received);
    CloseHandle(thread);dd_net_close(writer);dd_net_close(reader);
}
static void encrypted_send_failure(void) {
    dd_socket writer = DD_NO_SOCKET, reader = DD_NO_SOCKET;
    if (!pair_open(&writer, &reader, 0)) {CHECK(0);dd_net_close(writer);dd_net_close(reader);return;}
    dd_conn h = {0}, g = {0};
    int hi = dd_conn_init(&h, writer), gi = dd_conn_init(&g, reader);
    if (!hi || !gi) {CHECK(0);dd_conn_free(&h);dd_conn_free(&g);return;}
    uint8_t hs1[DD_HS1_LEN], hs2[DD_HS2_LEN], hs3[DD_HS3_LEN];
    int open = dd_sess_host_start(&h.sess, "synthetic-test-password", hs1) &&
        dd_sess_guest_answer(&g.sess, "synthetic-test-password", hs1, hs2) &&
        dd_sess_host_finish(&h.sess, hs2, hs3) && dd_sess_guest_finish(&g.sess, hs3);
    CHECK(open);
    if (open) {
        dd_msg ping = {0}, out = {0};ping.type = DD_MSG_PING;ping.u.ping.time_ms = 1234;
        CHECK(dd_conn_send(&h, &ping));
        CHECK(dd_conn_poll(&g, 1000, &out) == DD_POLL_MSG && out.type == DD_MSG_PING && out.u.ping.time_ms == 1234);
        CHECK(shutdown(h.sock, SD_SEND) == 0);
        CHECK(!dd_conn_send(&h, &ping) && h.failed);
        uint64_t after = h.sess.counter_tx;
        CHECK(!dd_conn_send(&h, &ping) && h.sess.counter_tx == after);
        puts("TCP encrypted send: valid frame roundtrip; failed send is terminal, nonce cannot advance on retry");
    }
    dd_conn_free(&h);dd_conn_free(&g);
}
int main(void) {
    CHECK(dd_net_startup());
    uint8_t *payload = malloc(PAYLOAD); if (!payload) return 2;
    for (size_t i = 0; i < PAYLOAD; i++) payload[i] = (uint8_t)(i % 251u);
    run_case(0,payload);run_case(1,payload);run_case(2,payload);
    CHECK(send_all(DD_NO_SOCKET,NULL,0));CHECK(!send_all(DD_NO_SOCKET,NULL,1));
    encrypted_send_failure();
    free(payload);dd_net_cleanup();
    printf("TCP send deadline: %s (%d failures)\n",failures ? "FAILED" : "PASS",failures);
    return failures ? 1 : 0;
}
