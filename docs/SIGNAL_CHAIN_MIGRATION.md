# Editable loop effect chain

Work in progress, 10 November 2026. This is an Arp integration prerequisite:
`QCATFODBVKZG` is a default, never a fixed path. Product A stays disabled.

Reference: unchanged `Loop.setSignalChain`, `AudioGraph.buildEffectsChain` and
`UIManager.generateEffectsControlsHTML` in `.reference/Ascii-Looper/Modules/loopTracks.js`.
The requested sibling reference is absent. No reference or preserved UI edits.

Acceptance defined before implementation:

- Retain built-in codes; uppercase/filter/deduplicate preserving first occurrence;
  default on empty valid input. Omitted effects bypass even when enabled.
- One GUI/CLI/state model for twenty runtime tracks, legacy defaults and atomic
  malformed-state rejection. Preserve old action/parameter/MIDI IDs.
- Connected nonzero QC/CQ PCM against independent persistent stage owners at
  <=1e-6, audible order difference, 64/128 and callback allocation/free watches.
- Included effects retain state on reorder. Reinserting an omitted effect starts
  a fresh instance at current controls, matching original node removal.
- Original field, toggle order/omission, text shortcut isolation, editor reopen,
  send-failure recovery and DEL checked with actual Windows pointers/keys.

`SignalChain` stores thirteen built-in codes in fixed storage. The appended
`LoopAction::signalChain` uses the existing bounded FIFO, generation and clear
epoch. Message operation `signalChain` supports `get`, `set` (`order`) and
`reset`, with expiration and strict payload bounds. Accepted intent can be saved
before audio resumes. Version-1 `LoopSignalChains` stores twenty canonical entries;
missing legacy data defaults; malformed entries reject before APVTS mutation.
Restore publishes all sixty EQ/compressor/chain commands or retries without
publishing a prefix. This is atomic admission, not a new bank transaction system.

Native rendering follows user order for Q/eQ and C/compressor. Other codes retain
positions only; their controls stay disabled. Original toggle nodes/listeners
survive reorder/omission/reinsertion; Reverse stays outside the chain. No custom
effect, chain/full preset, wet export or A enablement is claimed. The original
chain field has no new invented MIDI target; existing Q/C MIDI controls use the
same active routing. PLAY/RTRG/history keep the approved persistent FX behavior.

Owner approved live chain reordering during OD/SUB/MULT and scheduled STOP on
10 November 2026, consistent with the approved live FX controls. Original chain
edits store then defer rebuild in those states. This is an intentional native
extension, separately tested rather than claimed as unchanged-JS behavior.
The existing runtime already applies that policy; this follow-up adds focused
live-state audio/interaction acceptance without changing product DSP or IDs.

Executed software evidence:

- Unchanged setter 310 built-in ASCII cases; enabled-order/omission builder 24 cases.
- Connected 32/48/96 kHz at 64/128, reorder at sample 17, omission/reinsertion,
  source/history preservation, overflow/recovery, panic/DEL/arm cancel/prepare,
  invalid epochs/generations, sixty-command bank overflow and clear-all pass.
  Zero watched callback new/delete. Focused final selection: 2/2, 1.07s.
- Production chain/EQ/compressor processor plus external CLI: 6/6, 45.91s at
  64/128; queued-save, malformed/legacy restore, rate changes and DEL covered.
- Actual WebView2 64/128 fields, toggle order/omission/reinsertion, typing,
  send failure/recovery, reload/DEL and nonzero synthetic callback PCM pass.
- Existing EQ/compressor connected, Arp preparation and original asset checks
  pass. Windows retained core passes (804.84s); frontend passes after the LF
  adapter repair (5.33s), with CLI finalization admission also passing.
- Linux Standalone builds; all six chain/EQ/compressor processor plus external
  CLI cases pass at 64/128 using WSL display `:0`, without opening an editor.
  ASan/UBSan/leaks pass 1/1 (5.42s); TSAN passes 1/1 (8.47s) with process-only
  `setarch x86_64 -R`; no diagnostics. Linux core passes (268.18s), frontend
  passes (9.45s). Its nine-case selection initially passed 8/9; the new reference
  script required Node 18-compatible copy-and-reverse instead of `toReversed`.
  The affected reference rerun passes 1/1 (0.55s), with identical assertions.
  All cases pass across these runs; the whole suite was not repeated after a
  test-only compatibility fix.

Fixture failures were fixed without changing DSP/tolerances: the arm test exceeded
its 32768-frame pool; restore rendered before message-side admission; UI queried
before bootstrap. Review found missing visible toggle order and added a regression
before fixing that presentation defect. The unchanged frontend source comparison
then caught CRLF endings introduced by the edit script in the native adapter.
Restored LF, keeping the original comparison and preserved assets unchanged.
Expanded actual UI checks additionally pass LIVE PERFORMANCE order/omission and
the listener on a reinserted compressor toggle at both block sizes.
Runtime source is `6d731ac`; Node 18/LIVE regression extension is `963722b`.
Later changes record evidence only. No original-JS audio kernel or tolerance changed.

