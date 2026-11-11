/* Protocol35 consistency controls: no native effects or game files. */
static void rw_put16(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void rw_put32(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t rw_random(uint32_t *s) {*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return *s;}
static dd_msg rw_control(unsigned which) {
    dd_msg m={0};
    if(which==0) {
        m.type=DD_MSG_TOWN_APPLY;m.seq=77;m.u.town.request_id=42;
        m.u.town.action.kind=DD_ACT_RECRUIT;m.u.town.action.u.recruit.hero_id=20;
        m.u.town.action.u.recruit.fingerprint=UINT64_C(0x123456789abcdef0);
    } else if(which==1) {
        m.type=DD_MSG_TOWN_REQUEST;m.seq=77;m.u.town.request_id=43;
        m.u.town.action.kind=DD_ACT_DISMISS;m.u.town.action.u.hero.hero_id=20;
    } else if(which==2) {
        m.type=DD_MSG_POOL;m.u.pool.epoch=5;m.u.pool.anchor=77;
        m.u.pool.digest=UINT64_C(0xfedcba9876543210);m.u.pool.count=7;m.u.pool.op=1;
    } else {
        m.type=DD_MSG_WEEKLY;m.u.weekly.epoch=4;m.u.weekly.rules=2147483646u;m.u.weekly.guid=1024;
    }
    return m;
}
static void rw_expect_invalid(dd_msg invalid,const uint8_t *valid,size_t n,size_t offset,
                              uint32_t value,unsigned bytes) {
    uint8_t wire[128];dd_msg out={0};size_t used=0;
    CHECK(!dd_encode(&invalid,wire,sizeof wire),"protocol35 invalid encode type%u at%u",invalid.type,(unsigned)offset);
    memcpy(wire,valid,n);
    if(bytes==1)wire[offset]=(uint8_t)value;else rw_put32(wire+offset,value);
    CHECK(dd_decode(wire,n,&out,&used)==DD_DECODE_BAD_VALUE,
          "protocol35 invalid decode type%u at%u",invalid.type,(unsigned)offset);
}
static void recruit_wire_checks(void) {
    uint8_t wire[128],bad[128],roundtrip[128];dd_msg out={0};size_t used=0;
    static const uint64_t fingerprints[]={1,UINT64_MAX,UINT64_C(0x123456789abcdef0),UINT64_C(0x100000000)};
    for(unsigned type=DD_MSG_TOWN_REQUEST;type<=DD_MSG_TOWN_APPLY;type++)for(unsigned j=0;j<4;j++) {
        dd_msg m=rw_control(0);m.type=(uint16_t)type;m.u.town.action.u.recruit.fingerprint=fingerprints[j];
        size_t n=dd_encode(&m,wire,sizeof wire);
        CHECK(n==33 && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && used==n &&
              out.u.town.action.kind==DD_ACT_RECRUIT && out.u.town.action.u.recruit.hero_id==20 &&
              out.u.town.action.u.recruit.fingerprint==fingerprints[j],"full64 recruit fingerprint/request+apply");
        for(unsigned i=0;i<8;i++)CHECK(wire[25+i]==(uint8_t)(fingerprints[j]>>(8*i)),"fingerprint LE byte%u",i);
    }
    dd_msg m=rw_control(0);size_t n=dd_encode(&m,wire,sizeof wire);
    dd_msg invalid=m;invalid.u.town.action.u.recruit.hero_id=0;
    rw_expect_invalid(invalid,wire,n,21,0,4);
    invalid=m;invalid.u.town.action.u.recruit.hero_id=UINT32_MAX;
    rw_expect_invalid(invalid,wire,n,21,UINT32_MAX,4);
    invalid=m;invalid.u.town.action.u.recruit.fingerprint=0;
    CHECK(!dd_encode(&invalid,bad,sizeof bad),"recruit must supply a content fingerprint");
    memcpy(bad,wire,n);memset(bad+25,0,8);
    CHECK(dd_decode(bad,n,&out,&used)==DD_DECODE_BAD_VALUE,"zero recruit fingerprint refused on wire");
    /* Old recruit payload is not accepted merely by relabelling its header34. */
    memcpy(bad,wire,n);rw_put32(bad+12,9);
    CHECK(dd_decode(bad,25,&out,&used)==DD_DECODE_BAD_LENGTH,"old ID-only recruit body refused");
    for(unsigned type=DD_MSG_TOWN_REQUEST;type<=DD_MSG_TOWN_APPLY;type++) {
        m=rw_control(1);m.type=(uint16_t)type;n=dd_encode(&m,wire,sizeof wire);
        CHECK(n==25 && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && used==25 &&
              out.u.town.action.kind==DD_ACT_DISMISS && out.u.town.action.u.hero.hero_id==20,
              "dismiss remains exactly ID-only with9-byte body");
        memcpy(bad,wire,n);rw_put32(bad+12,17);memset(bad+n,0,8);
        CHECK(dd_decode(bad,n+8,&out,&used)==DD_DECODE_BAD_LENGTH,"dismiss cannot inherit recruit extension");
    }
    /* Every control op/player combination is canonical in both directions. */
    for(unsigned op=0;op<=255;op++)for(unsigned player=0;player<=255;player++) {
        m=rw_control(2);m.u.pool.op=(uint8_t)op;m.u.pool.player=(uint8_t)player;
        int valid=(op==2 ? player>=1 && player<=3 : (op==1 || op==3) && !player);
        n=dd_encode(&m,wire,sizeof wire);
        CHECK((n!=0)==valid,"pool encode op/player%u/%u",op,player);
        dd_msg base=rw_control(2);size_t base_n=dd_encode(&base,bad,sizeof bad);
        bad[36]=(uint8_t)op;bad[37]=(uint8_t)player;
        dd_decode_result result=dd_decode(bad,base_n,&out,&used);
        CHECK((result==DD_DECODE_OK)==valid,"pool decode op/player%u/%u",op,player);
    }
    m=rw_control(2);n=dd_encode(&m,wire,sizeof wire);CHECK(n==38,"pool exact22-byte body");
    invalid=m;invalid.seq=1;rw_expect_invalid(invalid,wire,n,8,1,4);
    invalid=m;invalid.u.pool.epoch=0;rw_expect_invalid(invalid,wire,n,16,0,4);
    invalid=m;invalid.u.pool.anchor=UINT32_MAX;rw_expect_invalid(invalid,wire,n,20,UINT32_MAX,4);
    invalid=m;invalid.u.pool.count=65;rw_expect_invalid(invalid,wire,n,32,65,4);
    invalid=m;invalid.u.pool.count=UINT32_MAX;rw_expect_invalid(invalid,wire,n,32,UINT32_MAX,4);
    for(unsigned high=0;high<2;high++) {
        m=rw_control(2);m.u.pool.epoch=high?UINT32_MAX:1;m.u.pool.anchor=high?UINT32_MAX-1:0;
        m.u.pool.count=high?64:0;m.u.pool.digest=fingerprints[high];n=dd_encode(&m,wire,sizeof wire);
        CHECK(n==38 && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && used==n &&
              out.u.pool.epoch==m.u.pool.epoch && out.u.pool.anchor==m.u.pool.anchor &&
              out.u.pool.count==m.u.pool.count && out.u.pool.digest==m.u.pool.digest,
              "pool boundary epoch/anchor/count/full64 digest");
    }
    for(unsigned high=0;high<2;high++) {
        m=rw_control(3);m.u.weekly.epoch=high?UINT32_MAX:1;m.u.weekly.rules=high?2147483646u:1;
        m.u.weekly.guid=high?DD_PROTO_GUID_MAX:1;
        n=dd_encode(&m,wire,sizeof wire);
        CHECK(n==28 && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && used==n &&
              out.u.weekly.epoch==m.u.weekly.epoch && out.u.weekly.rules==m.u.weekly.rules &&
              out.u.weekly.guid==m.u.weekly.guid && !out.seq,
              "weekly exact12-byte body/boundary seed epoch guid");
    }
    m=rw_control(3);n=dd_encode(&m,wire,sizeof wire);
    invalid=m;invalid.seq=1;rw_expect_invalid(invalid,wire,n,8,1,4);
    invalid=m;invalid.u.weekly.epoch=0;rw_expect_invalid(invalid,wire,n,16,0,4);
    static const uint32_t bad_rules[]={0,2147483647u,2147483648u,UINT32_MAX};
    for(unsigned i=0;i<4;i++) {
        invalid=m;invalid.u.weekly.rules=bad_rules[i];rw_expect_invalid(invalid,wire,n,20,bad_rules[i],4);
    }
    static const uint32_t bad_guids[]={0,DD_PROTO_GUID_MAX+1u,UINT32_MAX};
    for(unsigned i=0;i<3;i++) {
        invalid=m;invalid.u.weekly.guid=bad_guids[i];rw_expect_invalid(invalid,wire,n,24,bad_guids[i],4);
    }
    /* A stream buffer may contain the next frame; declared body length may not
     * contain extra bytes or omit any of this frame's fields. */
    uint32_t random=0x74b192a3;
    for(unsigned which=0;which<4;which++) {
        m=rw_control(which);n=dd_encode(&m,wire,sizeof wire);CHECK(n,"base protocol35 fixture%u",which);
        for(size_t cap=0;cap<n;cap++)CHECK(!dd_encode(&m,bad,cap),"small encode buffer%u/%u",which,(unsigned)cap);
        for(size_t length=0;length<n;length++)
            CHECK(dd_decode(wire,length,&out,&used)==DD_DECODE_SHORT,"truncation fixture%u/%u",which,(unsigned)length);
        for(uint32_t body=0;body<n-DD_PROTO_HEADER;body++) {
            memcpy(bad,wire,n);rw_put32(bad+12,body);
            CHECK(dd_decode(bad,DD_PROTO_HEADER+body,&out,&used)==DD_DECODE_BAD_LENGTH,
                  "declared short body%u/%u",which,body);
        }
        memcpy(bad,wire,n);bad[n]=0xab;rw_put32(bad+12,(uint32_t)(n-DD_PROTO_HEADER+1));
        CHECK(dd_decode(bad,n+1,&out,&used)==DD_DECODE_BAD_LENGTH,"overlong body fixture%u",which);
        memcpy(bad,wire,n);bad[n]=0xab;
        CHECK(dd_decode(bad,n+1,&out,&used)==DD_DECODE_OK && used==n,"next-frame byte untouched fixture%u",which);
        memcpy(bad,wire,n);rw_put16(bad+4,33);
        CHECK(dd_decode(bad,n,&out,&used)==DD_DECODE_BAD_VERSION,"old33 peer refused fixture%u",which);
        for(unsigned iteration=0;iteration<25000;iteration++) {
            memcpy(bad,wire,n);size_t length=rw_random(&random)%(n+2);
            bad[n]=0;
            unsigned mutations=1+rw_random(&random)%5;
            for(unsigned j=0;j<mutations;j++)bad[rw_random(&random)%n]^=(uint8_t)(1+rw_random(&random)%255);
            if(dd_decode(bad,length,&out,&used)==DD_DECODE_OK) {
                size_t encoded=dd_encode(&out,roundtrip,sizeof roundtrip);
                CHECK(encoded && encoded==used && used<=length && !memcmp(bad,roundtrip,used),
                      "accepted mutation must re-encode byte-identically fixture%u",which);
            }
        }
    }
    puts("Recruit/pool/weekly protocol35: full64 fingerprints, legacy dismiss shape, canonical fields, all op/player pairs, exact lengths, old33 refusal and100000 mutation roundtrips verified");
}
