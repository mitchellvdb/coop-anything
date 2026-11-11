/* End to end: two launcher cores over an in-memory "Steam" (with backpressure and optional corruption),
 * fake games on real 127.0.0.1 TCP, and real save folders on disk.
 *
 * The host's fake game stands in for the co-op runtime: it starts listening on the ini's port only after a
 * "loading" delay, and answers every byte XOR 0x5A. The guest's fake game connects to the guest's relay port
 * once the core says "launch", pushes 2 MB through and checks every answer. */
#include "lr_core.h"
#include "lr_snapshot.h"
#include "lr_restart.h"

#include "dson_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                         \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static uint64_t now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

/* ---- fake Steam ---- */

typedef struct qmsg {
    uint64_t from, to;
    int channel;
    size_t len;
    struct qmsg *next;
    uint8_t data[];
} qmsg;

static qmsg *q_head, *q_tail;
static int backpressure;   /* every Nth send says "full" (0 = never) */
static int corrupt_chunks; /* flip a byte in the next SAVE_CHUNK */
static long send_count;

typedef struct {
    uint64_t me;
    int game_running;
    int allow_all;
    int no_dlc;     /* this node has no DLC installed */
    char game_dir[LR_PATH_MAX]; /* mod tests: <node>\steamapps\common\DarkestDungeon */
    lr_core core;
    lr_restart restart;
    int restart_closes,restart_starts;
    uint64_t recovery_ready_at;
} node;

static int fake_send(void *ctx, uint64_t peer, int channel, const uint8_t *buf, size_t len) {
    node *n = ctx;
    if (backpressure && ++send_count % backpressure == 0) return 0;
    qmsg *m = malloc(sizeof *m + len);
    m->from = n->me;
    m->to = peer;
    m->channel = channel;
    m->len = len;
    m->next = NULL;
    memcpy(m->data, buf, len);
    if (corrupt_chunks && len > 20 && buf[3] == LR_SAVE_CHUNK) {
        m->data[len - 1] ^= 1;
        corrupt_chunks = 0;
    }
    if (q_tail) q_tail->next = m; else q_head = m;
    q_tail = m;
    return 1;
}

static int allowed(void *ctx, uint64_t peer) {
    node *n = ctx;
    return n->allow_all || peer != 3; /* peer 3 is not in the lobby */
}

static int game_running(void *ctx) { return ((node *)ctx)->game_running; }
static int has_dlc(void *ctx, const char *name) { (void)name; return !((node *)ctx)->no_dlc; }

static void fake_random(void *ctx, uint8_t *buf, size_t len) {
    (void)ctx;
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)rand();
}

static void quiet_log(void *ctx, const char *line) {
    (void)ctx;
    if (getenv("LR_TEST_VERBOSE")) printf("  [%llu] %s\n", (unsigned long long)((node *)ctx)->me, line);
}

static node *nodes[8];

static void deliver(void) {
    while (q_head) {
        qmsg *m = q_head;
        q_head = m->next;
        if (!q_head) q_tail = NULL;
        if (m->to < 8 && nodes[m->to]) {
            node *to=nodes[m->to];lr_msg decoded;
            if(to->restart.kit && lr_decode(m->data,m->len,m->channel,&decoded) && decoded.type==LR_RESTART)
                (void)lr_restart_message(&to->restart,m->from,&decoded,now_ms());
            else lr_core_on_message(&to->core,m->from,m->channel,m->data,m->len,now_ms());
        }
        free(m);
    }
}

static void drop_queue(void) {
    while (q_head) {
        qmsg *m = q_head;
        q_head = m->next;
        free(m);
    }
    q_tail = NULL;
}

/* ---- disk ---- */

static char root[512];

static void write_text(const char *dir, const char *name, const char *text) {
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, dir, name);
    CHECK(lr_write_file(path, (const uint8_t *)text, strlen(text)));
}

static char *read_text(const char *path) {
    uint8_t *data;
    size_t len;
    if (!lr_read_file(path, 1 << 26, &data, &len)) return NULL;
    char *s = realloc(data, len + 1);
    s[len] = 0;
    return s;
}

static int same_file(const char *a, const char *b) {
    uint8_t *x, *y;
    size_t lx, ly;
    if (!lr_read_file(a, 1 << 26, &x, &lx)) return 0;
    if (!lr_read_file(b, 1 << 26, &y, &ly)) { free(x); return 0; }
    int same = lx == ly && memcmp(x, y, lx) == 0;
    free(x);
    free(y);
    return same;
}

static void setup_node(node *n, uint64_t me, const char *name) {
    memset(n, 0, sizeof *n);
    n->me = me;
    lr_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.send = fake_send;
    cfg.ctx = n;
    cfg.allowed = allowed;
    cfg.game_running = game_running;
    cfg.has_dlc = has_dlc;
    cfg.random = fake_random;
    cfg.log = quiet_log;
    cfg.build = 0x4d78fbfa;
    cfg.kit = 0xaff98e68;
    cfg.coop_slot = 3;
    char base[LR_PATH_MAX], mods[LR_PATH_MAX];
    lr_join(base, sizeof base, root, name);
    lr_join(mods, sizeof mods, base, "mods");
    lr_make_dirs(mods);
    lr_join(cfg.ini_path, sizeof cfg.ini_path, mods, "ddcoop.ini");
    lr_join(cfg.heroes_path, sizeof cfg.heroes_path, mods, "ddcoop-heroes.ini");
    lr_join(cfg.remote_dir, sizeof cfg.remote_dir, base, "remote");
    lr_join(cfg.backup_root, sizeof cfg.backup_root, base, "backups");
    lr_make_dirs(cfg.remote_dir);
    lr_core_init(&n->core, &cfg);
    nodes[me] = n;
}

/* ---- fake games ---- */

typedef struct {
    lr_sock listener, conn;
    int port;
    uint64_t listen_at;
    int saw_eof;
    long echoed;
} host_game;

static void host_game_tick(host_game *g) {
    if (g->listener == LR_NOSOCK && now_ms() >= g->listen_at) g->listener = lr_listen_local(g->port, NULL);
    if (g->listener == LR_NOSOCK) return;
    if (g->conn == LR_NOSOCK) g->conn = lr_accept_nb(g->listener);
    if (g->conn == LR_NOSOCK || g->saw_eof) return;
    uint8_t buf[8192];
    int n = lr_recv_nb(g->conn, buf, sizeof buf);
    if (n < 0) {
        g->saw_eof = 1;
        lr_close(g->conn);
        return;
    }
    for (int i = 0; i < n; i++) buf[i] ^= 0x5A;
    int off = 0;
    while (off < n) { /* the test socket buffers are large; spin until written */
        int w = lr_send_nb(g->conn, buf + off, (size_t)(n - off));
        if (w < 0) return;
        off += w;
    }
    g->echoed += n;
}

#define PUSH_BYTES (2 * 1024 * 1024)

typedef struct {
    lr_sock s;
    int connected;
    long sent, got;
    int bad;
} guest_game;

static uint8_t pattern(long i) { return (uint8_t)(i * 31 + (i >> 9)); }

static void guest_game_tick(guest_game *g) {
    if (!g->connected) {
        int c = lr_connect_check(g->s);
        if (c < 0) g->bad = 1;
        if (c != 1) return;
        g->connected = 1;
    }
    if (g->sent < PUSH_BYTES) {
        uint8_t buf[4096];
        size_t n = PUSH_BYTES - g->sent < (long)sizeof buf ? (size_t)(PUSH_BYTES - g->sent) : sizeof buf;
        for (size_t i = 0; i < n; i++) buf[i] = pattern(g->sent + (long)i);
        int w = lr_send_nb(g->s, buf, n);
        if (w > 0) g->sent += w;
    }
    uint8_t in[8192];
    int r = lr_recv_nb(g->s, in, sizeof in);
    for (int i = 0; i < r; i++)
        if ((in[i] ^ 0x5A) != pattern(g->got + i)) g->bad = 1;
    if (r > 0) g->got += r;
}

/* ---- scenario ---- */

