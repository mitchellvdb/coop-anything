/* Actual portable reservation model, modeled ordered links/native commit effects.
 * No game, UI, Steam, saves or real player data. Wire integration is tested by
 * the runtime fixture after an action format has been chosen. */
#include "dd_activity_reservations.h"
#include <stdio.h>
#include <string.h>
static unsigned failures;
#define CHECK(c) do {if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);failures++;}}while(0)
typedef struct {
    dd_activity_reservations state;
    unsigned effects[DD_ACTIVITY_COUNT_][DD_ACTIVITY_RES_SLOTS];
    unsigned releases[DD_ACTIVITY_COUNT_][DD_ACTIVITY_RES_SLOTS];
    uint8_t active[DD_ACTIVITY_COUNT_][DD_ACTIVITY_RES_SLOTS];
    unsigned calls,release_calls;
    int gold, refuse, reenter;
} world;
static dd_activity_res_command command(const world *w,unsigned op,unsigned a,unsigned slot,unsigned hero,unsigned player) {
    dd_activity_res_command c={0};c.op=(uint8_t)op;c.activity=(uint8_t)a;c.slot=(uint8_t)slot;
    c.hero=hero;c.player=(uint8_t)player;c.revision=w->state.slots[a][slot].revision;return c;
}
static int native_commit(void *ctx,unsigned a,unsigned slot,uint32_t hero) {
    world *w=ctx;w->calls++;
    CHECK(w->state.slots[a][slot].phase==DD_ACTIVITY_RES_COMMITTING);
    CHECK(w->state.slots[a][slot].hero==hero);
    if(w->reenter) {
        dd_activity_res_command c=command(w,DD_ACTIVITY_CLEAR,a,slot,hero,3);
        CHECK(dd_activity_reservations_apply(&w->state,&c,native_commit,w)==DD_ACTIVITY_RES_LOCKED);
        c.op=DD_ACTIVITY_ACCEPT;
        CHECK(dd_activity_reservations_apply(&w->state,&c,native_commit,w)==DD_ACTIVITY_RES_LOCKED);
        CHECK(!dd_activity_reservations_release_drafts(&w->state));
    }
    if(w->refuse || w->gold<100)return 0;
    CHECK(!w->active[a][slot]);w->active[a][slot]=1;w->effects[a][slot]++;w->gold-=100;return 1;
}
/* Modeled red-X effect, with an exact fake100-gold refund. Native refund
 * semantics are the adapter's responsibility, not claimed by this model test. */
