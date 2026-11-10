#ifndef LR_LOGS_H
#define LR_LOGS_H
#include <stddef.h>
#include <stdint.h>
/* Compact an exported copy in place. Never reads beyond len or grows it.
 * Source logs are not modified. Returned bytes keep original non-secret lines. */
size_t lr_logs_redact(uint8_t *data, size_t len, unsigned *removed);
#endif