static void make_host_campaign(const char *remote) {
    char dir[LR_PATH_MAX];
    lr_join(dir, sizeof dir, remote, "profile_2");
    lr_make_dirs(dir);
    for (int i = 0; i < 26; i++) {
        char name[64], text[128];
        snprintf(name, sizeof name, "persist.part%02d.json", i);
        snprintf(text, sizeof text, "{\"host\": %d}", i);
        write_text(dir, name, text);
    }
    /* one big file, spread over many chunks */
    size_t big = 100000;
    uint8_t *data = malloc(big);
    for (size_t i = 0; i < big; i++) data[i] = (uint8_t)rand();
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, dir, "persist.map.json");
    CHECK(lr_write_file(path, data, big));
    free(data);
    /* the roster, in the game's binary format */
    static uint8_t roster[65536];
    static const dw_hero heroes[] = {{"2", "Dismas", "highwayman", 4, 1}, {"1", "Reynauld", "crusader", 6, 0},
                                     {"8", "Bourneville", "vestal", 13, 2}};
    size_t n = dw_roster(roster, heroes, 3);
    lr_join(path, sizeof path, dir, "persist.roster.json");
    CHECK(lr_write_file(path, roster, n));
    /* in town, with the Musketeer DLC (the game turns it on for anyone who has it) */
    static const char *const dlc[] = {"musketeer"};
    n = dw_game(roster, 0, "", dlc, 1);
    lr_join(path, sizeof path, dir, "persist.game.json");
    CHECK(lr_write_file(path, roster, n));
    /* another campaign that must never travel */
    char other[LR_PATH_MAX];
    lr_join(other, sizeof other, remote, "profile_0");
    lr_make_dirs(other);
    write_text(other, "persist.town.json", "{\"single player\": true}");
}

static void make_guest_campaign(const char *remote) {
    char dir[LR_PATH_MAX];
    lr_join(dir, sizeof dir, remote, "profile_2");
    lr_make_dirs(dir);
    write_text(dir, "persist.part00.json", "{\"guest\": \"own\"}");
    write_text(dir, "persist.extra.json", "{\"guest only\": 1}");
}

static int run_until(node *a, node *b, int (*done)(void *), void *arg, int max_ms) {
    uint64_t end = now_ms() + (uint64_t)max_ms;
    while (now_ms() < end) {
        lr_core_tick(&a->core, now_ms());
        if (b) lr_core_tick(&b->core, now_ms());
        deliver();
        if (done(arg)) return 1;
#ifdef _WIN32
        Sleep(1);
#else
        usleep(200);
#endif
    }
    return 0;
}

static int state_is(void *p) { node *n = p; return n->core.state == LR_ST_GUEST_LAUNCH || n->core.state == LR_ST_FAILED; }
static int failed(void *p) { return ((node *)p)->core.state == LR_ST_FAILED; }
static int never(void *p) { (void)p; return 0; }

static node *g_host, *g_guest;

/* The guest has the host's current table. */
static int owners_match(void *p) {
    (void)p;
    const lr_owners *h = &g_host->core.owners, *g = &g_guest->core.owners;
    return g->revision == h->revision && g->count == h->count && memcmp(g->owner, h->owner, (size_t)h->count) == 0;
}

/* "<id><h|g>": that hero belongs to the host / guest in both tables. */
static int owner_is(void *p) {
    const char *spec = p;
    uint32_t id = (uint32_t)strtoul(spec, NULL, 10);
    int want = spec[strlen(spec) - 1] == 'g' ? LR_PLAYER_GUEST : LR_PLAYER_HOST;
    int hi = lr_owners_find(&g_host->core.owners, id), gi = lr_owners_find(&g_guest->core.owners, id);
    return hi >= 0 && gi >= 0 && g_host->core.owners.owner[hi] == want && g_guest->core.owners.owner[gi] == want;
}

typedef struct { int attempts, sent; } close_sender;
static int backpressured_close(void *ctx,uint64_t peer,int channel,const uint8_t *buf,size_t len) {
    close_sender *sender=ctx;
    lr_msg m;
    CHECK(peer==2 && channel==LR_CH_STREAM);
    CHECK(lr_decode(buf,len,channel,&m) && m.type==LR_STREAM_CLOSE && m.id==77);
    if(++sender->attempts==1)return 0;
    sender->sent++;return 1;
}
static void test_pending_close(void) {
    /* No socket is opened: isolate the queued CLOSE state after a local EOF. */
    for(int receive_close=0;receive_close<2;receive_close++) {
        lr_relay relay;close_sender sender={0};
        CHECK(lr_relay_init(&relay,LR_ROLE_HOST,1,backpressured_close,&sender));
        lr_stream *s=&relay.streams[0];
        s->used=s->connected=s->local_closed=1;s->peer=2;s->id=77;
        lr_msg m={0};m.type=LR_STREAM_CLOSE;m.id=77;
        s->pend_len=lr_encode(&m,s->pend);
        if(receive_close)lr_relay_on_message(&relay,2,&m,0);
        else s->peer_closed=1;
        CHECK(lr_relay_active_streams(&relay)==1);
        lr_relay_tick(&relay,1); /* transport full: keep queued CLOSE, even with peer EOF */
        CHECK(sender.attempts==1 && sender.sent==0 && lr_relay_active_streams(&relay)==1);
        lr_relay_tick(&relay,2);
        CHECK(sender.sent==1 && lr_relay_active_streams(&relay)==0);
        lr_relay_free(&relay);
    }
    printf("relay close: queued EOF survives peer close and backpressure\n");
}


typedef struct {unsigned tries[8],accepted[8],rosters[8];} resync_sender;
static int resync_send(void *ctx,uint64_t peer,int channel,const uint8_t *buf,size_t len) {
    resync_sender *sender=ctx;lr_msg m={0};
    CHECK(peer<8 && lr_decode(buf,len,channel,&m));
    if(m.type==LR_ROSTER) {lr_owners o;CHECK(lr_owners_decode(m.data,m.len,m.id,&o) && o.count==1 && o.heroes[0].id==7 && o.owner[0]==3);sender->rosters[peer]++;return 1;}
    if(m.type!=LR_BYE)return 1;
    CHECK(m.a==LR_BYE_RESYNC);
    if(++sender->tries[peer]==1)return 0;
    sender->accepted[peer]++;return 1;
}
static void test_checkpoint_resync(void) {
    static lr_core h,g;resync_sender hs={0},gs={0};lr_config cfg={0};
    cfg.send=resync_send;cfg.ctx=&hs;lr_core_init(&h,&cfg);
    CHECK(!lr_core_request_resync(&h));h.role=LR_ROLE_HOST;h.state=LR_ST_HOST_PLAYING;
    uint64_t at=now_ms();
    for(int i=0;i<3;i++) {h.guests[i].peer=(uint64_t)i+2;h.guests[i].playing=1;h.guests[i].last_rx=h.guests[i].last_tx=at;}
    lr_hero hero={0};hero.id=7;snprintf(hero.name,sizeof hero.name,"Hero");snprintf(hero.cls,sizeof hero.cls,"vestal");
    CHECK(lr_owners_set_roster(&h.owners,&hero,1) && lr_core_assign(&h,7,3));
    lr_core_tick(&h,at);
    for(int i=2;i<=4;i++)CHECK(hs.rosters[i]==1);
    CHECK(lr_core_request_resync(&h) && !lr_core_request_resync(&h));
    CHECK(!lr_core_assign(&h,1,0) && !lr_core_claim(&h,1,1));
    lr_core_tick(&h,at);CHECK(h.resync_requested==1);
    lr_core_tick(&h,at+1001);
    for(int i=2;i<=4;i++) CHECK(hs.tries[i]==2 && hs.accepted[i]==1);
    lr_msg ack={0};ack.type=LR_BYE;ack.a=LR_BYE_RESYNC;uint8_t bytes[LR_MAX_MSG];size_t n=lr_encode(&ack,bytes);
    for(int i=2;i<=4;i++)lr_core_on_message(&h,(uint64_t)i,LR_CH_CONTROL,bytes,n,at+1002);
    lr_core_tick(&h,at+1003);CHECK(h.resync_requested==2 && !lr_core_guest_count(&h));
    cfg.ctx=&gs;lr_core_init(&g,&cfg);g.role=LR_ROLE_GUEST;g.state=LR_ST_GUEST_PLAYING;g.peer=1;
    CHECK(!lr_core_request_resync(&g));
    lr_core_on_message(&g,1,LR_CH_CONTROL,bytes,n,at);
    CHECK(g.state==LR_ST_FAILED && g.resync_requested==2 && g.resend_len);
    lr_core_tick(&g,at+1);CHECK(!g.resend_len && gs.accepted[1]==1);
    CHECK(strstr(g.error,"resync") && !g.cfg.restore_path[0]);
    lr_core_free(&h);lr_core_free(&g);
    puts("Checkpoint resync: all three guests notified/acknowledged, retry under backpressure, host-only, no live save writes");
}


/* Real core file transfer and ownership, fake Steam transport and close/recovery
 * boundaries. New joins arrive in reverse player order; original slots persist. */
