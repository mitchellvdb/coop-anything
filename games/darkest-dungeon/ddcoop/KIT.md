# Test69 / duo52 — review release

See [the review report](runtime/REVIEW-2026-11-10.md) for findings and open work.
Before-fix regressions reproduce false ACK/combat RNG continuation and the native
hook boundary error. Final native/fuzz/FIFO, MSVC ASan, isolated OpenGL overlay,
both full launcher/fake-Steam suites and four-core restart/epoch tests pass.
Nine read-only native proofs include all 81 hook targets and 121 named signature
pins / 117 distinct engine entrypoints. Static analysis of 13 major units has
no new analyzer diagnostics; existing C4702 compiler warning is documented.
Synthetic log-redaction, English, hidden native layout, embedded GIF and read-only
DLC readiness tests pass. Native effects are modeled, not real-client execution.

RELEASES.json records exact normal/solo runtime, launcher and kit SHA256 hashes.
ZIP payload/pins/labels, 247 development source files, normal/duo source parity
and patches versus test68/Claude64 reproduce every byte. Duo differs only in
runtime pin and test instructions; its runtime uses DD_DUO. All peers must run69.

No live game/installer/owner save, real Steam storage, multi-PC restart,
Linux/MinGW or optional Capstone model check was run. Direct loot-to-hero equip
and earlier provisions C0000005 remain unresolved; the fixed tutorial hook is
not proven to explain that earlier crash. Full combat-state digest, live DLC
and unsupported inventory drag/sort operations remain follow-up work.
Resync resumes native host checkpoint and can lose unsaved progress; solo .bat
guests restart manually. Copy logs removes known auth-secret lines only; original
logs unchanged, names/paths are not automatically anonymized. Source only.
