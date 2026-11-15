/* Sanitarium treatment choice (DD_ACT_TREATMENT, protocol38).
 *
 * Owner, 15 Nov: a hero placed in the Sanitarium to lose kleptomania kept it on the guest. Placing a hero there was shared,
 * but the quirk clicked in that slot's list was not: QuirkTreatmentActivityDisplay's click (6a3910, lambda_1 of
 * DrawQuirkTreatmentData) only writes the activity's treatment record on the copy it was clicked on, and the week reads
 * that record. The click is now one ordered action; every copy produces the same record.
 *
 * QuirkTreatment (vtable e9b468; the quirk and the disease activity are both of this class, +23e set on the disease one):
 *   +78   slots, 24 bytes each: hero ID at +0
 *   +190  treatment data, 0x1e0 per slot = three 0xa0 groups (positive, negative, disease)
 *   group +0 entries (0x88 each: name at +0, flags +81 may be removed / +82 may be locked, state at +84),
 *         +18 the chosen quirk's name (0x80), +98 the group's state (0 none, 1, 2).
 * 6a3910 (functor: +8 activity, +10 group, +14 slot, +18 entry) toggles exactly as treatment_result below. Its entries
 * exist where that slot's list was built (loaded from the save or shown); where they are not, the record itself is
 * written with the same result, which is what the week and the save read. tests/test_treatment_layout.py proves this. */
