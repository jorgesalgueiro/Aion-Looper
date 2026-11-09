# Running-Standalone CLI

This development CLI controls an already running, newly built Aion-Looper
Standalone. It shares the GUI's processor, audio/MIDI devices, parameter IDs,
loop commands and session. It does not launch a second audio engine or control
DAW plugin instances. Python 3.8+ is required; there are no third-party Python
dependencies. Current Windows development preview ZIPs include `tools/aion.py`;
a compiled CLI and release-ready packaging remain future work.

From the repository, start the development Standalone built as described in
BUILDING.md, then run:

```sh
python tools/aion.py instances
python tools/aion.py status
python tools/aion.py parameter masterGain 0.5
python tools/aion.py parameter syncBpm 120
python tools/aion.py loop record 1
python tools/aion.py loop finish 1
python tools/aion.py loop play 1
python tools/aion.py loop stopImmediate 1
python tools/aion.py rename 1 "My loop"
python tools/aion.py metronome
python tools/aion.py midi learn --data '{"target":0}'
python tools/aion.py midi clear --data '{"target":0}'
python tools/aion.py midi save
```

The JSON quoting above is valid in PowerShell and Bash. Named track arguments
are **1..20**. Loop actions and values use the native handler's conventions;
for example `loop volume 1 0.8`, `loop pan 1 3`, `loop undo 1`, `loop redo 1`,
`loop normalize 1`, `loop reverse 1 1`, and `loop stopAll`. MIDI target IDs are
the existing native IDs, not display track numbers. MIDI Learn still requires
an incoming routed MIDI message. Native input availability, queue bounds,
capture exclusivity and state guards remain enforced.

EQ slider targets **93..116** follow the selected EQ loop tab (one binding per
parameter, matching the original effect-controller workflow). Order: lcFreq,
lsFreq, lsGain; P1 through P6 frequency/gain/Q; hsFreq, hsGain, hcFreq.
Select the controller destination from the running app or CLI (this MIDI field
uses the native **0..19** track index):

```sh
python tools/aion.py midi eqTrack --data '{"track":19}'
python tools/aion.py midi learn --data '{"target":97}'
python tools/aion.py midi cancelLearn
python tools/aion.py midi clear --data '{"target":97}'
python tools/aion.py midi save
```

Target 97 is P1 gain. Send a CC to learn it; its first packet is consumed, then
later values apply continuously, including zero. Right-click the corresponding
EQ slider to learn, Shift+right-click to clear, Escape or ten seconds to cancel.
Mappings use original slider bounds, clear the preset name to Custom, and do
not enable bypassed EQ or trigger transport. The selected tab and mappings
persist in native state and keep working without an editor. Existing channel
filters/conflict rules apply; physical controller verification remains separate.
CC120/123 retain the existing native panic behavior and cannot be learned as EQ.

MERGE also uses the shared loop command:

```sh
python tools/aion.py loop merge 1 1
```

This merges source **Loop 2** into destination **Loop 1**: the destination track
argument is 1-based, while the generic action's numeric `value` retains the
native **0-based source ID**. A queued source is supported before and after graph
construction; its stored audio is copied/mixed without changing its playback grid.
An empty destination stays stopped; an existing destination gains one local/global
Undo entry. The original editor asks whether to clear the source after success.
Declining preserves it. Queued destinations also retain their original playback
grid and source snapshots during the mix. A source and/or destination already
waiting for scheduled STOP also keeps its existing stop deadline; finishing the
worker after that deadline never restarts playback. As with other generic
loop actions, native capture/worker/state guards apply; a rejected command is not
replayed. Raw `clearMergedSource` requires the current destination merge serial,
source identity and clear epoch; clients must not blindly clear a source.

If multiple Standalone instances are running, select explicitly:

```sh
python tools/aion.py --instance INSTANCE_ID status
```

Use the ID returned by `instances`. Ambiguous selection fails without submitting
a command. `instances` lists discovered endpoints, not a guarantee of liveness;
an endpoint left by an app crash may time out. Clean app shutdown removes its
endpoint. The client never kills processes or deletes another instance's files.
Run the client as the same user and on the same OS as the app; cross-OS or remote
mailbox access is not supported.

`request` exposes every existing native JSON operation without changing its
field conventions, including zero-based native track fields:

```sh
python tools/aion.py request '{"operation":"midiDevices"}'
python tools/aion.py request '{"operation":"midiConfig","action":"enable","enabled":false}'
```

