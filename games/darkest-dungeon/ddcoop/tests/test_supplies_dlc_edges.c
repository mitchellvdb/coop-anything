#include "dd_supplies.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SHARD 0x12345678u
static unsigned failures, cases;
#define CHECK(x) do { if(!(x)) { failures++;printf("FAIL shard edge line %d case %u\n",__LINE__,cases); } } while(0)
static dd_provisions_snapshot initial(int32_t gold,int32_t shard) {
 dd_provisions_snapshot s={0};s.stock_slots=s.bag_slots=2;s.stock_occupied=s.bag_occupied=1;
 s.stock[0]=(dd_provisions_item){99,0,3};s.bag[0]=(dd_provisions_item){99,0,1};
 s.currencies=3;s.wallet[0]=(dd_provisions_currency){DD_SUPPLIES_GOLD,gold};
 s.wallet[1]=(dd_provisions_currency){SHARD,shard};s.wallet[2]=(dd_provisions_currency){77,9};return s;
}
int main(void) {
 const uint32_t balance[]={0,1,INT32_MAX-1,INT32_MAX};
 const uint32_t prices[]={0,1,2,INT32_MAX,UINT32_MAX};
 for(unsigned g=0;g<4;g++)for(unsigned sh=0;sh<4;sh++)for(unsigned gp=0;gp<5;gp++)for(unsigned sp=0;sp<5;sp++)for(unsigned op=1;op<=3;op++) {
  cases++;dd_provisions_snapshot before=initial((int32_t)balance[g],(int32_t)balance[sh]);dd_action a;
  uint32_t pg=prices[gp],ps=prices[sp];
  int expected=pg<=INT32_MAX && ps<=INT32_MAX;
  if(op==DD_SUPPLY_BUY) expected=expected && balance[g]>=pg && balance[sh]>=ps;
  else expected=expected && (uint64_t)balance[g]+pg<=INT32_MAX && (uint64_t)balance[sh]+ps<=INT32_MAX;
  int result=dd_supplies_quote_with_shards(&before,op,0,pg,ps,SHARD,&a);
  CHECK(result==expected);
  if(!result)continue;
  CHECK(dd_supplies_before_with_shards(&before,&a,SHARD));
  dd_provisions_snapshot after=before;int sign=op==DD_SUPPLY_BUY?-1:1;
  after.wallet[0].quantity=(int32_t)((int64_t)balance[g]+sign*(int64_t)pg);
  after.wallet[1].quantity=(int32_t)((int64_t)balance[sh]+sign*(int64_t)ps);
  if(op==DD_SUPPLY_CANCEL) {
   memset(after.stock,0,sizeof after.stock);memset(after.bag,0,sizeof after.bag);after.stock_occupied=after.bag_occupied=0;
  } else if(op==DD_SUPPLY_BUY) {after.stock[0].quantity=2;after.bag[0].quantity=2;}
  else {after.stock[0].quantity=4;memset(after.bag,0,sizeof after.bag);after.bag_occupied=0;}
  CHECK(dd_supplies_after_with_shards(&before,&after,&a,SHARD));
  after.wallet[2].quantity++;
  CHECK(!dd_supplies_after_with_shards(&before,&after,&a,SHARD));
 }
 dd_provisions_snapshot bad=initial(-1,1);dd_action a;
 CHECK(!dd_supplies_quote_with_shards(&bad,DD_SUPPLY_BUY,0,0,0,SHARD,&a));
 bad=initial(1,-1);CHECK(!dd_supplies_quote_with_shards(&bad,DD_SUPPLY_BUY,0,0,0,SHARD,&a));
 bad=initial(1,1);bad.wallet[2].type=SHARD;CHECK(!dd_supplies_quote_with_shards(&bad,DD_SUPPLY_BUY,0,0,0,SHARD,&a));
 printf("Independent shard boundary matrix: %u quotes plus exact conserved post-states, 0/INTMAX/UINTMAX funds/refunds, negative/duplicate wallets, unchanged unrelated currency: %s (%u failures)\n",cases,failures?"FAILED":"PASS",failures);
 return failures!=0;
}
