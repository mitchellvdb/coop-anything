/* Portable coordinator for an explicit host-requested checkpoint restart.
 * It controls no processes/files itself. The UI performs its emitted actions. */
#ifndef LR_RESTART_H
#define LR_RESTART_H
#include "lr_wire.h"
typedef int (*lr_restart_send_fn)(void *,uint64_t,int,const uint8_t *,size_t);
enum { LR_R_IDLE,LR_R_CLOSING,LR_R_WAITING,LR_R_RESTARTING,LR_R_DONE,LR_R_FAILED };
enum { LR_R_BEGIN=1,LR_R_READY,LR_R_GO,LR_R_JOINED,LR_R_ABORT };
enum { LR_R_NONE,LR_R_CLOSE,LR_R_HOST_START,LR_R_GUEST_JOIN,LR_R_FAIL };
typedef struct {
    int role,phase,close_issued,start_issued,go_enabled,local_ready,fail_issued;
    uint32_t kit,generation;
    uint64_t host,peers[3],deadline,next_send;
    unsigned ready,joined,cursor,abort_pending;
    lr_restart_send_fn send;
    void *ctx;
} lr_restart;
void lr_restart_init(lr_restart *,int role,uint64_t host,uint32_t kit,lr_restart_send_fn,void *);
int lr_restart_begin(lr_restart *,const uint64_t peers[3],uint32_t generation,uint64_t now);
int lr_restart_message(lr_restart *,uint64_t peer,const lr_msg *,uint64_t now);
int lr_restart_tick(lr_restart *,uint64_t now,int closed_and_recovered);
void lr_restart_started(lr_restart *,int success);
void lr_restart_complete(lr_restart *); /* fresh game connected; host also waits for all joins */
void lr_restart_abort(lr_restart *);
int lr_restart_active(const lr_restart *);
#endif
