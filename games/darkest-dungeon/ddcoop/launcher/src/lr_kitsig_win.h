/* Windows: ECDSA P-256 signature check for kit manifests (lr_kit.h). */
#ifndef LR_KITSIG_WIN_H
#define LR_KITSIG_WIN_H

#include <stddef.h>
#include <stdint.h>

/* public_hex: the 64-byte public point X||Y as 128 lowercase hex. 1 = valid signature (r||s) over SHA-256(msg). */
int lr_kit_verify_p256(const char *public_hex, const uint8_t *msg, size_t n, const uint8_t signature[64]);

/* The same with the owner's public key built into this launcher (LR_KIT_PUBKEY in lr_hashes.h). */
int lr_kit_verify_owner(const uint8_t *msg, size_t n, const uint8_t signature[64]);

#endif