typedef void(__fastcall *treatment_select_fn)(void *);
static treatment_select_fn g_treatment_select;
#define DD_TREATMENT_SLOT 0x1e0u
#define DD_TREATMENT_GROUP 0xa0u
#define DD_TREATMENT_ENTRY 0x88u
static int treatment_record(uint64_t activity, unsigned slot, unsigned group, uint64_t *record) {
    uint64_t begin = 0, end = 0;
    if (!activity || slot >= 3 || group >= 3 || !safe_read((uintptr_t)activity + 0x190, &begin, 8) ||
        !safe_read((uintptr_t)activity + 0x198, &end, 8) || end < begin || (end - begin) % DD_TREATMENT_SLOT ||
        (end - begin) / DD_TREATMENT_SLOT <= slot)
        return 0;
    *record = begin + (uint64_t)slot * DD_TREATMENT_SLOT + (uint64_t)group * DD_TREATMENT_GROUP;
    return 1;
}
static int treatment_entry(uint64_t record, unsigned index, uint64_t *entry) {
    uint64_t begin = 0, end = 0;
    if (!safe_read((uintptr_t)record, &begin, 8) || !safe_read((uintptr_t)record + 8, &end, 8) || end < begin ||
        (end - begin) % DD_TREATMENT_ENTRY || (end - begin) / DD_TREATMENT_ENTRY <= index)
        return 0;
    *entry = begin + (uint64_t)index * DD_TREATMENT_ENTRY;
    return 1;
}
static int treatment_slot_hero(uint64_t activity, unsigned slot, uint32_t *hero) {
    uint64_t begin = 0, end = 0;
    *hero = 0;
    return safe_read((uintptr_t)activity + 0x78, &begin, 8) && safe_read((uintptr_t)activity + 0x80, &end, 8) && end >= begin &&
           (end - begin) % 24 == 0 && (end - begin) / 24 > slot && safe_read((uintptr_t)(begin + (uint64_t)slot * 24), hero, 4) && *hero;
}
/* 538580: the next state the entry allows after `state` (1 needs +81, 2 needs +82, after 2 comes 0). */
static uint32_t treatment_next(const uint8_t flags[2], uint32_t state) {
    for (unsigned guard = 0; guard < 4; guard++) {
        state = state == 3 ? 0 : state + 1;
        if (state == 0 || state >= 3 || flags[state - 1]) return state >= 3 ? 0 : state;
    }
    return 0;
}
/* What 6a3910 does for this entry, without doing it: the record's new state and whether its name becomes the entry's. */
static int treatment_result(uint64_t record, uint64_t entry, char name[64], uint32_t *state) {
    char chosen[0x80], quirk[0x80];
    uint8_t flags[2] = {0, 0};
    uint32_t current = 0;
    if (!safe_read((uintptr_t)record + 0x18, chosen, sizeof chosen) || !safe_read((uintptr_t)entry, quirk, sizeof quirk) ||
        !safe_read((uintptr_t)entry + 0x81, flags, 2) || !safe_read((uintptr_t)entry + 0x84, &current, 4))
        return 0;
    quirk[sizeof quirk - 1] = 0;
    if (strnlen(quirk, sizeof quirk) >= 64) return 0;
    memcpy(name, quirk, 64);
    if (strncmp(chosen, quirk, sizeof quirk)) *state = flags[0] ? 1 : treatment_next(flags, 1);
    else *state = treatment_next(flags, current == 3 ? 3 : current);
    return 1;
}
static void treatment_call(uint64_t activity, unsigned group, unsigned slot, unsigned entry) {
    uint8_t functor[0x20] = {0};
    int32_t g = (int32_t)group, s = (int32_t)slot, e = (int32_t)entry;
    memcpy(functor + 8, &activity, 8);
    memcpy(functor + 0x10, &g, 4);
    memcpy(functor + 0x14, &s, 4);
    memcpy(functor + 0x18, &e, 4);
    g_treatment_select(functor);
}
static int treatment_apply(const dd_action *a) {
    uint64_t activity = (uintptr_t)activity_in_town(g_town_object(), a->u.treatment.activity), record = 0, entry = 0;
    uint32_t hero = 0, state = 0;
    char name[64] = {0}, chosen[0x80] = {0};
    if (!g_treatment_select || !activity || !treatment_slot_hero(activity, a->u.treatment.slot, &hero) ||
        hero != a->u.treatment.hero_id || !treatment_record(activity, a->u.treatment.slot, a->u.treatment.group, &record)) {
        dd_log("Sanitarium: treatment refused: activity %u slot %u has another hero or no record here (hero %u, wanted %u)",
               a->u.treatment.activity, a->u.treatment.slot, hero, a->u.treatment.hero_id);
        return 0;
    }
    int have_entry = treatment_entry(record, a->u.treatment.entry, &entry) && treatment_result(record, entry, name, &state) &&
                     !strcmp(name, a->u.treatment.quirk);
    if (have_entry && state == a->u.treatment.state) {
        treatment_call(activity, a->u.treatment.group, a->u.treatment.slot, a->u.treatment.entry);
    } else {
        /* No list here (never built), or this copy's list would give another state: write what 6a3910 writes with the
         * sender's result. A new name is always copied; the name is only cleared when the same quirk goes to state 0. */
        char current[0x80] = {0}, value[0x80] = {0};
        uint32_t s = a->u.treatment.state;
        if (!safe_read((uintptr_t)record + 0x18, current, sizeof current)) return 0;
        int same = !strncmp(current, a->u.treatment.quirk, 64);
        if (!same) memcpy(value, a->u.treatment.quirk, strnlen(a->u.treatment.quirk, 63));
        if ((!same || !s) && !WriteProcessMemory(GetCurrentProcess(), (void *)(uintptr_t)(record + 0x18), value, sizeof value, NULL))
            return 0;
        if (have_entry && !WriteProcessMemory(GetCurrentProcess(), (void *)(uintptr_t)(entry + 0x84), &s, 4, NULL)) return 0;
        if (!WriteProcessMemory(GetCurrentProcess(), (void *)(uintptr_t)(record + 0x98), &s, 4, NULL)) return 0;
        if (have_entry) dd_log("Sanitarium: treatment of %s gives state %u here, %u at the sender; the sender's is written", name, state, s);
        else dd_log("Sanitarium: treatment written into the record (no list built here)");
    }
    uint32_t after = 99;
    if (!safe_read((uintptr_t)record + 0x98, &after, 4) || after != a->u.treatment.state ||
        !safe_read((uintptr_t)record + 0x18, chosen, sizeof chosen) ||
        (a->u.treatment.state && strncmp(chosen, a->u.treatment.quirk, 64))) {
        dd_log("Sanitarium: treatment record differs after the click; shared actions stop");
        runtime_sync_stop(1);
        return 0;
    }
    dd_log("Sanitarium: treatment activity=%u slot=%u group=%u %s -> state %u (role=%s)", a->u.treatment.activity,
           a->u.treatment.slot, a->u.treatment.group, a->u.treatment.quirk, a->u.treatment.state, dd_role_name(g_cfg.role));
    return 1;
}
static void __fastcall hook_treatment_select(void *functor) {
    uint64_t activity = 0, record = 0, entry = 0;
    int32_t group = -1, slot = -1, index = -1;
    uint32_t hero = 0, state = 0;
    dd_action a = {0};
    if (!g_treatment_select) return;
    if (g_game_apply_depth || g_cfg.role == DD_ROLE_OFF || !InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        g_treatment_select(functor);
        return;
    }
    if (runtime_shared_blocked() || runtime_waiting_for_peer()) return;
    int kind = -1;
    if (!safe_read((uintptr_t)functor + 8, &activity, 8) || !safe_read((uintptr_t)functor + 0x10, &group, 4) ||
        !safe_read((uintptr_t)functor + 0x14, &slot, 4) || !safe_read((uintptr_t)functor + 0x18, &index, 4) ||
        group < 0 || slot < 0 || index < 0 ||
        ((kind = g_activity_kind((void *)(uintptr_t)activity)) != DD_ACTIVITY_QUIRK_TREATMENT && kind != DD_ACTIVITY_DISEASE_TREATMENT) ||
        !treatment_slot_hero(activity, (unsigned)slot, &hero) || !treatment_record(activity, (unsigned)slot, (unsigned)group, &record) ||
        !treatment_entry(record, (unsigned)index, &entry) || !treatment_result(record, entry, a.u.treatment.quirk, &state)) {
        dd_log("Sanitarium: treatment click not recognised (kind=%d slot=%d group=%d entry=%d); nothing changed", kind, slot, group, index);
        shop_notice("This treatment choice could not be shared. Nothing changed; send the logs.");
        return;
    }
    a.kind = DD_ACT_TREATMENT;
    a.u.treatment.activity = (uint8_t)kind;
    a.u.treatment.slot = (uint8_t)slot;
    a.u.treatment.group = (uint8_t)group;
    a.u.treatment.entry = (uint16_t)index;
    a.u.treatment.state = (uint8_t)state;
    a.u.treatment.hero_id = hero;
    if (!dd_action_valid(&a)) {
        dd_log("Sanitarium: treatment choice not shareable (entry %d); nothing changed", index);
        return;
    }
    dd_log("Sanitarium: treatment clicked here: activity=%d slot=%d group=%d %s -> state %u (role=%s)", kind, slot, group,
           a.u.treatment.quirk, state, dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) (void)dd_town_guest_local(&g_town, &a);
    else (void)dd_town_host_local(&g_town, &a);
}
