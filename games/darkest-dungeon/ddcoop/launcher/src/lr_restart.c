#include "lr_restart.h"
#include <string.h>
#define RESTART_TIMEOUT 180000u
static unsigned mask(const lr_restart *r) {
    unsigned out=0;for(unsigned i=0;i<3;i++)if(r->peers[i])out|=1u<<i;return out;
}
static int send_step(lr_restart *r,uint64_t peer,unsigned step) {
    lr_msg m={0};uint8_t bytes[LR_MAX_MSG];m.type=LR_RESTART;
    m.id=r->generation;m.a=step;m.b=r->kit;
    size_t n=lr_encode(&m,bytes);
    return n && r->send && r->send(r->ctx,peer,LR_CH_CONTROL,bytes,n)>0;
}
void lr_restart_init(lr_restart *r,int role,uint64_t host,uint32_t kit,lr_restart_send_fn send,void *ctx) {
    memset(r,0,sizeof *r);r->role=role;r->host=host;r->kit=kit;r->send=send;r->ctx=ctx;
}
int lr_restart_active(const lr_restart *r) {
    return r->phase==LR_R_CLOSING || r->phase==LR_R_WAITING || r->phase==LR_R_RESTARTING;
}
int lr_restart_begin(lr_restart *r,const uint64_t peers[3],uint32_t generation,uint64_t now) {
    if(r->role!=LR_ROLE_HOST || !r->kit || !generation || generation==r->generation || lr_restart_active(r))return 0;
    unsigned count=0;
    for(unsigned i=0;i<3;i++)if(peers[i]) {
        for(unsigned j=0;j<i;j++)if(peers[j]==peers[i])return 0;
        count++;
    }
    if(!count)return 0;
    memcpy(r->peers,peers,sizeof r->peers);r->generation=generation;
    r->phase=LR_R_CLOSING;r->close_issued=r->start_issued=r->go_enabled=r->local_ready=r->fail_issued=0;
    r->ready=r->joined=0;r->deadline=now+RESTART_TIMEOUT;r->next_send=0;return 1;
}
void lr_restart_abort(lr_restart *r) {
    if(!r->generation || r->phase==LR_R_IDLE || r->phase==LR_R_FAILED)return;
    r->phase=LR_R_FAILED;
    r->abort_pending=r->role==LR_ROLE_HOST?mask(r):r->host?1u:0u;
    r->next_send=0;
}
int lr_restart_message(lr_restart *r,uint64_t peer,const lr_msg *m,uint64_t now) {
    if(m->type!=LR_RESTART || !m->id || m->b!=r->kit || !r->kit || m->a<LR_R_BEGIN || m->a>LR_R_ABORT)return 0;
    if(r->role==LR_ROLE_HOST) {
        unsigned i=0;for(;i<3;i++)if(r->peers[i]==peer && peer)break;
        if(i==3 || m->id!=r->generation)return 0;
        if(m->a==LR_R_ABORT && (lr_restart_active(r) || r->phase==LR_R_DONE)){lr_restart_abort(r);return 1;}
        if(!lr_restart_active(r))return 0;
        if(m->a==LR_R_READY)r->ready|=1u<<i;
        else if(m->a==LR_R_JOINED && r->go_enabled)r->joined|=1u<<i;
        else if(m->a==LR_R_ABORT)lr_restart_abort(r);
        else return 0;
        return 1;
    }
    if(r->role!=LR_ROLE_GUEST || !peer || peer!=r->host)return 0;
    if(m->a==LR_R_BEGIN) {
        if(m->id==r->generation) {
            if(r->local_ready && lr_restart_active(r))send_step(r,r->host,LR_R_READY);
            return 1; /* A late duplicate cannot close a resumed game. */
        }
        if(lr_restart_active(r))return 0;
        r->generation=m->id;r->phase=LR_R_CLOSING;r->ready=r->joined=0;
        r->close_issued=r->start_issued=r->go_enabled=r->local_ready=r->fail_issued=0;
        r->deadline=now+RESTART_TIMEOUT;r->next_send=0;return 1;
    }
    if(m->id!=r->generation)return 0;
    if(m->a==LR_R_ABORT) {lr_restart_abort(r);return 1;}
    if(m->a!=LR_R_GO || !r->local_ready)return 0;
    if(r->phase==LR_R_DONE) {send_step(r,r->host,LR_R_JOINED);return 1;}
    if(r->phase==LR_R_WAITING) {r->phase=LR_R_RESTARTING;r->start_issued=0;return 1;}
    if(r->phase==LR_R_RESTARTING && r->go_enabled)send_step(r,r->host,LR_R_JOINED);
    return r->phase==LR_R_RESTARTING;
}
int lr_restart_tick(lr_restart *r,uint64_t now,int closed_and_recovered) {
    if(lr_restart_active(r) && now>r->deadline)lr_restart_abort(r);
    if(r->phase==LR_R_FAILED) {
        if(now>=r->next_send) {
            r->next_send=now+100;
            for(unsigned k=0;k<3;k++) {
                unsigned i=(r->cursor+k)%3;
                if((r->abort_pending&(1u<<i)) && send_step(r,r->role==LR_ROLE_HOST?r->peers[i]:r->host,LR_R_ABORT))r->abort_pending&=~(1u<<i);
            }
            r->cursor=(r->cursor+1)%3;
        }
        if(!r->fail_issued){r->fail_issued=1;return LR_R_FAIL;}return LR_R_NONE;
    }
    if(!lr_restart_active(r))return LR_R_NONE;
    if(!r->close_issued){r->close_issued=1;return LR_R_CLOSE;}
    if(closed_and_recovered && r->phase==LR_R_CLOSING) {
        r->local_ready=1;r->phase=LR_R_WAITING;r->next_send=0;
    }
    if(r->role==LR_ROLE_HOST && r->phase==LR_R_WAITING && closed_and_recovered && r->ready==mask(r)) {
        r->phase=LR_R_RESTARTING;r->start_issued=1;return LR_R_HOST_START;
    }
    if(r->role==LR_ROLE_GUEST && r->phase==LR_R_RESTARTING && !r->start_issued) {
        r->start_issued=1;return LR_R_GUEST_JOIN;
    }
    if(now<r->next_send)return LR_R_NONE;
    r->next_send=now+500;
    if(r->role==LR_ROLE_HOST) {
        for(unsigned k=0;k<3;k++) {
            unsigned i=(r->cursor+k)%3;
            if(!r->peers[i])continue;
            if(r->go_enabled) {if(!(r->joined&(1u<<i)))send_step(r,r->peers[i],LR_R_GO);}
            else if(!(r->ready&(1u<<i)))send_step(r,r->peers[i],LR_R_BEGIN);
        }
        r->cursor=(r->cursor+1)%3;
    } else if(r->local_ready && r->phase==LR_R_WAITING)send_step(r,r->host,LR_R_READY);
    else if(r->phase==LR_R_RESTARTING && r->go_enabled)send_step(r,r->host,LR_R_JOINED);
    return LR_R_NONE;
}
void lr_restart_started(lr_restart *r,int success) {
    if(r->phase!=LR_R_RESTARTING || !r->start_issued)return;
    if(!success){lr_restart_abort(r);return;}
    r->go_enabled=1;r->next_send=0;
    if(r->role==LR_ROLE_GUEST)send_step(r,r->host,LR_R_JOINED);
}
void lr_restart_complete(lr_restart *r) {
    if(r->phase==LR_R_RESTARTING && r->go_enabled &&
       (r->role!=LR_ROLE_HOST || r->joined==mask(r)))r->phase=LR_R_DONE;
}