static void test_restart_stream_epoch(void) {
    lr_relay r;CHECK(lr_relay_init(&r,LR_ROLE_HOST,27360,NULL,NULL));
    CHECK(lr_relay_set_epoch(&r,91));
    lr_msg m={0};m.type=LR_STREAM_OPEN;m.id=1;m.b=0;
    lr_relay_on_message(&r,2,&m,1);CHECK(!lr_relay_active_streams(&r));
    m.b=91;lr_relay_on_message(&r,2,&m,1);CHECK(lr_relay_active_streams(&r)==1);
    CHECK(!lr_relay_set_epoch(&r,92));
    m.type=LR_STREAM_DATA;m.data=(const uint8_t *)"old";m.len=3;m.b=0;
    lr_relay_on_message(&r,2,&m,2);CHECK(!r.bytes_from_peer && !r.streams[0].to_local_len);
    m.type=LR_STREAM_CLOSE;lr_relay_on_message(&r,2,&m,3);CHECK(!r.streams[0].peer_closed);
    m.type=LR_STREAM_DATA;m.b=91;m.data=(const uint8_t *)"new";
    lr_relay_on_message(&r,2,&m,4);CHECK(r.bytes_from_peer==3 && r.streams[0].to_local_len==3 && !memcmp(r.streams[0].to_local,"new",3));
    lr_relay_free(&r);
    puts("Restart stream epoch: delayed old OPEN/DATA/CLOSE cannot reach or close a new same-ID stream");
}

static void test_automatic_transfer_restart(void) {
    static node n[4];
    drop_queue();backpressure=0;corrupt_chunks=0;
    for(int i=0;i<4;i++) {char name[32];snprintf(name,sizeof name,"auto-restart-%d",i);setup_node(&n[i],(uint64_t)i+1,name);}
    n[0].allow_all=1;n[0].core.cfg.max_guests=3;
    make_host_campaign(n[0].core.cfg.remote_dir);
    CHECK(lr_core_host_start(&n[0].core,now_ms()));
    for(int i=1;i<4;i++) {
        make_guest_campaign(n[i].core.cfg.remote_dir);
        CHECK(lr_core_guest_join(&n[i].core,1,now_ms()));
        CHECK(run_until(&n[0],&n[i],state_is,&n[i],10000) && n[i].core.state==LR_ST_GUEST_LAUNCH);
        CHECK(lr_core_take_launch(&n[i].core) && n[i].core.player==i);
    }
    CHECK(lr_core_assign(&n[0].core,8,3));
    for(int i=0;i<4;i++) {
        n[i].game_running=1;
        lr_restart_init(&n[i].restart,i?LR_ROLE_GUEST:LR_ROLE_HOST,i?1:0,n[i].core.cfg.kit,fake_send,&n[i]);
    }
    uint64_t peers[3]={2,3,4},begin=now_ms();
    CHECK(lr_restart_begin(&n[0].restart,peers,91,begin));
    char password[LR_PASSWORD_LEN+1];snprintf(password,sizeof password,"%s",n[0].core.password);
    char hostdir[LR_PATH_MAX],hostfile[LR_PATH_MAX];
    lr_join(hostdir,sizeof hostdir,n[0].core.cfg.remote_dir,"profile_2");lr_join(hostfile,sizeof hostfile,hostdir,"persist.part00.json");
    backpressure=3;
    uint64_t end=begin+15000;int done=0;
    while(now_ms()<end && !done) {
        uint64_t now=now_ms();
        /* Reverse callbacks/joins deliberately, while keeping each peer's ID. */
        for(int index=3;index>=0;index--) {
            node *x=&n[index];lr_core_tick(&x->core,now);
            if(x->restart_closes && now>=x->recovery_ready_at && !x->restart_starts)x->game_running=0;
            int ready=x->restart_closes && !x->game_running && now>=x->recovery_ready_at;
            int action=lr_restart_tick(&x->restart,now,ready);
            if(action==LR_R_CLOSE) {
                x->restart_closes++;x->recovery_ready_at=now+(uint64_t)(4-index)*60;
                lr_core_pause_restart(&x->core);
                CHECK(x->core.state==LR_ST_IDLE && !x->core.relay_up);
            } else if(action==LR_R_HOST_START) {
                CHECK(index==0 && !x->game_running);
                for(int j=1;j<4;j++)CHECK(!n[j].game_running && now>=n[j].recovery_ready_at);
                /* A new native checkpoint appeared on host close: snapshot THIS one. */
                write_text(hostdir,"persist.part00.json","{\"checkpoint\":2}");
                CHECK(lr_core_host_start(&x->core,now));CHECK(lr_core_reserve_restart(&x->core,peers));
                CHECK(lr_relay_set_epoch(&x->core.relay,x->restart.generation));
                CHECK(strcmp(password,x->core.password)!=0);
                x->restart_starts++;x->game_running=1;lr_restart_started(&x->restart,1);
            } else if(action==LR_R_GUEST_JOIN) {
                CHECK(index>0 && !x->game_running && now>=x->recovery_ready_at);
                CHECK(lr_core_guest_join(&x->core,1,now));CHECK(lr_relay_set_epoch(&x->core.relay,x->restart.generation));x->restart_starts++;
                lr_restart_started(&x->restart,1);
            } else CHECK(action!=LR_R_FAIL);
            if(index && lr_core_take_launch(&x->core)) {
                CHECK(x->restart_starts==1 && !x->game_running);x->game_running=1;
                CHECK(x->core.player==index);lr_restart_complete(&x->restart);
            }
            if(!index && x->restart_starts)lr_restart_complete(&x->restart);
        }
        deliver();done=n[0].restart.phase==LR_R_DONE;
        for(int i=1;i<4;i++) {
            int hero=lr_owners_find(&n[i].core.owners,8);
            done &= n[i].core.state==LR_ST_GUEST_PLAYING && n[i].restart.phase==LR_R_DONE &&
                hero>=0 && n[i].core.owners.owner[hero]==3 && n[i].core.owners.revision==n[0].core.owners.revision;
        }
#ifdef _WIN32
        Sleep(1);
#else
        usleep(1000);
#endif
    }
    if(!done)for(int i=0;i<4;i++)printf("restart node %d: phase=%d state=%d player=%d starts=%d ready=%u joined=%u error=%s\n",i,n[i].restart.phase,n[i].core.state,n[i].core.player,n[i].restart_starts,n[i].restart.ready,n[i].restart.joined,n[i].core.error);
    CHECK(done && lr_core_guest_count(&n[0].core)==3);
    for(int i=0;i<4;i++)CHECK(n[i].restart_closes==1 && n[i].restart_starts==1 && n[i].core.relay.epoch==91);
    for(int i=1;i<4;i++) {
        char dir[LR_PATH_MAX],file[LR_PATH_MAX];lr_join(dir,sizeof dir,n[i].core.cfg.remote_dir,"profile_2");lr_join(file,sizeof file,dir,"persist.part00.json");
        CHECK(same_file(hostfile,file) && n[i].core.player==i);
        int hero=lr_owners_find(&n[i].core.owners,8);CHECK(hero>=0 && n[i].core.owners.owner[hero]==3);
    }
    for(int i=0;i<4;i++)lr_core_free(&n[i].core);
    drop_queue();backpressure=0;
    puts("Automatic restart transfer: 4 real cores, delayed modeled close/recovery, reverse joins preserve Player1..4 and ownership; fresh host checkpoint transferred byte-identically to all guests");
}


