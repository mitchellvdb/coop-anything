/* Using items, the torch and the loot window on both copies. A click is an order, each copy runs the game's own function.
 * Without this one copy draws numbers the other does not (the torch's bark, owner's run of 6 Nov 15:17), and inventories differ. */
enum { DD_WALK_LOOT_OVERLAY = 0x2f10, DD_WALK_RAID_INVENTORY = 0x218, DD_WALK_RAID_LOOT = 0x5240 };
#define DD_ITEM_CONTROLLER_RVA 0x118e058u
typedef uint64_t(__fastcall *item_use_fn)(void *, void *, void *, int32_t, uint32_t);
typedef void(__fastcall *torch_fn)(void *, float, uint8_t);
typedef void(__fastcall *loot_button_fn)(void *);
typedef uint8_t(__fastcall *curio_item_fn)(void *, void *);
static item_use_fn g_item_use;
static torch_fn g_torch;
static loot_button_fn g_loot_take_all, g_loot_close, g_loot_take_item;
static curio_item_fn g_curio_item;
static uint64_t g_item_wait_until, g_loot_done_at, g_loot_asked_at;
static uint32_t g_loot_request, g_loot_asked_key;
static int g_item_gate_told;
static uint64_t g_item_use_asked_at;
static void g_tutorial_panel_reset(void);
static uint8_t g_loot_dummy_widget[0x40]; /* 7eda80 only plays the item's sound, and skips that when +30 is empty. */
static uint64_t item_controller(void) {
    uint64_t controller = 0;
    safe_read(image_base() + DD_ITEM_CONTROLLER_RVA, &controller, 8);
    return controller;
}
static int item_use_phase(const walk_view *v) {
    uint64_t now = GetTickCount64();
    return !v->battle && v->state > 1 && v->state != 6 && v->state != 7 &&
        (!g_walk_door_pending || now >= g_walk_door_pending) && now >= g_walk_door_until;
}
static uint64_t (*g_item_controller)(void) = item_controller;
static void results_bag_reset(void);
static void hunger_reset(void);
static void quest_return_reset(void);
static void items_reset(void) {
    g_tutorial_panel_reset();
    results_bag_reset();
    hunger_reset();
    quest_return_reset();
    g_item_wait_until = g_loot_done_at = g_loot_asked_at = 0;
    g_loot_request = g_loot_asked_key = 0;
    g_item_gate_told = 0;
    g_item_use_asked_at = 0;
}
/* The torch's click (782d20): 783d26 snuffs it; reducing is a stored function called at 783dd3, which is 4328d0 and jumps to 6155f0, so it returns behind that call.
 * Not 430dbb: that is inside using a torch item, which is an order already (owner's run: the torch went up twice on the guest). */
static int item_torch_caller(uintptr_t rva) { return rva == 0x783d2b || rva == 0x783dd6; }
static int (*g_item_torch_caller)(uintptr_t) = item_torch_caller;
/* Where this choice happens, or 0 if it may not be shared now. In a fight only at the turn's choosing states. */
static int item_order(uint8_t op, dd_action *a, walk_view *v) {
    battle_view b;
    memset(a, 0, sizeof *a);
    a->kind = DD_ACT_ITEM;
    a->u.item.op = op;
    a->u.item.hero = 255;
    if (!g_walk_view(v)) return 0;
    if (op == DD_ITEM_OP_USE && !v->battle && !item_use_phase(v)) {
        dd_log("Item: use waits until door/loading has finished");
        return 0;
    }
    a->u.item.area = v->area;
    memcpy(&a->u.item.pos, &v->pos, 4);
    a->u.item.rules = battle_rules();
    if (v->battle) {
        if (!battle_read(v, &b) || (b.state != DD_BATTLE_CHOOSE_SKILL_STATE && b.state != DD_BATTLE_CHOOSE_TARGET_STATE)) {
            dd_log("Item: op %u outside a turn's choosing (battle state=%u); dropped", op, v->battle);
            return 0;
        }
        a->u.item.round = b.round;
        a->u.item.turn = b.turn;
        a->u.item.state = (uint8_t)b.state;
    }
    return 1;
}
static void item_send(const dd_action *a) {
    dd_log("Item: op=%u slot=%d inv=%u hero=%u state=%u rules=%08x (role=%s)", a->u.item.op, a->u.item.slot, a->u.item.inv, a->u.item.hero, a->u.item.state, a->u.item.rules,
           dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) dd_town_guest_local(&g_town, a);
    else dd_town_host_observed(&g_town, a);
}
/* Inventory uses need the same area/phase, not the same walking pixel. Other prop
 * choices retain their stricter positional Rules barrier. */
static void item_use_rules_send(const walk_view *v) {
    if (g_cfg.role != DD_ROLE_HOST || v->battle || !g_rules_object) return;
    dd_action a = {0};
    a.kind = DD_ACT_ITEM; a.u.item.op = DD_ITEM_OP_RULES; a.u.item.hero = 255;
    a.u.item.area = v->area; a.u.item.rules = battle_rules(); a.u.item.flag = 1;
    dd_town_host_observed(&g_town, &a);
}
#include "dd_hunger_bridge.inc"
/* 429750 DungeonController::HeroUseItem (controller, hero, inventory, slot, 1): the panel's click, the loot window's, the controller's. */
static uint64_t __fastcall hook_item_use(void *controller, void *hero, void *inventory, int32_t slot, uint32_t flag) {
    walk_view v;
    dd_action a;
    if (!g_item_use) return 0;
    if(g_game_apply_depth || !walk_coop_active())return g_item_use(controller,hero,inventory,slot,flag);
    if(!walk_shared())return 0;
    if (runtime_shared_blocked()) return 0;
    if (!item_order(DD_ITEM_OP_USE, &a, &v)) return 0;
    if ((uintptr_t)inventory == v.raid + DD_WALK_RAID_INVENTORY) a.u.item.inv = 0;
    else if ((uintptr_t)inventory == v.raid + DD_WALK_RAID_LOOT) a.u.item.inv = 1;
    else {
        dd_log("Item: another inventory; not shared");
        return g_item_use(controller, hero, inventory, slot, flag);
    }
    if (hero && !walk_hero_rank(v.raid, (uintptr_t)hero, &a.u.item.hero)) {
        dd_log("Item: hero not in the party; dropped");
        return 0;
    }
    if (a.u.item.state && !hero_is_mine((uintptr_t)hero)) {
        dd_log("Item: this hero belongs to the other player; dropped");
        return 0;
    }
    dd_action choice={0};
    int route=prop_item_route(&v,controller,inventory,slot,&choice);
    if(route) {if(route>0)walk_choice_dispatch(&choice);return 0;}
    a.u.item.slot = slot;
    a.u.item.flag = flag != 0;
    if (g_cfg.role == DD_ROLE_GUEST) {
        /* The first click has no visible effect until the order comes back, so a player clicks on, and every click used an item (owner's run: 5 in 5 seconds). */
        uint64_t now = GetTickCount64();
        if (g_item_use_asked_at && now - g_item_use_asked_at < 4000) {
            dd_log("Item: a use is already on its way; this click is dropped");
            return 0;
        }
        g_item_use_asked_at = now;
    }
    item_use_rules_send(&v);
    item_send(&a); /* Sent whatever the game says: it decides the same on both. */
    return g_cfg.role == DD_ROLE_HOST ? g_item_use(controller, hero, inventory, slot, flag) : 0;
}
/* 6155f0 Raid: change the torch (raid, amount, 1). The game's own burning and effects pass; only the player's reducing is a choice. */
static void __fastcall hook_torch(void *raid, float amount, uint8_t flag) {
#ifdef _MSC_VER
    uintptr_t caller = (uintptr_t)_ReturnAddress();
#else
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
#endif
    walk_view v;
    dd_action a;
    if (!g_torch) return;
    if (g_game_apply_depth || !walk_shared() || !g_item_torch_caller(caller - image_base())) {
        g_torch(raid, amount, flag);
        return;
    }
    if (!item_order(DD_ITEM_OP_TORCH, &a, &v)) return;
    memcpy(&a.u.item.amount, &amount, 4);
    a.u.item.flag = flag != 0;
    item_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_torch(raid, amount, flag);
}
static int loot_open(const walk_view *v) {
    uint32_t state = 0;
    return safe_read((uintptr_t)v->display + DD_WALK_LOOT_OVERLAY, &state, 4) && state && state != 3 && state != 4; /* 717620 closes it with 3 or 4. */
}
/* 715610 consumes +218/+219 only in state 2, with +170 set, no
 * blocking overlay (display+4118) or queued presentation (+40e0/+40e8).
 * Earlier states clear both flags at 717014. In the owner's first fight
 * the guest applied take-all while still in battle state 34, losing it. */
static int loot_ready(const walk_view *v) {
    uint32_t state = 0;
    uint8_t enabled = 0, blocked = 0;
    uint64_t begin = 0, end = 0;
    return !v->battle && safe_read((uintptr_t)v->display + DD_WALK_LOOT_OVERLAY, &state, 4) && state == 2 &&
        safe_read((uintptr_t)v->display + DD_WALK_LOOT_OVERLAY + 0x170, &enabled, 1) && enabled &&
        safe_read((uintptr_t)v->display + 0x4118, &blocked, 1) && !blocked &&
        safe_read((uintptr_t)v->display + 0x40e0, &begin, 8) &&
        safe_read((uintptr_t)v->display + 0x40e8, &end, 8) && begin == end;
}
/* Every player's click remains shared, including a second loot window. */
static int loot_local(void) {
    return g_game_apply_depth || !walk_shared();
}
/* Whether this click may become an order. Outside state 2 the game ignores it itself (717014), and an order for it would wait on the other copy for a window that is already gone: a second
 * click on a closing window stopped the session 30 s later. On the guest the window stays as it is until the order comes back, so the same button again is a repeat. */