static int native_release(void *ctx,unsigned a,unsigned slot,uint32_t hero) {
    world *w=ctx;w->release_calls++;
    CHECK(w->state.slots[a][slot].phase==DD_ACTIVITY_RES_COMMITTING && w->state.slots[a][slot].hero==hero);
    if(w->reenter) {
        dd_activity_res_command c=command(w,DD_ACTIVITY_RELEASE,a,slot,hero,3);
        CHECK(dd_activity_reservations_apply(&w->state,&c,native_release,w)==DD_ACTIVITY_RES_LOCKED);
        c.op=DD_ACTIVITY_ACCEPT;
        CHECK(dd_activity_reservations_apply(&w->state,&c,native_commit,w)==DD_ACTIVITY_RES_LOCKED);
        CHECK(!dd_activity_reservations_release_drafts(&w->state));
    }
    if(w->refuse)return 0;
    CHECK(w->active[a][slot]);w->active[a][slot]=0;w->releases[a][slot]++;w->gold+=100;return 1;
}
static dd_activity_res_result apply(world *w,const dd_activity_res_command *c) {
    dd_activity_res_result v=dd_activity_reservations_apply(&w->state,c,c->op==DD_ACTIVITY_RELEASE?native_release:native_commit,w);
    CHECK(dd_activity_reservations_valid(&w->state));return v;
}
static void init(world *w) {memset(w,0,sizeof *w);CHECK(dd_activity_reservations_seed(&w->state,NULL));w->gold=10000;}
static void direct_cases(void) {
    world w;init(&w);
    for(unsigned initiator=0;initiator<4;initiator++) {
        init(&w);dd_activity_res_command first=command(&w,DD_ACTIVITY_RESERVE,0,0,11,initiator),cross=first;
        cross.hero=22;cross.player=(uint8_t)((initiator+1)%4);
        CHECK(apply(&w,&first)==DD_ACTIVITY_RES_OK && !w.calls && w.gold==10000);
        CHECK(apply(&w,&cross)==DD_ACTIVITY_RES_STALE && w.state.slots[0][0].hero==11);
        cross.revision=w.state.slots[0][0].revision;
        CHECK(apply(&w,&cross)==DD_ACTIVITY_RES_OCCUPIED);
        dd_activity_res_command duplicate=command(&w,DD_ACTIVITY_RESERVE,0,0,11,(initiator+2)%4);
        CHECK(apply(&w,&duplicate)==DD_ACTIVITY_RES_UNCHANGED);
        dd_activity_res_command other=command(&w,DD_ACTIVITY_RESERVE,7,2,11,initiator);
        CHECK(apply(&w,&other)==DD_ACTIVITY_RES_HERO_BUSY);
        /* Any other player can accept an exact shared draft, even with creator idle. */
        dd_activity_res_command accept=command(&w,DD_ACTIVITY_ACCEPT,0,0,11,(initiator+1)%4);
        w.reenter=1;
        CHECK(apply(&w,&accept)==DD_ACTIVITY_RES_OK && w.calls==1 && w.effects[0][0]==1 && w.gold==9900);
        CHECK(apply(&w,&accept)==DD_ACTIVITY_RES_STALE && w.calls==1);
        accept.revision=w.state.slots[0][0].revision;
        CHECK(apply(&w,&accept)==DD_ACTIVITY_RES_LOCKED && w.calls==1);
        dd_activity_res_command wrong_clear=accept;wrong_clear.op=DD_ACTIVITY_CLEAR;
        CHECK(apply(&w,&wrong_clear)==DD_ACTIVITY_RES_LOCKED && w.state.slots[0][0].hero==11);
        other=command(&w,DD_ACTIVITY_RESERVE,1,1,11,initiator);
        CHECK(apply(&w,&other)==DD_ACTIVITY_RES_HERO_BUSY);
        /* Any player may release a confirmed placement. A refusal leaves it
         * committed; duplicate/stale red-X cannot remove/refund twice. */
        dd_activity_res_command release=command(&w,DD_ACTIVITY_RELEASE,0,0,11,(initiator+2)%4);
        dd_activity_reservations locked=w.state;
        w.refuse=1;
        CHECK(apply(&w,&release)==DD_ACTIVITY_RES_NATIVE_REFUSED && !memcmp(&locked,&w.state,sizeof locked));
        CHECK(w.gold==9900 && w.release_calls==1 && !w.releases[0][0]);
        w.refuse=0;
        CHECK(apply(&w,&release)==DD_ACTIVITY_RES_OK && w.release_calls==2 && w.releases[0][0]==1 && w.gold==10000);
        CHECK(!w.state.slots[0][0].hero && w.state.slots[0][0].phase==DD_ACTIVITY_RES_EMPTY);
        CHECK(apply(&w,&release)==DD_ACTIVITY_RES_STALE && w.release_calls==2 && w.gold==10000);
        release.revision=w.state.slots[0][0].revision;
        CHECK(apply(&w,&release)==DD_ACTIVITY_RES_STALE && w.release_calls==2);
        other=command(&w,DD_ACTIVITY_RESERVE,1,1,11,initiator);
        CHECK(apply(&w,&other)==DD_ACTIVITY_RES_OK); /* hero and slot are free again */
        release=command(&w,DD_ACTIVITY_RELEASE,1,1,11,initiator);
        CHECK(apply(&w,&release)==DD_ACTIVITY_RES_STALE && w.release_calls==2); /* draft uses CLEAR */
    }
    /* ABA: old clear/accept never affects a released-and-recreated reservation. */
    init(&w);dd_activity_res_command reserve=command(&w,DD_ACTIVITY_RESERVE,0,0,9,1);
    CHECK(apply(&w,&reserve)==DD_ACTIVITY_RES_OK);
    dd_activity_res_command clear=command(&w,DD_ACTIVITY_CLEAR,0,0,9,2),accept=clear;accept.op=DD_ACTIVITY_ACCEPT;
    CHECK(apply(&w,&clear)==DD_ACTIVITY_RES_OK);
    reserve=command(&w,DD_ACTIVITY_RESERVE,0,0,9,3);CHECK(apply(&w,&reserve)==DD_ACTIVITY_RES_OK);
    CHECK(apply(&w,&clear)==DD_ACTIVITY_RES_STALE && apply(&w,&accept)==DD_ACTIVITY_RES_STALE && !w.calls);
    accept=command(&w,DD_ACTIVITY_ACCEPT,0,0,9,0);dd_activity_reservations before=w.state;
    w.refuse=1;CHECK(apply(&w,&accept)==DD_ACTIVITY_RES_NATIVE_REFUSED && !memcmp(&before,&w.state,sizeof before) && w.gold==10000);
    w.refuse=0;CHECK(apply(&w,&accept)==DD_ACTIVITY_RES_OK && w.gold==9900 && w.effects[0][0]==1);
    /* Native committed baseline and malformed input. */
    uint32_t existing[DD_ACTIVITY_COUNT_][DD_ACTIVITY_RES_SLOTS]={{0}};existing[2][1]=30;
    CHECK(dd_activity_reservations_seed(&w.state,existing) && w.state.slots[2][1].phase==DD_ACTIVITY_RES_COMMITTED);
    existing[3][1]=30;CHECK(!dd_activity_reservations_seed(&w.state,existing) && !dd_activity_reservations_valid(&w.state));
    init(&w);reserve=command(&w,DD_ACTIVITY_RESERVE,0,0,1,0);
    const unsigned invalid_fields[]={0,1,2,3,4,5};
    for(unsigned k=0;k<sizeof invalid_fields/sizeof *invalid_fields;k++) {
        dd_activity_res_command bad=reserve;
        if(k==0)bad.op=0;if(k==1)bad.activity=DD_ACTIVITY_COUNT_;if(k==2)bad.slot=3;
        if(k==3)bad.player=4;if(k==4)bad.hero=0;if(k==5)bad.revision=0;
        CHECK(apply(&w,&bad)==DD_ACTIVITY_RES_INVALID);
    }
    w.state.slots[0][0].revision=UINT32_MAX;reserve.revision=UINT32_MAX;
    CHECK(apply(&w,&reserve)==DD_ACTIVITY_RES_EXHAUSTED);
    before=w.state;CHECK(!dd_activity_reservations_release_drafts(&w.state) && !memcmp(&before,&w.state,sizeof before));
    /* Shared navigation releases drafts but preserves committed treatment and
     * invalidates requests made against an empty old screen too. */
    init(&w);reserve=command(&w,DD_ACTIVITY_RESERVE,0,0,8,3);CHECK(apply(&w,&reserve)==DD_ACTIVITY_RES_OK);
    accept=command(&w,DD_ACTIVITY_ACCEPT,0,0,8,2);CHECK(apply(&w,&accept)==DD_ACTIVITY_RES_OK);
    reserve=command(&w,DD_ACTIVITY_RESERVE,1,2,9,1);CHECK(apply(&w,&reserve)==DD_ACTIVITY_RES_OK);
    clear=command(&w,DD_ACTIVITY_CLEAR,1,2,9,0);reserve=command(&w,DD_ACTIVITY_RESERVE,7,2,10,0);
    CHECK(dd_activity_reservations_release_drafts(&w.state));
    CHECK(w.state.slots[0][0].hero==8 && w.state.slots[0][0].phase==DD_ACTIVITY_RES_COMMITTED);
    CHECK(!w.state.slots[1][2].hero && apply(&w,&clear)==DD_ACTIVITY_RES_STALE && apply(&w,&reserve)==DD_ACTIVITY_RES_STALE);
}
#define CAP 512u
typedef struct {dd_activity_res_command commands[CAP];unsigned head,count;} fifo;
static fifo requests[4],updates[3];static world copies[4];
static uint32_t random_state=0x7152abcdu;
static uint32_t random_(void) {random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static unsigned accepted[3],seen[3],commits,refusals;
static void push(fifo *q,const dd_activity_res_command *c) {CHECK(q->count<CAP);if(q->count<CAP){q->commands[(q->head+q->count)%CAP]=*c;q->count++;}}
static int pop(fifo *q,dd_activity_res_command *c) {if(!q->count)return 0;*c=q->commands[q->head];q->head=(q->head+1)%CAP;q->count--;return 1;}
static void host_receive(const dd_activity_res_command *c) {
    dd_activity_res_result result=apply(&copies[0],c);
    if(result==DD_ACTIVITY_RES_OK) {commits++;for(unsigned i=0;i<3;i++){push(updates+i,c);accepted[i]++;}}
    else refusals++;
}
static int host_one(unsigned from) {dd_activity_res_command c;if(!pop(requests+from,&c))return 0;host_receive(&c);return 1;}
static int guest_one(unsigned who) {dd_activity_res_command c;if(!pop(updates+who,&c))return 0;CHECK(apply(&copies[who+1],&c)==DD_ACTIVITY_RES_OK);seen[who]++;return 1;}
static void random_cases(void) {
    unsigned issued=0;
    for(unsigned scenario=0;scenario<5000;scenario++) {
        memset(requests,0,sizeof requests);memset(updates,0,sizeof updates);memset(accepted,0,sizeof accepted);memset(seen,0,sizeof seen);
        for(unsigned p=0;p<4;p++)init(copies+p);
        /* UI openness is intentionally absent from canonical state. Lagging
         * clients issue valid-looking requests against their own old view. */
        for(unsigned tick=0;tick<128;tick++) {
            unsigned p=random_()%4,a=random_()%DD_ACTIVITY_COUNT_,slot=random_()%3;
            const dd_activity_res_slot *v=&copies[p].state.slots[a][slot];
            unsigned op=v->phase==DD_ACTIVITY_RES_DRAFT?(random_()%3?DD_ACTIVITY_CLEAR:DD_ACTIVITY_ACCEPT):
                v->phase==DD_ACTIVITY_RES_COMMITTED && random_()%2?DD_ACTIVITY_RELEASE:DD_ACTIVITY_RESERVE;
            unsigned hero=(v->phase==DD_ACTIVITY_RES_DRAFT || op==DD_ACTIVITY_RELEASE)?v->hero:1+random_()%64;
            dd_activity_res_command c=command(copies+p,op,a,slot,hero,p);issued++;
            if(p)push(requests+p,&c);else host_receive(&c);
            if(random_()%3)host_one(1+random_()%3);
            if(random_()%2)guest_one(random_()%3);
        }
        while(requests[1].count || requests[2].count || requests[3].count)host_one(1+random_()%3);
        while(updates[0].count || updates[1].count || updates[2].count)guest_one(random_()%3);
        for(unsigned p=1;p<4;p++) {
            CHECK(!memcmp(&copies[0].state,&copies[p].state,sizeof copies[0].state));
            CHECK(copies[0].gold==copies[p].gold && !memcmp(copies[0].effects,copies[p].effects,sizeof copies[0].effects));
            CHECK(accepted[p-1]==seen[p-1]);
            CHECK(!memcmp(copies[0].releases,copies[p].releases,sizeof copies[0].releases));
            CHECK(!memcmp(copies[0].active,copies[p].active,sizeof copies[0].active));
        }
    }
    printf("Reservations:5000 four-copy ordered-link simulations,%u concurrent/stale requests,%u accepted,%u refused; identical draft/locked slots,gold and one native effect per accepted commit/release\n",issued,commits,refusals);
}
int main(void) {direct_cases();random_cases();printf("Activity reservations: %s (%u failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;}
