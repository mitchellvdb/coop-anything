/* Real initializer with simulated native Item lifetime/swaps. Four copies begin
 * with different physical free-item orders and must produce identical quotes. */
static uint8_t so_bag[32],so_items[6][0x5a8];
static int so_alive,so_broken;
static unsigned so_swaps;
static int so_open(supply_model *m,dd_provisions_snapshot *s) {
    sw_counts(); *s=sw;
    m->campaign=(uintptr_t)sw_campaign;m->panel=(uintptr_t)sw_panel;
    m->stock=(uintptr_t)sw_stock;m->bag=(uintptr_t)so_bag;
    for(unsigned i=0;i<s->bag_slots;i++) memcpy(&s->bag[i],so_items[i],sizeof s->bag[i]);
    s->bag_occupied=0;
    for(unsigned i=0;i<s->bag_slots;i++) s->bag_occupied+=s->bag[i].quantity!=0;
    return 1;
}
static uintptr_t __fastcall so_copy(void *to,const void *from) {
    CHECK(!so_alive);so_alive++;memcpy(to,from,0x5a8);return (uintptr_t)to;
}
static void __fastcall so_clear(void *item) {CHECK(so_alive==1);memset(item,0,0x5a8);}
static void __fastcall so_destroy(void *item) {(void)item;CHECK(so_alive==1);so_alive--;}
static void __fastcall so_swap(void *bag,uint32_t slot,void *item) {
    CHECK(bag==so_bag && slot<6 && so_alive==1);so_swaps++;
    if(so_broken && so_swaps==2)return;
    uint8_t temp[0x5a8];memcpy(temp,so_items[slot],sizeof temp);
    memcpy(so_items[slot],item,sizeof temp);memcpy(item,temp,sizeof temp);
}
static void supply_initial_order_checks(void) {
    dd_provisions_snapshot reference={0};uint8_t digest[32];
    const dd_provisions_item free_items[4]={{9,5,1},{3,0,2},{3,1,1},{7,2,1}};
    const unsigned orders[4][6]={{0,1,2,3,4,5},{3,4,2,1,0,5},{5,2,0,4,3,1},{1,3,5,0,2,4}};
    for(unsigned copy=0;copy<5;copy++) {
        sw_reset(DD_ROLE_HOST);g_supply_open=so_open;
        g_hamlet_copy=so_copy;g_hamlet_clear=so_clear;g_hamlet_swap=so_swap;g_hamlet_destroy=so_destroy;
        so_alive=0;so_swaps=0;so_broken=copy==4;
        sw.bag_slots=6;memset(so_items,0,sizeof so_items);memset(so_bag,0,sizeof so_bag);
        putptr(so_bag+8,(uintptr_t)so_items);putptr(so_bag+0x10,(uintptr_t)(so_items+6));
        for(unsigned slot=0;slot<6;slot++) {
            unsigned k=orders[copy%4][slot];if(k>=4)continue;
            memcpy(so_items[slot],&free_items[k],sizeof free_items[k]);so_items[slot][0x200]=(uint8_t)(k+1);
        }
        if(so_broken) {CHECK(!supply_initial_order() && !so_alive);continue;}
        CHECK(supply_initial_order() && !so_alive);
        supply_model model;dd_provisions_snapshot after;uint8_t hash[32];dd_action purchase;
        CHECK(so_open(&model,&after) && dd_supplies_digest(&after,hash));
        if(!copy){reference=after;memcpy(digest,hash,32);}else CHECK(dd_supplies_equal(&reference,&after) && !memcmp(digest,hash,32));
        CHECK(dd_supplies_quote(&reference,DD_SUPPLY_BUY,0,75,0,&purchase) && dd_supplies_before(&after,&purchase));
        for(unsigned slot=0;slot<4;slot++) {
            unsigned original=so_items[slot][0x200]-1;
            CHECK(original<4 && !memcmp(so_items[slot],&free_items[original],sizeof free_items[original]));
        }
        unsigned swaps=so_swaps;CHECK(supply_initial_order() && so_swaps==swaps && !so_alive);
    }
    g_supply_open=supply_open;g_hamlet_copy=NULL;g_hamlet_clear=NULL;g_hamlet_swap=NULL;g_hamlet_destroy=NULL;
    puts("Supply initialization: four free-item orders converge before the first quote; native metadata/lifetime preserved, repeat is idempotent and partial swap refuses");
}