static int loot_click_counts(const walk_view *v, uint8_t op, int32_t slot) {
    uint64_t now = GetTickCount64();
    uint32_t key = ((uint32_t)op << 16) | ((uint32_t)slot & 0xffffu);
    if (!loot_ready(v)) {
        dd_log("Item: a loot click on a window that is not ready; dropped");
        return 0;
    }
    if (g_cfg.role == DD_ROLE_HOST) {
        g_loot_done_at = now;
        return 1;
    }
    if (g_loot_asked_at && now - g_loot_asked_at < 3000 && g_loot_asked_key == key) {
        dd_log("Item: this loot click is already on its way; dropped");
        return 0;
    }
    g_loot_asked_at = now;
    g_loot_asked_key = key;
    return 1;
}
static void loot_send(const dd_action *a) {
    dd_log("Item: op=%u slot=%d inv=%u hero=%u state=%u rules=%08x (role=%s)", a->u.item.op, a->u.item.slot, a->u.item.inv, a->u.item.hero, a->u.item.state, a->u.item.rules, dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) g_loot_request = dd_town_guest_local(&g_town, a);
    else dd_town_host_observed(&g_town, a);
}
/* The host said no to this copy's loot click. If this window is still open, nothing shared can close it any more. */
static void loot_request_done(uint32_t request) {
    walk_view v;
    if (!g_loot_request || request != g_loot_request) return;
    g_loot_request = 0;
    g_loot_asked_at = 0;
    if (g_walk_view(&v) && loot_open(&v)) {
        dd_log("Item: host refused a loot action while this panel remains open; shared recovery required");
        runtime_sync_stop(3);
    }
}
static void loot_button(loot_button_fn native, uint8_t op, void *functor) {
    walk_view v;
    dd_action a;
    if (!native) return;
    if (runtime_shared_blocked()) return;
    if (loot_local()) {
        native(functor);
        return;
    }
    if (!g_walk_view(&v) || !loot_click_counts(&v, op, 0)) return;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = op;
    a.u.item.hero = 255;
    a.u.item.area = v.area;
    memcpy(&a.u.item.pos, &v.pos, 4);
    a.u.item.rules = battle_rules();
    loot_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) native(functor);
}
static void __fastcall hook_loot_take_all(void *functor) { loot_button(g_loot_take_all, DD_ITEM_OP_LOOT_ALL, functor); } /* 717d80, sets +218. */
static void __fastcall hook_loot_close(void *functor) { loot_button(g_loot_close, DD_ITEM_OP_LOOT_CLOSE, functor); }    /* 717d40, sets +219. */
/* 7171d0: one item from the window: +10 its place among the filled slots, +20 the loot (raid+5140), +28 its widget. */
static void __fastcall hook_loot_take_item(void *button) {
    walk_view v;
    dd_action a;
    uint64_t loot = 0;
    int32_t slot = 0;
    if (!g_loot_take_item) return;
    if (runtime_shared_blocked()) return;
    if (loot_local() || !g_walk_view(&v) || !safe_read((uintptr_t)button + 0x20, &loot, 8) || loot != v.raid + DD_WALK_RAID_LOOT - 0x100 ||
        !safe_read((uintptr_t)button + 0x10, &slot, 4)) {
        g_loot_take_item(button);
        return;
    }
    if (!loot_click_counts(&v, DD_ITEM_OP_LOOT_ITEM, slot)) return;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = DD_ITEM_OP_LOOT_ITEM;
    a.u.item.hero = 255;
    a.u.item.slot = slot;
    a.u.item.area = v.area;
    memcpy(&a.u.item.pos, &v.pos, 4);
    a.u.item.rules = battle_rules();
    loot_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_loot_take_item(button);
}
/* Late native curio item mutation is allowed only within ordered apply.
 * Player drags are intercepted earlier at70bf70; clicks route through USE7. */
static uint8_t __fastcall hook_curio_item(void *overlay, void *item) {
    if(!g_curio_item)return 0;
    if(g_game_apply_depth || !walk_coop_active())return g_curio_item(overlay,item);
    dd_log("Prop choice: late local curio item mutation refused; use the shared drop/click path");
    return 0;
}
static uint64_t item_in_bag(uint64_t raid, uint32_t type, uint32_t id) {
    uint64_t begin = 0, end = 0;
    if (!safe_read((uintptr_t)raid + DD_WALK_RAID_INVENTORY + 8, &begin, 8) || !safe_read((uintptr_t)raid + DD_WALK_RAID_INVENTORY + 0x10, &end, 8) || end < begin) return 0;
    for (uint64_t p = begin; p + 0x5a8 <= end; p += 0x5a8) {
        int32_t count = 0;
        uint32_t t = 0, i = 0;
        if (!safe_read((uintptr_t)p, &count, 4) || !safe_read((uintptr_t)p + 0x44, &t, 4) || !safe_read((uintptr_t)p + 0x88, &i, 4)) return 0;
        if (count > 0 && t == type && i == id) return p;
    }
    return 0;
}
static int curio_open(const walk_view *v) {
    uint64_t prop = 0;
    uint32_t mode = 0;
    return safe_read((uintptr_t)v->display + DD_WALK_OVERLAY + 0x6a0, &prop, 8) && prop && safe_read((uintptr_t)v->display + DD_WALK_OVERLAY + 0xd88, &mode, 4) && (mode == 1 || mode == 2);
}
/* 75b970: a bark on starting or stopping to walk. It picks its hero with a Rules number (6057f0), on a chance that differs per copy. The next tile then read the wrong number, and one copy got
 * stress the other did not (owner's run, 16:07:20). It is only talk: the number goes back. */
typedef void(__fastcall *bark_fn)(void *, void *, float);
static bark_fn g_bark_moving;
static void __fastcall hook_bark_moving(void *self, void *text, float chance) {
    uint32_t state = 0;
    if (!g_bark_moving) return;
    int keep = g_rules_object && walk_shared() && safe_read((uintptr_t)g_rules_object + 8, &state, 4);
    g_bark_moving(self, text, chance);
    if (keep) memcpy((uint8_t *)g_rules_object + 8, &state, 4);
}
/* The tutorial popup. 820ae0 is its show function, the only way to know the panel; 7a8c80 is a close button several panels share, with the panel at +8. Its [+58]: 0 hidden, 5 to 7 closing (7f6680). */
typedef void(__fastcall *panel_show_fn)(void *, uint64_t, uint64_t, uint64_t);
typedef void(__fastcall *panel_close_fn)(void *);
static panel_show_fn g_tutorial_show;
static panel_close_fn g_panel_close;
static uint64_t g_tutorial_panel;
static void g_tutorial_panel_reset(void) { g_tutorial_panel = 0; }
static void __fastcall hook_tutorial_show(void *panel, uint64_t b, uint64_t c, uint64_t d) {
    g_tutorial_panel = (uintptr_t)panel;
    if (g_tutorial_show) g_tutorial_show(panel, b, c, d);
}
static int tutorial_open(void) {
    uint32_t state = 0;
    return g_tutorial_panel && safe_read((uintptr_t)g_tutorial_panel + 0x58, &state, 4) && state && (state < 5 || state > 7);
}
/* Each copy closes its own tutorial popup. It only informs, and a popup does not always open on every copy
 * (tutorials already seen, or shown at a different moment): a shared close then waited for the host's popup and
 * the guest could not close its own (owner's runs, 11 Nov after a camp and 20:03:58-20:04:08: four refusals,
 * the host waited for that window, Resync). The walk check still waits while any player has a window open. */
static void __fastcall hook_panel_close(void *functor) {
    uint64_t panel = 0;
    if (!g_panel_close) return;
    if (!g_game_apply_depth && walk_shared() && safe_read((uintptr_t)functor + 8, &panel, 8) && panel && panel == g_tutorial_panel)
        dd_log("Item: tutorial popup closed here only (role=%s)", dd_role_name(g_cfg.role));
    g_panel_close(functor);
}
/* The player clicked another hero, outside a fight. A curio or item then uses that hero, so the other copy must have it too (owner's run, 16:12: the destress went to the wrong hero). */
static void walk_selection_poll(const walk_view *v) {
    uint64_t hero = 0;
    uint32_t id = 0;
    uint8_t rank = 0;
    dd_action a;
    if (!walk_shared() || v->battle || !safe_read((uintptr_t)v->raid + 0x438, &hero, 8) || !hero || !walk_hero_rank(v->raid, hero, &rank)) return;
    id = battle_hero_id(hero);
    if (!id || id == UINT32_MAX) return;
    if (g_walk_selected == (int)rank) return;
    int first = g_walk_selected < 0;
    g_walk_selected = (int)rank;
    if (first) return;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = DD_ITEM_OP_SELECT;
    a.u.item.hero = rank;
    a.u.item.id = id;
    a.u.item.area = v->area;
    memcpy(&a.u.item.pos, &v->pos, 4);
    a.u.item.rules = battle_rules();
    item_send(&a);
}
/* 7499a0 QuestInfo::AttemptBattleRetreat (the panel at display+3590): the flag's confirmed click, in a fight and out of one. It rolls its chance with the battle's numbers and
 * then travels, so both copies run it. In a fight only on your own hero's turn. */
#define DD_ITEM_QUEST_INFO 0x3590u
typedef void(__fastcall *retreat_fn)(void *);
static retreat_fn g_retreat;
static void __fastcall hook_retreat(void *panel) {
    walk_view v;
    battle_view b;
    dd_action a;
    if (!g_retreat) return;
    if (g_game_apply_depth || !walk_shared() || !g_walk_view(&v) || (uintptr_t)panel != v.display + DD_ITEM_QUEST_INFO) {
        g_retreat(panel);
        return;
    }
    if (!item_order(DD_ITEM_OP_RETREAT, &a, &v)) return;
    if (a.u.item.state && (!battle_read(&v, &b) || !hero_is_mine(b.actor))) {
        dd_log("Item: retreat on the other player's turn; dropped");
        return;
    }
    item_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_retreat(panel);
}
/* Camp. Overlay_Camp is at display+2058; raid+5118 says which part of the camp this is. Each choice waits for the same part on the other copy.
 * 709cf0: the meal's button; only +8 the amount's flag, +c the food, +10, +18, +1c what the meal does, +20 the overlay are read. */
#define DD_ITEM_CAMP_OVERLAY 0x2058u
#define DD_ITEM_CAMP_STAGE 0x5118u
typedef void(__fastcall *camp_meal_fn)(void *);
typedef void(__fastcall *camp_skill_fn)(void *, void *, void *, uint32_t, uint32_t, uint32_t);
typedef void(__fastcall *camp_leave_fn)(void *);
static camp_meal_fn g_camp_meal;
static camp_skill_fn g_camp_skill;
static camp_leave_fn g_camp_leave;
/* 70b7c0 (Leave) does nothing while overlay+24 is 0. The overlay's frame update 70a1e0 keeps +24 at 0 while
 * another panel is open over the camp (display+31a0), and the leave itself runs at the end of the fade it starts
 * (lambda 70b850: camp part 7, leave, +24 = 0). Applying Leave on a copy whose camp is inactive therefore left that
 * copy in camp while the others walked on (owner test74, 11 Nov). Wait until the camp is active here. */
