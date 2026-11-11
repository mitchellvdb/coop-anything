#ifndef LR_DIAGNOSTICS_H
#define LR_DIAGNOSTICS_H
#include "lr_relay.h"
#define LR_DIAG_MAX (16u * 1024 * 1024)
#define LR_DIAG_TIMEOUT 120000u
enum { LR_DIAG_MANUAL = 1, LR_DIAG_DESYNC, LR_DIAG_RESYNC, LR_DIAG_FAILURE };
enum { LR_D_NONE, LR_D_WAITING, LR_D_RECEIVING, LR_D_OK, LR_D_MISSING, LR_D_BAD };
typedef struct {
    uint64_t peer;
    uint8_t *data;
    uint32_t size, got, crc;
    int status;
} lr_diag_slot;
typedef struct {
    void *ctx;
    lr_send_fn send;
    int (*collect)(void *, uint8_t **, size_t *); /* malloc; fixed log packet, no saves */
    int (*save)(void *, uint32_t report, unsigned player, const uint8_t *, size_t);
    void (*changed)(void *, int finished); /* bounded, no gameplay effects */
} lr_diag_config;
typedef struct {
    lr_diag_config cfg;
    int role, active, phase, reason, notice_pending;
    uint32_t kit, report, next_report, seen[8];
    unsigned seen_next;
    uint64_t host, peers[3], deadline, next_send, last_request;
    unsigned cursor, expected;
    lr_diag_slot slots[4];
    struct {
        uint64_t peer;
        uint32_t report, status;
    } receipts[12];
    unsigned next_receipt;
    uint8_t *tx;
    uint32_t tx_size, tx_offset, tx_crc;
} lr_diagnostics;
void lr_diag_init(lr_diagnostics *, const lr_diag_config *, int role, uint64_t host, uint32_t kit, uint32_t seed);
void lr_diag_free(lr_diagnostics *);
void lr_diag_players(lr_diagnostics *, unsigned players);
void lr_diag_peers(lr_diagnostics *, const uint64_t peers[3]);
int lr_diag_request(lr_diagnostics *, unsigned reason, uint64_t now);
int lr_diag_message(lr_diagnostics *, uint64_t peer, const lr_msg *, uint64_t now);
void lr_diag_tick(lr_diagnostics *, uint64_t now);
void lr_diag_left(lr_diagnostics *, uint64_t peer);
#endif