`release-stutter`, `cancel-export`, and `export TRACK FILENAME.wav` expose the
corresponding existing operations. Export retains the native save picker and
current stored-clip identity validation; it is not unattended rendering. Raw loop
requests retain the native generation/expiry requirements. The named `loop`
command obtains a current generation, submits once and waits for that exact
native request outcome. It fails if the audio owner rejects it or a concurrent
controller advances past its outcome before it can be observed. `applied:true`
means the engine accepted the action, not that recording/finalization/export has
finished. Inspect `status` for those longer workflows.

Apply, Q, nudge, Speed, RTRG, NORM, MULT and local Undo/Redo convenience commands
wait for their target to leave `finalizing` **before submission**, using a three-second admission
deadline. Generation changes, track deletion or an expired admission deadline
return `applied:false` without submitting that action. Existing mailbox lock and
response timeouts can add elapsed time; a late snapshot cannot authorize an
expired gesture. An intervening controller can still change the state after the
snapshot: native state guards then reject it explicitly, with no automatic replay.
Raw `request` bypasses this client wait and keeps the native guards. Global Undo/Redo
wait for all finalizing tracks, then let the audio owner select the current global
history target. Clearing any track during that global wait cancels admission.
MULT retains its original local-only history behavior; this wait does not create
a global history entry.

```sh
python tools/aion.py loop applyStart 1
python tools/aion.py loop quantizeStart 1
python tools/aion.py loop nudgeStart 1 -1
python tools/aion.py loop speed 1 0.5
python tools/aion.py loop retrigger 1
python tools/aion.py loop normalize 1
```

Pan uses the original integer range **0..10**, with **5** at center; volume uses
**0..2**. Speed and other state-dependent controls keep their native restrictions.

RTRG uses the same action as the original button and `/` shortcut. A stopped
loop, an unborn queued PLAY, or a loop with STOP pending starts immediately,
retiring its old start/stop and temporary STUT/Start events. An already healthy
playing or constructed queued source keeps its 5ms replacement timing. Stored
audio and Undo/Redo history are preserved; capture/empty-state guards and the
finalization admission wait above still apply.

Undo/Redo restores stored audio while a constructed queued PLAY has STOP pending,
preserving that source's original start and stop. It also keeps the audio already
playing during a scheduled STOP. The next PLAY uses the restored buffer.

```sh
python tools/aion.py loop undo 1
python tools/aion.py loop redo 1
python tools/aion.py loop undoGlobal
python tools/aion.py loop redoGlobal
```

Unported drone/sampler/tracker/FX and remaining persistence/routing features are
not implemented by the CLI. Unknown operations are rejected. Newly ported
features must extend the shared native command layer and CLI coverage together.

## Results and transport

Successful command results are JSON on stdout. Exit 0 means native success;
exit 1 means a native rejection or unconfirmed loop outcome; exit 2 means CLI
usage, discovery or transport failure. Raw `request` returns native acknowledgments
unchanged, including `queued:true` where applicable. A queued acknowledgment alone
does not prove successful audio-side application. Shell argument errors use
normal argparse diagnostics on stderr.

The local protocol is version 1, with a private per-instance directory/token,
random request ID and bounded deadline. Requests are limited to 32 KiB, replies
to 2 MiB and JSON nesting to 32 levels. Clients serialize through a kernel-owned
file lock; process exit releases the lock. The app claims one request per 25ms
message tick, parses it and invokes the existing handler on the message thread.
No command file, JSON or CLI work runs on the audio callback. This polling
transport is for control, not sample-accurate MIDI or realtime automation.

The most recent dispatched native request's result is cached for an immediate
same-ID reply retry. The client does **not** automatically resubmit commands.
An unanswered command can already have executed: after timeout, inspect status
before retrying a mutating action. Timeout is not cancellation; a command already
accepted into the native queue may still execute when paused audio processing
resumes. Every request has an expiry checked before
invoking the handler. Reply-read sharing violations during Windows replacement
are retried within the original response deadline, without resubmission.

Set `AION_CLI_DIR` to the same **absolute directory** in both app and CLI processes
to override discovery, or use client `--root` to point to the app's directory.
Defaults are the user's application-data directory under `Aion-Looper/cli`.
Each app instance owns only its generated subdirectory and fixed mailbox files.
No network port is opened. Keep runtime files outside the reference/UI folders.

Windows/Linux native tests run external Python CLI processes against isolated
production processors with synthetic audio and the actual JUCE message loop.
They do not prove physical device behavior, OS audio-driver latency, DAW/plugin
control, Apple operation, or full migration parity. See VALIDATION.md for executed
results and remaining validation.

