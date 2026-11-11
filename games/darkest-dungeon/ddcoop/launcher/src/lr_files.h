/* Small file helpers with UTF-8 paths (wide-character APIs on Windows, so user folders with accents work). */
#ifndef LR_FILES_H
#define LR_FILES_H

#include <stddef.h>
#include <stdint.h>

#define LR_PATH_MAX 1024

int lr_dir_exists(const char *path);
int lr_file_exists(const char *path);
int lr_make_dirs(const char *path); /* like mkdir -p; 1 if it exists afterwards */

/* Calls fn for each regular file directly in dir (no subfolders). Returns the number of files, -1 if the
 * folder cannot be read. fn returns 0 to stop. */
int lr_list_files(const char *dir, int (*fn)(void *ctx, const char *name), void *ctx);

/* Calls fn for each file and folder directly in dir ("." and ".." left out; is_dir = 1 for a folder).
 * Symbolic links/junctions (including the root), nonregular entries and any incomplete enumeration
 * are refused, never omitted or followed. Returns the number of entries, -1 on refusal/read failure.
 * fn returns 0 to stop intentionally. */
int lr_list_dir(const char *dir, int (*fn)(void *ctx, const char *name, int is_dir), void *ctx);

/* Reads a file of any size in pieces and hands each piece to fn (fn returns 0 to stop). Returns 1 when
 * the whole file was read; *size is the number of bytes read. */
int lr_read_stream(const char *path, int (*fn)(void *ctx, const uint8_t *p, size_t n), void *ctx, uint64_t *size);

/* Reads a whole file (max bytes); *out is malloc'd. Returns 1 on success. */
int lr_read_file(const char *path, size_t max, uint8_t **out, size_t *len);
/* Writes path.ddcoop-tmp then renames it over path. */
int lr_write_file(const char *path, const uint8_t *data, size_t len);
int lr_sync_file(const char *path); /* flush a completed recovery file before the original may be replaced */
int lr_delete_file(const char *path);
int lr_copy_dir_files(const char *from, const char *to); /* regular files only; 1 on success */

void lr_join(char *out, size_t cap, const char *a, const char *b); /* a + separator + b */

#endif