#define DD_ITEM_CAMP_ACTIVE 0x24u
static const char g_camp_leave_wait_text[] = "Close the open window: the other player has left the camp, and this camp can only follow once it is not covered.";
static int g_camp_leave_wait_told;
static int camp_leave_ready(const walk_view *v) {
    uint8_t active = 0;
    return safe_read((uintptr_t)v->display + DD_ITEM_CAMP_OVERLAY + DD_ITEM_CAMP_ACTIVE, &active, 1) && active;
}
static void camp_leave_wait_notice(int waiting) {
    if (waiting && !g_camp_leave_wait_told) shop_notice(g_camp_leave_wait_text);
    if (!waiting && g_camp_leave_wait_told) shop_notice_clear(g_camp_leave_wait_text);
    g_camp_leave_wait_told = waiting;
}
static int camp_order(uint8_t op, dd_action *a, walk_view *v) {
    uint32_t stage = 0;
    if (!item_order(op, a, v) || v->battle || !safe_read((uintptr_t)v->raid + DD_ITEM_CAMP_STAGE, &stage, 4)) return 0;
    a->u.item.round = stage;
    return 1;
}
static void __fastcall hook_camp_meal(void *args) {
    walk_view v;
    dd_action a;
    uint64_t overlay = 0;
    uint8_t flag = 0;
    if (!g_camp_meal) return;
    if (g_game_apply_depth || !walk_shared() || !g_walk_view(&v) || !safe_read((uintptr_t)args + 0x20, &overlay, 8) || overlay != v.display + DD_ITEM_CAMP_OVERLAY ||
        !camp_order(DD_ITEM_OP_CAMP_MEAL, &a, &v) || !safe_read((uintptr_t)args + 8, &flag, 1)) {
        g_camp_meal(args);
        return;
    }
    a.u.item.flag = flag != 0;
    safe_read((uintptr_t)args + 0x0c, &a.u.item.amount, 4);
    safe_read((uintptr_t)args + 0x10, &a.u.item.turn, 4);
    safe_read((uintptr_t)args + 0x18, &a.u.item.type, 4);
    safe_read((uintptr_t)args + 0x1c, &a.u.item.id, 4);
    item_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_camp_meal(args);
}
/* 5c7420 Camping::PerformCampingSkill (camp raid+5100, party raid+8, loot raid+5140, three numbers), from the skill's button (70af49, 71f080). */
static int camp_skill_caller(uintptr_t rva) { return rva == 0x70af4e || rva == 0x71f085; }
static int (*g_camp_skill_caller)(uintptr_t) = camp_skill_caller;
/* The native function indexes party.begin[x] and dereferences the selected
 * camping-skill lookup(y) without checking either result. Validate the actual
 * local vectors before any native effect, including trusted-looking packets. */
static int camp_skill_ready(const walk_view *v, uint32_t rank, uint32_t selected) {
    uint64_t begin = 0, end = 0, actor = 0, skills = 0, skills_end = 0;
    uint32_t index = 0; int active = 0;
    if (rank >= 4 || selected >= 8 ||
        !safe_read((uintptr_t)v->raid + 8, &begin, 8) || !begin ||
        !safe_read((uintptr_t)v->raid + 0x10, &end, 8) || end < begin ||
        (end - begin) % 8 || (end - begin) / 8 > 4 || rank >= (end - begin) / 8 ||
        !safe_read((uintptr_t)(begin + rank * 8), &actor, 8) || !battle_hero_id(actor) ||
        !safe_read((uintptr_t)actor + 0x1398, &skills, 8) || !skills ||
        !safe_read((uintptr_t)actor + 0x13a0, &skills_end, 8) || skills_end < skills ||
        (skills_end - skills) % 4 || (skills_end - skills) / 4 > 8 ||
        selected >= (skills_end - skills) / 4 ||
        !safe_read((uintptr_t)(skills + selected * 4), &index, 4)) return 0;
    dd_hamlet model = {0}; dd_hamlet_hero hero = {0};
    model.read = shop_read; hero.hero = actor;
    return dd_hamlet_skill(&model, &hero, 1, index, &active) && active;
}
static void __fastcall hook_camp_skill(void *camp, void *party, void *loot, uint32_t x, uint32_t y, uint32_t z) {
#ifdef _MSC_VER
    uintptr_t caller = (uintptr_t)_ReturnAddress();
#else
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
#endif
    walk_view v;
    dd_action a;
    if (!g_camp_skill) return;
    if (g_game_apply_depth || !walk_shared() || !g_camp_skill_caller(caller - image_base()) || !camp_order(DD_ITEM_OP_CAMP_SKILL, &a, &v)) {
        g_camp_skill(camp, party, loot, x, y, z);
        return;
    }
    if (!camp_skill_ready(&v, x, y)) {
        dd_log("Camping skill refused: local actor/selected skill is unavailable (rank=%u selected=%u)", x, y);
        runtime_sync_stop(3); return;
    }
    a.u.item.turn = x;
    a.u.item.type = y;
    a.u.item.id = z;
    item_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_camp_skill(camp, party, loot, x, y, z);
}
/* 70b7c0: the camp's Leave button: a functor with the overlay at +8. */
static void __fastcall hook_camp_leave(void *functor) {
    walk_view v;
    dd_action a;
    uint64_t overlay = 0;
    if (!g_camp_leave) return;
    if (g_game_apply_depth || !walk_shared() || !g_walk_view(&v) || !safe_read((uintptr_t)functor + 8, &overlay, 8) || overlay != v.display + DD_ITEM_CAMP_OVERLAY ||
        !camp_order(DD_ITEM_OP_CAMP_LEAVE, &a, &v)) {
        g_camp_leave(functor);
        return;
    }
    item_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) g_camp_leave(functor);
}
/* The two buttons after a finished quest (7506c0 and 7504e0): go on exploring, or leave. A functor with the panel at +8. */
typedef void(__fastcall *quest_choice_fn)(void *);
static quest_choice_fn g_quest_one, g_quest_two;
static void quest_choice(quest_choice_fn native, uint8_t op, void *functor) {
    walk_view v;
    dd_action a;
    uint64_t panel = 0;
    if (!native) return;
    if (g_game_apply_depth || !walk_shared() || !g_walk_view(&v) || !safe_read((uintptr_t)functor + 8, &panel, 8) || panel != v.display + DD_ITEM_QUEST_INFO) {
        native(functor);
        return;
    }
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = op;
    a.u.item.hero = 255;
    a.u.item.area = v.area;
    memcpy(&a.u.item.pos, &v.pos, 4);
    a.u.item.rules = battle_rules();
    item_send(&a);
    if (g_cfg.role == DD_ROLE_HOST) native(functor);
}
static void __fastcall hook_quest_one(void *functor) { quest_choice(g_quest_one, DD_ITEM_OP_QUEST_ONE, functor); }
static void __fastcall hook_quest_two(void *functor) { quest_choice(g_quest_two, DD_ITEM_OP_QUEST_TWO, functor); }
/* After the raid. RaidResultsDisplay: 7a0170 sets it up, 7a2350 says whether it may move on, 7a2440 moves on, at the last step to town (7a2a50). The first click counts, as in the loot window.
 * Before each step the host's Rules state goes along: from here on the numbers decide the new week in town, and both copies must enter it the same. */
typedef void(__fastcall *results_setup_fn)(void *, uint64_t, uint64_t, uint64_t);
typedef uint64_t(__fastcall *results_next_fn)(void *);
typedef uint8_t(__fastcall *results_can_fn)(void *);
static results_setup_fn g_results_setup;
static results_next_fn g_results_next;
/* 7a2350, entry 19 of the display's table. Its bytes are not unique in the game, so it is not in the address table. */
static uint8_t __fastcall results_can_native(void *display) {
    uint64_t table = 0, entry = 0;
    if (!safe_read((uintptr_t)display, &table, 8) || !safe_read((uintptr_t)table + 19 * 8, &entry, 8) || !entry) return 0;
    return ((results_can_fn)(uintptr_t)entry)(display);
}
static results_can_fn g_results_can = results_can_native;
static uint64_t g_results_display, g_results_asked_at;
static void bag_state_reset(void);
static void results_bag_reset(void) {
    g_results_display = g_results_asked_at = 0;
    bag_state_reset();
}
static void __fastcall hook_results_setup(void *display, uint64_t b, uint64_t c, uint64_t d) {
    g_results_display = (uintptr_t)display;
    g_results_asked_at = 0;
    if (g_results_setup) g_results_setup(display, b, c, d);
}
static void results_send_rules(void) {
    dd_action a;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = DD_ITEM_OP_RULES;
    a.u.item.hero = 255;
    a.u.item.rules = battle_rules();
    dd_town_host_observed(&g_town, &a);
}
static uint64_t __fastcall hook_results_next(void *display) {
    dd_action a;
    if (!g_results_next) return 0;
    if (runtime_shared_blocked()) return 0;
    g_results_display = (uintptr_t)display;
    if (g_game_apply_depth || !walk_shared()) return g_results_next(display);
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = DD_ITEM_OP_RESULTS_NEXT;
    a.u.item.hero = 255;
    a.u.item.rules = battle_rules();
    if (g_cfg.role == DD_ROLE_GUEST) {
        uint64_t now = GetTickCount64();
        /* An unanswered choice must not advance this copy's week privately. */
        if (g_results_asked_at && now - g_results_asked_at > 3000) {
            if (now - g_results_asked_at > 30000) runtime_sync_stop(3);
            else shop_notice("Waiting for the shared results action. Do not advance this game separately.");
            return 0;
        }
        if (g_results_asked_at) return 0;
        g_results_asked_at = now;
        item_send(&a);
        return 0;
    }
    results_send_rules();
    item_send(&a);
    return g_results_next(display);
}
#include "dd_quest_return_bridge.inc"
/* The bag. Dragging runs on the slot widgets (7ed600 at the drop, 7ed150 and 7ecf00 at the source, 737c20 and 737070 to throw away). Each copy lets its own drag happen, then says how the changed
 * slots look now, and the other copy makes them so. Slots are swapped whole, so the items stay the items they are. */
