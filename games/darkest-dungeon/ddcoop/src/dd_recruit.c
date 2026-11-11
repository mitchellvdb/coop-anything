#include "dd_recruit.h"
#include "dd_text.h"
#include <string.h>

static int rd(const dd_recruit *s, uint64_t at, void *out, size_t n) {
    return at && at <= UINT64_MAX - n && s->read(s->ctx, at, out, n);
}
static int ptr(const dd_recruit *s, uint64_t at, uint64_t *out) { return rd(s, at, out, 8); }
static int vec(const dd_recruit *s, uint64_t at, unsigned stride, unsigned cap, uint64_t *begin,
               unsigned *count) {
    uint64_t end;
    if (at > UINT64_MAX-16 || !ptr(s, at, begin) || !ptr(s, at + 8, &end) || end < *begin || (end - *begin) % stride ||
        (end - *begin) / stride > cap || (!*begin && end))
        return 0;
    *count = (unsigned)((end - *begin) / stride);
    return 1;
}
int dd_recruit_open(dd_recruit *s, dd_recruit_read_fn read, void *ctx, uint64_t image, uint64_t campaign) {
    memset(s, 0, sizeof *s);
    s->read = read;
    s->ctx = ctx;
    s->image = image;
    s->campaign = campaign;
    uint8_t alternate = 0, nil = 0;
    uint64_t head = 0, root = 0, coach = 0;
    if (!read || !campaign || !rd(s, image + 0x2a6d4bb, &alternate, 1) || alternate ||
        !ptr(s, campaign + 0x11f8, &head) || !head || !rd(s, head + 0x19, &nil, 1) || !nil ||
        !ptr(s, head + 8, &root))
        return 0;
    uint64_t pending[128];
    unsigned n = 0, seen = 0;
    if (root != head) pending[n++] = root;
    while (n) {
        uint64_t node = pending[--n], left, right, model, vt;
        if (++seen > 128 || !node || node == head || !rd(s, node + 0x19, &nil, 1) || nil ||
            !ptr(s, node, &left) || !ptr(s, node + 0x10, &right) || !ptr(s, node + 0x28, &model) || !model ||
            !ptr(s, model, &vt))
            return 0;
        if (vt == image + 0xe9d088) {
            if (coach) return 0;
            coach = model;
        }
        if (left != head) {
            if (n == 128) return 0;
            pending[n++] = left;
        }
        if (right != head) {
            if (n == 128) return 0;
            pending[n++] = right;
        }
    }
    uint64_t children;
    unsigned count;
    if (!coach || !vec(s, coach + 0xa8, 8, 64, &children, &count)) return 0;
    for (unsigned i = 0; i < count; i++) {
        uint64_t child, vt;
        if (!ptr(s, children + i * 8, &child)) return 0;
        if (!child) continue;
        if (!ptr(s, child, &vt)) return 0;
        if (vt == image + 0xe9cfd0) {
            if (s->store) return 0;
            s->store = child;
        }
    }
    uint64_t entries;
    return s->store && vec(s, s->store + 0x68, 0x15b0, DD_RECRUIT_MAX_CANDIDATES, &entries, &count);
}
int dd_recruit_candidate(const dd_recruit *s, uint32_t id, dd_recruit_choice *out) {
    uint64_t begin;
    unsigned count;
    int found = 0;
    memset(out, 0, sizeof *out);
    if (!id || !s->store || !vec(s, s->store + 0x68, 0x15b0, DD_RECRUIT_MAX_CANDIDATES, &begin, &count))
        return 0;
    for (unsigned i = 0; i < count; i++) {
        uint64_t entry = begin + (uint64_t)i * 0x15b0;
        uint32_t key;
        uint8_t available;
        if (!rd(s, entry + 0x137c, &key, 4) || !rd(s, entry + 0x15a8, &available, 1) || available > 1)
            return 0;
        if (key == id) {
            if (found) return 0;
            found = 1;
            out->id = id;
            out->entry = entry;
            out->available = available;
        }
    }
    return found;
}
int dd_recruit_panel_ready(const dd_recruit *s, uint64_t panel) {
    uint64_t vt = 0, store = 0, begin;
    unsigned count;
    return panel && ptr(s, panel, &vt) && vt == s->image + 0xec0cb0 && ptr(s, panel + 0x80, &store) &&
           store == s->store && vec(s, panel + 0x350, 16, DD_RECRUIT_MAX_CANDIDATES, &begin, &count) &&
           count > 0;
}
int dd_recruit_resolve(const dd_recruit *s, uint64_t panel, uint32_t id, dd_recruit_choice *out) {
    if (!dd_recruit_candidate(s, id, out) || !out->available || !dd_recruit_panel_ready(s, panel)) return 0;
    uint64_t begin;
    unsigned count;
    if (!vec(s, panel + 0x350, 16, DD_RECRUIT_MAX_CANDIDATES, &begin, &count)) return 0;
    int found = 0;
    for (unsigned i = 0; i < count; i++) {
        uint64_t row, vt, controller, hero = 0;
        uint32_t key = 0, enabled = 0;
        if (!ptr(s, begin + i * 16, &row)) return 0;
        if (!row) continue;
        if (!ptr(s, row, &vt) || vt != s->image + 0xebcc00 || !ptr(s, row + 0x158, &controller)) return 0;
        if (!controller) continue;
        if (!ptr(s, controller + 0x28, &hero)) return 0;
        if (!hero) continue;
        if (!rd(s, hero + 0x137c, &key, 4)) return 0;
        if (key != id) continue;
        if (found || hero != out->entry || !ptr(s, controller, &vt) || vt != s->image + 0xee0990 ||
            !rd(s, row + 0x58, &enabled, 4))
            return 0;
        found = 1;
        out->hero = hero;
        out->index = i;
        out->enabled = enabled != 0;
    }
    return found;
}
int dd_recruit_roster_has(const dd_recruit *s, uint32_t id, unsigned *count) {
    uint64_t begin;
    int found = 0;
    if (!vec(s, s->campaign + 0x20, 8, 64, &begin, count)) return -1;
    for (unsigned i = 0; i < *count; i++) {
        uint64_t hero;
        uint32_t key;
        /* Live roster wrapper embeds the Entry at +8; its ID is +0x1384, not Entry+0x137c. */
        if (!ptr(s, begin + i * 8, &hero) || !hero || !rd(s, hero + 0x1384, &key, 4)) return -1;
        if (key == id) found = 1;
    }
    return found;
}

