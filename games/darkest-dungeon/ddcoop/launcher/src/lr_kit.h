/* Signed kit updates between launchers (platform independent; launcher/tests/test_lr_kit.c, design in
 * launcher/UPDATE.md).
 *
 * Every test kit carries ddcoop-kit.sig: a small text manifest (version, serial, kit id and the size and
 * SHA-256 of DDCoop.exe, SDL2.dll and ddcoop-runtime.dll) and an ECDSA P-256 signature over it, made with
 * the owner's key (tools/sign_kit.ps1). A host whose installed files match its manifest offers them to a
 * friend whose launcher is older; the friend checks the signature with the public key built into its
 * launcher (LR_KIT_PUBKEY), that the serial is newer and that every file matches, and only then offers the
 * player to update. The signature check itself is a callback (Windows CNG in lr_kitsig_win.c).
 *
 *   DDCoop kit 1
 *   version=0.5.0-test77
 *   serial=77
 *   kit=719b6e74
 *   file=DDCoop.exe <size> <sha256>
 *   file=SDL2.dll <size> <sha256>
 *   file=ddcoop-runtime.dll <size> <sha256>
 *   signature=<r||s, 128 hex>
 *
 * The signed bytes are everything before "signature=". Lines end in LF; nothing else is accepted. */
#ifndef LR_KIT_H
#define LR_KIT_H

#include <stddef.h>
#include <stdint.h>

#define LR_KIT_FILES 3
#define LR_KIT_MAX_FILE (16u * 1024 * 1024)
#define LR_KIT_MANIFEST_MAX 1024
#define LR_KIT_VERSION_MAX 31

extern const char *const lr_kit_file_names[LR_KIT_FILES]; /* "DDCoop.exe", "SDL2.dll", "ddcoop-runtime.dll" */

typedef struct {
    char version[LR_KIT_VERSION_MAX + 1];
    uint32_t serial;  /* the test number: 0.5.0-test77 -> 77; only higher serials are offered as updates */
    uint32_t kit;     /* first 4 bytes of the runtime SHA-256, as in the launcher handshake */
    uint32_t size[LR_KIT_FILES];
    uint8_t sha256[LR_KIT_FILES][32];
    uint8_t signature[64];
    size_t signed_len; /* bytes of the manifest covered by the signature */
} lr_kit_manifest;

/* Signature check over msg[0..n): 1 = valid for the owner's key. */
typedef int (*lr_kit_verify_fn)(const uint8_t *msg, size_t n, const uint8_t signature[64]);

/* Parses a manifest exactly as described above. 1 = well formed (the signature is not checked here). */
int lr_kit_parse(const uint8_t *buf, size_t len, lr_kit_manifest *out);

/* Writes a manifest; with_signature = 0 writes only the signed part. Returns the length, 0 if it does not fit. */
size_t lr_kit_format(const lr_kit_manifest *m, int with_signature, char *out, size_t cap);

/* 0.5.0-test77 -> 77; anything that is not a "-test<number>" version (dev, duo builds) -> 0. */
uint32_t lr_kit_serial(const char *version);

/* Does this file's content match the manifest entry? */
int lr_kit_file_ok(const lr_kit_manifest *m, int index, const uint8_t *data, size_t len);

/* A manifest and its three files, in memory. */
typedef struct {
    uint8_t manifest[LR_KIT_MANIFEST_MAX];
    size_t manifest_len;
    lr_kit_manifest m;
    uint8_t *data[LR_KIT_FILES];
    uint32_t got[LR_KIT_FILES]; /* bytes received (guest) */
} lr_kit_bundle;

void lr_kit_bundle_free(lr_kit_bundle *b);

/* Checks a complete bundle: parsed manifest, valid signature, every file present with its hash.
 * want_kit != 0 also requires that kit id. Returns 1 when everything holds. */
int lr_kit_bundle_ok(const lr_kit_bundle *b, lr_kit_verify_fn verify, uint32_t want_kit);

#endif