#define DD_ITEM_SIZE 0x5a8u
#define DD_BAG_SLOTS 64u
typedef struct { int32_t count; uint32_t type, id; } bag_slot;
typedef uint64_t(__fastcall *bag_call_fn)(void *, void *, void *, void *);
typedef void(__fastcall *item_clear_fn)(void *);
static bag_call_fn g_bag_throw_a, g_bag_throw_b;
static item_clear_fn g_item_clear;
static bag_slot g_bag_seen[2][DD_BAG_SLOTS];
static unsigned g_bag_seen_count[2];
static int g_bag_moved, g_bag_seen_valid;
static int g_bag_repairing;
static uint32_t g_bag_repair_hash;
static unsigned g_bag_templates_count;
static bag_slot g_bag_templates_keys[DD_BAG_SLOTS*2];
__declspec(align(16)) static uint8_t g_bag_templates[DD_BAG_SLOTS*2][DD_ITEM_SIZE];
static void bag_templates_clear(void);
static int bag_repair_busy(void) { return g_bag_repairing; }
/* These three inventory callbacks are the SAME functions as the trinket
 * drop/finish/cancel callbacks. Their single installed hook dispatches both
 * jobs; a second hook at the same address silently replaced trinket safety. */
static void bag_drag_done(void) {
    if (!g_game_apply_depth && !g_trinket_drag.valid && walk_shared()) g_bag_moved = 1;
}
#define DD_BAG_HOOK(name, native)                                                     \
    static uint64_t __fastcall name(void *a, void *b, void *c, void *d) {             \
        uint64_t result = native ? native(a, b, c, d) : 0;                            \
        if (!g_game_apply_depth && walk_shared()) g_bag_moved = 1;                    \
        return result;                                                                \
    }
/* In the provisions screen a native drag buys or sells by itself on one copy, and from then on the two copies' gold and
 * stock differ, which stopped the session in the owner's run of 6 Nov 17:34: two torches sold back on the guest, unseen
 * by any order. So a drag never runs natively there. From the shop onto a bag slot it becomes the same shared purchase of
 * one as a click (hook_supply_buy); from the bag onto the shop (7ef6d0) the same return of one as a click
 * (hook_supply_return). The source is the slot widget the drag was picked up from (+28 its inventory, +b8 its slot, as for
 * trinkets); the bag slot it is dropped on is the widget's +18. Anything else, such as moving inside the bag, is refused. */
static bag_call_fn g_bag_shop_drop;
static struct {
    uint64_t inventory, ui;
    uint32_t slot;
    int valid;
} g_supply_drag;
/* The source slot of a drag that became a shared purchase or return. Its native finish or cancel (7ecf00/7ed150 on the
 * bag, 7ee410/7ee6a0 on the shop) sets the slot's count from the drag's own copy, taken at the pickup: after a return
 * applied at once on the host, the cancel put the whole stack back, one item more than the guest (owner's run of 8 Nov
 * 02:41). The order owns the counts, so for this slot only the drag's flags are reset, as for trinkets. */
static uint64_t g_supply_drag_after;
static int supply_drag_finish(void *ui) {
    uint64_t item = 0;
    int32_t count = 1;
    uint8_t visible, dragging = 0;
    if (!ui || !g_supply_drag_after || (uintptr_t)ui != g_supply_drag_after) return 0;
    g_supply_drag_after = 0;
    if (safe_read((uintptr_t)ui + 0x30, &item, 8) && item) safe_read((uintptr_t)item, &count, 4);
    visible = (uint8_t)(count > 0);
    WriteProcessMemory(GetCurrentProcess(), (uint8_t *)ui + 0xc, &visible, 1, NULL);
    WriteProcessMemory(GetCurrentProcess(), (uint8_t *)ui + 0xc0, &dragging, 1, NULL);
    return 1;
}
static int supply_drag_screen(supply_model *m, dd_provisions_snapshot *s) {
    walk_view v;
    /* In a raid the bag is shared by bag_poll; the town's panel may still read as open there. */
    return !g_game_apply_depth && supply_active() && !g_walk_view(&v) && g_supply_open(m, s) && s->panel_visible;
}
static void supply_drag_pickup(void *ui) {
    supply_model m;
    dd_provisions_snapshot s;
    uint64_t inventory = 0;
    uint32_t slot = 0;
    memset(&g_supply_drag, 0, sizeof g_supply_drag);
    g_supply_drag_after = 0;
    if (!ui || !supply_drag_screen(&m, &s) || !safe_read((uintptr_t)ui + 0x28, &inventory, 8) || !safe_read((uintptr_t)ui + 0xb8, &slot, 4) ||
        (inventory != m.stock && inventory != m.bag))
        return;
    g_supply_drag.inventory = inventory;
    g_supply_drag.ui = (uintptr_t)ui;
    g_supply_drag.slot = slot;
    g_supply_drag.valid = 1;
}
static int bag_drag_refused(void *ui, int on_shop) {
    supply_model m;
    dd_provisions_snapshot s;
    uint64_t target = 0;
    unsigned op = 0;
    dd_action a;
    if (!supply_drag_screen(&m, &s)) return 0;
    if (g_supply_drag.valid && on_shop && g_supply_drag.inventory == m.bag)
        op = DD_SUPPLY_RETURN;
    else if (g_supply_drag.valid && !on_shop && g_supply_drag.inventory == m.stock && ui && safe_read((uintptr_t)ui + 0x18, &target, 8) && target == m.bag)
        op = DD_SUPPLY_BUY;
    if (op) {
        unsigned slot = g_supply_drag.slot;
        uint64_t source = g_supply_drag.ui;
        memset(&g_supply_drag, 0, sizeof g_supply_drag);
        if (supply_input_blocked()) return 1;
        if (supply_quote(&m, &s, op, slot, &a)) {
            dd_log("Item: a drag in the provisions screen is shared as a %s of slot %u", op == DD_SUPPLY_BUY ? "purchase" : "return", slot);
            g_supply_drag_after = source;
            if (!supply_click_later(op, a.u.supplies.type, a.u.supplies.id)) supply_dispatch(&a, 0);
            return 1;
        }
        dd_log("Item: a drag in the provisions screen had no quote (op=%u slot=%u)", op, slot);
    }
    memset(&g_supply_drag, 0, sizeof g_supply_drag);
    dd_log("Item: a drag in the provisions screen is refused");
    shop_notice("Rearranging the bag is not synchronized in provisions. Drag from shop to bag to buy, or bag to shop to return an item.");
    return 1;
}
/* 7ef6d0: an item dropped on the shop: the provisions screen's own way to sell back. */
static uint64_t __fastcall hook_bag_shop_drop(void *a, void *b, void *c, void *d) {
    if (bag_drag_refused(NULL, 1)) return 0;
    return g_bag_shop_drop ? g_bag_shop_drop(a, b, c, d) : 0;
}
/* 694ab0: an item dropped on one of the shop's slots in the provisions screen. It sells back through 7ebcf0 too (or, when
 * the slot does not take it, falls through to the InventorySlot drop 7eebb0). It was not hooked: in the owner's run of
 * 8 Nov 02:23 the host sold one item back this way on its own copy only, and the guest refused the next purchase, which
 * stopped the session. Only the provisions screen has this slot, so while shared it never runs natively: from the bag it
 * is the same shared return of one as a click, anything else is refused. */
static bag_call_fn g_provision_stock_drop;
static uint64_t __fastcall hook_provision_stock_drop(void *a, void *b, void *c, void *d) {
    walk_view v;
    if (g_game_apply_depth || !supply_active() || g_walk_view(&v))
        return g_provision_stock_drop ? g_provision_stock_drop(a, b, c, d) : 0;
    if (!bag_drag_refused(NULL, 1)) {
        memset(&g_supply_drag, 0, sizeof g_supply_drag);
        dd_log("Item: a drop on a shop slot is refused; the provisions screen could not be read");
    }
    return 0;
}
/* 696a20, the bag's other callback in the provisions screen (+a18 of the slot, Do_call 697870; the click that returns
 * one is 6978b0): it merges the stack into others of its kind, then sells the rest back through 7ebcf0. Several items
 * at once and a new order of the bag are not one shared return, so while shared it is refused, with a message. */
typedef void(__fastcall *supply_stack_fn)(void *, int);
static supply_stack_fn g_supply_stack_return;
static void __fastcall hook_supply_stack_return(void *capture, int index) {
    if (runtime_waiting_for_peer()) return;
    if (g_game_apply_depth || !supply_active()) {
        if (!runtime_shared_blocked() && g_supply_stack_return) g_supply_stack_return(capture, index);
        return;
    }
    dd_log("Supplies: a stack return (slot %d) is refused; it is not shared", index);
    shop_notice("Returning an entire stack is not supported in co-op yet. Click an item to return one, "
                "or drag it to the shop.");
}
DD_BAG_HOOK(hook_bag_throw_a, g_bag_throw_a)
DD_BAG_HOOK(hook_bag_throw_b, g_bag_throw_b)
static void bag_recount(uint64_t raid, uint8_t inv);
static void item_take_one(const walk_view *v, uint64_t item) {
    int32_t count = 0;
    if (!safe_read((uintptr_t)item, &count, 4) || count <= 0) return;
    count--;
    memcpy((void *)(uintptr_t)item, &count, 4);
    if (!count && g_item_clear) g_item_clear((void *)(uintptr_t)item);
    bag_recount(v->raid, 0);
    dd_log("Item: one taken from the bag for the curio, %d left (role=%s)", count, dd_role_name(g_cfg.role));
}
static void bag_state_reset(void) {
    bag_templates_clear();
    g_bag_repairing = 0; g_bag_repair_hash = 0;
    g_bag_moved = g_bag_seen_valid = 0;
    g_bag_seen_count[0] = g_bag_seen_count[1] = 0;
}
static uint64_t bag_base(uint64_t raid, uint8_t inv) { return raid + (inv ? DD_WALK_RAID_LOOT : DD_WALK_RAID_INVENTORY); }
static int bag_span(uint64_t raid, uint8_t inv, uint64_t *begin, unsigned *count) {
    uint64_t end = 0;
    *begin = 0;
    *count = 0;
    if (!safe_read((uintptr_t)bag_base(raid, inv) + 8, begin, 8) || !safe_read((uintptr_t)bag_base(raid, inv) + 0x10, &end, 8) || end < *begin) return 0;
    if((end-*begin)%DD_ITEM_SIZE)return 0;
    uint64_t slots = (end - *begin) / DD_ITEM_SIZE;
    if (slots > DD_BAG_SLOTS) return 0;
    *count = (unsigned)slots;
    return 1;
}
static int bag_read_slot(uint64_t p, bag_slot *s) {
    s->count = 0;
    s->type = s->id = 0;
    if (!safe_read((uintptr_t)p, &s->count, 4)) return 0;
    if (s->count <= 0) {
        s->count = 0;
        return 1;
    }
    return safe_read((uintptr_t)p + 0x44, &s->type, 4) && safe_read((uintptr_t)p + 0x88, &s->id, 4);
}
/* Keep one constructed native template per local identity throughout a full
 * repair. Later duplicate stacks must not steal an already finalized slot or
 * destroy the only exemplar of another kind that a future slot still needs. */
