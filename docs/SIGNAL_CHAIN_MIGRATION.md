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

Pending owner decision: candidate chain edits apply live during OD/SUB/MULT and
scheduled STOP, consistent with approved live FX controls. Original chain edits
store then defer rebuild in those states. This extension has been asked about;
do not call the slice complete before the answer and required validation.

Executed Windows evidence:

- Unchanged setter 311 cases; original enabled-order/omission builder 24 cases.
- Connected 32/48/96 kHz at 64/128, reorder at sample 17, omission/reinsertion,
  source/history preservation, overflow/recovery, panic/DEL/arm cancel/prepare,
  invalid epochs/generations, sixty-command bank overflow and clear-all pass.
  Zero watched callback new/delete. Focused final selection: 2/2, 1.07s.
- Production chain/EQ/compressor processor plus external CLI: 6/6, 45.91s at
  64/128; queued-save, malformed/legacy restore, rate changes and DEL covered.
- Actual WebView2 64/128 fields, toggle order/omission/reinsertion, typing,
  send failure/recovery, reload/DEL and nonzero synthetic callback PCM pass.
- Existing EQ/compressor connected, Arp preparation and original asset checks
  pass. Retained core/frontend and Linux/sanitizer checks remain pending.

Fixture failures were fixed without changing DSP/tolerances: the arm test exceeded
its 32768-frame pool; restore rendered before message-side admission; UI queried
before bootstrap. Review found missing visible toggle order and added a regression
before fixing that presentation defect.

Exact commands (use configured CMake/CTest executables on Windows):

```sh
cmake --build build/windows-core --config Release --parallel 2 --target aion_signal_chain_tests aion_core_tests aion_eq_connected_tests aion_compressor_connected_tests aion_arp_preparation_tests
ctest --test-dir build/windows-core -C Release --no-tests=error --output-on-failure -R '^(signal_chain_connected_contract|original_signal_chain_contract|core_realtime_contract|eq_connected_contract|compressor_connected_contract|arp_preparation_contract|native_frontend_commands|original_ui_bytes|cli_finalization_admission)$'
cmake --build build/windows --config Release --parallel 2 --target AionLooper_Standalone aion_mouse_host aion_signal_chain_processor_tests aion_eq_processor_tests aion_compressor_processor_tests
ctest --test-dir build/windows -C Release --no-tests=error --output-on-failure -R '^native_(signal_chain|eq|compressor)_processor_(64|128)$'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/TestSignalChainUi.ps1
```

Logs: ignored `build/chain-*`. Initial benchmark concurrent with other validation
observed one 2819.8us wall callback at 128 (2666.67us deadline). Keep that loaded
observation; it proves neither isolated performance nor physical driver latency.
Linux, sanitizers, isolated timing and routing policy remain pending. No package/push.
