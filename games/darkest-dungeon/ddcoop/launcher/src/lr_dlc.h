/* Small, read-only campaign-DLC data preflight. Presence is not ownership or
 * proof of live co-op support. Unknown features are refused deliberately. */
#ifndef LR_DLC_H
#define LR_DLC_H
int lr_dlc_known(const char *feature);
int lr_dlc_present(const char *game_root, const char *feature);
#endif
