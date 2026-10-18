# Aion-Looper quality gate

The user requires one feature at a time: implement, compare, test, fix and retest;
only continue to another feature when the current feature is as good as or better
than the unchanged Ascii-Looper behavior. Passing state/UI tests does not prove
sound parity. Preserve original interface and functionality; new UI is limited
to needed native audio/input/MIDI configuration using the original style.

Current feature under repair: native capture/overdub finalization. Feature
additions are frozen while the live sound comparison fails. The new offline DSP
kernels are only a component of this work, not a completed app feature.

Before advancing:

1. Run unchanged-JS comparisons for state and DSP and the live core audio audit.
2. Integrate bounded ownership transfer/finalization without full-buffer work,
   allocations or locks on the audio callback. Preserve old PCM on failed edits.
3. Test empty/short/long clips, stereo and mono, offsets/wraps, feedback/replacement,
   cancellation/clear/stop/reset and memory exhaustion. Exercise 64/128 blocks.
4. Run meaningful Windows and Ubuntu regressions and native bridge checks when
   changed. Document precisely which behavior was compared and which was not.
5. Do not loosen tolerances, remove a failing case or change preserved JS to make
   a gate pass. When extending a fixture to include more of the reference pipeline,
   retain the old regression coverage at its appropriate kernel/component layer.
6. Do not mark the feature complete or start another feature while its acceptance
   fails. Keep working on that feature. Do not label a build production-ready on
   the strength of unit tests; driver/DAW/platform validation is separate work.

The existing audio diagnostic intentionally fails until the live native pipeline
matches its reference comparison. See docs/REFERENCE_AUDIT.md for current limits.