/* These are serialized logical fields, not a hash of an Entry allocation.
 * Read-only native proof: HeroEntry save5d1ad0/quirks5d1dec..1ecd and load5d2867..296c.
 * Quirk records are38 bytes; the definition's stable name/ID replaces its pointer.
 * Fields08..1f and2b/34..37 are runtime/padding, deliberately excluded. */
static void fingerprint_word(uint64_t *h, uint64_t v) {
    for(unsigned i=0;i<8;i++) {*h ^= (uint8_t)(v>>(8*i));*h *= UINT64_C(1099511628211);}
}
static int fingerprint_name(const dd_recruit *s,uint64_t p,uint64_t *h) {
    char name[64];
    if(!rd(s,p,name,sizeof name) || !dd_utf8_name_valid(name,sizeof name))return 0;
    size_t n=strlen(name);fingerprint_word(h,n);
    for(size_t i=0;i<n;i++){*h^=(uint8_t)name[i];*h*=UINT64_C(1099511628211);}
    return 1;
}
static int fingerprint_vectors(const dd_recruit *s,uint64_t hero,unsigned camping,uint64_t *h) {
    uint64_t levels=0,selected=0,levels_after=0,selected_after=0;
    unsigned ln=0,sn=0,la=0,sa=0;uint32_t learned[32]={0},active[8]={0};
    uint64_t lp=hero+(camping?0x13b0:0x13e0),sp=hero+(camping?0x1398:0x13c8);
    if(!vec(s,lp,4,32,&levels,&ln) || !vec(s,sp,4,8,&selected,&sn) ||
       (ln && !rd(s,levels,learned,ln*4)) || (sn && !rd(s,selected,active,sn*4)))return 0;
    fingerprint_word(h,ln);
    for(unsigned i=0;i<ln;i++) {
        if(learned[i]>4 && learned[i]!=UINT32_MAX)return 0;
        fingerprint_word(h,learned[i]);
    }
    fingerprint_word(h,sn);
    for(unsigned i=0;i<sn;i++) {
        if(active[i]>=ln || learned[active[i]]==UINT32_MAX)return 0;
        for(unsigned j=0;j<i;j++)if(active[i]==active[j])return 0;
        fingerprint_word(h,active[i]);
    }
    return vec(s,lp,4,32,&levels_after,&la) && levels==levels_after && ln==la &&
           vec(s,sp,4,8,&selected_after,&sa) && selected==selected_after && sn==sa;
}
static int fingerprint_entry_once(const dd_recruit *s,uint64_t hero,uint64_t *out,uint32_t *id_out) {
    uint32_t id=0,klass_id=0,level[3]={0};uint64_t klass=0,quirks=0,after=0;
    unsigned qn=0,an=0;uint8_t available=0;
    uint64_t hash=UINT64_C(14695981039346656037);fingerprint_word(&hash,1); /* schema */
    if(hero>UINT64_MAX-0x15b0 || !rd(s,hero+0x137c,&id,4) || !id || !rd(s,hero+0x15a8,&available,1) || available>1 ||
       !ptr(s,hero+0x1320,&klass) || !klass || klass>UINT64_MAX-0xd8 || !rd(s,klass+0xd4,&klass_id,4) || !klass_id ||
       !rd(s,hero+0x1328,level,sizeof level))return 0;
    fingerprint_word(&hash,id);fingerprint_word(&hash,available);fingerprint_word(&hash,klass_id);
    if(!fingerprint_name(s,hero+8,&hash) || !fingerprint_name(s,klass+8,&hash))return 0;
    for(unsigned i=0;i<3;i++)fingerprint_word(&hash,level[i]);
    if(!fingerprint_vectors(s,hero,0,&hash) || !fingerprint_vectors(s,hero,1,&hash) ||
       !vec(s,hero+0x1380,0x38,64,&quirks,&qn))return 0;
    fingerprint_word(&hash,qn);uint32_t ids[64]={0};
    for(unsigned i=0;i<qn;i++) {
        uint64_t q=quirks+(uint64_t)i*0x38,definition=0;uint32_t data[4]={0};uint8_t flags[3]={0};
        if(!ptr(s,q,&definition) || !definition || definition>UINT64_MAX-0x44 || !rd(s,definition+0x40,&ids[i],4) || !ids[i] ||
           !rd(s,q+0x20,data,8) || !rd(s,q+0x28,flags,3) ||
           !rd(s,q+0x2c,data+2,8))return 0;
        for(unsigned j=0;j<i;j++)if(ids[j]==ids[i])return 0;
        for(unsigned j=0;j<3;j++)if(flags[j]>1)return 0;
        fingerprint_word(&hash,ids[i]);if(!fingerprint_name(s,definition,&hash))return 0;
        fingerprint_word(&hash,data[0]);fingerprint_word(&hash,data[1]);
        for(unsigned j=0;j<3;j++)fingerprint_word(&hash,flags[j]);
        fingerprint_word(&hash,data[2]);fingerprint_word(&hash,data[3]);
    }
    uint32_t id_after=0;uint8_t available_after=0;uint64_t klass_after=0;
    if(!vec(s,hero+0x1380,0x38,64,&after,&an) || after!=quirks || an!=qn ||
       !rd(s,hero+0x137c,&id_after,4) || id_after!=id ||
       !rd(s,hero+0x15a8,&available_after,1) || available_after!=available ||
       !ptr(s,hero+0x1320,&klass_after) || klass_after!=klass)return 0;
    *id_out=id;*out=hash?hash:UINT64_MAX;return 1;
}
static int fingerprint_entry(const dd_recruit *s,uint64_t hero,uint64_t *out,uint32_t *id) {
    uint64_t first=0,second=0;uint32_t a=0,b=0;
    if(!fingerprint_entry_once(s,hero,&first,&a) || !fingerprint_entry_once(s,hero,&second,&b) ||
       first!=second || a!=b)return 0;
    *out=first;*id=a;return 1;
}
int dd_recruit_fingerprint(const dd_recruit *s,uint32_t id,uint64_t *out) {
    if(!out)return 0;*out=0;
    if(!s || !s->read || !id)return 0;
    dd_recruit_choice choice;uint32_t captured=0;uint64_t hash=0,begin=0,end_begin=0;
    unsigned n=0,end_n=0;
    if(!s->store || s->store>UINT64_MAX-0x78 || !vec(s,s->store+0x68,0x15b0,DD_RECRUIT_MAX_CANDIDATES,&begin,&n) ||
       !dd_recruit_candidate(s,id,&choice) || !fingerprint_entry(s,choice.entry,&hash,&captured) || captured!=id ||
       !vec(s,s->store+0x68,0x15b0,DD_RECRUIT_MAX_CANDIDATES,&end_begin,&end_n) ||
       begin!=end_begin || n!=end_n)return 0;
    *out=hash;return 1;
}
static int pool_digest_once(const dd_recruit *s,uint64_t *out,unsigned *count) {
    if(out)*out=0;if(count)*count=0;
    if(!out || !count || !s || !s->read || !s->store || s->store>UINT64_MAX-0x78)return 0;
    uint64_t begin=0,end_begin=0,hashes[DD_RECRUIT_MAX_CANDIDATES]={0};
    uint32_t ids[DD_RECRUIT_MAX_CANDIDATES]={0};unsigned n=0,end_n=0;
    if(!vec(s,s->store+0x68,0x15b0,DD_RECRUIT_MAX_CANDIDATES,&begin,&n))return 0;
    for(unsigned i=0;i<n;i++) {
        if(!fingerprint_entry(s,begin+(uint64_t)i*0x15b0,&hashes[i],&ids[i]))return 0;
        for(unsigned j=0;j<i;j++)if(ids[j]==ids[i])return 0;
    }
    if(!vec(s,s->store+0x68,0x15b0,DD_RECRUIT_MAX_CANDIDATES,&end_begin,&end_n) ||
       begin!=end_begin || n!=end_n)return 0;
    /* The UI may reorder rows, but recruitment names a stable candidate ID. */
    for(unsigned i=1;i<n;i++)for(unsigned j=i;j && ids[j]<ids[j-1];j--) {
        uint32_t id=ids[j];ids[j]=ids[j-1];ids[j-1]=id;
        uint64_t h=hashes[j];hashes[j]=hashes[j-1];hashes[j-1]=h;
    }
    uint64_t result=UINT64_C(14695981039346656037);fingerprint_word(&result,1);fingerprint_word(&result,n);
    for(unsigned i=0;i<n;i++) {fingerprint_word(&result,ids[i]);fingerprint_word(&result,hashes[i]);}
    *out=result?result:UINT64_MAX;*count=n;return 1;
}

int dd_recruit_pool_digest(const dd_recruit *s,uint64_t *out,unsigned *count) {
    if(out)*out=0;if(count)*count=0;
    if(!out || !count)return 0;
    uint64_t first=0,second=0;unsigned a=0,b=0;
    if(!pool_digest_once(s,&first,&a) || !pool_digest_once(s,&second,&b) || first!=second || a!=b)return 0;
    *out=first;*count=a;return 1;
}