NORM accepts a queued loop with committed audio before or after its graph
callback. It retains the original play receipt and start grid; normalization does
not itself create a source. The existing asynchronous worker commits new PCM;
use `status` to distinguish acceptance, finalization and playback. Sources take
the PCM committed at construction; reversed snapshots remain independent. The
command controls the running session and does not create a second audio engine. A loop
awaiting scheduled STOP also accepts NORM. Its current stop/phase deadlines and
history remain intact; a worker result arriving after STOP never restarts it.
Forward sources observe normalized stored PCM; already constructed reversed
sources retain their own snapshot until source replacement.

## History during scheduled STOP

`loop undo TRACK`, `loop redo TRACK`, `loop undoGlobal` and `loop redoGlobal`
also accept loops awaiting scheduled STOP. Stored PCM/history change immediately;
the already sounding source (including its old length/direction) and original
stop deadline continue. A new play uses the restored buffer. The original local
and global buttons share this processor behavior; global buttons also reflect
already-supported queued history. Growth that cannot fit storage is rejected
without changing PCM, history or source ownership.

## Queued history

`loop undo TRACK` and `loop redo TRACK` now also work while PLAY is queued and
its independent source callback has not run. They restore stored audio and
history depth without changing the original start grid or callback deadline.
The callback uses the restored buffer, including its new length and current
direction. After source construction (and while playing), history schedules a
replacement source 5ms after audio-owner admission (not after typing the CLI
command; mailbox dispatch has its own cadence). Existing sources retain their buffers/lengths and
prior stops; the original grid still gates audible output. Global
`loop undoGlobal` / `loop redoGlobal` share the same implementation. Stopped
history does not start playback. Pending-stop queued history remains unavailable.
Storage/event/overlap-capacity rejection leaves PCM, history and sources unchanged.

## Multiply during scheduled STOP

`loop multiply TRACK` doubles stored PCM while the loop awaits scheduled STOP,
even with input available. It adds one local Undo entry; the sounding source,
its old length/rate and any earlier Start event keep their original STOP deadline.
The playhead uses the new stored duration, as in the original interface. The
MULT button and keyboard use the same processor command. Undo/Redo and NORM may
then edit storage without restarting that source. A delayed worker can commit
after STOP but cannot revive playback; allocation/capacity failure preserves
the previous PCM/history. The constructed queued-source case is described below.

`loop multiply TRACK` also accepts a constructed PLAY source with STOP pending
before its initial start. It retains both original start and STOP deadlines,
stays silent until the start, doubles stored PCM/local history and does not
capture input. The original MULT button uses the same command. If STOP resolves
before a delayed worker, its result can commit storage but cannot start playback.
Queued STOP history/RTRG retain their existing eligibility.

## Queued Multiply

`loop multiply TRACK` also doubles a queued loop before its source callback has
constructed playback. It adds one local history entry, retaining the original
PLAY receipt and grid; `loop undo TRACK` / `loop redo TRACK` use that same history.
The native worker builds PCM outside audio processing. A graph callback arriving
during doubling waits in one bounded audio-owner slot until completion, retaining
its original identity. STOP/STOP ALL, DEL, cleanup or prepare still cancel playback.
Allocation/capacity failure retains old PCM/history. After source construction,
input/no-input behavior is described below; an already pending STOP uses stored
doubling as described above.

## Constructed-source Multiply without input

When native input is unavailable, `loop multiply TRACK` uses instant buffer
doubling even after the queued source has been constructed. Existing sources
keep their own PCM/lengths/stops; the original grid gates their output. The worker
commits doubled storage/history only after reserving a replacement source for
5ms after audio-owner completion. Playing no-input doubling uses the same timing.
The GUI/keyboard and CLI share this implementation. OOM or event/overlap-capacity
failure preserves old storage/history; inspect `finalizationFailed` in snapshot.
An already pending STOP uses stored doubling as described above.

## Constructed-source Multiply with input

With native input available, `loop multiply TRACK` starts capture on an already
constructed queued graph, retaining its original audible grid. Recording starts
at the accepted command; crossing the grid starts playback while capture continues.
A second `loop multiply TRACK` or `loop finish TRACK` appends the conditioned take,
adds one local history entry and schedules the replacement source 5ms after worker
completion. Fewer than 128 captured samples leave the old PCM/history unchanged.
Undo/Redo restores the corresponding stored clip. The original MULT/main FINISH
buttons and CLI use this same processor path; no input flag is required from the
client. This is implemented in the native preview; see MIGRATION_CURRENT.md for
executed Windows/Linux coverage and the remaining integration observation.
