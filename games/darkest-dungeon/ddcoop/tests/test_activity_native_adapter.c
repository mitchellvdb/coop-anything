/* Actual adapter, fake bounded native effects. No game/Steam/save execution. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dd_hamlet.h"
#include "../runtime/dd_activity_native.inc"
static unsigned failures,calls,fault;
#define CHECK(x) do{if(!(x)){printf("FAIL activity adapter line%d: %s\n",__LINE__,#x);failures++;}}while(0)
static uint8_t campaign[0xf00],entry[2][0x1638],activity[0x100],records[3][24],wallet[2][0x48];
static uint64_t roster[2],virtuals[16];static uint8_t nop_code[3]={0xc2,0,0};
static dd_hamlet model;
static uint64_t vt_rva[6]={0xe9d728,0xe9d618,0xe9d6a0,0xe9aea8,0xe9ad28,0xe9adb0};
static void put32(void *p,uint32_t v){memcpy(p,&v,4);}static void put64(void *p,uint64_t v){memcpy(p,&v,8);}
static uint64_t get64(const void *p){uint64_t v;memcpy(&v,p,8);return v;}
static uint32_t get32(const void *p){uint32_t v;memcpy(&v,p,4);return v;}
static int reader(void *ctx,uint64_t at,void *out,size_t n){
    (void)ctx;
    if(at==model.image+0x2a6d4bb && n==1){*(uint8_t*)out=0;return 1;}
    if(at==model.image+0x2340a0 && n==3){memcpy(out,nop_code,n);return 1;}
    for(unsigned k=0;k<6;k++)if(at>=model.image+vt_rva[k] && at+n<=model.image+vt_rva[k]+sizeof virtuals){memcpy(out,(uint8_t*)virtuals+at-model.image-vt_rva[k],n);return 1;}
    SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)at,out,n,&got) && got==n;
}
static void __fastcall fake_set(void *widget,void *new_hero){
    uint8_t *w=widget;calls++;CHECK(!new_hero);CHECK(!get64(w));
    CHECK(get64(w+0x28)==(uintptr_t)(entry[0]+8));CHECK(get64(w+0x280)==(uintptr_t)activity);
    unsigned rank=get32(w+0x230);CHECK(rank<3);
    if(fault==1)return;
    if(fault!=2){put32(entry[0]+0x15b0,0);put64(entry[0]+0x15c0,0);}
    memset(records[rank],0,8);records[rank][0x14]=0;
    if(fault==3)records[rank][8]++;
    if(fault==4)wallet[0][0]++;
    put64(w+0x28,0);w[0xc]=0;w[0x20]=1;
}
static void reset(unsigned kind,unsigned slot){
    memset(campaign,0,sizeof campaign);memset(entry,0,sizeof entry);memset(activity,0,sizeof activity);
    memset(records,0,sizeof records);memset(wallet,0,sizeof wallet);memset(virtuals,0,sizeof virtuals);
    memset(&model,0,sizeof model);model.read=reader;model.image=0x140000000ULL;model.campaign=(uintptr_t)campaign;
    calls=fault=0;nop_code[0]=0xc2;
    for(unsigned i=0;i<2;i++){roster[i]=(uintptr_t)entry[i];put32(entry[i],i+7);put32(entry[i]+0x1384,i+7);put32(entry[i]+0x15b0,i?1:2);}
    put64(entry[0]+0x15c0,(uintptr_t)activity);
    put64(campaign+0x20,(uintptr_t)roster);put64(campaign+0x28,(uintptr_t)(roster+2));
    put64(campaign+0xe70,(uintptr_t)wallet);put64(campaign+0xe78,(uintptr_t)(wallet+2));put32(wallet[0],12345);put32(wallet[0]+0x44,1);put32(wallet[1],32);put32(wallet[1]+0x44,2);
    put64(activity,model.image+vt_rva[kind]);virtuals[7]=model.image+0x2340a0;
    put64(activity+0x78,(uintptr_t)records);put64(activity+0x80,(uintptr_t)(records+3));
    put32(records[slot],7);put32(records[slot]+4,1);put64(records[slot]+8,0x1122334455667788ULL);records[slot][0x14]=1;
}
int main(void){
    unsigned valid=0,negative=0;
    for(unsigned kind=0;kind<6;kind++)for(unsigned slot=0;slot<3;slot++){
        reset(kind,slot);int mutated=-1;uint8_t untouched[sizeof entry[1]],old_wallet[sizeof wallet];memcpy(untouched,entry[1],sizeof untouched);memcpy(old_wallet,wallet,sizeof old_wallet);
        CHECK(activity_native_release(&model,(uintptr_t)activity,kind,slot,7,fake_set,&mutated));
        CHECK(mutated==1 && calls==1 && get32(records[slot])==0 && get32(entry[0]+0x15b0)==0 && !get64(entry[0]+0x15c0));
        CHECK(!memcmp(untouched,entry[1],sizeof untouched) && !memcmp(old_wallet,wallet,sizeof old_wallet));valid++;
    }
    for(unsigned scenario=0;scenario<10;scenario++){
        reset(0,0);int mutated=-1;uint32_t hero=7;unsigned kind=0,slot=0;
        if(scenario==0)hero=8;if(scenario==1)kind=6;if(scenario==2)slot=3;
        if(scenario==3)put64(activity,model.image+vt_rva[1]);
        if(scenario==4)virtuals[7]++;
        if(scenario==5)nop_code[0]=0xc3;
        if(scenario==6)put64(activity+0x80,(uintptr_t)records+1);
        if(scenario==7)put64(entry[0]+0x15c0,123);
        if(scenario==8)put32(records[0]+4,0);
        if(scenario==9)put32(entry[0]+0x15b0,1);
        CHECK(!activity_native_release(&model,(uintptr_t)activity,kind,slot,hero,fake_set,&mutated));CHECK(!mutated && !calls);negative++;
    }
    for(unsigned scenario=1;scenario<=4;scenario++){
        reset(0,0);fault=scenario;int mutated=0;
        CHECK(!activity_native_release(&model,(uintptr_t)activity,0,0,7,fake_set,&mutated));CHECK(mutated && calls==1);negative++;
    }
    printf("Activity native capture: %u six-activity/three-slot clears, %u stale/invalid/postcheck faults, canonical slot/hero association and unchanged wallet/other hero: %s (%u failures); native effects modeled\n",valid,negative,failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
