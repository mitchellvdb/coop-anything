/* Ordered prop choices: real input hooks/FIFO/codec, modeled native effects.
 * Native callback-address reachability is proved separately against the EXE. */
enum { PCQ_HAND=0, PCQ_TOOL=1, PCQ_CURIO_HAND=2, PCQ_CURIO_TOOL=3, PCQ_CANCEL=4, PCQ_CURIO_CANCEL=5, PCQ_DIRECT_ITEM=6 };
typedef struct { uint32_t hp[4],stress[4],rules,tools,calls,damage,used,cancels,bad,selected,native_null; } pcq_snapshot;
static uint8_t pcq_bag[0x5a8],pcq_rules[16];
static uint8_t pcq_drop_ui[0xd0],pcq_drop_data[8+0x5a8];
static uint8_t pcq_option_data[0x218],pcq_option_row[0xa0],pcq_long_prop[0x180];
static uint64_t pcq_option_rows[1];
static uint64_t pcq_match_table[2];
static int pcq_match_result;
static unsigned pcq_match_calls;
static pcq_snapshot pcq_state;
static unsigned pcq_route;
static const uint32_t pcq_tool_type=41,pcq_tool_id=42;
static int __fastcall pcq_native_match(const void *options,const char *variation,uint32_t type,uint32_t id) {
    CHECK(options==pcq_option_data && variation==(const char *)wk_props[1]+0x125 &&
          !strcmp(variation,"choice") && type==pcq_tool_type && id==pcq_tool_id);
    pcq_match_calls++;return pcq_match_result;
}
static int pcq_is_curio(unsigned route) {return route==PCQ_CURIO_HAND || route==PCQ_CURIO_TOOL || route==PCQ_CURIO_CANCEL || route==PCQ_DIRECT_ITEM;}
static uintptr_t pcq_panel_at(void) {return (uintptr_t)wk_display+DD_WALK_OVERLAY;}
static uint32_t pcq_mode(void) {uint32_t mode=0;memcpy(&mode,(void *)(pcq_panel_at()+0xd88),4);return mode;}
static void pcq_panel(unsigned mode) {
    uint64_t panel=pcq_panel_at(),prop=(uintptr_t)wk_props[1],zero=0;
    putint((void *)(panel+0xd88),mode);
    putptr((void *)(panel+0x6a0),pcq_is_curio(pcq_route)?prop:zero);
    putptr((void *)(panel+0xd70),pcq_is_curio(pcq_route)?zero:prop);
    putptr((void *)(panel+0xd78),bt_party[2]);
}
static void pcq_effect(unsigned kind) {
    pcq_state.calls++;
    if(pcq_mode()!=2) {pcq_state.bad++;return;}
    uint64_t native_hero=0;memcpy(&native_hero,(void *)(pcq_panel_at()+0xd78),8);
    if(!native_hero)pcq_state.native_null++;
    if(kind==PCQ_CANCEL || kind==PCQ_CURIO_CANCEL) pcq_state.cancels++;
    else {
        uint32_t rules=0;memcpy(&rules,pcq_rules+8,4);rules=rules*1664525u+1013904223u;putint(pcq_rules+8,rules);
        if(kind==PCQ_HAND) {
            pcq_state.damage++;
            for(unsigned i=0;i<4;i++) {pcq_state.hp[i]-=5;pcq_state.stress[i]+=10;}
        } else if(kind==PCQ_TOOL || kind==PCQ_CURIO_TOOL) {
            uint32_t count=0;memcpy(&count,pcq_bag,4);CHECK(count>0);
            if(count)count--;putint(pcq_bag,count);pcq_state.used++;
        } else if(kind==PCQ_DIRECT_ITEM)pcq_state.used++; /* The adapter owns the canonical debit. */
        else pcq_state.stress[2]+=rules%5;
    }
    putint((void *)(pcq_panel_at()+0xd88),3); /* native fade, not an immediate reset */
}
static void __fastcall pcq_hand(void *functor) {
    uint64_t panel=0;memcpy(&panel,(uint8_t *)functor+8,8);CHECK(panel==pcq_panel_at());pcq_effect(PCQ_HAND);
}
static void __fastcall pcq_tool(void *capture) {
    uint64_t panel=0;uint32_t type=0,id=0;
    memcpy(&panel,capture,8);memcpy(&type,(uint8_t *)capture+0x4c,4);memcpy(&id,(uint8_t *)capture+0x90,4);
    CHECK(panel==pcq_panel_at() && type==pcq_tool_type && id==pcq_tool_id);pcq_effect(PCQ_TOOL);
}
static void __fastcall pcq_curio(void *functor) {
    uint64_t panel=0;uint32_t type=0,id=0;
    memcpy(&panel,(uint8_t *)functor+8,8);CHECK(panel==pcq_panel_at());
    memcpy(&type,(void *)(panel+0x76c),4);memcpy(&id,(void *)(panel+0x770),4);
    CHECK(!type || (type==pcq_tool_type && id==pcq_tool_id));pcq_effect(type?PCQ_CURIO_TOOL:PCQ_CURIO_HAND);
}
static void __fastcall pcq_cancel_native(void *functor) {
    uint64_t panel=0;memcpy(&panel,(uint8_t *)functor+8,8);CHECK(panel==pcq_panel_at());pcq_effect(PCQ_CANCEL);
}
static uint8_t __fastcall pcq_direct_item(void *overlay,void *item) {
    CHECK((uintptr_t)overlay==pcq_panel_at() && item==pcq_bag);
    pcq_effect(PCQ_DIRECT_ITEM);return 1;
}
static uint8_t __fastcall pcq_open(void *display,void *prop,int32_t option,uint8_t flag) {
    (void)option;(void)flag;CHECK(display==wk_display && prop==wk_props[1]);
    if(pcq_mode()) {pcq_state.bad++;return 0;}
    pcq_panel(2);return 1;
}
static void pcq_reset(int role,unsigned player,unsigned route) {
    it_reset(role);pcq_route=route;g_cfg.player=role==DD_ROLE_GUEST?player:0;
    g_town.next_request=role==DD_ROLE_GUEST?(player<<24)|1u:1;
    memset(&pcq_state,0,sizeof pcq_state);memset(pcq_bag,0,sizeof pcq_bag);memset(pcq_rules,0,sizeof pcq_rules);
    for(unsigned i=0;i<4;i++){pcq_state.hp[i]=100+i;pcq_state.stress[i]=20+i;}
    putint(pcq_rules+8,0x12345678);putint(pcq_bag,2);putint(pcq_bag+0x44,pcq_tool_type);putint(pcq_bag+0x88,pcq_tool_id);
    putptr(wk_raid_lists+DD_WALK_RAID_INVENTORY+8,(uintptr_t)pcq_bag);
    putptr(wk_raid_lists+DD_WALK_RAID_INVENTORY+0x10,(uintptr_t)(pcq_bag+sizeof pcq_bag));
    memset(pcq_option_data,0,sizeof pcq_option_data);memset(pcq_option_row,0,sizeof pcq_option_row);
    pcq_option_rows[0]=(uintptr_t)pcq_option_row;
    putptr(pcq_option_data+0x208,(uintptr_t)pcq_option_rows);
    putptr(pcq_option_data+0x210,(uintptr_t)(pcq_option_rows+1));
    putptr(wk_props[1]+0xd0,(uintptr_t)pcq_option_data);memcpy(wk_props[1]+0x125,"choice",7);
    pcq_match_result=0;pcq_match_calls=0;g_prop_item_match=pcq_native_match;
    memset((void *)pcq_panel_at(),0,0xd90);putptr(wk_raid_lists+0x438,bt_party[2]);
    g_walk_selected=2;g_overlay_use=pcq_hand;g_curio_tool=pcq_tool;g_curio_button=pcq_curio;
    g_curio_item=pcq_direct_item;g_item_curio_route=NULL;
    g_prop_cancel=g_curio_cancel=pcq_cancel_native;g_raid_use=pcq_open;
}
static pcq_snapshot pcq_read_state(void) {
    pcq_snapshot copy=pcq_state;uint64_t selected=0;
    memcpy(&copy.rules,pcq_rules+8,4);memcpy(&copy.tools,pcq_bag,4);
    memcpy(&selected,wk_raid_lists+0x438,8);copy.selected=battle_hero_id(selected);return copy;
}
static dd_msg pcq_wire(dd_msg m) {
    uint8_t bytes[DD_PROTO_HEADER+DD_PROTO_MAX_BODY];dd_msg out={0};size_t used=0;
    size_t n=dd_encode(&m,bytes,sizeof bytes);
    CHECK(n && dd_decode(bytes,n,&out,&used)==DD_DECODE_OK && used==n);return out;
}
static void pcq_click(unsigned route) {
    uint64_t panel=pcq_panel_at(),functor[2]={0,panel};uint8_t capture[8+0x5a8]={0};
    if(route==PCQ_DIRECT_ITEM) {
        memset(pcq_drop_ui,0,sizeof pcq_drop_ui);memset(pcq_drop_data,0,sizeof pcq_drop_data);
        putptr(pcq_drop_ui+0x30,(uintptr_t)pcq_bag);pcq_drop_ui[0xc]=pcq_drop_ui[0xc0]=1;
        putptr(pcq_drop_data,image_base()+0xeddd10);memcpy(pcq_drop_data+8,pcq_bag,sizeof pcq_bag);
        putptr((void *)(panel+0x780),image_base()+0xec85b0);
        prop_item_pickup(1,pcq_drop_ui,pcq_drop_data);
        int tracked=g_prop_item_drag.tracked;
        CHECK(hook_prop_item_drop((void *)(panel+0x780),NULL,pcq_drop_data)==0);
        CHECK(prop_item_source_done(pcq_drop_ui,pcq_drop_data)==tracked);
        int32_t left=0;memcpy(&left,pcq_bag,4);
        if(tracked)CHECK(!pcq_drop_ui[0xc0] && pcq_drop_ui[0xc]==(uint8_t)(left>0));
    } else if(route==PCQ_HAND)hook_overlay_use(functor);
    else if(route==PCQ_TOOL) {
        memcpy(capture,&panel,8);memcpy(capture+0x4c,&pcq_tool_type,4);memcpy(capture+0x90,&pcq_tool_id,4);hook_curio_tool(capture);
    } else if(route==PCQ_CANCEL)hook_prop_cancel(functor);
    else if(route==PCQ_CURIO_CANCEL)hook_curio_cancel(functor);
    else {
        putint((void *)(panel+0x76c),route==PCQ_CURIO_TOOL?pcq_tool_type:0);
        putint((void *)(panel+0x770),route==PCQ_CURIO_TOOL?pcq_tool_id:0);
        hook_curio_button(functor);
    }
}
/* Start with the same real shared OPEN, so a future per-popup generation is
 * built by production code rather than invented in this fixture. */
