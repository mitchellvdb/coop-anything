#include "dd_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#include <windows.h>
#define CLOSESOCK closesocket
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#define CLOSESOCK close
#endif

int dd_net_startup(void) {
#ifdef _WIN32
    WSADATA w;
    return WSAStartup(MAKEWORD(2, 2), &w) == 0;
#else
    return 1;
#endif
}

void dd_net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

void dd_net_close(dd_socket s) {
    if (s != DD_NO_SOCKET) CLOSESOCK(s);
}

/* Waits until s is readable (want_write = 0) or writable; 1 = ready, 0 = timeout or error. */
static int wait_ready(dd_socket s, int want_write, int timeout_ms) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    int r = select((int)s + 1, want_write ? NULL : &set, want_write ? &set : NULL, NULL, &tv);
    return r > 0;
}

static void set_nodelay(dd_socket s) {
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one); /* small messages, no batching */
}

dd_socket dd_net_listen(int port, int bind_any) {
    dd_socket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == DD_NO_SOCKET) return DD_NO_SOCKET;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(bind_any ? INADDR_ANY : INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0 || listen(s, 4) != 0) {
        CLOSESOCK(s);
        return DD_NO_SOCKET;
    }
    return s;
}

dd_socket dd_net_accept(dd_socket listener, int timeout_ms) {
    if (!wait_ready(listener, 0, timeout_ms)) return DD_NO_SOCKET;
    dd_socket s = accept(listener, NULL, NULL);
    if (s != DD_NO_SOCKET) set_nodelay(s);
    return s;
}

dd_socket dd_net_connect(const char *host, int port, int timeout_ms) {
    struct addrinfo hints, *res = NULL;
    char portstr[8];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof portstr, "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) return DD_NO_SOCKET;
    dd_socket s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == DD_NO_SOCKET) {
        freeaddrinfo(res);
        return DD_NO_SOCKET;
    }
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
    int r = connect(s, res->ai_addr, (socklen_t)res->ai_addrlen);
    freeaddrinfo(res);
    int ok = r == 0;
    if (!ok && wait_ready(s, 1, timeout_ms)) {
        int err = 0;
        socklen_t len = sizeof err;
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
        ok = err == 0;
    }
#ifdef _WIN32
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) & ~O_NONBLOCK);
#endif
    if (!ok) {
        CLOSESOCK(s);
        return DD_NO_SOCKET;
    }
    set_nodelay(s);
    return s;
}

static int monotonic_ms(uint64_t *value) {
#ifdef _WIN32
    *value = GetTickCount64();
    return 1;
#else
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    *value = (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
    return 1;
#endif
}
/* Blocking send must have its own bound: a local relay can stop reading while
 * its Steam peer is paused. A STOP cannot overtake a send already in progress.
 * Recompute the remaining total budget before every chunk; a dribbling reader
 * must not extend this deadline indefinitely. */
static int send_budget(dd_socket s, uint32_t remaining) {
#ifdef _WIN32
    DWORD timeout = remaining;
#else
    struct timeval timeout = {(long)(remaining / 1000u), (long)(remaining % 1000u) * 1000};
#endif
    return setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout) == 0;
}
static int send_all(dd_socket s, const uint8_t *p, size_t len) {
    uint64_t started = 0, now = 0;
    if (!len) return 1;
    if (!p || !DD_NET_SEND_TIMEOUT_MS || !monotonic_ms(&started)) return 0;
    while (len) {
        if (!monotonic_ms(&now) || now - started >= DD_NET_SEND_TIMEOUT_MS ||
            !send_budget(s, (uint32_t)(DD_NET_SEND_TIMEOUT_MS - (now - started)))) return 0;
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL; /* Peer close is a transport error, not a process-wide SIGPIPE. */
#endif
        int n = (int)send(s, (const char *)p, (int)(len > 65536 ? 65536 : len), flags);
        if (n <= 0) return 0;
        p += n;
        len -= (size_t)n;
    }
    return monotonic_ms(&now) && now - started < DD_NET_SEND_TIMEOUT_MS;
}

