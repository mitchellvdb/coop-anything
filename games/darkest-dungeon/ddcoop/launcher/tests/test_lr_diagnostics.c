#include "lr_diagnostics.h"
#include "lr_logpack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                                     \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)
typedef struct {
    uint64_t from, to;
    size_t n;
    uint8_t data[LR_MAX_MSG];
} packet;
static packet queue[1024];
static unsigned head, tail, sends, pressure;
static int corrupt, fail_send, fail_collect, fail_save, block_acks;
typedef struct {
    lr_diagnostics d;
    unsigned id, collected, saved, finished, slots;
} node;
static node nodes[4];
static int send_(void *ctx, uint64_t to, int channel, const uint8_t *data, size_t n) {
    node *p = ctx;
    CHECK(channel == LR_CH_CONTROL);
    ++sends;
    if (block_acks && p->id == 0 && data[3] == LR_DIAG_ACK)
        return 0;
    if (fail_send)
        return -1;
    if (pressure && sends % pressure == 0)
        return 0;
    CHECK(tail < 1024);
    if (tail >= 1024)
        return -1;
    packet *m = &queue[tail++];
    m->from = p->id + 101;
    m->to = to;
    m->n = n;
    memcpy(m->data, data, n);
    if (corrupt && data[3] == LR_DIAG_CHUNK) {
        m->data[n - 1] ^= 1;
        corrupt = 0;
    }
    return 1;
}
static int collect_(void *ctx, uint8_t **out, size_t *n) {
    node *p = ctx;
    ++p->collected;
    if (fail_collect && p->id == 2)
        return 0;
    lr_logpack b = {0};
    char data[LR_CHUNK + 600];
    memset(data, 'a' + (int)p->id, sizeof data);
    memcpy(data, "synthetic log\n", 14);
    CHECK(lr_logpack_add(&b, 1, 0, sizeof data, (uint8_t *)data, sizeof data));
    *out = b.data;
    *n = b.len;
    return 1;
}
static int save_(void *ctx, uint32_t id, unsigned slot, const uint8_t *data, size_t n) {
    node *p = ctx;
    CHECK(p->id == 0 && id == p->d.report && slot < 4);
    CHECK(lr_logpack_visit(data, n, NULL, NULL));
    if (fail_save && slot == 2)
        return 0;
    ++p->saved;
    p->slots |= 1u << slot;
    return 1;
}
static void changed_(void *ctx, int finished) {
    node *p = ctx;
    if (finished)
        ++p->finished;
}
static void reset(void) {
    for (unsigned i = 0; i < 4; ++i) {
        lr_diag_free(&nodes[i].d);
        memset(&nodes[i], 0, sizeof nodes[i]);
        nodes[i].id = i;
        lr_diag_config c = {&nodes[i], send_, collect_, save_, changed_};
        lr_diag_init(&nodes[i].d, &c, i ? 2 : 1, i ? 101 : 0, 0x82828282, 99);
    }
    uint64_t peers[3] = {102, 103, 104};
    lr_diag_peers(&nodes[0].d, peers);
    head = tail = sends = pressure = 0;
    corrupt = fail_send = fail_collect = fail_save = block_acks = 0;
}
static void deliver(uint64_t now) {
    unsigned end = tail;
    while (head < end) {
        packet *p = &queue[head++];
        lr_msg m;
        CHECK(p->to >= 101 && p->to <= 104);
        CHECK(lr_decode(p->data, p->n, LR_CH_CONTROL, &m));
        lr_diag_message(&nodes[p->to - 101].d, p->from, &m, now);
    }
}
static void run_range(uint64_t from, uint64_t until) {
    for (uint64_t now = from; now < until; now += 25) {
        for (unsigned i = 0; i < 4; ++i)
            lr_diag_tick(&nodes[i].d, now);
        deliver(now);
    }
}
static void run(uint64_t until) {
    for (uint64_t now = 1; now < until; now += 25) {
        for (unsigned i = 0; i < 4; ++i)
            lr_diag_tick(&nodes[i].d, now);
        deliver(now);
    }
}
static void wire_checks(void) {
    uint8_t bytes[LR_MAX_MSG], data[8] = {0};
    lr_msg m = {0}, back;
    for (unsigned type = LR_DIAG_REQUEST; type <= LR_DIAG_NOTICE; ++type) {
        memset(&m, 0, sizeof m);
        m.type = (lr_type)type;
        m.id = 7;
        m.a = type == LR_DIAG_BEGIN ? 16u * 1024 * 1024 : type == LR_DIAG_CHUNK ? 0 : 1;
        m.b = 0x82828282;
        if (type == LR_DIAG_CHUNK) {
            m.data = data;
            m.len = sizeof data;
        }
        size_t n = lr_encode(&m, bytes);
        CHECK(n && lr_decode(bytes, n, LR_CH_CONTROL, &back));
        CHECK(!lr_decode(bytes, n, LR_CH_STREAM, &back));
        if (type != LR_DIAG_CHUNK) {
            for (size_t i = 0; i < n; ++i)
                CHECK(!lr_decode(bytes, i, LR_CH_CONTROL, &back));
            bytes[n] = 0;
            CHECK(!lr_decode(bytes, n + 1, LR_CH_CONTROL, &back));
        }
    }
    m.type = LR_DIAG_BEGIN;
    m.id = 7;
    m.a = LR_DIAG_MAX + 1;
    CHECK(!lr_encode(&m, bytes));
    m.a = 0;
    CHECK(!lr_encode(&m, bytes));
    m.a = LR_DIAG_MAX;
    CHECK(lr_encode(&m, bytes));
    m.type = LR_DIAG_ACK;
    m.a = 2;
    CHECK(!lr_encode(&m, bytes));
    m.type = LR_DIAG_REQUEST;
    m.a = 5;
    CHECK(!lr_encode(&m, bytes));
    m.type = LR_DIAG_CHUNK;
    m.data = NULL;
    m.len = 1;
    CHECK(!lr_encode(&m, bytes));
}
int main(void) {
    wire_checks();
    for (unsigned first = 0; first < 4; ++first) {
        reset();
        pressure = 3;
        CHECK(lr_diag_request(&nodes[first].d, LR_DIAG_DESYNC, 1));
        run(8000);
        CHECK(nodes[0].saved == 4 && nodes[0].slots == 15 && nodes[0].finished == 1 && !nodes[0].d.active);
        for (unsigned i = 1; i < 4; ++i)
            CHECK(nodes[i].collected == 1 && !nodes[i].d.active && nodes[i].d.phase == 6);
    }
    /* Delivery ACK backpressure can outlive a finished report. Starting the
     * next report must still acknowledge the old END, freeing that sender to
     * accept the current request without losing or relabelling either report. */
    reset();
    block_acks = 1;
    lr_diag_request(&nodes[0].d, LR_DIAG_MANUAL, 1);
    run(3000);
    CHECK(nodes[0].finished == 1 && nodes[1].d.active && nodes[1].d.phase == 4);
    lr_diag_request(&nodes[0].d, LR_DIAG_MANUAL, 4000);
    block_acks = 0;
    run_range(4000, 12000);
    CHECK(nodes[0].finished == 2 && nodes[0].saved == 8 && nodes[1].collected == 2 && !nodes[1].d.active);
    /* Simultaneous notices share one host incident and one local snapshot each. */
    reset();
    for (unsigned i = 0; i < 4; ++i)
        CHECK(lr_diag_request(&nodes[i].d, LR_DIAG_MANUAL, 1));
    run(2000);
    CHECK(nodes[0].saved == 4 && nodes[0].finished == 1 && nodes[0].collected == 1);
    lr_msg stale = {0};
    stale.type = LR_DIAG_REQUEST;
    stale.id = nodes[1].d.report;
    stale.a = 2;
    stale.b = nodes[1].d.kit;
    CHECK(lr_diag_message(&nodes[1].d, 101, &stale, 9000) && nodes[1].collected == 1);
    CHECK(!lr_diag_message(&nodes[1].d, 666, &stale, 9000));
    stale.b ^= 1;
    stale.id++;
    CHECK(!lr_diag_message(&nodes[1].d, 101, &stale, 9000));
    reset();
    corrupt = 1;
    lr_diag_request(&nodes[0].d, 2, 1);
    run(8000);
    CHECK(nodes[0].saved == 3 && nodes[0].finished == 1);
    unsigned bad = 0;
    for (unsigned i = 1; i < 4; ++i)
        bad += nodes[0].d.slots[i].status == LR_D_BAD;
    CHECK(bad == 1);
    reset();
    fail_collect = 1;
    lr_diag_request(&nodes[0].d, 2, 1);
    run(8000);
    CHECK(nodes[0].d.slots[2].status == LR_D_BAD && nodes[0].saved == 3 && nodes[0].finished == 1);
    reset();
    fail_save = 1;
    lr_diag_request(&nodes[0].d, 2, 1);
    run(8000);
    CHECK(nodes[0].d.slots[2].status == LR_D_BAD && nodes[0].saved == 3 && nodes[0].finished == 1);
    /* A player that left before collection is still expected and marked
     * missing, rather than quietly disappearing from the report. */
    reset();
    uint64_t fewer[3] = {102, 0, 104};
    lr_diag_peers(&nodes[0].d, fewer);
    lr_diag_request(&nodes[0].d, 2, 1);
    run(8000);
    CHECK(nodes[0].d.slots[2].status == LR_D_MISSING && nodes[0].saved == 3 && nodes[0].finished == 1);
    /* Disconnect and unknown/stale transfer never mark a player received. */
    reset();
    lr_diag_request(&nodes[0].d, 2, 1);
    lr_diag_left(&nodes[0].d, 103);
    run(8000);
    CHECK(nodes[0].d.slots[2].status == LR_D_MISSING && nodes[0].saved == 3);
    reset();
    lr_diag_request(&nodes[0].d, 2, 1);
    lr_msg m = {0};
    m.type = LR_DIAG_BEGIN;
    m.id = nodes[0].d.report;
    m.a = LR_DIAG_MAX + 1;
    CHECK(!lr_diag_message(&nodes[0].d, 102, &m, 2));
    m.a = 40;
    m.id--;
    CHECK(!lr_diag_message(&nodes[0].d, 102, &m, 2));
    m.id++;
    CHECK(!lr_diag_message(&nodes[0].d, 999, &m, 2));
    lr_diag_tick(&nodes[0].d, LR_DIAG_TIMEOUT + 2);
    CHECK(nodes[0].finished == 1 && nodes[0].saved == 1);
    for (unsigned i = 1; i < 4; ++i)
        CHECK(nodes[0].d.slots[i].status == LR_D_MISSING && !nodes[0].d.slots[i].data);
    /* A guest that only notified the host still has pending diagnostic
     * transport to drain if its game closes before the request arrives. */
    reset();
    lr_diag_request(&nodes[1].d, 2, 1);
    CHECK(nodes[1].d.notice_pending);
    lr_diag_left(&nodes[1].d, 101);
    CHECK(!nodes[1].d.notice_pending && !nodes[1].d.active && nodes[1].finished == 1);
    /* Exact byte offset required; partial/forged payload cannot reach the sink. */
    reset();
    lr_diag_request(&nodes[0].d, 2, 1);
    m.type = LR_DIAG_BEGIN;
    m.id = nodes[0].d.report;
    m.a = 40;
    m.b = 0;
    CHECK(lr_diag_message(&nodes[0].d, 102, &m, 2));
    uint8_t b[8] = {0};
    m.type = LR_DIAG_CHUNK;
    m.a = 1;
    m.data = b;
    m.len = 8;
    CHECK(lr_diag_message(&nodes[0].d, 102, &m, 3));
    CHECK(nodes[0].d.slots[1].status == LR_D_BAD && nodes[0].saved == 1);
    reset();
    fail_send = 1;
    lr_diag_request(&nodes[1].d, 2, 1);
    run(5000);
    lr_diag_tick(&nodes[1].d, LR_DIAG_TIMEOUT + 2);
    CHECK(!nodes[1].d.active && !nodes[1].d.notice_pending && nodes[1].finished == 1);
    for (unsigned i = 0; i < 4; ++i)
        lr_diag_free(&nodes[i].d);
    printf("Automatic diagnostics: all4 initiators/3 receivers, simultaneous/backpressure, CRC/offset/size/auth/stale, "
           "collection/disk errors, disconnect/timeout: %s (%d failures)\n",
           failures ? "FAILED" : "PASS", failures);
    return failures != 0;
}
