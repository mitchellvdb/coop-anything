#ifndef LR_LOGPACK_H
#define LR_LOGPACK_H
#include <stddef.h>
#include <stdint.h>
#define LR_LOG_FILES 13
#define LR_LOG_OTHER_MAX (512u * 1024)
#define LR_LOG_RUNTIME_MAX (2u * 1024 * 1024)
#define LR_LOG_PACKET_MAX (16u * 1024 * 1024)
enum { LR_LOG_TRUNCATED = 1, LR_LOG_MISSING = 2 };
extern const char *const lr_log_names[LR_LOG_FILES];
typedef struct {
    uint8_t *data;
    size_t len;
    unsigned count, seen;
} lr_logpack;
void lr_logpack_free(lr_logpack *);
int lr_logpack_add(lr_logpack *, unsigned file, unsigned flags, uint64_t original, const uint8_t *, size_t);
typedef int (*lr_log_visit)(void *, unsigned file, unsigned flags, uint64_t original, const uint8_t *, size_t);
/* Validate the entire fixed-name packet before invoking any callback. */
int lr_logpack_visit(const uint8_t *, size_t, lr_log_visit, void *);
/* Bounded, ordinary ZIP (stored entries, no external compressor). The caller
 * supplies only locally generated allowlisted paths, never guest paths. */
typedef struct {
    char name[80];
    uint32_t offset, size, crc;
} lr_zip_entry;
typedef struct {
    uint8_t *data;
    size_t len, capacity;
    unsigned count;
    int finished;
    lr_zip_entry entries[64];
} lr_logzip;
void lr_logzip_free(lr_logzip *);
int lr_logzip_add(lr_logzip *, const char *name, const uint8_t *, size_t);
int lr_logzip_finish(lr_logzip *);
#endif
