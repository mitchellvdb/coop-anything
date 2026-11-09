/* No Steam/game/files: four launcher restart coordinators, delayed native close
 * and guest recovery, send backpressure, dropped retries and hostile messages. */
#include "lr_restart.h"
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){printf("FAIL restart:%d %s\n",__LINE__,#x);failures++;}}while(0)
typedef struct {lr_restart restart;unsigned id,closes,starts;uint64_t ready_at;} node;
typedef struct {unsigned from,to;lr_msg m;} packet;
static node nodes[4];static packet queue[1024];static unsigned used,calls,dropped_go;
static int send_packet(void *ctx,uint64_t peer,int channel,const uint8_t *data,size_t len) {
    node *n=ctx;lr_msg m;
    CHECK(lr_decode(data,len,channel,&m) && peer>=1 && peer<=4 && m.type==LR_RESTART);
    if(++calls%3==0)return 0;
    if(m.a==LR_R_GO && !dropped_go++){return 1;} /* Lost control packet also needs an idempotent retry. */
    CHECK(used<1024);if(used>=1024)return -1;
    queue[used++]=(packet){n->id,(unsigned)peer,m};return 1;
}
static void deliver(uint64_t now) {
    packet batch[1024];unsigned count=used;memcpy(batch,queue,count*sizeof *batch);used=0;
    for(unsigned i=0;i<count;i++) {
        packet p=batch[i];
        CHECK(lr_restart_message(&nodes[p.to-1].restart,p.from,&p.m,now));
    }
}
static void initialize(void) {
    memset(nodes,0,sizeof nodes);used=calls=dropped_go=0;
    for(unsigned i=0;i<4;i++) {
        node *n=&nodes[i];n->id=i+1;n->ready_at=1600+i*700;
        lr_restart_init(&n->restart,i?LR_ROLE_GUEST:LR_ROLE_HOST,i?1:0,123,send_packet,n);
    }
}
static void drive_round(uint64_t begin,uint64_t end,uint64_t complete_after) {
    for(uint64_t now=begin;now<end;now+=50) {
        for(unsigned i=0;i<4;i++) {
            node *n=&nodes[i];
            int action=lr_restart_tick(&n->restart,now,n->closes && now>=n->ready_at);
            if(action==LR_R_CLOSE)n->closes++;
            else if(action==LR_R_HOST_START) {
                CHECK(i==0 && now>=nodes[3].ready_at && n->restart.ready==7);
                n->starts++;lr_restart_started(&n->restart,1);
            } else if(action==LR_R_GUEST_JOIN) {
                CHECK(i>0 && nodes[0].starts==1 && now>=n->ready_at);
                n->starts++;lr_restart_started(&n->restart,1);
            } else CHECK(action!=LR_R_FAIL);
            if(n->starts && now>complete_after)lr_restart_complete(&n->restart);
        }
        deliver(now);
    }
}
static void round_trip(void) {
    initialize();uint64_t peers[3]={2,3,4};
    CHECK(!lr_restart_begin(&nodes[1].restart,peers,7,1000));
    CHECK(lr_restart_begin(&nodes[0].restart,peers,7,1000));
    CHECK(!lr_restart_begin(&nodes[0].restart,peers,8,1000));
    lr_msg forged={0};forged.type=LR_RESTART;forged.id=7;forged.b=123;forged.a=LR_R_BEGIN;
    CHECK(!lr_restart_message(&nodes[1].restart,3,&forged,1000));
    forged.b=124;CHECK(!lr_restart_message(&nodes[1].restart,1,&forged,1000));forged.b=123;
    CHECK(lr_restart_message(&nodes[1].restart,1,&forged,1000));
    forged.a=LR_R_GO;CHECK(!lr_restart_message(&nodes[1].restart,1,&forged,1001));
    drive_round(1000,12000,6000);
    for(unsigned i=0;i<4;i++) {
        if(nodes[i].restart.phase!=LR_R_DONE)printf("node %u phase=%d closes=%u starts=%u ready=%u joined=%u\n",i,nodes[i].restart.phase,nodes[i].closes,nodes[i].starts,nodes[i].restart.ready,nodes[i].restart.joined);
        CHECK(nodes[i].restart.phase==LR_R_DONE && nodes[i].closes==1 && nodes[i].starts==1);
    }
    /* Delayed duplicates after loading cannot close or launch a second time. */
    forged.a=LR_R_BEGIN;CHECK(lr_restart_message(&nodes[1].restart,1,&forged,13000));
    forged.a=LR_R_GO;CHECK(lr_restart_message(&nodes[1].restart,1,&forged,13000));
    CHECK(lr_restart_tick(&nodes[1].restart,13001,0)==LR_R_NONE && nodes[1].restart.phase==LR_R_DONE);
    forged.id=8;forged.a=LR_R_READY;CHECK(!lr_restart_message(&nodes[0].restart,2,&forged,13000));
    used=0;dropped_go=0;
    for(unsigned i=0;i<4;i++){nodes[i].closes=nodes[i].starts=0;nodes[i].ready_at=14600+i*700;}
    CHECK(lr_restart_begin(&nodes[0].restart,peers,8,14000));
    drive_round(14000,25000,19000);
    for(unsigned i=0;i<4;i++)CHECK(nodes[i].restart.phase==LR_R_DONE && nodes[i].restart.generation==8 && nodes[i].closes==1 && nodes[i].starts==1);

}
static void failures_and_codec(void) {
    initialize();uint64_t peers[3]={2,3,4},duplicates[3]={2,2,4};
    CHECK(!lr_restart_begin(&nodes[0].restart,duplicates,1,10));
    CHECK(lr_restart_begin(&nodes[0].restart,peers,1,10));
    CHECK(lr_restart_tick(&nodes[0].restart,10,0)==LR_R_CLOSE);
    CHECK(lr_restart_tick(&nodes[0].restart,1000,0)==LR_R_NONE);
    CHECK(!nodes[0].restart.go_enabled && !nodes[0].starts);
    CHECK(lr_restart_tick(&nodes[0].restart,180011,0)==LR_R_FAIL);
    CHECK(lr_restart_tick(&nodes[0].restart,180012,1)==LR_R_NONE && nodes[0].restart.phase==LR_R_FAILED);
    used=0;initialize();CHECK(lr_restart_begin(&nodes[0].restart,peers,2,10));
    nodes[0].restart.phase=LR_R_RESTARTING;nodes[0].restart.start_issued=1;
    lr_restart_started(&nodes[0].restart,0);CHECK(nodes[0].restart.phase==LR_R_FAILED);
    lr_msg m={0},decoded;uint8_t bytes[LR_MAX_MSG];m.type=LR_RESTART;m.id=2;m.b=123;
    for(unsigned step=1;step<=5;step++) {
        m.a=step;size_t len=lr_encode(&m,bytes);
        CHECK(len==16 && lr_decode(bytes,len,LR_CH_CONTROL,&decoded) && decoded.a==step);
        CHECK(!lr_decode(bytes,len,LR_CH_STREAM,&decoded));
        for(size_t cut=0;cut<len;cut++)CHECK(!lr_decode(bytes,cut,LR_CH_CONTROL,&decoded));
    }
    m.a=6;CHECK(!lr_encode(&m,bytes));m.a=1;m.id=0;CHECK(!lr_encode(&m,bytes));m.id=1;m.b=0;CHECK(!lr_encode(&m,bytes));
}
int main(void) {
    round_trip();failures_and_codec();
    printf("Automatic restart: 4 coordinators, delayed close/recovery, preserved participants, retry/idempotence, forged/early/stale messages, timeout/start refusal and strict codec: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures!=0;
}
