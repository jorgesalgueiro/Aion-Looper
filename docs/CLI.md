# Running-Standalone CLI

This development CLI controls an already running, newly built Aion-Looper
Standalone. It shares the GUI's processor, audio/MIDI devices, parameter IDs,
loop commands and session. It does not launch a second audio engine or control
DAW plugin instances. Python 3.8+ is required; there are no third-party Python
dependencies. A compiled/bundled CLI and release packaging remain future work.

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
for example `loop volume 1 0.8`, `loop pan 1 -3`, `loop undo 1`, `loop redo 1`,
`loop normalize 1`, `loop reverse 1 1`, and `loop stopAll`. MIDI target IDs are
the existing native IDs, not display track numbers. MIDI Learn still requires
an incoming routed MIDI message. Native input availability, queue bounds,
capture exclusivity and state guards remain enforced.

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
