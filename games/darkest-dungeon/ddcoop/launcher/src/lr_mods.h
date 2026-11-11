/* Mods (platform independent; launcher/tests/test_lr_mods.c, design in launcher/MODS.md).
 *
 * A Darkest Dungeon campaign lists the mods it uses in persist.game.json (base_root.persistent_ugcs, read
 * by lr_game_parse). The guest receives the host's save, so its game loads the same list; but its game
 * would simulate differently, and the copies drift apart without any error, when a mod is missing or has a
 * different version on one PC. So the host finds each listed mod on its PC and makes a fingerprint of
 * all its files; the guest does the same and joins only when every fingerprint matches.
 *
 * Where mods are: Steam Workshop items in <steamapps>\workshop\content\262060\<item id>, local mods in
 * <game>\mods\<folder>. The fingerprint is SHA-256 over every file of the mod folder, in sorted order:
 * relative path ('/' separated), size and the file's own SHA-256. Links are not followed. */
#ifndef LR_MODS_H
#define LR_MODS_H

#include <stddef.h>
#include <stdint.h>

#include "lr_roster.h"

#define LR_MOD_TITLE_MAX 63
#define LR_MOD_MAX_FILES 50000 /* per mod; a bigger folder is refused rather than half checked */
#define LR_MOD_MAX_DEPTH 12

typedef struct {
    uint32_t h[8];
    uint64_t bytes;
    uint8_t block[64];
    size_t used;
} lr_sha256_ctx;

void lr_sha256_init(lr_sha256_ctx *s);
void lr_sha256_update(lr_sha256_ctx *s, const uint8_t *p, size_t n);
void lr_sha256_final(lr_sha256_ctx *s, uint8_t out[32]);

typedef struct {
    lr_ugc id;                        /* the mod as the campaign names it */
    int found;                        /* 1 = its folder was found and every file read */
    uint32_t files;
    uint64_t bytes;
    uint8_t sha256[32];               /* all zero when not found */
    char title[LR_MOD_TITLE_MAX + 1]; /* <Title> of the mod's project.xml, for messages; may be empty */
} lr_mod_print;

/* The folder of a campaign's mod on this PC. game_dir is ...\steamapps\common\DarkestDungeon.
 * Returns 1 and the folder, 0 when it is not there. */
int lr_mods_locate(const char *game_dir, const lr_ugc *u, char *dir, size_t cap);

/* Fingerprint of every file in dir (and its title). 1 = complete. */
int lr_mods_fingerprint(const char *dir, lr_mod_print *p);

/* Both: locate, then fingerprint. Always fills p->id; p->found says whether it worked. */
void lr_mods_print(const char *game_dir, const lr_ugc *u, lr_mod_print *p);

/* The host's list for the LR_MODS message. 1 = encoded (*len may be 0 for no mods). */
int lr_mods_encode(const lr_mod_print *m, int count, uint8_t *out, size_t cap, size_t *len);
/* Decodes exactly count entries; returns count, or -1 when the body is not exactly that. */
int lr_mods_decode(const uint8_t *buf, size_t len, uint32_t count, lr_mod_print *out, int cap);

/* Guest: the campaign's mods (game), the host's prints and ours (mine[i] belongs to game->ugc[i]).
 * Returns 1 when every mod is found on both PCs with the same fingerprint; else 0 and a message for the
 * window that names what is missing or different. */
int lr_mods_match(const lr_game_info *game, const lr_mod_print *host, int host_count, const lr_mod_print *mine,
                  char *msg, size_t cap);

/* "Workshop 1154908982" or "mods\name", with the title when known: for logs and messages. */
void lr_mods_describe(const lr_mod_print *p, char *out, size_t cap);

#endif