static void bag_templates_clear(void) {
    while(g_bag_templates_count) {
        unsigned i=--g_bag_templates_count;
        if(g_hamlet_destroy)g_hamlet_destroy(g_bag_templates[i]);
    }
}
static int bag_templates_begin(const walk_view *v) {
    bag_templates_clear();
    if(!g_hamlet_copy || !g_hamlet_swap || !g_hamlet_destroy)return 0;
    for(uint8_t inv=0;inv<2;inv++) {
        uint64_t begin=0;unsigned n=0;
        if(!bag_span(v->raid,inv,&begin,&n))goto fail;
        for(unsigned i=0;i<n;i++) {
            bag_slot item;
            if(!bag_read_slot(begin+(uint64_t)i*DD_ITEM_SIZE,&item))goto fail;
            if(item.count<=0)continue;
            unsigned k=0;for(;k<g_bag_templates_count;k++)
                if(g_bag_templates_keys[k].type==item.type && g_bag_templates_keys[k].id==item.id)break;
            if(k<g_bag_templates_count)continue;
            if(k>=DD_BAG_SLOTS*2)goto fail;
            g_hamlet_copy(g_bag_templates[k],(const void *)(uintptr_t)(begin+(uint64_t)i*DD_ITEM_SIZE));
            g_bag_templates_keys[k]=item;g_bag_templates_count++;
        }
    }
    return 1;
fail:bag_templates_clear();return 0;
}
static int bag_repair_slot(const walk_view *v,const dd_action *a) {
    uint64_t begin=0;unsigned n=0;bag_slot here;
    int32_t want=(int32_t)a->u.item.amount;
    if(a->u.item.inv || !bag_span(v->raid,0,&begin,&n) || (unsigned)a->u.item.slot>=n)return 0;
    uint64_t p=begin+(uint64_t)(unsigned)a->u.item.slot*DD_ITEM_SIZE;
    if(!bag_read_slot(p,&here))return 0;
    if(want<=0) {
        if(here.count>0) {int32_t zero=0;memcpy((void *)(uintptr_t)p,&zero,4);if(g_item_clear)g_item_clear((void *)(uintptr_t)p);}
    } else {
        if(here.count<=0 || here.type!=a->u.item.type || here.id!=a->u.item.id) {
            unsigned k=0;for(;k<g_bag_templates_count;k++)
                if(g_bag_templates_keys[k].type==a->u.item.type && g_bag_templates_keys[k].id==a->u.item.id)break;
            if(k==g_bag_templates_count) {dd_log("Bag repair missing native template %08x/%08x",a->u.item.type,a->u.item.id);bag_templates_clear();return 0;}
            __declspec(align(16)) uint8_t temporary[DD_ITEM_SIZE];
            g_hamlet_copy(temporary,g_bag_templates[k]);
            g_hamlet_swap((void *)(uintptr_t)bag_base(v->raid,0),(uint32_t)a->u.item.slot,temporary);
            g_hamlet_destroy(temporary);
        }
        memcpy((void *)(uintptr_t)p,&want,4);
    }
    bag_recount(v->raid,0);
    dd_log("Bag repair slot %d = %d of %08x/%08x",a->u.item.slot,want,a->u.item.type,a->u.item.id);
    return 1;
}
/* src: 1 + the slot its item came from (0: not known), in inventory src_inv; copy: that slot keeps its own (a split). */
static void bag_send_slot_from(const walk_view *v, uint8_t inv, unsigned slot, const bag_slot *s, uint8_t src_inv, uint32_t src, uint8_t copy) {
    dd_action a;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = DD_ITEM_OP_BAG_SLOT;
    a.u.item.hero = 255;
    a.u.item.inv = inv;
    a.u.item.slot = (int32_t)slot;
    a.u.item.amount = (uint32_t)s->count;
    a.u.item.type = s->type;
    a.u.item.id = s->id;
    a.u.item.area = v->area;
    a.u.item.round = src;
    a.u.item.turn = src_inv;
    a.u.item.flag = copy;
    item_send(&a);
}
static void bag_send_slot(const walk_view *v, uint8_t inv, unsigned slot, const bag_slot *s) { bag_send_slot_from(v, inv, slot, s, 0, 0, 0); }
static int bag_same(const bag_slot *a, const bag_slot *b) { return a->count > 0 && b->count > 0 && a->type == b->type && a->id == b->id; }
/* Runs every frame. A change without a drag is the game's or an order's, and happens on the other copy by itself. */
/* The guest found another bag at a door: every slot of the host's, filled ones first so that what moves is still there to find. */
static void bag_send_all(const walk_view *v) {
    uint64_t begin = 0;
    unsigned count = 0;
    bag_slot s[DD_BAG_SLOTS];
    if (!bag_span(v->raid, 0, &begin, &count)) return;
    for (unsigned i = 0; i < count; i++)
        if (!bag_read_slot(begin + (uint64_t)i * DD_ITEM_SIZE, &s[i])) return;
    dd_log("Item: the guest asks for the bag; %u slots go out", count);
    dd_action check = {0};
    check.kind = DD_ACT_ITEM; check.u.item.op = DD_ITEM_OP_BAG_CHECK; check.u.item.hero = 255;
    check.u.item.type = walk_bag_hash(v->raid);
    dd_town_host_observed(&g_town, &check);
    for (int empties = 0; empties < 2; empties++)
        for (unsigned i = 0; i < count; i++)
            if ((s[i].count <= 0) == (empties != 0)) bag_send_slot(v, 0, i, &s[i]);
    check.u.item.flag = 1;
    dd_town_host_observed(&g_town, &check);
}
static void bag_poll(const walk_view *v) {
    bag_slot now[2][DD_BAG_SLOTS];
    unsigned count[2] = {0, 0};
    for (uint8_t inv = 0; inv < 2; inv++) {
        uint64_t begin = 0;
        if (!bag_span(v->raid, inv, &begin, &count[inv])) count[inv] = 0;
        for (unsigned i = 0; i < count[inv]; i++)
            if (!bag_read_slot(begin + (uint64_t)i * DD_ITEM_SIZE, &now[inv][i])) return;
    }
    if (g_bag_moved && g_bag_seen_valid && walk_shared()) {
        /* Filled slots first, so what moves is still there to find; emptied ones after. */
        for (int empties = 0; empties < 2; empties++)
            for (uint8_t inv = 0; inv < 2; inv++)
                for (unsigned i = 0; i < count[inv]; i++) {
                    const bag_slot *n = &now[inv][i];
                    bag_slot old = {0, 0, 0};
                    if (i < g_bag_seen_count[inv]) old = g_bag_seen[inv][i];
                    if ((n->count == 0) != (empties != 0) || !memcmp(n, &old, sizeof old)) continue;
                    if (n->count > 0 && !bag_same(n, &old)) {
                        /* Where it came from: a slot that had this kind and has it no more (a move or a swap), else one that still has
                         * it (a split: copied, so that slot keeps its share). Two stacks of one kind are then never mixed up. */
                        uint8_t src_inv = 0, copy = 0;
                        uint32_t src = 0;
                        for (int pass = 0; pass < 2 && !src; pass++)
                            for (uint8_t i2 = 0; i2 < 2 && !src; i2++)
                                for (unsigned j = 0; j < count[i2] && !src; j++) {
                                    bag_slot was = {0, 0, 0};
                                    if (i2 == inv && j == i) continue;
                                    if (j < g_bag_seen_count[i2]) was = g_bag_seen[i2][j];
                                    if (pass == 0 ? bag_same(&was, n) && !bag_same(&now[i2][j], n) : bag_same(&now[i2][j], n)) {
                                        src = j + 1;
                                        src_inv = i2;
                                        copy = (uint8_t)pass;
                                    }
                                }
                        bag_send_slot_from(v, inv, i, n, src_inv, src, copy);
                    } else
                        bag_send_slot(v, inv, i, n);
                }
    }
    g_bag_moved = 0;
    for (uint8_t inv = 0; inv < 2; inv++) {
        memcpy(g_bag_seen[inv], now[inv], count[inv] * sizeof(bag_slot));
        g_bag_seen_count[inv] = count[inv];
    }
    g_bag_seen_valid = 1;
}
static void bag_recount(uint64_t raid, uint8_t inv) {
    uint64_t begin = 0;
    unsigned count = 0;
    int32_t filled = 0;
    if (!bag_span(raid, inv, &begin, &count)) return;
    for (unsigned i = 0; i < count; i++) {
        int32_t c = 0;
        if (safe_read((uintptr_t)(begin + (uint64_t)i * DD_ITEM_SIZE), &c, 4) && c > 0) filled++;
    }
    memcpy((void *)(uintptr_t)(bag_base(raid, inv) + 0x24), &filled, 4);
}
static int bag_apply(const walk_view *v, const dd_action *a) {
    if(g_bag_repairing)return bag_repair_slot(v,a);
    uint64_t begin = 0, p;
    unsigned count = 0;
    bag_slot here;
    int32_t want = (int32_t)a->u.item.amount;
    if (!bag_span(v->raid, a->u.item.inv, &begin, &count) || (unsigned)a->u.item.slot >= count) {
        dd_log("Item: no bag slot %d here", a->u.item.slot);
        return 0;
    }
    p = begin + (uint64_t)a->u.item.slot * DD_ITEM_SIZE;
    if (!bag_read_slot(p, &here)) return 0;
    if (want <= 0) {
        if (here.count > 0) {
            int32_t zero = 0;
            memcpy((void *)(uintptr_t)p, &zero, 4);
            if (g_item_clear) g_item_clear((void *)(uintptr_t)p);
        }
    } else if (here.count > 0 && here.type == a->u.item.type && here.id == a->u.item.id) {
        memcpy((void *)(uintptr_t)p, &want, 4);
    } else {
        uint64_t found = 0;
        if (a->u.item.round && a->u.item.turn < 2) { /* the sender says where it came from */
            uint64_t b = 0;
            unsigned n = 0;
            bag_slot other;
            uint64_t q = 0;
            if (bag_span(v->raid, (uint8_t)a->u.item.turn, &b, &n) && a->u.item.round - 1 < n) q = b + (uint64_t)(a->u.item.round - 1) * DD_ITEM_SIZE;
            if (q && q != p && bag_read_slot(q, &other) && other.count > 0 && other.type == a->u.item.type && other.id == a->u.item.id) found = q;
        }
        if (found && a->u.item.flag && g_hamlet_copy && g_hamlet_swap && g_hamlet_destroy) {
            /* A split: the source keeps its share, this slot gets a copy made by the game's own Item copy (an Item owns
             * containers, so never a memcpy); what was here goes out with the temporary. */
#ifdef _MSC_VER
            __declspec(align(16))
#else
            _Alignas(16)
#endif
            uint8_t temporary[DD_ITEM_SIZE] = {0};
            g_hamlet_copy(temporary, (const void *)(uintptr_t)found);
            g_hamlet_swap((void *)(uintptr_t)bag_base(v->raid, a->u.item.inv), (uint32_t)a->u.item.slot, temporary);
            g_hamlet_destroy(temporary);
            memcpy((void *)(uintptr_t)p, &want, 4);
            bag_recount(v->raid, 0);
            bag_recount(v->raid, 1);
            dd_log("Item: bag %u slot %d is now %d of %08x, split off (role=%s)", a->u.item.inv, a->u.item.slot, want, a->u.item.type, dd_role_name(g_cfg.role));
            return 1;
        }
        for (uint8_t inv = 0; inv < 2 && !found; inv++) {
            uint64_t b = 0;
            unsigned n = 0;
            if (!bag_span(v->raid, inv, &b, &n)) continue;
            for (unsigned i = 0; i < n && !found; i++) {
                uint64_t q = b + (uint64_t)i * DD_ITEM_SIZE;
                bag_slot other;
                if (q != p && bag_read_slot(q, &other) && other.count > 0 && other.type == a->u.item.type && other.id == a->u.item.id) found = q;
            }
        }
        if (!found) {
            dd_log("Item: no item of kind %08x %08x here for bag slot %d", a->u.item.type, a->u.item.id, a->u.item.slot);
            return 0;
        }
        /* Item owns containers: move through native Inventory swaps so internal
         * references and lifetime follow the item, including the displaced one. */
        if(!g_hamlet_copy || !g_hamlet_clear || !g_hamlet_swap || !g_hamlet_destroy)return 0;
        uint8_t source_inv=0;unsigned source_slot=0;int source_ok=0;
        for(uint8_t inv=0;inv<2 && !source_ok;inv++) {
            uint64_t b=0;unsigned n=0;
            if(bag_span(v->raid,inv,&b,&n) && found>=b && found-b<(uint64_t)n*DD_ITEM_SIZE && (found-b)%DD_ITEM_SIZE==0) {
                source_inv=inv;source_slot=(unsigned)((found-b)/DD_ITEM_SIZE);source_ok=1;
            }
        }
        if(!source_ok)return 0;
        __declspec(align(16)) uint8_t temporary[DD_ITEM_SIZE];
        g_hamlet_copy(temporary,(const void *)(uintptr_t)p);g_hamlet_clear(temporary);
        g_hamlet_swap((void *)(uintptr_t)bag_base(v->raid,a->u.item.inv),(uint32_t)a->u.item.slot,temporary);
        g_hamlet_swap((void *)(uintptr_t)bag_base(v->raid,source_inv),source_slot,temporary);
        g_hamlet_swap((void *)(uintptr_t)bag_base(v->raid,a->u.item.inv),(uint32_t)a->u.item.slot,temporary);
        g_hamlet_destroy(temporary);
        memcpy((void *)(uintptr_t)p, &want, 4);
    }
    bag_recount(v->raid, 0);
    bag_recount(v->raid, 1);
    dd_log("Item: bag %u slot %d is now %d of %08x (role=%s)", a->u.item.inv, a->u.item.slot, want, a->u.item.type, dd_role_name(g_cfg.role));
    return 1;
}
static int item_is_camp(uint8_t op) { return op == DD_ITEM_OP_CAMP_MEAL || op == DD_ITEM_OP_CAMP_SKILL || op == DD_ITEM_OP_CAMP_LEAVE; }
static int item_is_loot(uint8_t op) { return op == DD_ITEM_OP_LOOT_ALL || op == DD_ITEM_OP_LOOT_CLOSE || op == DD_ITEM_OP_LOOT_ITEM; }
/* The host, for a guest's click: its own window has just been dealt with, so the first click has counted. Say no at once: everything the guest asks for waits behind this. */
static int loot_settled(const walk_view *v) {
    return g_cfg.role == DD_ROLE_HOST && g_loot_done_at && GetTickCount64() - g_loot_done_at < 5000 && !loot_open(v);
}
/* The town after a raid, as three numbers both logs can show: the currencies (vector at campaign+e70, 0x48 each: amount +0, kind +44),
 * the heroes in roster order (vector at campaign+20: id at the entry, Hero at entry+8 with its stress and trinkets) and their count.
 * Only in town: the town display is there and no raid is. */