static void deliver_metadata(node *guest,const uint8_t *game,size_t size) {
 lr_msg m={0};uint8_t wire[LR_MAX_MSG];size_t len;
 m.type=LR_HELLO;m.a=LR_ROLE_HOST;m.b=guest->core.cfg.build;m.id=guest->core.cfg.kit;
 len=lr_encode(&m,wire);CHECK(len);lr_core_on_message(&guest->core,1,LR_CH_CONTROL,wire,len,now_ms());
 memset(&m,0,sizeof m);m.type=LR_SESSION;m.a=3;memset(m.text,'a',LR_PASSWORD_LEN);
 len=lr_encode(&m,wire);CHECK(len);lr_core_on_message(&guest->core,1,LR_CH_CONTROL,wire,len,now_ms());
 memset(&m,0,sizeof m);m.type=LR_SAVE_BEGIN;m.a=1;m.b=(uint32_t)size;strcpy(m.text,"profile_2");
 len=lr_encode(&m,wire);CHECK(len);lr_core_on_message(&guest->core,1,LR_CH_CONTROL,wire,len,now_ms());
 memset(&m,0,sizeof m);m.type=LR_SAVE_FILE;m.a=(uint32_t)size;m.b=lr_crc32(0,game,size);strcpy(m.text,"persist.game.json");
 len=lr_encode(&m,wire);CHECK(len);lr_core_on_message(&guest->core,1,LR_CH_CONTROL,wire,len,now_ms());
 for(size_t pos=0;pos<size;) {
  memset(&m,0,sizeof m);m.type=LR_SAVE_CHUNK;m.a=(uint32_t)pos;m.data=game+pos;m.len=size-pos<LR_CHUNK?size-pos:LR_CHUNK;
  len=lr_encode(&m,wire);CHECK(len);lr_core_on_message(&guest->core,1,LR_CH_CONTROL,wire,len,now_ms());pos+=m.len;
 }
 memset(&m,0,sizeof m);m.type=LR_SAVE_END;len=lr_encode(&m,wire);CHECK(len);lr_core_on_message(&guest->core,1,LR_CH_CONTROL,wire,len,now_ms());
}
static void test_campaign_metadata_refusal(void) {
 static uint8_t game[65536];size_t size;char original_root[sizeof root];strcpy(original_root,root);
 char path[LR_PATH_MAX],dir[LR_PATH_MAX],name[64];node h,g;
 static const char *const invalid_dlc[]={"musketeer","Bad Name"};
 for(unsigned kind=0;kind<5;kind++) {
  snprintf(name,sizeof name,"metadata-host-%u",kind);setup_node(&h,1,name);make_host_campaign(h.core.cfg.remote_dir);
  lr_join(dir,sizeof dir,h.core.cfg.remote_dir,"profile_2");lr_join(path,sizeof path,dir,"persist.game.json");
  if(kind==0)CHECK(lr_delete_file(path));
  else if(kind==1)write_text(dir,"persist.game.json","not DSON metadata");
  else if(kind==2){size=dw_game(game,0,"",invalid_dlc,2);CHECK(lr_write_file(path,game,size));}
  else if(kind==3){static const char *const future[]={"future_feature"};size=dw_game(game,0,"",future,1);CHECK(lr_write_file(path,game,size));}
  else h.no_dlc=1;
  CHECK(!lr_core_host_start(&h.core,now_ms()) && h.core.state==LR_ST_FAILED && !h.core.relay_up);
  lr_core_free(&h.core);drop_queue();
 }
 for(unsigned kind=0;kind<5;kind++) {
  snprintf(name,sizeof name,"metadata-guest-%u",kind);setup_node(&g,2,name);make_guest_campaign(g.core.cfg.remote_dir);
  lr_join(g.core.cfg.restore_path,sizeof g.core.cfg.restore_path,g.core.cfg.remote_dir,"original-recovery.bin");
  CHECK(lr_core_guest_join(&g.core,1,now_ms()));drop_queue();
  if(kind==0){memcpy(game,"not DSON metadata",17);size=17;}
  else if(kind==1)size=dw_game(game,0,"",invalid_dlc,2);
  else if(kind==2)size=dw_game(game,1,"tutorial",NULL,0);
  else if(kind==4){static const char *const future[]={"future_feature"};size=dw_game(game,0,"",future,1);}
  else {const char *text="{\"data\":{\"inraid\":true,\"raiddungeon\":\"tutorial\"}}";size=strlen(text);memcpy(game,text,size);}
  deliver_metadata(&g,game,size);
  CHECK(g.core.state==LR_ST_FAILED && !lr_core_take_launch(&g.core));
  CHECK(!g.core.backup_path[0] && !lr_dir_exists(g.core.cfg.backup_root) && !lr_file_exists(g.core.cfg.restore_path));
  lr_join(dir,sizeof dir,g.core.cfg.remote_dir,"profile_2");lr_join(path,sizeof path,dir,"persist.part00.json");
  char *own=read_text(path);CHECK(own && !strcmp(own,"{\"guest\": \"own\"}"));free(own);
  lr_join(path,sizeof path,dir,"persist.game.json");CHECK(!lr_file_exists(path));
  lr_core_free(&g.core);drop_queue();
 }
 static const char *const all[]={"musketeer","crimson_court","flagellant","districts","shieldbreaker","color_of_madness","fires_edge","runaway","duelist"};
 for(unsigned flag=0;flag<9;flag++) {
  snprintf(name,sizeof name,"metadata-feature-%u",flag);setup_node(&g,2,name);make_guest_campaign(g.core.cfg.remote_dir);
  lr_join(g.core.cfg.restore_path,sizeof g.core.cfg.restore_path,g.core.cfg.remote_dir,"original-recovery.bin");
  g.no_dlc=1;CHECK(lr_core_guest_join(&g.core,1,now_ms()));drop_queue();
  size=dw_game(game,0,"",all+flag,1);deliver_metadata(&g,game,size);
  CHECK(g.core.state==LR_ST_FAILED && strstr(g.core.error,lr_dlc_title(all[flag])));
  CHECK(!g.core.backup_path[0] && !lr_dir_exists(g.core.cfg.backup_root) && !lr_file_exists(g.core.cfg.restore_path));
  lr_core_free(&g.core);drop_queue();
 }
 setup_node(&g,2,"metadata-all-present");make_guest_campaign(g.core.cfg.remote_dir);
 lr_join(g.core.cfg.restore_path,sizeof g.core.cfg.restore_path,g.core.cfg.remote_dir,"original-recovery.bin");
 CHECK(lr_core_guest_join(&g.core,1,now_ms()));drop_queue();size=dw_game(game,0,"",all,9);deliver_metadata(&g,game,size);
 CHECK(g.core.state==LR_ST_GUEST_LAUNCH && lr_file_exists(g.core.cfg.restore_path) && g.core.backup_path[0]);
 lr_core_free(&g.core);drop_queue();
 /* Passive arena_mp metadata is present in real regular saves; it does not activate PvP. */
 setup_node(&g,2,"metadata-passive-arena");make_guest_campaign(g.core.cfg.remote_dir);g.no_dlc=1;
 lr_join(g.core.cfg.restore_path,sizeof g.core.cfg.restore_path,g.core.cfg.remote_dir,"original-recovery.bin");
 CHECK(lr_core_guest_join(&g.core,1,now_ms()));drop_queue();
 static const char *const passive[]={"arena_mp"};size=dw_game(game,0,"",passive,1);deliver_metadata(&g,game,size);
 CHECK(g.core.state==LR_ST_GUEST_LAUNCH && lr_file_exists(g.core.cfg.restore_path));
 lr_core_free(&g.core);drop_queue();strcpy(root,original_root);
 printf("Campaign preflight: missing/corrupt metadata and tutorial refused before backup/write; all nine feature flags reject missing DLC and valid complete feature list launches via recovery journal\n");
}

/* ---- campaign mods ---- */

static void node_mod_print(void *ctx, const lr_ugc *mod, lr_mod_print *out) {
    lr_mods_print(((node *)ctx)->game_dir, mod, out);
}

static void setup_mod_node(node *n, uint64_t me, const char *name) {
    char base[LR_PATH_MAX], a[LR_PATH_MAX], b[LR_PATH_MAX];
    setup_node(n, me, name);
    lr_join(base, sizeof base, root, name);
    lr_join(a, sizeof a, base, "steamapps");
    lr_join(b, sizeof b, a, "common");
    lr_join(n->game_dir, sizeof n->game_dir, b, "DarkestDungeon");
    CHECK(lr_make_dirs(n->game_dir));
    n->core.cfg.mod_print = node_mod_print;
    lr_join(n->core.cfg.restore_path, sizeof n->core.cfg.restore_path, n->core.cfg.remote_dir, "original-recovery.bin");
}

/* Workshop item <id> on this node, with one data file whose text is the version. */
static void put_workshop_mod(node *n, const char *id, const char *version) {
    char dir[LR_PATH_MAX], a[LR_PATH_MAX], data[LR_PATH_MAX];
    lr_join(a, sizeof a, n->game_dir, "..");
    lr_join(dir, sizeof dir, a, "..");
    lr_join(a, sizeof a, dir, "workshop");
    lr_join(dir, sizeof dir, a, "content");
    lr_join(a, sizeof a, dir, "262060");
    lr_join(dir, sizeof dir, a, id);
    lr_join(data, sizeof data, dir, "heroes");
    CHECK(lr_make_dirs(data));
    write_text(dir, "project.xml", "<project><Title>Test Hero</Title></project>");
    write_text(data, "hero.darkest", version);
}

