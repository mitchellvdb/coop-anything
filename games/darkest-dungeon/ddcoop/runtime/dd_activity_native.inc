/* Build28385 activity clear capture. Include after dd_hamlet.h/stdint/string.
 * Native setter is pinned by the runtime; ONLY six verified stress activities.
 * This helper performs no UI projection and never constructs/destructs a C++ UI
 * object: the exact setter reads only the POD offsets populated below. */
typedef void (__fastcall *activity_native_set_fn)(void *, void *);
#define ACTIVITY_NATIVE_SLOT_BYTES 24u
#define ACTIVITY_NATIVE_WALLET_BYTES (32u*0x48u)
static int activity_native_read(const dd_hamlet *s, uint64_t address, void *out, size_t n) {
    return s && s->read && address && address <= UINT64_MAX-n && s->read(s->ctx,address,out,n);
}
static int activity_native_stress(const dd_hamlet *s,uint64_t activity,unsigned kind) {
    static const uint32_t vt[6]={0xe9d728,0xe9d618,0xe9d6a0,0xe9aea8,0xe9ad28,0xe9adb0};
    uint64_t table=0,notify=0;uint8_t mode=1,noop[3]={0};
    return kind<6 && s && s->image &&
        activity_native_read(s,s->image+0x2a6d4bb,&mode,1) && !mode &&
        activity_native_read(s,activity,&table,8) && table==s->image+vt[kind] &&
        activity_native_read(s,table+0x38,&notify,8) && notify==s->image+0x2340a0 &&
        activity_native_read(s,notify,noop,3) && noop[0]==0xc2 && !noop[1] && !noop[2];
}
static int activity_native_slot(const dd_hamlet *s,uint64_t activity,unsigned slot,uint64_t *out) {
    uint64_t begin=0,end=0;*out=0;
    if(slot>=3 || !activity_native_read(s,activity+0x78,&begin,8) || !begin ||
       !activity_native_read(s,activity+0x80,&end,8) || end<begin ||
       (end-begin)%ACTIVITY_NATIVE_SLOT_BYTES || (end-begin)/ACTIVITY_NATIVE_SLOT_BYTES>3 ||
       slot>=(end-begin)/ACTIVITY_NATIVE_SLOT_BYTES)return 0;
    *out=begin+slot*ACTIVITY_NATIVE_SLOT_BYTES;return 1;
}
static int activity_native_wallet(const dd_hamlet *s,uint64_t span[2],uint8_t bytes[ACTIVITY_NATIVE_WALLET_BYTES],size_t *n) {
    *n=0;
    if(!activity_native_read(s,s->campaign+0xe70,span,16) || span[1]<span[0] ||
       (span[1]-span[0])%0x48 || span[1]-span[0]>ACTIVITY_NATIVE_WALLET_BYTES ||
       (!span[0] && span[1]))return 0;
    *n=(size_t)(span[1]-span[0]);
    return !*n || activity_native_read(s,span[0],bytes,*n);
}
/* mutated is set before the native effect; a failed postcheck requires shared
 * recovery, not a retry. Refusing a stale/unsupported pre-state has mutated=0. */
static int activity_native_release(const dd_hamlet *s,uint64_t activity,unsigned kind,unsigned slot,
                                   uint32_t hero_id,activity_native_set_fn setter,int *mutated) {
    uint64_t address=0,association=0,span[2],after_span[2],same_address=0;
    uint8_t before[24],expected[24],after[24];uint32_t actual_id=0,state=0;
    uint8_t wallet[ACTIVITY_NATIVE_WALLET_BYTES],after_wallet[ACTIVITY_NATIVE_WALLET_BYTES];
    size_t n=0,after_n=0;dd_hamlet_hero hero,current;
    if(mutated)*mutated=0;
    if(!setter || !hero_id || !activity_native_stress(s,activity,kind) ||
       !activity_native_slot(s,activity,slot,&address) ||
       !activity_native_read(s,address,before,sizeof before))return 0;
    memcpy(&actual_id,before,4);memcpy(&state,before+4,4);
    if(actual_id!=hero_id || state!=1 || dd_hamlet_find(s,hero_id,&hero)!=1 || hero.location!=2 ||
       !activity_native_read(s,hero.entry+0x15c0,&association,8) || association!=activity ||
       !activity_native_wallet(s,span,wallet,&n))return 0;
    /* Exact POD capture for7e1dc0: selected Hero*, rank, activity. Native code
     * calls its own roster-location setter and clears the canonical record. */
    __declspec(align(16)) uint8_t capture[0x2d0]={0};
    memcpy(capture+0x28,&hero.hero,8);memcpy(capture+0x230,&slot,4);memcpy(capture+0x280,&activity,8);
    if(mutated)*mutated=1;
    setter(capture,NULL);
    memcpy(expected,before,sizeof expected);memset(expected,0,8);expected[0x14]=0;
    uint64_t clear=1,ui_pointer=1;
    if(!activity_native_slot(s,activity,slot,&same_address) || same_address!=address ||
       !activity_native_read(s,address,after,sizeof after) || memcmp(expected,after,sizeof after) ||
       dd_hamlet_find(s,hero_id,&current)!=1 || current.entry!=hero.entry || current.hero!=hero.hero || current.location!=0 ||
       !activity_native_read(s,hero.entry+0x15c0,&clear,8) || clear ||
       !activity_native_wallet(s,after_span,after_wallet,&after_n) || after_n!=n ||
       memcmp(span,after_span,sizeof span) || memcmp(wallet,after_wallet,n))return 0;
    memcpy(&ui_pointer,capture+0x28,8);
    return !ui_pointer && !capture[0xc] && capture[0x20]==1;
}
