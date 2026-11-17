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
deadline as hardware latency. Listening, shared GUI/CLI/state, MIDI and actual
chain/lifecycle integration remain separate gates; do not enable disconnected UI.

Design references: [Faust antialiased nonlinearities](https://faustlibraries.grame.fr/libs/aanl/)
(ADAA precision, half-sample delay, combining low-factor oversampling),
[ChowDSP ADAA experiments](https://github.com/jatinchowdhury18/ADAA)
(BSD-3-Clause), and pinned JUCE 9.0.4's `juce_FilterDesign.cpp`/
`juce_Oversampling.cpp` (AGPL-compatible project dependency). Coefficients use the
existing JUCE elliptic halfband design at widths .06/.12, stops -100/-90 dB.
No additional third-party model/library dependency or interpreter is required.
Already accepted Q/C/A processing and the retained legacy T comparisons remain.