/* The wallet as the town check last read it: currencies sorted by kind, empty ones left out. The order of the
 * native vector and a currency the engine created with 0 on one copy only are not differences (owner test77:
 * a currency mismatch after a raid whose loot, rewards and conversions were identical in both game logs). */
static uint32_t g_town_wallet[32][2];
static unsigned g_town_wallet_count;
static void town_wallet_text(char *out, size_t cap) {
    size_t k = 0;
    out[0] = 0;
    for (unsigned i = 0; i < g_town_wallet_count && k + 24 < cap; i++)
        k += (size_t)snprintf(out + k, cap - k, "%s%08x=%u", i ? " " : "", g_town_wallet[i][0], g_town_wallet[i][1]);
}
static int town_check_here(uint32_t out[3]) {
    dd_hamlet s;
    walk_view v;
    uint64_t panel = 0, begin = 0, end = 0;
    uint32_t money = 2166136261u, heroes = 2166136261u;
    uint32_t wallet[32][2];
    unsigned n = 0;
    if (g_walk_view(&v) || !g_hamlet_open_model(&s) || dd_hamlet_panel(&s, 0xec00a0, &panel) != 1) return 0;
    if (!hamlet_read(&s, s.campaign + 0xe70, &begin, 8) || !hamlet_read(&s, s.campaign + 0xe78, &end, 8) || end < begin ||
        (end - begin) % 0x48 || (end - begin) / 0x48 > 32)
        return 0;
    for (uint64_t p = begin; p < end; p += 0x48) {
        uint32_t amount = 0, kind = 0;
        if (!hamlet_read(&s, p, &amount, 4) || !hamlet_read(&s, p + 0x44, &kind, 4)) return 0;
        if (!amount) continue;
        unsigned at = n;
        while (at && wallet[at - 1][0] > kind) { wallet[at][0] = wallet[at - 1][0]; wallet[at][1] = wallet[at - 1][1]; at--; }
        if (at && wallet[at - 1][0] == kind) return 0; /* the same currency twice: not a wallet we understand */
        wallet[at][0] = kind;
        wallet[at][1] = amount;
        n++;
    }
    for (unsigned i = 0; i < n; i++) {
        money = (money ^ wallet[i][0]) * 16777619u;
        money = (money ^ wallet[i][1]) * 16777619u;
    }
    memcpy(g_town_wallet, wallet, sizeof wallet[0] * n);
    g_town_wallet_count = n;
    if (!hamlet_read(&s, s.campaign + 0x20, &begin, 8) || !hamlet_read(&s, s.campaign + 0x28, &end, 8) || end < begin ||
        (end - begin) % 8 || (end - begin) / 8 > DD_HAMLET_HEROES)
        return 0;
    for (uint64_t p = begin; p < end; p += 8) {
        uint64_t entry = 0;
        uint32_t values[3] = {0};
        if (!hamlet_read(&s, p, &entry, 8) || !entry || !hamlet_read(&s, entry, &values[0], 4) ||
            !hamlet_read(&s, entry + 8 + DD_HERO_STRESS, &values[1], 4) || !(values[2] = walk_trinket_hash(entry + 8)))
            return 0;
        for (unsigned i = 0; i < 3; i++) heroes = (heroes ^ values[i]) * 16777619u;
    }
    out[0] = money;
    out[1] = heroes;
    out[2] = (uint32_t)((end - begin) / 8);
    return 1;
}
/* Host: two seconds after the town is back, once per raid. Before that the town may still be filling in. */
static void town_check_poll(void) {
    uint32_t d[3];
    uint64_t now = GetTickCount64();
    dd_action a;
    if (g_cfg.role != DD_ROLE_HOST || !g_town_check_raid) return;
    if (!town_check_here(d)) {
        g_town_check_at = 0;
        return;
    }
    if (!g_town_check_at) {
        g_town_check_at = now + 2000;
        return;
    }
    if (now < g_town_check_at) return;
    g_town_check_raid = 0;
    g_town_check_at = 0;
    memset(&a, 0, sizeof a);
    a.kind = DD_ACT_ITEM;
    a.u.item.op = DD_ITEM_OP_TOWN;
    a.u.item.hero = 255;
    a.u.item.type = d[0];
    a.u.item.id = d[1];
    a.u.item.round = d[2];
    a.u.item.amount = g_town.next_apply_seq;
    activity_new_epoch(a.u.item.amount);
    dd_log("Town: back from the raid; currencies %08x, heroes %08x, %u heroes", d[0], d[1], d[2]);
    {
        char wallet[32 * 24 + 1];
        town_wallet_text(wallet, sizeof wallet);
        dd_log("Town: wallet here %s", wallet);
    }
    dd_town_host_observed(&g_town, &a);
    pool_start();
}
static int town_check_apply(const dd_action *a) {
    uint32_t d[3];
    if (g_cfg.role == DD_ROLE_HOST) return 0; /* only the host says what its town is */
    if (!town_check_here(d)) {
        dd_log("Town: not readable after the bounded load wait; shared comparison failed");
        runtime_sync_stop(3);
        return 0;
    }
    if (d[0] == a->u.item.type && d[1] == a->u.item.id && d[2] == a->u.item.round) {
        activity_new_epoch(a->u.item.amount);
        dd_log("Town: same as the host after the raid (%u heroes)", d[2]);
        return 1;
    }
    dd_log("Town: differs from the host after the raid: currencies %08x/%08x, heroes %08x/%08x, count %u/%u (here/host)", d[0], a->u.item.type, d[1],
           a->u.item.id, d[2], a->u.item.round);
    {
        char wallet[32 * 24 + 1];
        town_wallet_text(wallet, sizeof wallet);
        dd_log("Town: wallet here %s (compare with the host's \"Town: wallet here\" line)", wallet);
    }
    runtime_sync_stop(3);
    return 0; /* Never acknowledge a known different town or continue the FIFO. */
}
static int item_gate(const dd_msg *m) {
    walk_view v = {0};
    battle_view b;
    int go = 0;
    uint64_t limit = 3000;
    if (m->type != DD_MSG_TOWN_REQUEST && m->type != DD_MSG_TOWN_APPLY) return 0;
    /* A trinket taken out of the chest: the other copy's chest may not have given its loot yet. */
    int trinket = m->u.town.action.kind == DD_ACT_TRINKET_MOVE && (m->u.town.action.u.move.src_hero == DD_TRINKET_LOOT || m->u.town.action.u.move.dst_hero == DD_TRINKET_LOOT);
    if ((m->u.town.action.kind != DD_ACT_ITEM && !trinket) || !dd_action_valid(&m->u.town.action)) return 0;
    if (g_cfg.role == DD_ROLE_GUEST ? (m->type != DD_MSG_TOWN_APPLY || g_town.applied_seq == UINT32_MAX || m->seq != g_town.applied_seq + 1) : m->type != DD_MSG_TOWN_REQUEST)
        return 0;
    const dd_action *a = &m->u.town.action;
    int have = g_walk_view(&v);
    if (trinket) {
        limit = 30000;
        go = have && (loot_ready(&v) || loot_settled(&v));
    } else if (a->u.item.op == DD_ITEM_OP_RULES && a->u.item.flag) {
        limit = 10000;
        go = have && item_use_phase(&v) && v.area == a->u.item.area;
    } else if (a->u.item.op == DD_ITEM_OP_RULES && a->u.item.area && have && !v.battle && v.area == a->u.item.area) {
        /* Before a use: taken over at that spot, or the tiles this copy still passes draw on top of it. */
        float pos, delta;
        memcpy(&pos, &a->u.item.pos, 4);
        delta = pos - v.pos;
        go = delta < DD_WALK_USE_NEAR && delta > -DD_WALK_USE_NEAR;
    } else if (a->u.item.op == DD_ITEM_OP_RULES || a->u.item.op == DD_ITEM_OP_BAG_SLOT || a->u.item.op == DD_ITEM_OP_BAG_ASK) {
        go = 1;
    } else if (a->u.item.op == DD_ITEM_OP_BAG_CHECK) {
        limit = 10000; go = have;
    } else if (a->u.item.op == DD_ITEM_OP_USE && !a->u.item.state) {
        limit = 10000;
        go = have && item_use_phase(&v) && v.area == a->u.item.area;
    } else if (a->u.item.op == DD_ITEM_OP_QUEST_RETURN) {
        limit = 30000;
        go = g_quest_return_done || (have && quest_return_gate_ready(&v));
    } else if (a->u.item.op == DD_ITEM_OP_HUNGER) {
        limit = 30000;
        go = have && hunger_gate_ready(&v, a);
    } else if (a->u.item.op == DD_ITEM_OP_TOWN) {
        uint32_t d[3];
        limit = 60000; /* the guest may still be loading the town */
        go = g_cfg.role == DD_ROLE_HOST || town_check_here(d);
    } else if (a->u.item.op == DD_ITEM_OP_RESULTS_NEXT) {
        limit = 10000;
        go = g_results_display && g_results_can && g_results_can((void *)(uintptr_t)g_results_display);
    } else if (item_is_loot(a->u.item.op)) {
        limit = 30000;
        go = have && (loot_ready(&v) || loot_settled(&v));
    } else if (a->u.item.op == DD_ITEM_OP_CURIO) {
        limit = 5000;
        go = have && curio_open(&v);
    } else if (a->u.item.op == DD_ITEM_OP_SELECT) {
        /* The faster copy emits its post-battle selection before the other
         * native victory presentation finishes. Keep the expected FIFO head
         * intact; selecting during that last battle frame would be refused. */
        limit = 30000;
        go = have && !v.battle && v.area == a->u.item.area;
    } else if (a->u.item.op == DD_ITEM_OP_QUEST_ONE || a->u.item.op == DD_ITEM_OP_QUEST_TWO) {
        go = 1;
    } else if (item_is_camp(a->u.item.op)) {
        uint32_t stage = 0;
        /* A player may be reading a panel over the camp before Leave can follow; give them time and say why. */
        limit = a->u.item.op == DD_ITEM_OP_CAMP_LEAVE ? 300000 : 30000;
        go = have && safe_read((uintptr_t)v.raid + DD_ITEM_CAMP_STAGE, &stage, 4) && stage == a->u.item.round;
        if (a->u.item.op == DD_ITEM_OP_CAMP_LEAVE && go) {
            go = camp_leave_ready(&v);
            camp_leave_wait_notice(!go);
        }
    } else if (a->u.item.op == DD_ITEM_OP_TUTORIAL) {
        go = 1; /* No longer sent; an old order closes a popup that is open here, else does nothing. */
    } else if (a->u.item.state) {
        limit = 30000;
        go = have && battle_read(&v, &b) && battle_place_compare(&b, a->u.item.round, a->u.item.turn, a->u.item.state) >= 0;
    } else if (g_cfg.role == DD_ROLE_HOST) {
        go = 1;
    } else {
        float pos, delta;
        memcpy(&pos, &a->u.item.pos, 4);
        delta = pos - v.pos;
        go = have && !v.battle && v.state > 1 && v.area == a->u.item.area && delta < DD_WALK_USE_NEAR && delta > -DD_WALK_USE_NEAR;
    }
    uint64_t now = GetTickCount64();
    if (!g_item_wait_until) g_item_wait_until = now + limit;
    if (!go && now <= g_item_wait_until) {
        if (!g_item_gate_told) {
            g_item_gate_told = 1;
            dd_log("Item: order op=%u waits for this copy (readable=%d battle=%u area=%08x expected=%08x)",
                   a->u.item.op, have, v.battle, v.area, a->u.item.area);
        }
        return 1;
    }
    g_item_gate_told = 0;
    g_item_wait_until = 0;
    return 0;
}
/* SELECT has a persistent identity, not just a rank that a shuffle may reuse.
 * The pinned606450 native selector writes Party.selected+430 (raid+438).
 * Check the effect before ACK; a missing native function is never success. */