/* Reads exactly len bytes within timeout_ms per read. */
static int recv_exact(dd_socket s, uint8_t *p, size_t len, int timeout_ms) {
    while (len) {
        if (!wait_ready(s, 0, timeout_ms)) return 0;
        int n = (int)recv(s, (char *)p, (int)len, 0);
        if (n <= 0) return 0;
        p += n;
        len -= (size_t)n;
    }
    return 1;
}

int dd_conn_init(dd_conn *c, dd_socket s) {
    memset(c, 0, sizeof *c);
    c->sock = s;
    c->rx = malloc(DD_RX_BUF);
    return c->rx != NULL && s != DD_NO_SOCKET;
}

void dd_conn_free(dd_conn *c) {
    dd_net_close(c->sock);
    c->sock = DD_NO_SOCKET;
    dd_sess_close(&c->sess);
    free(c->rx);
    c->rx = NULL;
}

int dd_conn_handshake_host(dd_conn *c, const char *password, int timeout_ms) {
    uint8_t hs1[DD_HS1_LEN], hs2[DD_HS2_LEN], hs3[DD_HS3_LEN];
    if (!dd_sess_host_start(&c->sess, password, hs1) || !send_all(c->sock, hs1, sizeof hs1) ||
        !recv_exact(c->sock, hs2, sizeof hs2, timeout_ms))
        return 0;
    int ok = dd_sess_host_finish(&c->sess, hs2, hs3);
    int sent = send_all(c->sock, hs3, sizeof hs3); /* tells a guest with the wrong password that it was refused */
    return ok && sent;
}

int dd_conn_handshake_guest(dd_conn *c, const char *password, int timeout_ms) {
    uint8_t hs1[DD_HS1_LEN], hs2[DD_HS2_LEN], hs3[DD_HS3_LEN];
    return recv_exact(c->sock, hs1, sizeof hs1, timeout_ms) && dd_sess_guest_answer(&c->sess, password, hs1, hs2) &&
           send_all(c->sock, hs2, sizeof hs2) && recv_exact(c->sock, hs3, sizeof hs3, timeout_ms) &&
           dd_sess_guest_finish(&c->sess, hs3);
}

int dd_conn_send(dd_conn *c, const dd_msg *m) {
    uint8_t plain[DD_PROTO_HEADER + DD_PROTO_MAX_BODY], frame[sizeof plain + DD_FRAME_OVERHEAD];
    size_t n = dd_encode(m, plain, sizeof plain);
    if (!n || c->failed) return 0;
    size_t f = dd_sess_seal(&c->sess, plain, n, frame, sizeof frame);
    if (f && send_all(c->sock, frame, f)) return 1;
    c->failed = 1; /* A partial frame/advanced nonce may never be retried on this stream. */
    return 0;
}

dd_poll_result dd_conn_poll(dd_conn *c, int timeout_ms, dd_msg *out) {
    for (;;) {
        if (c->failed) return DD_POLL_CLOSED;
        uint8_t plain[DD_PROTO_HEADER + DD_PROTO_MAX_BODY];
        size_t plen = 0, used = 0;
        dd_open_result r = dd_sess_open(&c->sess, c->rx, c->rx_len, plain, sizeof plain, &plen, &used);
        if (r == DD_OPEN_OK) {
            memmove(c->rx, c->rx + used, c->rx_len - used);
            c->rx_len -= used;
            size_t mused = 0;
            if (dd_decode(plain, plen, out, &mused) != DD_DECODE_OK || mused != plen) {
                c->failed = 1; /* authentic but malformed: the peer is broken or hostile */
                return DD_POLL_CLOSED;
            }
            return DD_POLL_MSG;
        }
        if (r != DD_OPEN_SHORT) {
            c->failed = 1;
            return DD_POLL_CLOSED;
        }
        if (!wait_ready(c->sock, 0, timeout_ms)) return DD_POLL_TIMEOUT;
        if (c->rx_len == DD_RX_BUF) {
            c->failed = 1;
            return DD_POLL_CLOSED;
        }
        int n = (int)recv(c->sock, (char *)c->rx + c->rx_len, (int)(DD_RX_BUF - c->rx_len), 0);
        if (n <= 0) {
            c->failed = 1;
            return DD_POLL_CLOSED;
        }
        c->rx_len += (size_t)n;
    }
}
