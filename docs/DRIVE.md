# Native distortion, overdrive and fuzz

The owner requested all three effects and authorized improved organic sound over
JS audio identity for remaining effects. Preserve T/O/F controls, preset names
and values. The initial native models are independent mathematical saturators:
asymmetric algebraic saturation (T), cubic soft clipping (O), and bias-controlled
hard clipping (F). Preset names are compatibility labels, not circuit-emulation
claims. These models are not presented as universally "best" or audition-approved.

All use double-precision first-order antiderivative antialiasing, fixed-storage
4x polyphase oversampling, a 15 Hz DC blocker, stable two-pole tone filtering and
10 ms parameter/bypass smoothing. Tone is clamped below Nyquist without changing
stored settings. Dry passes through matching oversampling and first-order ADAA
averaging to avoid phase cancellation in parallel mix. This is a minimum-phase
path with frequency-dependent group delay, not a fixed zero-latency claim. Mix=0
while enabled therefore follows that matched dry path; disabled bypass is direct.

Acceptance before product integration: unchanged original setting/preset values;
independent numerical-integration agreement within 3e-6 on the declared matrix;
more than 10 dB normalized alias-energy reduction versus naive base-rate shaping
at coherent 802.7/4107.4/10998 Hz, 48 kHz; finite bounded output across all supported
control extremes; bit-identical 64/128/irregular processing; stereo/in-place/reset,
rapid controls and zero callback new/delete. Measure CPU without treating a buffer
deadline as hardware latency. Shared GUI/CLI/state, MIDI and connected
chain/lifecycle tests are separate from kernel acceptance; listening remains
pending and is not established by these numerical tests.

The shared native owner now exposes all three original colored control panels,
original loop/live-card toggles, selected-loop CC Learn, presets and persistence.
`LoopDrives` version 1 stores 20 track banks with three effects each. Missing
legacy banks restore disabled original defaults; malformed banks reject the
entire state before APVTS replacement. MIDI version 7 appends fourteen targets
133..146 without shifting earlier IDs. Partial GUI edits merge only their owned
fields on audio, preserving newer MIDI values on other controls. Explicit DEL
resets all three; ordinary PLAY/RTRG/history retain processor memory. Omission
from the chain bypasses it; reinsertion resets that stage like existing Q/C.

The Python client controls the running Standalone (track numbers are 1..20):

```text
python tools/aion.py distortion 1 preset --preset tube-warmth
python tools/aion.py distortion 1 on
python tools/aion.py overdrive 1 preset --preset "American Sparkle"
python tools/aion.py overdrive 1 on
python tools/aion.py fuzz 1 preset --preset "Big Muff"
python tools/aion.py fuzz 1 on
python tools/aion.py fuzz 1 set --values '{"gain":35,"bias":0.1,"mix":0.7}'
python tools/aion.py chain 1 set FOT
```

Each effect also accepts `get`, `off`, `toggle` and `reset`. A reset keeps enable
state and restores untouched defaults. Right-click a knob for CC Learn;
Shift-right-click removes its binding. Binding follows the selected loop tab.
Device/hardware, listening and full-workspace performance acceptance remain
separate; synthetic messages do not establish a physical-pedal test.

One local Windows Release measurement (1,000 stereo blocks, 64 samples, 48 kHz)
reported mean / observed maximum kernel times of T 23.52 / 56.80 us,
O 22.53 / 39.00 us and F 22.09 / 57.10 us (`build/drive-kernel-performance.log`).
These measure one isolated effect, not a loaded workspace, worst-case execution
guarantees or hardware latency. The declared spectral matrix exceeds its 10 dB
alias-reduction gate; full-path improvements range from 36.53 to 101.60 dB in
that fixture, not an assurance for every input or setting.

The subsequent stereo optimization preserves the accepted PCM while reducing
loaded-loop CPU time. See [measured performance and capacity limits](DRIVE_PERFORMANCE.md)
for Windows/Linux before/after data, exact verification and reproduction commands.

Design references: [Faust antialiased nonlinearities](https://faustlibraries.grame.fr/libs/aanl/)
(ADAA precision, half-sample delay, combining low-factor oversampling),
[ChowDSP ADAA experiments](https://github.com/jatinchowdhury18/ADAA)
(BSD-3-Clause), and pinned JUCE 9.0.4's `juce_FilterDesign.cpp`/
`juce_Oversampling.cpp` (AGPL-compatible project dependency). Coefficients use the
existing JUCE elliptic halfband design at widths .06/.12, stops -100/-90 dB.
No additional third-party model/library dependency or interpreter is required.
Already accepted Q/C/A processing and the retained legacy T comparisons remain.
