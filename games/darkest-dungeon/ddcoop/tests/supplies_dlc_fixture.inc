/* Pure DLC transaction simulations. Engine pricing/effects are modeled; the
 * ordinary runtime intentionally stays gold-only. No real DLC data or saves. */
#define TEST_SHARDS 0x12345678u
static dd_provisions_snapshot dlc_base(void) {
    dd_provisions_snapshot s = {0};
    s.stock_slots = s.bag_slots = 4; s.stock_occupied = 1;
    s.stock[0] = (dd_provisions_item){12,34,100};
    s.currencies = 3;
    s.wallet[0] = (dd_provisions_currency){DD_SUPPLIES_GOLD,100000};
    s.wallet[1] = (dd_provisions_currency){TEST_SHARDS,100000};
    s.wallet[2] = (dd_provisions_currency){9876,20};
    return s;
}
static void dlc_model_effect(dd_provisions_snapshot *s,const dd_action *a) {
    int sign = a->u.supplies.op == DD_SUPPLY_BUY ? -1 : 1;
    for (unsigned i=0;i<s->currencies;i++) {
        if(s->wallet[i].type==DD_SUPPLIES_GOLD)s->wallet[i].quantity+=sign*(int32_t)a->u.supplies.gold;
        if(s->wallet[i].type==TEST_SHARDS)s->wallet[i].quantity+=sign*(int32_t)a->u.supplies.shards;
    }
    if(a->u.supplies.op==DD_SUPPLY_CANCEL) {
        memset(s->bag,0,sizeof s->bag);memset(s->stock,0,sizeof s->stock);
        s->bag_occupied=s->stock_occupied=0;
    } else {
        s->stock[0].quantity+=sign;
        s->bag[0]=(dd_provisions_item){12,34,s->bag[0].quantity-sign};
        if(!s->bag[0].quantity)s->bag[0]=(dd_provisions_item){0};
        s->bag_occupied=s->bag[0].quantity!=0;
    }
}
static void dlc_supply_checks(void) {
    dd_action a, crossed; dd_provisions_snapshot before=dlc_base(), after, wrong;
    CHECK(dd_supplies_quote_with_shards(&before,DD_SUPPLY_BUY,0,5,3,TEST_SHARDS,&a));
    CHECK(!dd_supplies_quote(&before,DD_SUPPLY_BUY,0,5,3,&crossed)); /* runtime guard retained */
    after=before;dlc_model_effect(&after,&a);
    CHECK(dd_supplies_after_with_shards(&before,&after,&a,TEST_SHARDS));
    wrong=after;wrong.wallet[1].quantity+=3;CHECK(!dd_supplies_after_with_shards(&before,&wrong,&a,TEST_SHARDS));
    wrong=after;wrong.wallet[0].quantity+=5;CHECK(!dd_supplies_after_with_shards(&before,&wrong,&a,TEST_SHARDS));
    wrong=after;wrong.wallet[2].quantity++;CHECK(!dd_supplies_after_with_shards(&before,&wrong,&a,TEST_SHARDS));
    wrong=after;wrong.bag[1]=(dd_provisions_item){88,99,1};wrong.bag_occupied++;
    CHECK(!dd_supplies_after_with_shards(&before,&wrong,&a,TEST_SHARDS));
    CHECK(!dd_supplies_before_with_shards(&after,&a,TEST_SHARDS));
    wrong=before;wrong.wallet[1].quantity=2;
    CHECK(!dd_supplies_quote_with_shards(&wrong,DD_SUPPLY_BUY,0,0,3,TEST_SHARDS,&a));
    wrong=before;wrong.currencies=1;
    CHECK(!dd_supplies_quote_with_shards(&wrong,DD_SUPPLY_BUY,0,0,3,TEST_SHARDS,&a));
    CHECK(!dd_supplies_quote_with_shards(&before,DD_SUPPLY_BUY,0,0,3,0,&a));
    CHECK(!dd_supplies_quote_with_shards(&before,DD_SUPPLY_BUY,0,0,3,DD_SUPPLIES_GOLD,&a));
    CHECK(!dd_supplies_quote_with_shards(&before,DD_SUPPLY_BUY,0,0,UINT32_MAX,TEST_SHARDS,&a));
    before=after;
    CHECK(dd_supplies_quote_with_shards(&before,DD_SUPPLY_RETURN,0,5,3,TEST_SHARDS,&a));
    dlc_model_effect(&after,&a);CHECK(dd_supplies_after_with_shards(&before,&after,&a,TEST_SHARDS));
    before=dlc_base();before.wallet[1].quantity=INT32_MAX;
    CHECK(!dd_supplies_quote_with_shards(&before,DD_SUPPLY_CANCEL,0,0,1,TEST_SHARDS,&a));
    before=dlc_base();
    CHECK(dd_supplies_quote_with_shards(&before,DD_SUPPLY_BUY,0,5,3,TEST_SHARDS,&a));
    after=before;dlc_model_effect(&after,&a);before=after;
    CHECK(dd_supplies_quote_with_shards(&before,DD_SUPPLY_CANCEL,0,5,3,TEST_SHARDS,&a));
    dlc_model_effect(&after,&a);CHECK(dd_supplies_after_with_shards(&before,&after,&a,TEST_SHARDS));
    /* Independent 4-copy sequences: all encoded commands conserve both funds
     * and items; crossed stale requests refuse before a second mutation. */
    uint32_t rng=0x70534d19u;
    for(unsigned scenario=0;scenario<5000;scenario++) {
        dd_provisions_snapshot peers[4];for(unsigned p=0;p<4;p++)peers[p]=dlc_base();
        rng=rng*1664525u+1013904223u;
        uint32_t price=(rng>>16)%6,shards=(rng>>8)%7;
        for(unsigned step=0;step<4;step++) {
            unsigned op=step%2?DD_SUPPLY_RETURN:DD_SUPPLY_BUY;
            CHECK(dd_supplies_quote_with_shards(&peers[0],op,0,price,shards,TEST_SHARDS,&a));
            crossed=a;
            dd_msg msg={0},decoded;msg.type=DD_MSG_TOWN_APPLY;msg.seq=step+1;msg.u.town.action=a;
            uint8_t bytes[512];size_t used=0,n=dd_encode(&msg,bytes,sizeof bytes);
            CHECK(n && dd_decode(bytes,n,&decoded,&used)==DD_DECODE_OK && used==n);
            for(unsigned p=0;p<4;p++) {
                before=peers[p];CHECK(dd_supplies_before_with_shards(&before,&decoded.u.town.action,TEST_SHARDS));
                dlc_model_effect(&peers[p],&decoded.u.town.action);
                CHECK(dd_supplies_after_with_shards(&before,&peers[p],&decoded.u.town.action,TEST_SHARDS));
                CHECK(!dd_supplies_before_with_shards(&peers[p],&crossed,TEST_SHARDS));
                CHECK(dd_supplies_equal(&peers[0],&peers[p]));
            }
        }
    }
    puts("DLC supplies model: 5000 four-copy encoded mixed-currency sequences; stale/crossed commands, buy/return/refund, shortage, overflow and all-currency/item conservation verified; native runtime gold-only guard retained");
}
