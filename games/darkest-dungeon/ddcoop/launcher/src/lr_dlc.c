#include "lr_dlc.h"
#include "lr_files.h"
#include "lr_wire.h"
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name, *relative, *files[3], *base; } feature;
/* Paths match the installed campaign feature layout, including independently
 * enabled hero/district flags. Do not accept an empty package directory. */
static const feature features[] = {
    {"musketeer", "dlc/445700_musketeer", {"heroes/musketeer/musketeer.info.darkest", "upgrades/heroes/musketeer.upgrades.json", NULL}, "raid/camping/default.camping_skills.json"},
    {"shieldbreaker", "dlc/702540_shieldbreaker", {"heroes/shieldbreaker/shieldbreaker.info.darkest", "upgrades/heroes/shieldbreaker.upgrades.json", "raid/camping/shieldbreaker.camping_skills.json"}, NULL},
    {"flagellant", "dlc/580100_crimson_court/features/flagellant", {"heroes/flagellant/flagellant.info.darkest", "upgrades/heroes/flagellant.upgrades.json", "raid/camping/crimson_court.camping_skills.json"}, NULL},
    {"districts", "dlc/580100_crimson_court/features/districts", {"campaign/town/districts/districts_districts.json", NULL, NULL}, NULL},
    {"crimson_court", "dlc/580100_crimson_court/features/crimson_court", {"campaign/quest/crimson_court.quest.plot_quests.json", "campaign/roster/roster.contagion.json", NULL}, NULL},
    {"color_of_madness", "dlc/735730_color_of_madness", {"campaign/quest/com.quest.plot_quests.json", "campaign/provision/com.provision.json", NULL}, NULL},
    {"fires_edge", "dlc/4964110_fires_edge/features/fires_edge", {"campaign/town/districts/runaway_duelist.districts.json", NULL, NULL}, NULL},
    {"runaway", "dlc/4964110_fires_edge/features/runaway", {"heroes/runaway/runaway.info.darkest", "upgrades/heroes/runaway.upgrades.json", "raid/camping/runaway.camping_skills.json"}, NULL},
    {"duelist", "dlc/4964110_fires_edge/features/duelist", {"heroes/duelist/duelist.info.darkest", "upgrades/heroes/duelist.upgrades.json", "raid/camping/duelist.camping_skills.json"}, NULL},
};
static const feature *find(const char *name) {
    if (name) for (size_t i = 0; i < sizeof features / sizeof *features; i++)
        if (!strcmp(name, features[i].name)) return &features[i];
    return NULL;
}
int lr_dlc_known(const char *name) { return find(name) != NULL; }
static int nonempty(const char *root, const char *relative) {
    char path[LR_PATH_MAX]; uint8_t *data = NULL; size_t len = 0;
    lr_join(path, sizeof path, root, relative);
    int ok = lr_read_file(path, LR_MAX_FILE_SIZE, &data, &len) && len > 0;
    free(data); return ok;
}
int lr_dlc_present(const char *root, const char *name) {
    const feature *f = find(name);
    if (!root || !f) return 0;
    char dir[LR_PATH_MAX]; lr_join(dir, sizeof dir, root, f->relative);
    for (size_t i = 0; i < 3 && f->files[i]; i++) if (!nonempty(dir, f->files[i])) return 0;
    return !f->base || nonempty(root, f->base);
}