Exact commands (use configured CMake/CTest executables on Windows):

```sh
cmake --build build/windows-core --config Release --parallel 2 --target aion_signal_chain_tests aion_core_tests aion_eq_connected_tests aion_compressor_connected_tests aion_arp_preparation_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(signal_chain_connected_contract|original_signal_chain_contract|core_realtime_contract|eq_connected_contract|compressor_connected_contract|arp_preparation_contract|native_frontend_commands|original_ui_bytes|cli_finalization_admission)$'
cmake --build build/windows --config Release --parallel 2 --target AionLooper_Standalone aion_mouse_host aion_signal_chain_processor_tests aion_eq_processor_tests aion_compressor_processor_tests
ctest --test-dir build/windows -C Release --no-tests=error --output-on-failure -R '^native_(signal_chain|eq|compressor)_processor_(64|128)$'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/TestSignalChainUi.ps1
```

Linux, after committing in Windows and using `tools/SetupWslWorkspace.sh`:

```sh
cd /home/jorge/projects/Aion-Looper
cmake --build build/linux --parallel 2 --target aion_core
cmake --build build/linux --parallel 2 --target AionLooper_Standalone aion_signal_chain_tests aion_signal_chain_processor_tests aion_core_tests aion_eq_connected_tests aion_compressor_connected_tests aion_eq_processor_tests aion_compressor_processor_tests aion_arp_preparation_tests aion_effects_engine_benchmark
ctest --test-dir build/linux --no-tests=error --output-on-failure -R '^(signal_chain_connected_contract|original_signal_chain_contract|core_realtime_contract|eq_connected_contract|compressor_connected_contract|arp_preparation_contract|native_frontend_commands|original_ui_bytes|cli_finalization_admission)$'
for program in aion_signal_chain_processor_tests aion_compressor_processor_tests aion_eq_processor_tests; do for block in 64 128; do build/linux/$program $block || exit; done; done
cmake --build build/overlap-asan --parallel 2 --target aion_core
cmake --build build/overlap-asan --parallel 2 --target aion_signal_chain_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/overlap-asan --no-tests=error --output-on-failure -R '^signal_chain_connected_contract$'
cmake --build build/arp-storage-tsan --parallel 2 --target aion_core
cmake --build build/arp-storage-tsan --parallel 2 --target aion_signal_chain_tests
TSAN_OPTIONS=halt_on_error=1 setarch x86_64 -R ctest --test-dir build/arp-storage-tsan --no-tests=error --output-on-failure -R '^signal_chain_connected_contract$'
```

The existing Linux cache leaves optional processor CTest registration off; its
six native cases were invoked directly. This is not an empty-selection pass.
Linux WebKit interaction, physical drivers/controllers/listening and original
browser rebuild transition envelopes remain unvalidated for this feature.

Timing observations use `aion_effects_engine_benchmark`: 48 kHz, twenty playing
stereo loops, Q/C and bounded telemetry, 512 warm callbacks then 2048 measured
per configuration, fixed and automated controls. They measure wall callback time,
not thread CPU or device round-trip latency. With builds/tests finished:

| OS | Block | Mean range (us) | Largest callback (us) | Deadline (us) | Observed overruns |
| --- | --- | --- | --- | --- | --- |
| Windows | 64 | 414.949–432.525 | 604.1 | 1333.33 | 0 |
| Windows | 128 | 836.186–854.070 | 1302.1 | 2666.67 | 0 |
| Linux/WSL | 64 | 389.513–403.685 | 820.987 | 1333.33 | 0 |
| Linux/WSL | 128 | 718.146–819.314 | 1560.06 | 2666.67 | 0 |

Initial Windows benchmark concurrent with validation observed one 2819.8us wall
callback at 128; its log is retained. Neither the later observations nor any buffer
size proves physical latency, sustained driver performance or full FX headroom.
No speedup against a comparable old binary is claimed.

Logs: ignored `build/chain-*` in each OS build workspace. Windows core details
are in `chain-regression-details-windows.log`; repaired frontend result is
`chain-frontend-ctest-windows.log`. Linux selection and fixed reference results
are `chain-ctest-linux.log` and `chain-reference-fixed-linux.log`; sanitizer and
processor logs use their explicit names. UI including LIVE is
`chain-ui-live-windows.log`. Both initial loaded and later isolated benchmark
logs remain. Routing policy alone is pending before slice completion. No package/push.
