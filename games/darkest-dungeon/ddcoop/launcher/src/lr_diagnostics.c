#include "lr_diagnostics.h"
#include <stdlib.h>
#include <string.h>
static int emit_report(lr_diagnostics *d, uint64_t peer, uint32_t report, lr_type type, uint32_t a, uint32_t b,
                       const uint8_t *data, size_t size) {
    lr_msg m = {0};
    uint8_t bytes[LR_MAX_MSG];
    m.type = type;
    m.id = report;
    m.a = a;
    m.b = b;
    m.data = data;
    m.len = size;
    size_t n = lr_encode(&m, bytes);
    return n && peer && d->cfg.send ? d->cfg.send(d->cfg.ctx, peer, LR_CH_CONTROL, bytes, n) : -1;
}
static int emit(lr_diagnostics *d, uint64_t peer, lr_type type, uint32_t a, uint32_t b, const uint8_t *data,
                size_t size) {
    return emit_report(d, peer, d->report, type, a, b, data, size);
}
static void remember_receipt(lr_diagnostics *d, uint64_t peer, unsigned status) {
    unsigned i = 0;
    for (; i < 12; ++i)
        if (d->receipts[i].peer == peer && d->receipts[i].report == d->report)
            break;
    if (i == 12)
        i = d->next_receipt++ % 12;
    d->receipts[i].peer = peer;
    d->receipts[i].report = d->report;
    d->receipts[i].status = status;
}
static void changed(lr_diagnostics *d, int done) {
    if (d->cfg.changed)
        d->cfg.changed(d->cfg.ctx, done);
}
void lr_diag_free(lr_diagnostics *d) {
    for (unsigned i = 0; i < 4; ++i) {
        free(d->slots[i].data);
        d->slots[i].data = NULL;
    }
    free(d->tx);
    d->tx = NULL;
    d->active = d->notice_pending = 0;
}
void lr_diag_init(lr_diagnostics *d, const lr_diag_config *cfg, int role, uint64_t host, uint32_t kit, uint32_t seed) {
    memset(d, 0, sizeof *d);
    d->cfg = *cfg;
    d->role = role;
    d->host = host;
    d->kit = kit;
    d->next_report = seed ? seed : 1;
    d->expected = 1;
}
void lr_diag_players(lr_diagnostics *d, unsigned players) {
    if (players >= 1 && players <= 4)
        d->expected = (1u << players) - 1;
}
void lr_diag_peers(lr_diagnostics *d, const uint64_t peers[3]) {
    for (unsigned i = 0; i < 3; ++i) {
        d->peers[i] = peers[i];
        if (peers[i])
            d->expected |= 1u << (i + 1);
        for (unsigned j = 0; j < i; ++j)
            if (d->peers[j] == peers[i])
                d->peers[i] = 0;
    }
}
static void finish(lr_diagnostics *d) {
    for (unsigned i = 1; i < 4; ++i)
        if (d->slots[i].status == LR_D_WAITING || d->slots[i].status == LR_D_RECEIVING)
            return;
    d->active = 0;
    changed(d, 1);
}
static int begin(lr_diagnostics *d, unsigned reason, uint64_t now) {
    lr_diag_free(d);
    memset(d->slots, 0, sizeof d->slots);
    if (++d->next_report == 0)
        ++d->next_report;
    d->report = d->next_report;
    d->active = 1;
    d->reason = (int)reason;
    d->last_request = now;
    d->deadline = now + LR_DIAG_TIMEOUT;
    d->next_send = now;
    d->cursor = 0;
    for (unsigned i = 0; i < 3; ++i)
        if (d->peers[i]) {
            d->slots[i + 1].peer = d->peers[i];
            d->slots[i + 1].status = LR_D_WAITING;
        }
    for (unsigned i = 1; i < 4; ++i)
        if ((d->expected & (1u << i)) && !d->slots[i].peer)
            d->slots[i].status = LR_D_MISSING;
    changed(d, 0); /* create output folder before the local snapshot */
    uint8_t *data = NULL;
    size_t n = 0;
    int ok = d->cfg.collect && d->cfg.collect(d->cfg.ctx, &data, &n) && data && n && n <= LR_DIAG_MAX;
    if (ok)
        ok = d->cfg.save && d->cfg.save(d->cfg.ctx, d->report, 0, data, n);
    free(data);
    d->slots[0].status = ok ? LR_D_OK : LR_D_BAD;
    changed(d, 0);
    finish(d);
    return 1;
}
int lr_diag_request(lr_diagnostics *d, unsigned reason, uint64_t now) {
    if (!d->kit || reason < 1 || reason > 4 || !d->cfg.send)
        return 0;
    if (d->active || d->notice_pending)
        return 1;
    if (d->last_request && now - d->last_request < 3000)
        return 1;
    if (d->role == LR_ROLE_HOST)
        return begin(d, reason, now);
    if (d->role != LR_ROLE_GUEST || !d->host)
        return 0;
    d->reason = (int)reason;
    d->notice_pending = 1;
    d->deadline = now + LR_DIAG_TIMEOUT;
    d->next_send = now;
    d->last_request = now;
    return 1;
}
int lr_diag_message(lr_diagnostics *d, uint64_t peer, const lr_msg *m, uint64_t now) {
    if (!peer || m->type < LR_DIAG_REQUEST || m->type > LR_DIAG_NOTICE)
        return 0;
    if (d->role == LR_ROLE_HOST) {
        unsigned slot = 1;
        if (m->type == LR_DIAG_NOTICE) {
            for (; slot < 4; ++slot)
                if (d->peers[slot - 1] == peer)
                    break;
            if (slot == 4 || m->b != d->kit)
                return 0;
            return lr_diag_request(d, m->a, now);
        }
        if (m->type == LR_DIAG_END && m->id != d->report) {
            for (unsigned i = 0; i < 12; ++i)
                if (d->receipts[i].peer == peer && d->receipts[i].report == m->id) {
                    emit_report(d, peer, m->id, LR_DIAG_ACK, d->receipts[i].status, 0, NULL, 0);
                    return 1;
                }
            return 0;
        }
        for (; slot < 4; ++slot)
            if (d->slots[slot].peer == peer)
                break;
        if (slot == 4 || m->id != d->report)
            return 0;
        lr_diag_slot *r = &d->slots[slot];
        if (m->type == LR_DIAG_END && (r->status == LR_D_OK || r->status == LR_D_BAD)) {
            emit(d, peer, LR_DIAG_ACK, r->status == LR_D_OK ? 0 : 1, 0, NULL, 0);
            return 1;
        }
        if (!d->active)
            return 0;
        if (m->type == LR_DIAG_BEGIN) {
            if (r->status == LR_D_RECEIVING && r->size == m->a && r->crc == m->b)
                return 1;
            if (r->status != LR_D_WAITING || !m->a || m->a > LR_DIAG_MAX)
                return 0;
            r->data = malloc(m->a);
            r->size = m->a;
            r->crc = m->b;
            r->got = 0;
            r->status = r->data ? LR_D_RECEIVING : LR_D_BAD;
        } else if (m->type == LR_DIAG_CHUNK) {
            if (r->status != LR_D_RECEIVING || !m->data || !m->len || m->len > LR_CHUNK)
                return 0;
            if (m->a < r->got && m->len <= r->got - m->a && !memcmp(r->data + m->a, m->data, m->len))
                return 1;
            if (m->a != r->got || m->len > r->size - r->got)
                r->status = LR_D_BAD;
            else {
                memcpy(r->data + r->got, m->data, m->len);
                r->got += (uint32_t)m->len;
                return 1;
            }
        } else if (m->type == LR_DIAG_END) {
            if (r->status != LR_D_RECEIVING)
                return 0;
            int ok = r->got == r->size && lr_crc32(0, r->data, r->size) == r->crc;
            if (ok)
                ok = d->cfg.save && d->cfg.save(d->cfg.ctx, d->report, slot, r->data, r->size);
            r->status = ok ? LR_D_OK : LR_D_BAD;
            emit(d, peer, LR_DIAG_ACK, ok ? 0 : 1, 0, NULL, 0);
        } else if (m->type == LR_DIAG_ACK && m->a == 1 && r->status == LR_D_WAITING)
            r->status = LR_D_BAD;
        else
            return 0;
        if (r->status == LR_D_BAD || r->status == LR_D_OK) {
            remember_receipt(d, peer, r->status == LR_D_OK ? 0 : 1);
            if (r->status == LR_D_BAD && m->type != LR_DIAG_END)
                emit(d, peer, LR_DIAG_ACK, 1, 0, NULL, 0);
            free(r->data);
            r->data = NULL;
        }
        changed(d, 0);
        finish(d);
        return 1;
    }
    if (d->role != LR_ROLE_GUEST || peer != d->host)
        return 0;
    if (m->type == LR_DIAG_REQUEST) {
        if (m->b != d->kit || !m->id || m->a < 1 || m->a > 4)
            return 0;
        for (unsigned i = 0; i < 8; ++i)
            if (m->id == d->seen[i])
                return 1;
        if (d->active)
            return 0;
        free(d->tx);
        d->tx = NULL;
        d->tx_size = d->tx_offset = 0;
        d->seen[d->seen_next++ % 8] = m->id;
        d->report = m->id;
        d->reason = (int)m->a;
        d->notice_pending = 0;
        d->active = 1;
        d->deadline = now + LR_DIAG_TIMEOUT;
        d->next_send = now;
        size_t n = 0;
        int ok = d->cfg.collect && d->cfg.collect(d->cfg.ctx, &d->tx, &n) && d->tx && n && n <= LR_DIAG_MAX;
        if (ok) {
            d->tx_size = (uint32_t)n;
            d->tx_crc = lr_crc32(0, d->tx, n);
            d->phase = 1;
        } else {
            free(d->tx);
            d->tx = NULL;
            d->phase = 5;
        }
        changed(d, 0);
        return 1;
    }
    if (m->type == LR_DIAG_ACK && m->id == d->report && d->active && (d->phase == 4 || m->a == 1)) {
        d->active = 0;
        d->phase = m->a ? 5 : 6;
        free(d->tx);
        d->tx = NULL;
        changed(d, 1);
        return 1;
    }
    return 0;
}
void lr_diag_tick(lr_diagnostics *d, uint64_t now) {
    if ((d->active || d->notice_pending) && now > d->deadline) {
        d->notice_pending = 0;
        if (d->role == LR_ROLE_HOST) {
            for (unsigned i = 1; i < 4; ++i)
                if (d->slots[i].status == LR_D_WAITING || d->slots[i].status == LR_D_RECEIVING) {
                    d->slots[i].status = LR_D_MISSING;
                    free(d->slots[i].data);
                    d->slots[i].data = NULL;
                }
            finish(d);
        } else {
            d->active = 0;
            d->phase = 5;
            free(d->tx);
            d->tx = NULL;
            changed(d, 1);
        }
        return;
    }
    if (now < d->next_send)
        return;
    d->next_send = now + 100; /* at most one16KB chunk/100ms per guest */
    if (d->role == LR_ROLE_HOST && d->active) {
        for (unsigned n = 0; n < 3; ++n) {
            unsigned i = (d->cursor++ % 3) + 1;
            if (d->slots[i].status == LR_D_WAITING) {
                emit(d, d->slots[i].peer, LR_DIAG_REQUEST, (uint32_t)d->reason, d->kit, NULL, 0);
                break;
            }
        }
    } else if (d->role == LR_ROLE_GUEST) {
        if (d->notice_pending) {
            emit(d, d->host, LR_DIAG_NOTICE, (uint32_t)d->reason, d->kit, NULL, 0);
            d->next_send = now + 1000;
            return;
        }
        if (!d->active)
            return;
        int sent = 0;
        if (d->phase == 1) {
            sent = emit(d, d->host, LR_DIAG_BEGIN, d->tx_size, d->tx_crc, NULL, 0);
            if (sent > 0)
                d->phase = 2;
        } else if (d->phase == 2) {
            uint32_t n = d->tx_size - d->tx_offset;
            if (n > LR_CHUNK)
                n = LR_CHUNK;
            sent = emit(d, d->host, LR_DIAG_CHUNK, d->tx_offset, 0, d->tx + d->tx_offset, n);
            if (sent > 0) {
                d->tx_offset += n;
                if (d->tx_offset == d->tx_size)
                    d->phase = 3;
            }
        } else if (d->phase == 3 || d->phase == 4) {
            sent = emit(d, d->host, LR_DIAG_END, 0, 0, NULL, 0);
            if (sent > 0) {
                d->phase = 4;
                d->next_send = now + 1000;
            }
        } else if (d->phase == 5) {
            sent = emit(d, d->host, LR_DIAG_ACK, 1, 0, NULL, 0);
            if (sent > 0) {
                d->active = 0;
                changed(d, 1);
            }
        }
        if (sent < 0) {
            d->active = 0;
            d->phase = 5;
            free(d->tx);
            d->tx = NULL;
            changed(d, 1);
        }
    }
}
void lr_diag_left(lr_diagnostics *d, uint64_t peer) {
    if (!peer || (!d->active && !d->notice_pending))
        return;
    if (d->role == LR_ROLE_HOST) {
        for (unsigned i = 1; i < 4; ++i)
            if (d->slots[i].peer == peer &&
                (d->slots[i].status == LR_D_WAITING || d->slots[i].status == LR_D_RECEIVING)) {
                d->slots[i].status = LR_D_MISSING;
                free(d->slots[i].data);
                d->slots[i].data = NULL;
            }
        changed(d, 0);
        finish(d);
    } else if (peer == d->host) {
        free(d->tx);
        d->tx = NULL;
        d->active = d->notice_pending = 0;
        d->phase = 5;
        changed(d, 1);
    }
}
/* Review of test82 (12 Nov): every session end made a report, because a player who leaves makes the other
 * runtimes log "Sync stop: reason=1" right after "... disconnected" (and "shared actions stopped after live
 * session loss"). Reason 1 also marks a real failure (an order that could not be applied), so look at the line
 * before the first stop: a disconnect there is an ordinary end; anything else, or reason 2/3, is a real stop. */
int lr_diag_stop_marker(const char *text) {
    if (!text) return 0;
    if (strstr(text, "this copy needs a resync") || strstr(text, "Sync stop: reason=2") ||
        strstr(text, "Sync stop: reason=3"))
        return 1;
    const char *stop = strstr(text, "Sync stop: reason=");
    if (!stop) return 0;
    const char *start = stop; /* start of the stop's line */
    while (start > text && start[-1] != '\n') --start;
    if (start == text) return 1; /* no line before it in this chunk */
    const char *line = start - 1; /* the newline that ends the previous line */
    while (line > text && line[-1] != '\n') --line;
    for (const char *p = line; p + 12 <= start - 1; ++p)
        if (!strncmp(p, "disconnected", 12)) return 0;
    return 1;
}