static void use_mod_campaign(node *host) {
    static uint8_t game[65536];
    static const char *const names[] = {"12345"}, *const sources[] = {"Steam"};
    char dir[LR_PATH_MAX], path[LR_PATH_MAX];
    make_host_campaign(host->core.cfg.remote_dir);
    size_t n = dw_game_ugc(game, names, sources, 1, 1);
    lr_join(dir, sizeof dir, host->core.cfg.remote_dir, "profile_2");
    lr_join(path, sizeof path, dir, "persist.game.json");
    CHECK(lr_write_file(path, game, n));
}

static int own_save_intact(node *g) {
    char dir[LR_PATH_MAX], path[LR_PATH_MAX];
    lr_join(dir, sizeof dir, g->core.cfg.remote_dir, "profile_2");
    lr_join(path, sizeof path, dir, "persist.part00.json");
    char *own = read_text(path);
    int ok = own && !strcmp(own, "{\"guest\": \"own\"}");
    free(own);
    lr_join(path, sizeof path, dir, "persist.game.json");
    return ok && !lr_file_exists(path) && !g->core.backup_path[0] && !lr_file_exists(g->core.cfg.restore_path);
}

static void test_campaign_mods(void) {
    static node h, g;
    char name[64];
    for (int kind = 0; kind < 5; kind++) {
        snprintf(name, sizeof name, "mods-host-%d", kind);
        setup_mod_node(&h, 1, name);
        snprintf(name, sizeof name, "mods-guest-%d", kind);
        setup_mod_node(&g, 2, name);
        use_mod_campaign(&h);
        make_guest_campaign(g.core.cfg.remote_dir);
        if (kind != 3) put_workshop_mod(&h, "12345", "dmg 5");
        if (kind == 0 || kind == 3) put_workshop_mod(&g, "12345", "dmg 5");
        if (kind == 2) put_workshop_mod(&g, "12345", "dmg 6");
        if (kind == 4) h.core.cfg.mod_print = NULL; /* a host that does not fingerprint: the guest cannot confirm */
        if (kind == 3) {
            CHECK(!lr_core_host_start(&h.core, now_ms()) && h.core.state == LR_ST_FAILED &&
                  strstr(h.core.error, "cannot find") && strstr(h.core.error, "Workshop item 12345"));
        } else {
            CHECK(lr_core_host_start(&h.core, now_ms()));
            CHECK(lr_core_guest_join(&g.core, 1, now_ms()));
            CHECK(run_until(&h, &g, state_is, &g, 10000));
            if (kind == 0) {
                CHECK(g.core.state == LR_ST_GUEST_LAUNCH && g.core.mods_known && g.core.mod_count == 1 && g.core.backup_path[0]);
            } else {
                CHECK(g.core.state == LR_ST_FAILED && own_save_intact(&g) && strstr(g.core.error, "not replaced"));
                if (kind == 1) CHECK(strstr(g.core.error, "Missing: Test Hero (Workshop item 12345)") != NULL);
                if (kind == 2) CHECK(strstr(g.core.error, "Other version: Test Hero (Workshop item 12345)") != NULL);
                if (kind == 4) CHECK(strstr(g.core.error, "Not found by the host") != NULL);
                run_until(&h, &g, never, NULL, 200);
                CHECK(lr_core_guest_count(&h.core) == 0);
            }
        }
        lr_core_free(&h.core);
        lr_core_free(&g.core);
        drop_queue();
    }
    /* A host launcher without the mod message (older version) and a campaign with mods: refused. The same
     * host and a campaign without mods: joins as before. */
    static uint8_t game[65536];
    static const char *const names[] = {"12345"};
    for (int with = 0; with < 2; with++) {
        snprintf(name, sizeof name, "mods-old-host-%d", with);
        setup_mod_node(&g, 2, name);
        make_guest_campaign(g.core.cfg.remote_dir);
        put_workshop_mod(&g, "12345", "dmg 5");
        CHECK(lr_core_guest_join(&g.core, 1, now_ms()));
        drop_queue();
        size_t size = with ? dw_game_ugc(game, names, NULL, 1, 0) : dw_game(game, 0, "", NULL, 0);
        deliver_metadata(&g, game, size);
        if (with) CHECK(g.core.state == LR_ST_FAILED && strstr(g.core.error, "too old") && own_save_intact(&g));
        else CHECK(g.core.state == LR_ST_GUEST_LAUNCH);
        lr_core_free(&g.core);
        drop_queue();
    }
    /* A malformed mod list ends the session before anything is stored. */
    setup_mod_node(&g, 2, "mods-malformed");
    make_guest_campaign(g.core.cfg.remote_dir);
    CHECK(lr_core_guest_join(&g.core, 1, now_ms()));
    drop_queue();
    {
        lr_msg m = {0};
        uint8_t wire[LR_MAX_MSG], body[3] = {5, 'a', 'b'};
        size_t len;
        m.type = LR_HELLO;
        m.a = LR_ROLE_HOST;
        m.b = g.core.cfg.build;
        m.id = g.core.cfg.kit;
        len = lr_encode(&m, wire);
        lr_core_on_message(&g.core, 1, LR_CH_CONTROL, wire, len, now_ms());
        memset(&m, 0, sizeof m);
        m.type = LR_SESSION;
        m.a = 3;
        memset(m.text, 'a', LR_PASSWORD_LEN);
        len = lr_encode(&m, wire);
        lr_core_on_message(&g.core, 1, LR_CH_CONTROL, wire, len, now_ms());
        memset(&m, 0, sizeof m);
        m.type = LR_MODS;
        m.id = 1;
        m.data = body;
        m.len = sizeof body;
        len = lr_encode(&m, wire);
        CHECK(len);
        lr_core_on_message(&g.core, 1, LR_CH_CONTROL, wire, len, now_ms());
        CHECK(g.core.state == LR_ST_FAILED && strstr(g.core.error, "malformed mod list") && own_save_intact(&g));
    }
    lr_core_free(&g.core);
    drop_queue();
    printf("Campaign mods end to end: identical Workshop mod joins; missing, other version, host without the mod, host that cannot confirm, older host launcher and a malformed list are refused before the guest's save is touched\n");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    srand(4242);
    lr_sock_startup();
    test_pending_close();
    test_checkpoint_resync();
    test_restart_stream_epoch();
#ifdef _WIN32
    snprintf(root, sizeof root, "build/lrtest-%lu-%llu", GetCurrentProcessId(), (unsigned long long)now_ms());
    CHECK(!lr_dir_exists(root) && lr_make_dirs(root));
#else
    snprintf(root, sizeof root, "build/lrtest-XXXXXX");
    CHECK(mkdtemp(root) != NULL);
#endif

    node host, guest, intruder, guest2;
    setup_node(&host, 1, "host");
    setup_node(&guest, 2, "guest");
    setup_node(&intruder, 3, "intruder");
    setup_node(&guest2, 4, "guest2");
    g_host = &host;
    g_guest = &guest;

    /* 1. no campaign in slot 3: a clear refusal */
    CHECK(!lr_core_host_start(&host.core, now_ms()));
    CHECK(host.core.state == LR_ST_FAILED && strstr(host.core.error, "slot 3"));

    make_host_campaign(host.core.cfg.remote_dir);
    make_guest_campaign(guest.core.cfg.remote_dir);

    /* 2. host starts: ini says host, a 32-character password, the chosen port */
    CHECK(lr_core_host_start(&host.core, now_ms()));
    char *ini = read_text(host.core.cfg.ini_path);
    CHECK(ini && strstr(ini, "role=host") && strstr(ini, "bind_any=0") && strstr(ini, "coop_slots=3"));
    CHECK(ini && strstr(ini, host.core.password) && strlen(host.core.password) == LR_PASSWORD_LEN);
    free(ini);

    /* 3. someone outside the lobby is ignored */
    CHECK(lr_core_guest_join(&intruder.core, 1, now_ms()));
    deliver();
    CHECK(host.core.peer == 0 && intruder.core.state == LR_ST_GUEST_HELLO);

    /* 4. the guest joins with Steam's send buffer filling up every 3rd message */
    backpressure = 3;
    CHECK(lr_core_guest_join(&guest.core, 1, now_ms()));
    CHECK(run_until(&host, &guest, state_is, &guest, 10000));
    CHECK(guest.core.state == LR_ST_GUEST_LAUNCH);
    CHECK(strcmp(guest.core.password, host.core.password) == 0);
    char hdir[LR_PATH_MAX], gdir[LR_PATH_MAX];
    lr_join(hdir, sizeof hdir, host.core.cfg.remote_dir, "profile_2");
    lr_join(gdir, sizeof gdir, guest.core.cfg.remote_dir, "profile_2");
    for (int i = 0; i < 26; i++) {
        char name[64], a[LR_PATH_MAX], b[LR_PATH_MAX];
        snprintf(name, sizeof name, "persist.part%02d.json", i);
        lr_join(a, sizeof a, hdir, name);
        lr_join(b, sizeof b, gdir, name);
        CHECK(same_file(a, b));
    }
    char a[LR_PATH_MAX], b[LR_PATH_MAX];
    lr_join(a, sizeof a, hdir, "persist.roster.json");
    lr_join(b, sizeof b, gdir, "persist.roster.json");
    CHECK(same_file(a, b));
    lr_join(a, sizeof a, hdir, "persist.map.json");
    lr_join(b, sizeof b, gdir, "persist.map.json");
    CHECK(same_file(a, b));
    lr_join(b, sizeof b, gdir, "persist.extra.json");
    CHECK(!lr_file_exists(b)); /* the guest's own extra file is gone from the slot ... */
    lr_join(b, sizeof b, guest.core.backup_path, "persist.extra.json");
    CHECK(lr_file_exists(b));  /* ... but kept in the backup, with its own version of part00 */
    lr_join(b, sizeof b, guest.core.backup_path, "persist.part00.json");
    char *own = read_text(b);
    CHECK(own && strstr(own, "own"));
    free(own);
    lr_join(b, sizeof b, guest.core.cfg.remote_dir, "profile_0");
    CHECK(!lr_dir_exists(b)); /* the host's other campaign never travelled */
    ini = read_text(guest.core.cfg.ini_path);
    char port_line[64];
    snprintf(port_line, sizeof port_line, "port=%d", guest.core.game_port);
    CHECK(ini && strstr(ini, "role=guest") && strstr(ini, "host=127.0.0.1") && strstr(ini, port_line));
    CHECK(ini && strstr(ini, host.core.password));
    free(ini);
    CHECK(lr_core_take_launch(&guest.core) && !lr_core_take_launch(&guest.core));

    /* 4b. hero ownership: the host's table (from its campaign) reaches the guest; all heroes start as the host's */
    CHECK(host.core.owners.count == 3 && host.core.owners.revision >= 1);
    CHECK(run_until(&host, &guest, owners_match, NULL, 5000));
    CHECK(guest.core.owners.count == 3 && guest.core.owners.heroes[0].id == 1 && guest.core.owners.heroes[2].id == 8);
    CHECK(strcmp(guest.core.owners.heroes[1].name, "Dismas") == 0 && guest.core.owners.heroes[2].status == 2);
    for (int i = 0; i < 3; i++) CHECK(guest.core.owners.owner[i] == LR_PLAYER_HOST);
    CHECK(lr_core_player(&host.core) == LR_PLAYER_HOST && lr_core_player(&guest.core) == LR_PLAYER_GUEST);
    ini = read_text(host.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "written_by=host") && strstr(ini, "hero.1=0") && strstr(ini, "hero.8=0"));
    free(ini);
    ini = read_text(guest.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "written_by=guest") && strstr(ini, "hero.2=0"));
    free(ini);

    /* 4c. the guest ticks Dismas: the host decides, both tables and both files say guest */
    CHECK(lr_core_claim(&guest.core, 2, 1));
    CHECK(run_until(&host, &guest, owner_is, (void *)"2g", 5000));
    ini = read_text(host.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "hero.2=1") && strstr(ini, "hero.1=0"));
    free(ini);
    ini = read_text(guest.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "hero.2=1"));
    free(ini);

    /* 4d. the guest cannot give away the host's hero, nor tick an unknown one: refused, the table comes back */
    uint32_t rev = host.core.owners.revision;
    CHECK(lr_core_claim(&guest.core, 1, 0));
    CHECK(lr_core_claim(&guest.core, 99, 1));
    run_until(&host, &guest, never, NULL, 300);
    CHECK(host.core.owners.revision == rev && guest.core.owners.revision == rev);
    CHECK(guest.core.owners.owner[0] == LR_PLAYER_HOST && guest.core.owners.count == 3);

    /* 4e. the host hands Bourneville over and takes Dismas back */
    CHECK(lr_core_claim(&host.core, 8, 0) && lr_core_claim(&host.core, 2, 1));
    CHECK(!lr_core_claim(&host.core, 2, 1)); /* already the host's */
    CHECK(run_until(&host, &guest, owner_is, (void *)"8g", 5000) && run_until(&host, &guest, owner_is, (void *)"2h", 5000));
    CHECK(run_until(&host, &guest, owners_match, NULL, 5000));

    /* Direct host assignment: all four owners use the existing revision/table path. */
    CHECK(lr_core_assign(&host.core, 8, 3));
    CHECK(run_until(&host, &guest, owners_match, NULL, 5000));
    CHECK(guest.core.owners.owner[2] == 3);
    ini = read_text(host.core.cfg.heroes_path);
    CHECK(ini && strstr(ini, "written_by=host") && strstr(ini, "hero.8=3")); free(ini);
    rev = host.core.owners.revision;
    CHECK(!lr_core_assign(&host.core, 8, 3) && !lr_core_assign(&host.core, 99, 2));
    CHECK(!lr_core_assign(&host.core, 8, -1) && !lr_core_assign(&host.core, 8, 4));
    CHECK(!lr_core_assign(&guest.core, 8, 0) && host.core.owners.revision == rev);
    host.core.cfg.max_guests = 1;
    CHECK(!lr_core_assign(&host.core, 8, 2) && host.core.owners.revision == rev);
    host.core.cfg.max_guests = 0;
    CHECK(lr_core_assign(&host.core, 8, 2) && lr_core_assign(&host.core, 8, 1));
    CHECK(run_until(&host, &guest, owners_match, NULL, 5000));

    /* 4f. the roster changes while playing (Bourneville died, Corviser recruited): owners kept, new = host */
    CHECK(lr_core_claim(&guest.core, 1, 1));
    CHECK(run_until(&host, &guest, owner_is, (void *)"1g", 5000));
    static uint8_t roster2[65536];
    static const dw_hero heroes2[] = {{"1", "Reynauld", "crusader", 8, 0}, {"2", "Dismas", "highwayman", 4, 1},
                                      {"31", "Corviser", "bounty_hunter", 0, 1}};
    size_t rn = dw_roster(roster2, heroes2, 3);
    lr_core_host_roster(&host.core, roster2, rn);
    lr_core_host_roster(&host.core, (const uint8_t *)"not a roster", 12); /* ignored */
    CHECK(run_until(&host, &guest, owners_match, NULL, 5000));
    CHECK(lr_owners_find(&guest.core.owners, 8) == -1 && lr_owners_find(&guest.core.owners, 31) == 2);
    CHECK(guest.core.owners.owner[0] == LR_PLAYER_GUEST && guest.core.owners.owner[1] == LR_PLAYER_HOST &&
          guest.core.owners.owner[2] == LR_PLAYER_HOST && guest.core.owners.heroes[0].xp == 8);

    /* 5. a second friend joins as player 2: the campaign, its own player number in ddcoop.ini, and the host's
     *    number of friends for the runtime; then it leaves again and the first friend stays */
    make_guest_campaign(guest2.core.cfg.remote_dir);
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000));
    CHECK(guest2.core.state == LR_ST_GUEST_LAUNCH && guest2.core.player == 2 && guest.core.player == 1);
    CHECK(lr_core_guest_count(&host.core) == 2 && host.core.guests[1].peer);
    ini = read_text(guest2.core.cfg.ini_path);
    CHECK(ini && strstr(ini, "role=guest") && strstr(ini, "player=2"));
    free(ini);
    ini = read_text(host.core.guests_path);
    CHECK(!ini); /* No player count chosen (max_guests 0): the runtime is not told a number. */
    free(ini);
    lr_core_stop(&guest2.core);
    for (int i = 0; i < 3; i++) { lr_core_tick(&guest2.core, now_ms()); deliver(); } /* a BYE that waited for room goes out */
    CHECK(lr_core_guest_count(&host.core) == 1 && host.core.peer && host.core.state == LR_ST_HOST_PLAYING);
    /* Once the host's game has loaded the campaign, a new friend is refused with its own reason. */
    lr_core_host_loaded(&host.core);
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest2b");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(strstr(guest2.core.error, "already loaded") && lr_core_guest_count(&host.core) == 1);
    host.core.host_loaded = 0;
    /* The host chose two players: a second friend is told the session is full. */
    host.core.cfg.max_guests = 1;
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest2c");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(lr_core_guest_count(&host.core) == 1);
    host.core.cfg.max_guests = 0;

    /* 6. the games: the host's runtime listens only after 1.5 s of "loading" */
    CHECK(lr_relay_set_epoch(&host.core.relay,91) && lr_relay_set_epoch(&guest.core.relay,91));
    host_game hg = {LR_NOSOCK, LR_NOSOCK, host.core.game_port, now_ms() + 1500, 0, 0};
    guest_game gg = {lr_connect_local_nb(guest.core.game_port), 0, 0, 0, 0};
    CHECK(gg.s != LR_NOSOCK);
    uint64_t end = now_ms() + 30000;
    while (now_ms() < end && gg.got < PUSH_BYTES && !gg.bad) {
        lr_core_tick(&host.core, now_ms());
        lr_core_tick(&guest.core, now_ms());
        deliver();
        host_game_tick(&hg);
        guest_game_tick(&gg);
    }
    printf("relay: %ld bytes up, %ld echoed back, host game echoed %ld\n", gg.sent, gg.got, hg.echoed);
    CHECK(!gg.bad && gg.got == PUSH_BYTES);
    CHECK(host.core.state == LR_ST_HOST_PLAYING);

    /* 7. the guest's game closes: the host's game must see the end too */
    lr_close(gg.s);
    end = now_ms() + 5000;
    while (now_ms() < end && !hg.saw_eof) {
        lr_core_tick(&host.core, now_ms());
        lr_core_tick(&guest.core, now_ms());
        deliver();
        host_game_tick(&hg);
    }
    CHECK(hg.saw_eof);
    end = now_ms() + 2000;
    while (now_ms() < end && (lr_relay_active_streams(&host.core.relay) || lr_relay_active_streams(&guest.core.relay))) {
        lr_core_tick(&host.core, now_ms());
        lr_core_tick(&guest.core, now_ms());
        deliver();
    }
    CHECK(lr_relay_active_streams(&host.core.relay) == 0 && lr_relay_active_streams(&guest.core.relay) == 0);
    lr_close(hg.listener);
    backpressure = 0;

    /* 8. the guest leaves; the host waits for a new friend */
    lr_core_stop(&guest.core);
    deliver();
    CHECK(host.core.state == LR_ST_HOST_WAITING && host.core.peer == 0);
    ini = read_text(guest.core.cfg.ini_path);
    CHECK(ini && strstr(ini, "role=off"));
    free(ini);

    /* 9. a corrupted chunk: the guest refuses and keeps its own save */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest3");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    corrupt_chunks = 1;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000));
    CHECK(guest2.core.state == LR_ST_FAILED && strstr(guest2.core.error, "incomplete"));
    lr_join(b, sizeof b, guest2.core.cfg.remote_dir, "profile_2");
    lr_join(a, sizeof a, b, "persist.extra.json");
    CHECK(lr_file_exists(a));
    deliver();
    CHECK(host.core.state == LR_ST_HOST_WAITING && host.core.peer == 0);

    /* 10. a different game build is refused by the host */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest4");
    guest2.core.cfg.build = 0x12345678;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(strstr(guest2.core.error, "versions"));

    /* 10b. another DDCoop kit (another runtime) is refused with a message the friend can act on */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest4b");
    guest2.core.cfg.kit = 0x9d760035;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(strstr(guest2.core.error, "DDCoop versions"));

    /* 11. joining with the game already open is refused before anything is touched */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest5");
    guest2.game_running = 1;
    CHECK(!lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(strstr(guest2.core.error, "Close Darkest Dungeon first"));

    /* 11b. a guest without the campaign's DLC is refused before its own save is touched (the game would call
     * the campaign corrupted and start a new one in that slot) */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest5b");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    guest2.no_dlc = 1;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000));
    CHECK(guest2.core.state == LR_ST_FAILED && strstr(guest2.core.error, "The Musketeer (gratis)"));
    lr_join(b, sizeof b, guest2.core.cfg.remote_dir, "profile_2");
    lr_join(a, sizeof a, b, "persist.extra.json");
    CHECK(lr_file_exists(a)); /* own save untouched ... */
    lr_join(a, sizeof a, b, "persist.game.json");
    CHECK(!lr_file_exists(a));
    CHECK(!guest2.core.backup_path[0] && !lr_dir_exists(guest2.core.cfg.backup_root)); /* ... so no backup needed */
    ini = read_text(guest2.core.cfg.ini_path);
    CHECK(!ini || !strstr(ini, "role=guest"));
    free(ini);
    deliver();
    CHECK(host.core.state == LR_ST_HOST_WAITING && host.core.peer == 0 && strstr(host.core.status, "DLC"));

    /* 12. a silent host is noticed after 20 s (clock jumped forward) */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest6");
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    drop_queue();
    lr_core_tick(&guest2.core, now_ms() + LR_PEER_TIMEOUT_MS + 1000);
    CHECK(guest2.core.state == LR_ST_FAILED && strstr(guest2.core.error, "20 s"));

    /* 12b. a game whose session ended takes no new guest, and the friend hears why */
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "guest7");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    lr_core_host_spent(&host.core);
    CHECK(host.core.host_spent && strstr(host.core.status, "Host again"));
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, failed, &guest2, 3000));
    CHECK(strstr(guest2.core.error, "Host again") && host.core.peer == 0);
    lr_join(b, sizeof b, guest2.core.cfg.remote_dir, "profile_2");
    lr_join(a, sizeof a, b, "persist.extra.json");
    CHECK(lr_file_exists(a) && !guest2.core.backup_path[0]); /* Nothing of the friend's touched. */

    /* 13. the host stops: ini back to role=off; the heroes file stays */
    lr_core_stop(&host.core);
    ini = read_text(host.core.cfg.ini_path);
    CHECK(ini && strstr(ini, "role=off"));
    free(ini);
    CHECK(host.core.owners.count == 0 && lr_file_exists(host.core.cfg.heroes_path));

    CHECK(lr_core_host_start(&host.core, now_ms()) && !host.core.host_spent); /* A new game: a new session. */
    lr_core_stop(&host.core);
    /* 14. hosting again restores the host's earlier choices (Reynauld stays the guest's) ... */
    CHECK(lr_core_host_start(&host.core, now_ms()));
    CHECK(host.core.owners.count == 3 && host.core.owners.owner[lr_owners_find(&host.core.owners, 1)] == LR_PLAYER_GUEST);
    lr_core_stop(&host.core);
    /* ... but never choices from a file this PC got as a guest of another campaign */
    static const char guest_file[] = "version=1\r\nwritten_by=guest\r\nhero.1=1\r\n";
    CHECK(lr_write_file(host.core.cfg.heroes_path, (const uint8_t *)guest_file, sizeof guest_file - 1));
    CHECK(lr_core_host_start(&host.core, now_ms()));
    CHECK(host.core.owners.count == 3);
    for (int i = 0; i < host.core.owners.count; i++) CHECK(host.core.owners.owner[i] == LR_PLAYER_HOST);
    lr_core_stop(&host.core);

    /* 16. Managed preparation preserves the entire original root + backup tree before asking
     * the Windows coordinator to import through Steam. It never overwrites raw Steam files first. */
    CHECK(lr_core_host_start(&host.core, now_ms()));
    lr_core_free(&guest2.core);
    setup_node(&guest2, 4, "managed-guest");
    make_guest_campaign(guest2.core.cfg.remote_dir);
    char managed_dir[LR_PATH_MAX], nested[LR_PATH_MAX], journal[LR_PATH_MAX];
    lr_join(managed_dir, sizeof managed_dir, guest2.core.cfg.remote_dir, "profile_2");
    lr_join(nested, sizeof nested, managed_dir, "backup");
    CHECK(lr_make_dirs(nested));
    write_text(nested, "persist.game.json", "old guest backup");
    lr_join(journal, sizeof journal, root, "guest-original.bin");
    snprintf(guest2.core.cfg.restore_path, sizeof guest2.core.cfg.restore_path, "%s", journal);
    guest2.core.cfg.steam_user = 4;
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000) && guest2.core.state == LR_ST_GUEST_LAUNCH);
    lr_snapshot *recovery = calloc(1, sizeof *recovery);
    CHECK(lr_snapshot_read(journal, recovery) && recovery->user == 4 && recovery->count == 3);
    int original_found = 0, backup_found = 0;
    for (uint32_t i = 0; i < recovery->count; i++) {
        if (!strcmp(recovery->files[i].name, "persist.part00.json")) original_found = 1;
        if (!strcmp(recovery->files[i].name, "backup/persist.game.json")) backup_found = 1;
    }
    CHECK(original_found && backup_found);
    lr_snapshot_free(recovery);
    free(recovery);
    lr_join(a, sizeof a, managed_dir, "persist.part00.json");
    own = read_text(a);
    CHECK(own && strstr(own, "own"));
    free(own);
    lr_join(a, sizeof a, nested, "persist.game.json");
    own = read_text(a);
    CHECK(own && !strcmp(own, "old guest backup"));
    free(own);
    lr_core_stop(&guest2.core);
    CHECK(lr_file_exists(journal)); /* stop/crash must not discard recovery */
    lr_core_stop(&host.core);
    drop_queue();

    /* 17. A pending original is never replaced by the next borrowed campaign. */
    CHECK(lr_core_host_start(&host.core, now_ms()));
    CHECK(lr_core_guest_join(&guest2.core, 1, now_ms()));
    CHECK(run_until(&host, &guest2, state_is, &guest2, 5000) && guest2.core.state == LR_ST_FAILED);
    recovery = calloc(1, sizeof *recovery);
    CHECK(lr_snapshot_read(journal, recovery) && recovery->count == 3);
    lr_snapshot_free(recovery);
    free(recovery);
    lr_core_stop(&host.core);

    /* 15. a campaign still at the start of the tutorial is never hosted: as the game saved it ... */
    char gpath[LR_PATH_MAX];
    static uint8_t game[65536];
    lr_join(gpath, sizeof gpath, hdir, "persist.game.json");
    size_t gn = dw_game(game, 1, "tutorial", NULL, 0);
    CHECK(lr_write_file(gpath, game, gn));
    CHECK(!lr_core_host_start(&host.core, now_ms()));
    CHECK(host.core.state == LR_ST_FAILED && strstr(host.core.error, "tutorial") && host.core.file_count == 0);
    /* ... or still the template the game copies into a new campaign */
    static const char tmpl[] = "{\n    \"version\": 2,\n    \"data\": {\n        \"inraid\": true,\n"
                               "        \"raiddungeon\": \"tutorial\"\n    }\n}";
    CHECK(lr_write_file(gpath, (const uint8_t *)tmpl, sizeof tmpl - 1));
    CHECK(!lr_core_host_start(&host.core, now_ms()) && strstr(host.core.error, "tutorial"));
    /* a normal raid is fine */
    gn = dw_game(game, 1, "crypts", NULL, 0);
    CHECK(lr_write_file(gpath, game, gn));
    CHECK(lr_core_host_start(&host.core, now_ms()));
    lr_core_stop(&host.core);

    /* 16. the host chooses from what is there, in menu order; a folder without a campaign is none */
    {
        char d1[LR_PATH_MAX], d9[LR_PATH_MAX], empty[LR_PATH_MAX];
        lr_campaign_entry list[9];
        lr_join(d1, sizeof d1, host.core.cfg.remote_dir, "profile_1");
        lr_join(empty, sizeof empty, host.core.cfg.remote_dir, "profile_3");
        lr_join(d9, sizeof d9, host.core.cfg.remote_dir, "profile_9");
        lr_make_dirs(d1);
        lr_make_dirs(empty);
        lr_make_dirs(d9);
        lr_join(gpath, sizeof gpath, d1, "persist.game.json");
        gn = dw_game(game, 1, "tutorial", NULL, 0);
        CHECK(lr_write_file(gpath, game, gn));
        lr_join(gpath, sizeof gpath, d9, "persist.game.json");
        gn = dw_game(game, 0, "", NULL, 0);
        CHECK(lr_write_file(gpath, game, gn));
        int count = lr_list_campaigns(host.core.cfg.remote_dir, list);
        /* All nine are there; what has no campaign says so. profile_9 is no slot at all. */
        CHECK(count == 2);
        for (int i = 0; i < 9; i++) CHECK(list[i].slot == i + 1);
        CHECK(list[0].empty && !list[0].unreadable && !list[0].tutorial);
        CHECK(!list[1].empty && !list[1].unreadable && list[1].tutorial && !strcmp(list[1].estate, "Darkest"));
        CHECK(!list[2].empty && !list[2].tutorial);
        CHECK(list[3].empty && !list[3].unreadable); /* The folder is there, without a campaign. */
        CHECK(list[8].empty);
        /* A file that cannot be read is not called empty: it may be someone's save. */
        write_text(empty, "persist.game.json", "not a save");
        count = lr_list_campaigns(host.core.cfg.remote_dir, list);
        CHECK(count == 2 && !list[3].empty && list[3].unreadable);
        lr_join(gpath, sizeof gpath, empty, "persist.game.json");
        lr_delete_file(gpath);
        lr_join(gpath, sizeof gpath, d1, "persist.game.json");
        gn = dw_game(game, 1, "crypts", NULL, 0);
        CHECK(lr_write_file(gpath, game, gn));

        /* 17. the host's slot 2 ends up in the guest's slot 2, and the guest's recovery knows that */
        static node guest3;
        char journal3[LR_PATH_MAX], other[LR_PATH_MAX];
        setup_node(&guest3, 5, "guest3");
        lr_join(other, sizeof other, guest3.core.cfg.remote_dir, "profile_1");
        lr_make_dirs(other);
        write_text(other, "persist.part00.json", "own campaign");
        lr_join(journal3, sizeof journal3, root, "guest3-original.bin");
        snprintf(guest3.core.cfg.restore_path, sizeof guest3.core.cfg.restore_path, "%s", journal3);
        host.core.cfg.coop_slot = 2;
        CHECK(lr_core_host_start(&host.core, now_ms()));
        ini = read_text(host.core.cfg.ini_path);
        CHECK(ini && strstr(ini, "coop_slots=2"));
        free(ini);
        CHECK(!strcmp(host.core.save_folder, "profile_1"));
        CHECK(lr_core_guest_join(&guest3.core, 1, now_ms()));
        CHECK(run_until(&host, &guest3, state_is, &guest3, 10000));
        CHECK(guest3.core.state == LR_ST_GUEST_LAUNCH && guest3.core.cfg.coop_slot == 2 && !strcmp(guest3.core.save_folder, "profile_1"));
        CHECK(strstr(guest3.core.status, "slot 2"));
        ini = read_text(guest3.core.cfg.ini_path);
        CHECK(ini && strstr(ini, "coop_slots=2"));
        free(ini);
        {
            lr_snapshot *kept = calloc(1, sizeof *kept);
            CHECK(lr_snapshot_read(journal3, kept) && !strcmp(kept->folder, "profile_1") && kept->count == 1);
            lr_snapshot_free(kept);
            free(kept);
        }
        lr_core_stop(&host.core);
        lr_core_free(&guest3.core);
        lr_delete_file(journal3);
        drop_queue();

        /* 18. a host cannot make a guest replace anything else: the restore area, another slot, or an unknown slot */
        static const struct { const char *folder; uint32_t session; int ok; } hostile[] = {
            {"profile_9", 3, 0}, {"steam_profile_9", 3, 0}, {"profile_0", 3, 0}, {"../profile_2", 3, 0}, {"profile_9", 10, 0}, {"profile_2", 0, 0}, {"profile_8", 9, 1}};
        for (size_t i = 0; i < sizeof hostile / sizeof hostile[0]; i++) {
            uint8_t wire[LR_MAX_MSG];
            lr_msg m;
            setup_node(&guest3, 5, "guest3");
            CHECK(lr_core_guest_join(&guest3.core, 1, now_ms()));
            memset(&m, 0, sizeof m);
            m.type = LR_SESSION;
            m.a = hostile[i].session;
            memset(m.text, 'a', LR_PASSWORD_LEN);
            size_t len = lr_encode(&m, wire);
            CHECK(len > 0);
            lr_core_on_message(&guest3.core, 1, LR_CH_CONTROL, wire, len, now_ms());
            /* An unknown slot does not even decode: no session, and nothing is received without one. */
            CHECK((guest3.core.password[0] != 0) == (hostile[i].session >= 1 && hostile[i].session <= 9));
            memset(&m, 0, sizeof m);
            m.type = LR_SAVE_BEGIN;
            m.a = 1;
            m.b = 1;
            snprintf(m.text, sizeof m.text, "%s", hostile[i].folder);
            len = lr_encode(&m, wire);
            if (len) lr_core_on_message(&guest3.core, 1, LR_CH_CONTROL, wire, len, now_ms());
            CHECK((guest3.core.state == LR_ST_GUEST_RECEIVING) == hostile[i].ok);
            lr_core_free(&guest3.core);
            drop_queue();
        }
    }

    lr_core_free(&intruder.core);
    lr_core_free(&guest.core);
    lr_core_free(&guest2.core);
    drop_queue();
    test_automatic_transfer_restart();
    test_campaign_metadata_refusal();
    test_campaign_mods();
    printf(failures ? "test_lr_core: %d FAILURES\n" : "test_lr_core: all passed\n", failures);
    return failures != 0;
}