static dd_msg pcq_begin(unsigned route) {
    pcq_reset(DD_ROLE_HOST,0,route);
    CHECK(hook_raid_use(wk_display,wk_props[1],0,0) && sent>=1 && pcq_mode()==2);
    CHECK(last_message.type==DD_MSG_TOWN_APPLY && last_message.u.town.action.kind==DD_ACT_USE);
    return pcq_wire(last_message);
}
static void pcq_receive_open(const dd_msg *opened,unsigned player,unsigned route) {
    pcq_reset(DD_ROLE_GUEST,player,route);CHECK(queue_push(&g_incoming,opened));dd_runtime_poll();
    CHECK(pcq_mode()==2 && g_town.applied_seq==opened->seq && !pcq_state.calls && !g_town.needs_resync);
}
static void pcq_cross_choice_matrix(void) {
    static const unsigned obstacle[]={PCQ_HAND,PCQ_TOOL,PCQ_CANCEL};
    static const unsigned curio[]={PCQ_CURIO_HAND,PCQ_CURIO_TOOL,PCQ_CURIO_CANCEL,PCQ_DIRECT_ITEM};
    unsigned cases=0;
    for(unsigned family=0;family<2;family++) {
        const unsigned *routes=family?curio:obstacle;unsigned count=family?4:3;
        for(unsigned winning=0;winning<count;winning++)for(unsigned competing=0;competing<count;competing++)
        for(unsigned initiator=0;initiator<4;initiator++) {
            unsigned winner=routes[winning],rival=routes[competing],rival_player=initiator==3?1:3;
            /* Capture a real rival click while the original popup/revision is
             * still active, before the winner has executed anywhere. */
            dd_msg opened=pcq_begin(rival),approved[8],request={0};
            pcq_receive_open(&opened,rival_player,rival);g_rules_object=pcq_rules;pcq_click(rival);
            dd_msg crossed=pcq_wire(last_message);
            CHECK(crossed.type==DD_MSG_TOWN_REQUEST && !pcq_state.calls);
            opened=pcq_begin(winner);unsigned first=(unsigned)sent,n=0;
            if(initiator) {
                pcq_receive_open(&opened,initiator,winner);g_rules_object=pcq_rules;
                pcq_snapshot before=pcq_read_state();pcq_click(winner);request=pcq_wire(last_message);
                pcq_snapshot after=pcq_read_state();CHECK(!memcmp(&before,&after,sizeof before));
                pcq_begin(winner);g_rules_object=pcq_rules;first=(unsigned)sent;
                CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
            } else {g_rules_object=pcq_rules;pcq_click(winner);}
            pcq_snapshot want=pcq_read_state();
            CHECK(want.calls==1 && !want.bad && !g_town.needs_resync && pcq_mode()==3);
            CHECK(want.tools==((winner==PCQ_TOOL || winner==PCQ_CURIO_TOOL || winner==PCQ_DIRECT_ITEM)?1u:2u));
            for(unsigned j=first;j<(unsigned)sent;j++)if(sent_messages[j].type==DD_MSG_TOWN_APPLY) {
                CHECK(n<8);if(n<8)approved[n++]=pcq_wire(sent_messages[j]);
            }
            CHECK(n && approved[n-1].u.town.action.kind==DD_ACT_USE &&
                  crossed.u.town.action.u.use.revision==approved[n-1].u.town.action.u.use.revision);
            /* Neither hand-vs-tool nor X-vs-use may alter selection, native
             * counters, canonical item count, HP/stress or RNG after first win. */
            unsigned accepted=g_town.accepted,rejected=g_town.rejected;
            CHECK(queue_push(&g_incoming,&crossed));dd_runtime_poll();pcq_snapshot after=pcq_read_state();
            CHECK(!memcmp(&want,&after,sizeof want) && !g_town.needs_resync &&
                  g_town.accepted==accepted && g_town.rejected==rejected+1 &&
                  last_message.type==DD_MSG_TOWN_REJECT &&
                  last_message.u.reject.request_id==crossed.u.town.request_id);
            for(unsigned receiver=1;receiver<4;receiver++) {
                pcq_receive_open(&opened,receiver,winner);g_rules_object=pcq_rules;
                for(unsigned j=0;j<n;j++)CHECK(queue_push(&g_incoming,&approved[j]));dd_runtime_poll();
                pcq_snapshot got=pcq_read_state();
                CHECK(!g_town.needs_resync && !g_incoming.count && !memcmp(&want,&got,sizeof want));
                dd_runtime_poll();CHECK(pcq_state.calls==1);
            }
            cases++;
        }
    }
    CHECK(cases==100);
    puts("Prop cross-choice matrix: 25 same-panel hand/tool/X/item pairs x4 first initiators, each with3 receivers; captured rival revision refused without extra native/selection/HP/stress/tool/RNG effects");
}
static unsigned pcq_route_calls;
static uint8_t pcq_route_to_curio;
static uint8_t __fastcall pcq_native_route(void *controller,uint32_t type,uint32_t id) {
    CHECK(controller==(void *)(uintptr_t)0x1234 && type==pcq_tool_type && id==pcq_tool_id);
    pcq_route_calls++;return pcq_route_to_curio;
}
static void pcq_item_route_checks(void) {
    dd_action ordinary={0};
    for(unsigned curio=0;curio<2;curio++)for(unsigned initiator=0;initiator<4;initiator++) {
        dd_msg opened=pcq_begin(PCQ_DIRECT_ITEM),approved[8],request={0};unsigned first=(unsigned)sent,n=0;
        if(initiator)pcq_receive_open(&opened,initiator,PCQ_DIRECT_ITEM);
        g_rules_object=pcq_rules;g_item_curio_route=pcq_native_route;pcq_route_to_curio=(uint8_t)curio;pcq_route_calls=0;
        CHECK(hook_item_use((void *)(uintptr_t)0x1234,bt_heroes[2],wk_raid_lists+DD_WALK_RAID_INVENTORY,0,1)==(uint64_t)(!curio && !initiator));
        CHECK(pcq_route_calls>0 && last_message.type==(initiator?DD_MSG_TOWN_REQUEST:DD_MSG_TOWN_APPLY));
        if(curio)CHECK(last_message.u.town.action.kind==DD_ACT_USE && last_message.u.town.action.u.use.door==7);
        else {
            CHECK(last_message.u.town.action.kind==DD_ACT_ITEM && last_message.u.town.action.u.item.op==DD_ITEM_OP_USE);
            ordinary=last_message.u.town.action;
        }
        if(initiator) {
            CHECK(!pcq_state.calls && !it_uses && pcq_read_state().tools==2);
            request=pcq_wire(last_message);pcq_begin(PCQ_DIRECT_ITEM);g_rules_object=pcq_rules;
            g_item_curio_route=pcq_native_route;pcq_route_to_curio=(uint8_t)curio;first=(unsigned)sent;
            CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
        }
        CHECK(!g_town.needs_resync && pcq_state.calls==curio && it_uses==!curio);
        pcq_snapshot want=pcq_read_state();CHECK(want.tools==(curio?1u:2u));
        for(unsigned j=first;j<(unsigned)sent;j++)if(sent_messages[j].type==DD_MSG_TOWN_APPLY) {
            CHECK(n<8);if(n<8)approved[n++]=pcq_wire(sent_messages[j]);
        }
        CHECK(n);
        for(unsigned receiver=1;receiver<4;receiver++) {
            pcq_receive_open(&opened,receiver,PCQ_DIRECT_ITEM);g_rules_object=pcq_rules;
            g_item_curio_route=pcq_native_route;pcq_route_to_curio=(uint8_t)curio;
            for(unsigned j=0;j<n;j++)CHECK(queue_push(&g_incoming,&approved[j]));dd_runtime_poll();
            pcq_snapshot got=pcq_read_state();
            CHECK(!g_town.needs_resync && !g_incoming.count && !memcmp(&want,&got,sizeof want) && it_uses==!curio);
        }
    }
    /* The native predicate can change after an ordinary item request was made.
     * Do not ACK/reinterpret such a delayed item use as an unstamped curio use. */
    for(unsigned receiver=1;receiver<4;receiver++) {
        dd_msg opened=pcq_begin(PCQ_DIRECT_ITEM);pcq_receive_open(&opened,receiver,PCQ_DIRECT_ITEM);
        g_rules_object=pcq_rules;g_item_curio_route=pcq_native_route;pcq_route_to_curio=1;pcq_route_calls=0;
        pcq_snapshot before=pcq_read_state();wk_push(opened.seq+1,&ordinary);dd_runtime_poll();pcq_snapshot after=pcq_read_state();
        CHECK(!memcmp(&before,&after,sizeof before) && !it_uses && pcq_route_calls>0 &&
              g_town.needs_resync && g_town.applied_seq==opened.seq && g_sync_stop_sent);
    }
    g_item_curio_route=NULL;
    puts("Prop item routing: native predicate true uses shared revisioned USE7, false retains ordinary hero/food ITEM_USE; all4 initiators/3 receivers and changed apply-side routing refused without effects");
}
static void pcq_item_match_checks(void) {
    dd_msg opened=pcq_begin(PCQ_DIRECT_ITEM);pcq_receive_open(&opened,2,PCQ_DIRECT_ITEM);pcq_click(PCQ_DIRECT_ITEM);
    dd_msg request=pcq_wire(last_message);CHECK(request.type==DD_MSG_TOWN_REQUEST && !pcq_state.calls);
    for(unsigned broken=0;broken<14;broken++) {
        pcq_begin(PCQ_DIRECT_ITEM);g_rules_object=pcq_rules;
        if(broken==0)pcq_match_result=-1; /* native option lookup: item unsupported */
        if(broken==1)putptr(wk_props[1]+0xd0,0);
        if(broken==2)g_prop_item_match=NULL;
        if(broken==3)putptr(pcq_option_data+0x210,(uintptr_t)pcq_option_rows-8);
        if(broken==4)putptr(pcq_option_data+0x210,(uintptr_t)pcq_option_rows+9);
        if(broken==5)putptr(pcq_option_data+0x210,(uintptr_t)pcq_option_rows+4097u*8u);
        if(broken==6) {putptr(pcq_option_data+0x208,0);putptr(pcq_option_data+0x210,8);}
        if(broken==7) {
            memset(pcq_long_prop,0,sizeof pcq_long_prop);memcpy(pcq_long_prop,wk_props[1],sizeof wk_props[1]);
            memset(pcq_long_prop+0x125,'x',64);wk_prop_list[1]=(uintptr_t)pcq_long_prop;
            putptr((void *)(pcq_panel_at()+0x6a0),(uintptr_t)pcq_long_prop);
        }
        if(broken==8) {putptr(pcq_option_data+0x208,1);putptr(pcq_option_data+0x210,9);}
        if(broken==9)pcq_option_rows[0]=0;
        if(broken==10)pcq_option_rows[0]=1;
        if(broken>=11)putint(pcq_option_row+0x90,pcq_tool_type);
        if(broken==12) {
            pcq_match_table[0]=0;pcq_match_table[1]=(uintptr_t)pcq_option_data; /* committed, not executable */
            putptr(pcq_option_row,(uintptr_t)pcq_match_table);
        }
        if(broken==13)putptr(pcq_option_row,1);
        unsigned accepted=g_town.accepted,rejected=g_town.rejected;pcq_snapshot before=pcq_read_state();
        CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();pcq_snapshot after=pcq_read_state();
        CHECK(!memcmp(&before,&after,sizeof before) && pcq_mode()==2 && !g_town.needs_resync &&
              !g_sync_stop_sent && g_town.accepted==accepted && g_town.rejected==rejected+1 &&
              last_message.type==DD_MSG_TOWN_REJECT && last_message.u.reject.request_id==request.u.town.request_id);
        CHECK(pcq_match_calls==(unsigned)(broken==0));
    }
    puts("Prop item compatibility: 14 unsupported/missing/malformed span, entry, vtable/executable-method and variation cases reject host requests before selection/RNG/item/native effects without stopping healthy peers");
}
static void prop_choice_checks(void) {
    for(unsigned route=0;route<7;route++)for(unsigned initiator=0;initiator<4;initiator++) {
        dd_msg opened=pcq_begin(route),approved[8],request={0};unsigned first=(unsigned)sent,n=0;
        if(initiator) {
            pcq_receive_open(&opened,initiator,route);g_rules_object=pcq_rules;
            pcq_snapshot before=pcq_read_state();pcq_click(route);request=pcq_wire(last_message);
            pcq_snapshot after=pcq_read_state();
            CHECK(!memcmp(&before,&after,sizeof before) && request.type==DD_MSG_TOWN_REQUEST && !pcq_state.calls);
            pcq_begin(route);g_rules_object=pcq_rules;first=(unsigned)sent;
            CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
        } else {g_rules_object=pcq_rules;pcq_click(route);}
        CHECK(pcq_state.calls==1 && !pcq_state.bad && pcq_mode()==3 && !g_town.needs_resync);
        pcq_snapshot want=pcq_read_state();
        CHECK(want.tools==((route==PCQ_TOOL || route==PCQ_CURIO_TOOL || route==PCQ_DIRECT_ITEM)?1u:2u));
        for(unsigned j=first;j<(unsigned)sent;j++)if(sent_messages[j].type==DD_MSG_TOWN_APPLY) {
            CHECK(n<8);if(n<8)approved[n++]=pcq_wire(sent_messages[j]);
        }
        CHECK(n && approved[n-1].u.town.action.kind==DD_ACT_USE);
        /* A crossed choice after the first native effect cannot move selection,
         * consume a second item or change RNG/HP even after native fade reset. */
        dd_msg crossed={0};crossed.type=DD_MSG_TOWN_REQUEST;crossed.u.town.request_id=0x0300007f;
        crossed.u.town.action=approved[n-1].u.town.action;
        if(route==PCQ_HAND) {
            crossed.u.town.action.u.use.door=4;
            crossed.u.town.action.u.use.item_type=pcq_tool_type;crossed.u.town.action.u.use.item_id=pcq_tool_id;
        } else if(route==PCQ_TOOL) {
            crossed.u.town.action.u.use.door=5;
            crossed.u.town.action.u.use.item_type=crossed.u.town.action.u.use.item_id=0;
        }
        crossed.u.town.action.u.use.hero=1;
        CHECK(queue_push(&g_incoming,&crossed));dd_runtime_poll();
        pcq_snapshot crossed_state=pcq_read_state();CHECK(!memcmp(&want,&crossed_state,sizeof want) && !g_town.needs_resync);
        pcq_panel(0);crossed.u.town.request_id++;
        CHECK(queue_push(&g_incoming,&crossed));dd_runtime_poll();
        crossed_state=pcq_read_state();CHECK(!memcmp(&want,&crossed_state,sizeof want) && !g_town.needs_resync);
        for(unsigned receiver=1;receiver<4;receiver++) {
            pcq_receive_open(&opened,receiver,route);g_rules_object=pcq_rules;
            for(unsigned j=0;j<n;j++)CHECK(queue_push(&g_incoming,&approved[j]));
            pcq_panel(1);dd_runtime_poll();dd_runtime_poll();
            CHECK(!pcq_state.calls && !g_town.needs_resync && g_incoming.count);
            pcq_panel(2);dd_runtime_poll();pcq_snapshot got=pcq_read_state();
            CHECK(!g_incoming.count && !g_town.needs_resync && !memcmp(&want,&got,sizeof got));
            dd_runtime_poll();CHECK(pcq_state.calls==1);
        }
    }
    /* Last-item host debit must survive the source's cancelled drag. The
     * held drag object still contains count1, but the canonical slot is empty. */
    {
        dd_msg opened=pcq_begin(PCQ_DIRECT_ITEM),approved[8];unsigned first=(unsigned)sent,n=0;
        putint(pcq_bag,1);g_rules_object=pcq_rules;pcq_click(PCQ_DIRECT_ITEM);
        pcq_snapshot want=pcq_read_state();CHECK(want.tools==0 && want.used==1 && !pcq_drop_ui[0xc] && !pcq_drop_ui[0xc0]);
        for(unsigned j=first;j<(unsigned)sent;j++)if(sent_messages[j].type==DD_MSG_TOWN_APPLY){CHECK(n<8);if(n<8)approved[n++]=pcq_wire(sent_messages[j]);}
        for(unsigned receiver=1;receiver<4;receiver++) {
            pcq_receive_open(&opened,receiver,PCQ_DIRECT_ITEM);putint(pcq_bag,1);g_rules_object=pcq_rules;
            for(unsigned j=0;j<n;j++)CHECK(queue_push(&g_incoming,&approved[j]));dd_runtime_poll();
            pcq_snapshot got=pcq_read_state();CHECK(!memcmp(&want,&got,sizeof want) && !g_town.needs_resync);
        }
    }
    /* Inventory changed after pickup: reject before native use, then suppress
     * source Restore rather than resurrect the old stack quantity. */
    {
        (void)pcq_begin(PCQ_DIRECT_ITEM);unsigned before=(unsigned)sent;uint64_t panel=pcq_panel_at();
        memset(pcq_drop_ui,0,sizeof pcq_drop_ui);memset(pcq_drop_data,0,sizeof pcq_drop_data);
        putptr(pcq_drop_ui+0x30,(uintptr_t)pcq_bag);pcq_drop_ui[0xc]=pcq_drop_ui[0xc0]=1;
        putptr(pcq_drop_data,image_base()+0xeddd10);memcpy(pcq_drop_data+8,pcq_bag,sizeof pcq_bag);
        putptr((void *)(panel+0x780),image_base()+0xec85b0);prop_item_pickup(1,pcq_drop_ui,pcq_drop_data);
        putint(pcq_bag,1);CHECK(hook_prop_item_drop((void *)(panel+0x780),NULL,pcq_drop_data)==0);
        CHECK((unsigned)sent==before && !pcq_state.calls && prop_item_source_done(pcq_drop_ui,pcq_drop_data));
        uint32_t left=0;memcpy(&left,pcq_bag,4);CHECK(left==1 && pcq_drop_ui[0xc] && !pcq_drop_ui[0xc0]);
    }
    /* Obstacle panels can intentionally carry no native Hero*. Metadata uses
     * the selected hero, while the original nullable callback argument must
     * remain NULL on every receiving copy. This was absent in older fixtures. */
    for(unsigned route=PCQ_HAND;route<=PCQ_TOOL;route++)for(unsigned initiator=0;initiator<4;initiator++) {
        dd_msg opened=pcq_begin(route),approved[8],request={0};unsigned first=(unsigned)sent,n=0;
        if(initiator)pcq_receive_open(&opened,initiator,route);
        putptr((void *)(pcq_panel_at()+0xd78),0);g_rules_object=pcq_rules;pcq_click(route);
        if(initiator) {
            CHECK(!pcq_state.calls);request=pcq_wire(last_message);pcq_begin(route);g_rules_object=pcq_rules;first=(unsigned)sent;
            CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
        }
        CHECK(pcq_state.calls==1 && pcq_state.native_null==1 && !g_town.needs_resync);
        pcq_snapshot want=pcq_read_state();
        for(unsigned j=first;j<(unsigned)sent;j++)if(sent_messages[j].type==DD_MSG_TOWN_APPLY){CHECK(n<8);if(n<8)approved[n++]=pcq_wire(sent_messages[j]);}
        CHECK(n);
        for(unsigned receiver=1;receiver<4;receiver++) {
            pcq_receive_open(&opened,receiver,route);g_rules_object=pcq_rules;
            for(unsigned j=0;j<n;j++)CHECK(queue_push(&g_incoming,&approved[j]));dd_runtime_poll();
            pcq_snapshot got=pcq_read_state();CHECK(!memcmp(&want,&got,sizeof want) && !g_town.needs_resync);
        }
    }
    /* Real co-op callbacks never fall back to local effect because a known
     * game overlay is fading, still opening, unreadable or has a missing prop. */
    for(unsigned which=0;which<5;which++)for(unsigned bad=0;bad<7;bad++) {
        unsigned route=which<4?which:PCQ_DIRECT_ITEM;
        (void)pcq_begin(route);pcq_reset(DD_ROLE_GUEST,2,route);pcq_panel(2);
        if(bad<3)pcq_panel(bad==0?0:bad==1?1:3);
        if(bad==3)wk_view_fail=1;
        if(bad==4) {
            putptr((void *)(pcq_panel_at()+0x6a0),0);putptr((void *)(pcq_panel_at()+0xd70),0);
        }
        if(bad==5)g_town.needs_resync=1;
        if(bad==6)g_session_ready=0;
        g_rules_object=pcq_rules;pcq_snapshot before=pcq_read_state();pcq_click(route);pcq_snapshot after=pcq_read_state();
        CHECK(!pcq_state.calls && !memcmp(&before,&after,sizeof before));
    }
    /* After a cancellation, an old popup's competing hand/tool request cannot
     * apply to a freshly reopened panel of the same area and prop. */
    for(unsigned curio=0;curio<2;curio++) {
        unsigned cancel=curio?PCQ_CURIO_CANCEL:PCQ_CANCEL;
        dd_msg opened=pcq_begin(cancel);pcq_receive_open(&opened,1,cancel);pcq_click(cancel);
        dd_msg old=pcq_wire(last_message);pcq_begin(cancel);CHECK(queue_push(&g_incoming,&old));dd_runtime_poll();
        pcq_panel(0);
        dd_msg stale_open=opened;stale_open.type=DD_MSG_TOWN_REQUEST;stale_open.u.town.request_id=0x0200007f;
        unsigned rejected_before=g_town.rejected;
        CHECK(queue_push(&g_incoming,&stale_open));dd_runtime_poll();
        CHECK(pcq_mode()==0 && g_town.rejected==rejected_before+1 && !g_town.needs_resync);
        CHECK(hook_raid_use(wk_display,wk_props[1],0,0));
        pcq_snapshot before=pcq_read_state();old.u.town.request_id++;
        CHECK(queue_push(&g_incoming,&old));dd_runtime_poll();pcq_snapshot after=pcq_read_state();
        CHECK(!memcmp(&before,&after,sizeof before) && !g_town.needs_resync && pcq_mode()==2);
        pcq_click(cancel);CHECK(pcq_state.cancels==2 && pcq_mode()==3 && !g_town.needs_resync);
    }
    pcq_cross_choice_matrix();
    pcq_item_route_checks();
    pcq_item_match_checks();
    g_rules_object=NULL;g_overlay_use=g_curio_button=g_prop_cancel=g_curio_cancel=NULL;g_curio_tool=NULL;
    g_item_curio_route=NULL;g_prop_item_match=NULL;
    wk_reset(DD_ROLE_HOST);items_reset();
    puts("Prop choices: seven hand/tool/cancel/item-drop routes, all4 initiators/3 guests, nullable native actor, no local fallback, once-only native HP/stress/tool/RNG, crossed selections and stale X/reopen choices verified");
}
