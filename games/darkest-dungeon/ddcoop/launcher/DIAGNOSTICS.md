# Automatic diagnostics to the host (test82)

Every participating launcher may request a report (Copy logs, a new native
Sync stop marker, resync or session failure). The host assigns one report id,
freezes its current player slots, captures its own logs and asks the connected
guests for theirs over the existing reliable Steam control channel. Concurrent
requests coalesce. No outside service, account, HTTP upload or public publication.

Since test83 an ordinary end does not start a report: closing the game no longer
does, and a "Sync stop" right after a "... disconnected" line (another player left
or closed the game) is not a desync. A mismatch (reason 2/3), an order this copy
could not apply ("needs a resync") or another stop still does (lr_diag_stop_marker).

Only a fixed log allowlist is read; no campaign saves, ddcoop.ini, Steam tickets
or signing keys. Known authentication/password lines are removed before transfer
and again on the host. This is not full anonymization of names or paths: reports
remain private on the host. Each runtime log is bounded to its latest2MiB, other
files to512KiB, with leading partial lines discarded and truncation recorded.
Missing files are explicit. Guests cannot choose file paths. Packet size<=16MiB,
CRC and exact offsets/order checked; paced to one16KB chunk/100ms per guest; unknown/stale senders and ids rejected.

Host output: a new Desktop/DDCoop-report-<time>-<id>/ folder, host/ and player-2/
through player-4/ subfolders, plus REPORT.txt with every participant's status.
An ordinary ZIP of that same structure is written atomically after all replies
or a bounded timeout. A missing/disconnected player is marked missing, never
silently treated as complete. Keep launchers open until the report finishes;
a fully lost connection cannot forward logs, so local collection remains.

Wire3 adds diagnostics types26..31. They are used only inside the matching kit
session. Runtime exact label82 changes the kit id, so older update-capable
launchers receive the signed newer kit before they enter such a session. No
runtime gameplay behavior change is intended. Reports never change game state,
backups or recovery and require no gameplay approval or forced restart.
After gameplay stops, only diagnostic transport is retained for at most60s;
normal collection while games stay open has a120s deadline. A short final grace
lets receipt ACKs flush before leaving. New sessions can finalize older reports
as partial without waiting. Previously connected/selected but missing player
slots remain explicit. Solo keeps its local runtime logs in the host folder.

The transport/state machine and log packet/ZIP adapters are portable and tested
with four launchers, backpressure, corruption, duplicates, disconnect, stale/
foreign messages, size/time limits and file allowlisting. Actual live transfer
still requires the owner's next test. Native effects remain modeled elsewhere.
Written by Codex; launcher changes isolated per AGENTS section3.

Receipt ACKs are idempotent and retained for the last12 player/report receipts,
so a late END from a finished report cannot wedge the next one. A regression
that blocks all initial receipt ACKs, starts the next report and then releases
backpressure failed before this guard, then passed with every sender following.
Negative receipts stop failed uploads without touching gameplay. Windows adapter
fixtures use only workspace files and fake membership/transport, with independent
Python ZIP verification. Live Steam delivery remains a next owner test.

## Test86 local event and guest report location

A locally detected native synchronization failure also posts local overlay
command6 with the current process ID to its attached launcher. The launcher
accepts its tracked game only, once per game, and requests the existing report
before the next one-second log scan or a blocking connection-error notice.
Ordinary connection loss and a generic stop received from another copy do not
emit this event. The bounded new-log detector remains a fallback. No new Steam
message, external upload or gameplay mutation is introduced.

The host still writes one grouped report/ZIP to Desktop. A guest's local fallback
uses %LOCALAPPDATA%\DDCoop\Reports, never the guest Desktop. Known-folder failure
refuses that local output rather than falling back to Desktop. This does not
remove existing reports or delete anyone's files. Missing disconnected launchers
remain explicit: closing them before collection prevents remote retrieval.
Windows adapter fixtures cover host/guest folder selection, current/foreign PID,
once-only event requests, normal-end behavior and the existing report transport.
Written by Codex, 12 November 2026.
