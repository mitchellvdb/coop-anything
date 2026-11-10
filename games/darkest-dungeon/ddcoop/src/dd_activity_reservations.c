#include "dd_activity_reservations.h"
#include <string.h>

int dd_activity_reservations_valid(const dd_activity_reservations *s) {
    if(!s)return 0;
    for(unsigned a=0;a<DD_ACTIVITY_COUNT_;a++)for(unsigned i=0;i<DD_ACTIVITY_RES_SLOTS;i++) {
        const dd_activity_res_slot *v=&s->slots[a][i];
        if(!v->revision || v->phase>DD_ACTIVITY_RES_COMMITTED ||
           ((v->phase==DD_ACTIVITY_RES_EMPTY)!=(v->hero==0)))return 0;
        if(v->hero)for(unsigned b=0;b<=a;b++)for(unsigned j=0;j<DD_ACTIVITY_RES_SLOTS;j++) {
            if(b==a && j>=i)break;
            if(s->slots[b][j].hero==v->hero)return 0;
        }
    }
    return 1;
}
int dd_activity_reservations_seed(dd_activity_reservations *s,const uint32_t committed[DD_ACTIVITY_COUNT_][DD_ACTIVITY_RES_SLOTS]) {
    if(!s)return 0;
    memset(s,0,sizeof *s);
    for(unsigned a=0;a<DD_ACTIVITY_COUNT_;a++)for(unsigned i=0;i<DD_ACTIVITY_RES_SLOTS;i++) {
        dd_activity_res_slot *v=&s->slots[a][i];
        v->revision=1;v->hero=committed?committed[a][i]:0;
        v->phase=(uint8_t)(v->hero?DD_ACTIVITY_RES_COMMITTED:DD_ACTIVITY_RES_EMPTY);
    }
    if(dd_activity_reservations_valid(s))return 1;
    memset(s,0,sizeof *s);return 0;
}
dd_activity_res_result dd_activity_reservations_check(const dd_activity_reservations *s,const dd_activity_res_command *c) {
    if(!c || c->op<DD_ACTIVITY_RESERVE || c->op>DD_ACTIVITY_RELEASE ||
       c->activity>=DD_ACTIVITY_COUNT_ || c->slot>=DD_ACTIVITY_RES_SLOTS ||
       c->player>=DD_ACTIVITY_RES_PLAYERS || !c->hero || !c->revision ||
       !dd_activity_reservations_valid(s))return DD_ACTIVITY_RES_INVALID;
    const dd_activity_res_slot *v=&s->slots[c->activity][c->slot];
    if(v->revision!=c->revision)return DD_ACTIVITY_RES_STALE;
    if(v->phase==DD_ACTIVITY_RES_COMMITTING)return DD_ACTIVITY_RES_LOCKED;
    if(c->op==DD_ACTIVITY_RELEASE) {
        if(v->phase!=DD_ACTIVITY_RES_COMMITTED || v->hero!=c->hero)return DD_ACTIVITY_RES_STALE;
    } else if(v->phase==DD_ACTIVITY_RES_COMMITTED)return DD_ACTIVITY_RES_LOCKED;
    if(c->op==DD_ACTIVITY_RESERVE) {
        if(v->hero==c->hero)return DD_ACTIVITY_RES_UNCHANGED;
        if(v->phase!=DD_ACTIVITY_RES_EMPTY)return DD_ACTIVITY_RES_OCCUPIED;
        for(unsigned a=0;a<DD_ACTIVITY_COUNT_;a++)for(unsigned i=0;i<DD_ACTIVITY_RES_SLOTS;i++)
            if(s->slots[a][i].hero==c->hero)return DD_ACTIVITY_RES_HERO_BUSY;
    } else if(c->op!=DD_ACTIVITY_RELEASE && (v->phase!=DD_ACTIVITY_RES_DRAFT || v->hero!=c->hero))return DD_ACTIVITY_RES_STALE;
    if(v->revision==UINT32_MAX)return DD_ACTIVITY_RES_EXHAUSTED;
    return DD_ACTIVITY_RES_OK;
}
dd_activity_res_result dd_activity_reservations_apply(dd_activity_reservations *s,const dd_activity_res_command *c,
                                                     dd_activity_res_commit_fn commit,void *ctx) {
    dd_activity_res_result result=dd_activity_reservations_check(s,c);
    if(result!=DD_ACTIVITY_RES_OK)return result;
    dd_activity_res_slot *v=&s->slots[c->activity][c->slot];
    if(c->op==DD_ACTIVITY_RESERVE) {v->hero=c->hero;v->phase=DD_ACTIVITY_RES_DRAFT;}
    else if(c->op==DD_ACTIVITY_CLEAR) {v->hero=0;v->phase=DD_ACTIVITY_RES_EMPTY;}
    else {
        if(!commit)return DD_ACTIVITY_RES_NATIVE_REFUSED;
        uint8_t previous=v->phase;
        v->phase=DD_ACTIVITY_RES_COMMITTING;
        if(!commit(ctx,c->activity,c->slot,c->hero)) {
            v->phase=previous;
            return DD_ACTIVITY_RES_NATIVE_REFUSED;
        }
        if(c->op==DD_ACTIVITY_RELEASE) {v->hero=0;v->phase=DD_ACTIVITY_RES_EMPTY;}
        else v->phase=DD_ACTIVITY_RES_COMMITTED;
    }
    v->revision++;
    return DD_ACTIVITY_RES_OK;
}

int dd_activity_reservations_release_drafts(dd_activity_reservations *s) {
    if(!dd_activity_reservations_valid(s))return 0;
    for(unsigned a=0;a<DD_ACTIVITY_COUNT_;a++)for(unsigned i=0;i<DD_ACTIVITY_RES_SLOTS;i++) {
        const dd_activity_res_slot *v=&s->slots[a][i];
        if(v->phase==DD_ACTIVITY_RES_COMMITTING ||
           (v->phase!=DD_ACTIVITY_RES_COMMITTED && v->revision==UINT32_MAX))return 0;
    }
    for(unsigned a=0;a<DD_ACTIVITY_COUNT_;a++)for(unsigned i=0;i<DD_ACTIVITY_RES_SLOTS;i++) {
        dd_activity_res_slot *v=&s->slots[a][i];
        if(v->phase==DD_ACTIVITY_RES_COMMITTED)continue;
        v->hero=0;v->phase=DD_ACTIVITY_RES_EMPTY;v->revision++;
    }
    return 1;
}