static int item_select_apply(const walk_view *v, const dd_action *a) {
    uint64_t begin = 0, end = 0, hero = 0, selected = 0;
    int32_t before_field = 0, after_field = -1;
    unsigned rank = a->u.item.hero;
    if (v->battle || v->area != a->u.item.area || !g_party_select || rank >= 4 ||
        !a->u.item.id || !safe_read((uintptr_t)v->raid + 8, &begin, 8) || !begin ||
        !safe_read((uintptr_t)v->raid + 0x10, &end, 8) || end < begin ||
        (end-begin)%8 || (end-begin)/8 > 4 || rank >= (end-begin)/8 ||
        !safe_read((uintptr_t)(begin+rank*8), &hero, 8) || !hero ||
        battle_hero_id(hero) != a->u.item.id ||
        !safe_read((uintptr_t)v->display + 0x3f88, &before_field, 4)) {
        dd_log("Item: selection refused (battle=%u area=%08x expected=%08x rank=%u hero=%u)",
               v->battle, v->area, a->u.item.area, rank, a->u.item.id);
        return 0;
    }
    walk_select(v, (uint8_t)rank);
    if (!safe_read((uintptr_t)v->raid + 0x438, &selected, 8) || selected != hero ||
        !safe_read((uintptr_t)v->display + 0x3f88, &after_field, 4) ||
        after_field != (int32_t)((end-begin)/8)-1-(int32_t)rank ||
        battle_hero_id(selected) != a->u.item.id) {
        dd_log("Item: native selection effect could not be verified (rank=%u hero=%u)",rank,a->u.item.id);
        runtime_sync_stop(3);
        return 0;
    }
    dd_log("Item: hero %u selected (rank=%u role=%s)", a->u.item.id, rank, dd_role_name(g_cfg.role));
    return 1;
}
static int item_apply(const dd_action *a) {
    if (a->u.item.op == DD_ITEM_OP_QUEST_RETURN) return quest_return_apply(a);
    if (a->u.item.op == DD_ITEM_OP_HUNGER) return hunger_apply(a);
    walk_view v;
    battle_view b = {0};
    int refused = 0; /* A missing effect cannot be acknowledged on a guest. */
    uint64_t overlay;
    uint8_t one = 1;
    if (a->u.item.op == DD_ITEM_OP_RULES) {
        if (a->u.item.flag && (!g_walk_view(&v) || !item_use_phase(&v) || v.area != a->u.item.area)) {
            if (g_cfg.role == DD_ROLE_GUEST) runtime_sync_stop(3);
            return 0;
        }
        uint32_t here = battle_rules();
        if (g_cfg.role == DD_ROLE_GUEST && g_rules_object && a->u.item.rules && here != a->u.item.rules) {
            dd_log("Item: Rules differed: here %08x, host %08x; taken over from the host", here, a->u.item.rules);
            memcpy((uint8_t *)g_rules_object + 8, &a->u.item.rules, 4);
        }
        return 1;
    }
    if (a->u.item.op == DD_ITEM_OP_TOWN) return town_check_apply(a);
    if (a->u.item.op == DD_ITEM_OP_RESULTS_NEXT) {
        if (!g_results_next || !g_results_display || !g_results_can || !g_results_can((void *)(uintptr_t)g_results_display)) {
            dd_log("Item: the results screen cannot go on here");
            return refused;
        }
        if (g_cfg.role == DD_ROLE_HOST) results_send_rules();
        g_results_asked_at = 0;
        dd_log("Item: results screen on (role=%s)", dd_role_name(g_cfg.role));
        g_results_next((void *)(uintptr_t)g_results_display);
        return 1;
    }
    if (!g_walk_view(&v)) {
        if (a->u.item.op == DD_ITEM_OP_SELECT)
            dd_log("Item: selection has no readable raid after bounded wait (area=%08x rank=%u hero=%u)",a->u.item.area,a->u.item.hero,a->u.item.id);
        if (a->u.item.op == DD_ITEM_OP_BAG_CHECK) { runtime_sync_stop(2); return 0; }
        return refused;
    }
    if (a->u.item.state && battle_rules() != a->u.item.rules) {
        dd_log("Item: combat Rules differ before op=%u: here %08x, expected %08x", a->u.item.op, battle_rules(), a->u.item.rules);
        runtime_sync_stop(3); return 0;
    }
    if (a->u.item.op == DD_ITEM_OP_BAG_CHECK) {
        if (g_cfg.role != DD_ROLE_GUEST) return 0;
        if (!a->u.item.flag) {
            if(!bag_templates_begin(&v)){runtime_sync_stop(2);return 0;}
            g_bag_repairing = 1; g_bag_repair_hash = a->u.item.type; return 1;
        }
        uint32_t actual = walk_bag_hash(v.raid);
        if (!g_bag_repairing || a->u.item.type != g_bag_repair_hash || actual != a->u.item.type) {
            dd_log("Bag repair failed: expected=%08x actual=%08x", a->u.item.type, actual);
            bag_templates_clear();runtime_sync_stop(2); return 0;
        }
        bag_templates_clear();g_bag_repairing = 0; g_bag_repair_hash = 0;
        dd_log("Bag repair verified: %08x", actual);
        return 1;
    }
    if (a->u.item.op == DD_ITEM_OP_USE && !a->u.item.state && (!item_use_phase(&v) || v.area != a->u.item.area)) {
        dd_log("Item: delayed outside-fight use rejected after transition (battle=%u)", v.battle);
        if (g_cfg.role == DD_ROLE_GUEST) runtime_sync_stop(3);
        return 0;
    }
    if (a->u.item.op == DD_ITEM_OP_BAG_SLOT) return bag_apply(&v, a);
    overlay = v.display + DD_WALK_LOOT_OVERLAY;
    if (a->u.item.op == DD_ITEM_OP_SELECT) {
        return item_select_apply(&v, a);
    }
    if (item_is_camp(a->u.item.op)) {
        uint32_t stage = 0;
        if (v.battle || !safe_read((uintptr_t)v.raid + DD_ITEM_CAMP_STAGE, &stage, 4) || stage != a->u.item.round) {
            dd_log("Item: camp order op=%u for part %u, this copy is at part %u", a->u.item.op, a->u.item.round, stage);
            return refused;
        }
        dd_log("Item: camp op=%u (role=%s)", a->u.item.op, dd_role_name(g_cfg.role));
        if (a->u.item.op == DD_ITEM_OP_CAMP_MEAL) {
            uint8_t args[0x28] = {0};
            uint64_t camp = v.display + DD_ITEM_CAMP_OVERLAY;
            if (!g_camp_meal) return refused;
            args[8] = a->u.item.flag;
            memcpy(args + 0x0c, &a->u.item.amount, 4);
            memcpy(args + 0x10, &a->u.item.turn, 4);
            memcpy(args + 0x18, &a->u.item.type, 4);
            memcpy(args + 0x1c, &a->u.item.id, 4);
            memcpy(args + 0x20, &camp, 8);
            g_camp_meal(args);
        } else if (a->u.item.op == DD_ITEM_OP_CAMP_SKILL) {
            uint64_t hero = 0, none = 0;
            int32_t no_skill = -1;
            if (!g_camp_skill || !camp_skill_ready(&v, a->u.item.turn, a->u.item.type)) {
                dd_log("Camping skill refused: actor/selected skill is unavailable (rank=%u selected=%u)", a->u.item.turn, a->u.item.type);
                return refused;
            }
            g_camp_skill((void *)(uintptr_t)(v.raid + 0x5100), (void *)(uintptr_t)(v.raid + 8), (void *)(uintptr_t)(v.raid + 0x5140), a->u.item.turn, a->u.item.type, a->u.item.id);
            /* What the button does after it (70af4e): no skill chosen any more. */
            if (safe_read((uintptr_t)v.raid + 0x438, &hero, 8) && hero) memcpy((void *)(uintptr_t)(hero + 0x141c), &no_skill, 4);
            memcpy((void *)(uintptr_t)(v.display + DD_ITEM_CAMP_OVERLAY + 0x10), &none, 8);
        } else {
            uint64_t functor[2] = {0, v.display + DD_ITEM_CAMP_OVERLAY};
            camp_leave_wait_notice(0);
            if (!g_camp_leave) return refused;
            if (!camp_leave_ready(&v)) {
                /* The native call would silently do nothing here; never acknowledge a leave that did not start. */
                dd_log("Item: camp leave refused: this camp is covered by another panel (inactive)");
                return refused;
            }
            g_camp_leave(functor);
        }
        return 1;
    }
    if (a->u.item.op == DD_ITEM_OP_QUEST_ONE || a->u.item.op == DD_ITEM_OP_QUEST_TWO) {
        uint64_t functor[2] = {0, v.display + DD_ITEM_QUEST_INFO};
        quest_choice_fn native = a->u.item.op == DD_ITEM_OP_QUEST_ONE ? g_quest_one : g_quest_two;
        if (!native) return refused;
        dd_log("Item: quest choice op=%u (role=%s)", a->u.item.op, dd_role_name(g_cfg.role));
        native(functor);
        return 1;
    }
    if (a->u.item.op == DD_ITEM_OP_RETREAT) {
        if (a->u.item.state && (!battle_read(&v, &b) || battle_place_compare(&b, a->u.item.round, a->u.item.turn, a->u.item.state))) {
            dd_log("Item: retreat does not fit this fight here (round=%u turn=%u state=%u)", b.round, b.turn, b.state);
            return refused;
        }
        if (!g_retreat) return refused;
        dd_log("Item: retreat (role=%s)", dd_role_name(g_cfg.role));
        g_retreat((void *)(uintptr_t)(v.display + DD_ITEM_QUEST_INFO));
        return 1;
    }
    if (a->u.item.op == DD_ITEM_OP_TUTORIAL) {
        uint64_t functor[2] = {0, g_tutorial_panel};
        if (!g_panel_close || !tutorial_open()) {
            dd_log("Item: no tutorial popup open here; nothing to close");
            return 1;
        }
        dd_log("Item: tutorial popup closed (role=%s)", dd_role_name(g_cfg.role));
        g_panel_close(functor);
        return 1;
    }
    if (item_is_loot(a->u.item.op)) {
        if (!loot_ready(&v)) {
            dd_log("Item: loot window not ready here for op=%u", a->u.item.op);
            return 0;
        }
        dd_log("Item: loot op=%u slot=%d (role=%s)", a->u.item.op, a->u.item.slot, dd_role_name(g_cfg.role));
        g_loot_done_at = GetTickCount64();
        g_loot_asked_at = 0;
        g_loot_request = 0;
        if (a->u.item.op == DD_ITEM_OP_LOOT_ALL) memcpy((void *)(uintptr_t)(overlay + 0x218), &one, 1);
        else if (a->u.item.op == DD_ITEM_OP_LOOT_CLOSE) memcpy((void *)(uintptr_t)(overlay + 0x219), &one, 1);
        else if (g_loot_take_item) {
            uint64_t button[6] = {0};
            int32_t slot = a->u.item.slot;
            memcpy((uint8_t *)button + 0x10, &slot, 4);
            button[4] = v.raid + DD_WALK_RAID_LOOT - 0x100;
            button[5] = (uintptr_t)g_loot_dummy_widget;
            g_loot_take_item(button);
        }
        return 1;
    }
    if (a->u.item.state && (!battle_read(&v, &b) || battle_place_compare(&b, a->u.item.round, a->u.item.turn, a->u.item.state))) {
        dd_log("Item: order op=%u does not fit this fight here (round=%u turn=%u state=%u)", a->u.item.op, b.round, b.turn, b.state);
        return refused;
    }
    uint32_t rules = battle_rules();
    if (rules != a->u.item.rules) {
        dd_log("Item: Rules differ at this order: here %08x, there %08x", rules, a->u.item.rules);
    }
    if (a->u.item.op == DD_ITEM_OP_BAG_ASK) {
        if (g_cfg.role == DD_ROLE_HOST) bag_send_all(&v);
        return 1;
    }
    if (a->u.item.op == DD_ITEM_OP_CURIO) return 0; /* Removed optimistic legacy path in protocol33. */
    if (a->u.item.op == DD_ITEM_OP_TORCH) {
        float amount;
        memcpy(&amount, &a->u.item.amount, 4);
        if (!g_torch) return refused;
        dd_log("Item: torch by %f (role=%s)", amount, dd_role_name(g_cfg.role));
        g_torch((void *)(uintptr_t)v.raid, amount, a->u.item.flag);
        return 1;
    }
    uint64_t hero = a->u.item.hero == 255 ? 0 : battle_target_at(v.raid, 0, a->u.item.hero), controller = g_item_controller();
    if (!g_item_use || !controller || (a->u.item.hero != 255 && !hero)) {
        dd_log("Item: cannot use it here");
        return refused;
    }
    if (g_cfg.role == DD_ROLE_HOST && a->u.item.state && hero_owner(hero) != (g_request_player ? g_request_player : 1)) {
        dd_log("Item: guest player %d asked for a hero that is not theirs", g_request_player);
        return 0;
    }
    dd_action choice={0};
    int curio_route=prop_item_route(&v,(void *)(uintptr_t)controller,
        (void *)(uintptr_t)(v.raid+(a->u.item.inv?DD_WALK_RAID_LOOT:DD_WALK_RAID_INVENTORY)),a->u.item.slot,&choice);
    if(curio_route) {dd_log("Item: ordinary use now routes to a shared curio choice; refused without effects");return 0;}
    if (g_cfg.role == DD_ROLE_HOST) walk_rules_send(&v);
    g_item_use_asked_at = 0;
    dd_log("Item: use slot=%d inv=%u hero=%u (role=%s)", a->u.item.slot, a->u.item.inv, a->u.item.hero, dd_role_name(g_cfg.role));
    g_item_use((void *)(uintptr_t)controller, (void *)(uintptr_t)hero, (void *)(uintptr_t)(v.raid + (a->u.item.inv ? DD_WALK_RAID_LOOT : DD_WALK_RAID_INVENTORY)), a->u.item.slot, a->u.item.flag);
    return 1;
}
