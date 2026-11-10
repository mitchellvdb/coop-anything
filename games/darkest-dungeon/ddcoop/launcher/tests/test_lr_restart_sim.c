/* Deterministic four-controller simulation of the real restart coordinator.
 * No game, Steam, process control or file replacement. Engine effects modeled. */
#include "lr_restart.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define CAP 512
static unsigned failures,rng,seed_current;
#define CHECK(x) do { if(!(x)) { if(failures<15) printf("FAIL seed=%u line=%d %s\n",seed_current,__LINE__,#x); failures++; } } while(0)
typedef struct {lr_restart r;unsigned id,closes,starts,fails;uint64_t ready_at,start_at;} node;
typedef struct {unsigned from,to;lr_msg msg;uint64_t due;int used;} packet;
static node n[4];static packet q[CAP];static uint64_t now,last_due[4][4];static unsigned mode,fail_node,sends,delivered,dropped,blocked;
static unsigned random_(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static int send_(void *ctx,uint64_t peer,int channel,const uint8_t *bytes,size_t len) {
 node *from=ctx;lr_msg m;CHECK(channel==LR_CH_CONTROL && lr_decode(bytes,len,channel,&m));CHECK(peer>=1 && peer<=4);
 if(peer<1 || peer>4)return -1;
 sends++;
 if((mode==4 && from->id==fail_node) || random_()%5==0){blocked++;return 0;}
 if(random_()%11==0){dropped++;return 1;} /* Unneeded by reliable Steam, deliberately stronger retry challenge. */
 unsigned copies=1+(random_()%7==0);
 for(unsigned copy=0;copy<copies;copy++) {
  unsigned slot=0;while(slot<CAP && q[slot].used)slot++;
  CHECK(slot<CAP);if(slot==CAP)return 0;
  uint64_t due=now+random_()%1500;
  if(due<last_due[from->id-1][peer-1])due=last_due[from->id-1][peer-1];
  last_due[from->id-1][peer-1]=due+1;
  q[slot]=(packet){from->id,(unsigned)peer,m,due,1};
 }
 return 1;
}
static void run_seed(unsigned seed,unsigned kind) {
 seed_current=seed;rng=seed?seed:1;mode=kind;fail_node=1+random_()%4;
 memset(n,0,sizeof n);memset(q,0,sizeof q);memset(last_due,0,sizeof last_due);now=1000;
 for(unsigned i=0;i<4;i++) {
  n[i].id=i+1;n[i].ready_at=now+500+random_()%12000;
  lr_restart_init(&n[i].r,i?LR_ROLE_GUEST:LR_ROLE_HOST,i?1:0,0x1234,send_,&n[i]);
 }
 uint64_t peers[]={2,3,4};CHECK(lr_restart_begin(&n[0].r,peers,seed?seed:1,now));
 for(unsigned steps=0;steps<8000;steps++) {
  now+=10+random_()%80;
  /* Deliver due messages in due order per reliable source/destination; vary peers. */
  for(unsigned pass=0;pass<CAP;pass++) {
   unsigned best=CAP;
   for(unsigned j=0;j<CAP;j++)if(q[j].used && q[j].due<=now && (best==CAP || q[j].due<q[best].due))best=j;
   if(best==CAP)break;
   packet p=q[best];q[best].used=0;
   (void)lr_restart_message(&n[p.to-1].r,p.from,&p.msg,now);delivered++;
  }
  unsigned first=random_()%4;
  for(unsigned k=0;k<4;k++) {
   unsigned i=(first+k)%4;node *x=&n[i];
   int ready=x->closes && now>=x->ready_at && !(mode==1 && x->id==fail_node);
   if(mode==3 && x->id==fail_node && x->closes && now>=3500 && lr_restart_active(&x->r))lr_restart_abort(&x->r);
   int action=lr_restart_tick(&x->r,now,ready);
   if(action==LR_R_CLOSE) {x->closes++;CHECK(x->closes==1 && x->starts==0);}
   else if(action==LR_R_HOST_START) {
    CHECK(i==0 && x->closes==1 && ready);
    for(unsigned j=1;j<4;j++)CHECK(n[j].closes==1 && now>=n[j].ready_at && !(mode==1 && n[j].id==fail_node));
    x->starts++;x->start_at=now;CHECK(x->starts==1);
    lr_restart_started(&x->r,!(mode==2 && x->id==fail_node));
   } else if(action==LR_R_GUEST_JOIN) {
    CHECK(i>0 && ready && x->closes==1 && n[0].starts==1 && n[0].r.go_enabled);
    x->starts++;x->start_at=now;CHECK(x->starts==1);
    lr_restart_started(&x->r,!(mode==2 && x->id==fail_node));
   } else if(action==LR_R_FAIL) {x->fails++;CHECK(x->fails==1);}
   else CHECK(action==LR_R_NONE);
   if(x->starts && now>x->start_at+1500 && !(mode==2 && x->id==fail_node))lr_restart_complete(&x->r);
  }
  if(kind==0) {
   int done=1;for(unsigned i=0;i<4;i++)done&=n[i].r.phase==LR_R_DONE;
   if(done){for(unsigned i=0;i<4;i++)CHECK(n[i].closes==1 && n[i].starts==1 && !n[i].fails);return;}
  } else if(now>200000) {
   for(unsigned i=0;i<4;i++)CHECK(!lr_restart_active(&n[i].r));
   return;
  }
 }
 CHECK(0 && "bounded simulation ended without convergence/abort");
}
int main(void) {
 unsigned runs=0;
 for(unsigned kind=0;kind<5;kind++)for(unsigned seed=1;seed<=2000;seed++){run_seed(seed*2654435761u,kind);runs++;}
 printf("Restart interleavings: %u deterministic 4-controller runs; reliable ordered delivery plus duplicates/loss/backpressure, callbacks delayed, recovery/start/leave failure and timeout; sends=%u delivered=%u dropped=%u blocked=%u: %s (%u failures)\n",runs,sends,delivered,dropped,blocked,failures?"FAIL":"PASS",failures);
 return failures!=0;
}
