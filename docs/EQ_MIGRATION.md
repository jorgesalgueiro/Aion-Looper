# Original Q/eQ migration

Active slice: ten-band per-loop eQ, before the other unported FX/SAVE controls.
The filter kernel is a prerequisite, not acceptance of the complete GUI/CLI
effect. Keep the product checkbox disabled until integration passes.

## Observable acceptance, defined before integration

- Run the unchanged `EQProcessor` and `SVF` from the preserved assets. Compare
  finite stereo output at **1e-6 absolute PCM**: defaults, every band, all ten
  original EQ presets, gain/Q/frequency limits, impulses, tones/noise, silence
  tails, absent channels, rapid updates, bypass entry/re-entry and reset.
- Preserve Float32 AudioParams, double filter state/intermediate arithmetic,
  the original `<0.01 dB` bypass boundary and `1e-9` state flushing. Default EQ
  includes low/high cuts; it is not a unity bypass.
- Preserve 50 ms `smoothSetParam` automation sampled once per original 128-frame
  worklet quantum, independent of host block size. Validate actual browser
  rendering in addition to deterministic worklet execution before integration.
- Prove finite output and recovery from invalid settings/input, atomic rejection
  and zero kernel allocation/free, with 64/128 and irregular blocks on both OSes.
- Then integrate before loop volume/pan with bounded shared GUI/CLI commands,
  original controls/curve/presets, lifecycle/reset and compatible versioned state.
  Verify source ownership, record/OD/SUB capture exclusion, rapid bypass/fades,
  editor reload, multiple instances, queue rejection and real UI at 64/128.
  Passing the isolated filter does not establish these connected behaviors.

## Approved low-rate safety repair

Unchanged default worklet, 0.1 impulse followed by silence, stereo, 128-frame
quanta: first non-finite Float32 output at frame **224 at 22050 Hz**, **167 at
32000 Hz**. The default 20 kHz high-cut exceeds Nyquist and yields a negative
SVF integrator coefficient. At 8/16 kHz the same invalid setting can alias into
apparently finite but musically incorrect response. 44.1/48/96 kHz were finite
in this reproduction. This is reference behavior, not native acceptance.

The owner approved limiting effective frequencies below Nyquist while retaining
stored values on 9 November 2026. Native coefficient calculation uses
`min(requested, sampleRate * 0.5 * (1 - 1e-6))`, then retains the original positive
`g <= 1000` limit. The 44.1 kHz and higher descriptor range is unchanged. The
unchanged-worklet comparisons receive this approved effective frequency as input;
the raw failing cases remain separate regressions, not relabeled parity.

## Owner-approved graphic editing addition

The owner also requested study of open-source plugins/JSFX and direct graph
manipulation like MEqualizer: draggable points and adjustable horizontal ranges.
This is approved EQ interaction scope, not a request for new filter types,
dynamic EQ, linear-phase latency, oversampling or analog coloration.

| Studied primary source | Applicable approach | Decision |
| --- | --- | --- |
| [ZL Equalizer source](https://github.com/ZL-Audio/ZLEqualizer/blob/main/source/gui/dragger/dragger_component.cpp), [panel](https://github.com/ZL-Audio/ZLEqualizer/blob/main/source/panel/curve_panel/response_panel/dragger_panel/dragger_panel.cpp), [manual](https://zl-audio.github.io/plugins/zlequalizer/manual/) | Log-frequency mapping, drag start/end, fine/axis-constrained movement, explicit parameter gestures, sample-rate-aware limits | Use these interaction principles in the existing ASCII WebView; no JUCE GUI transplant or code copied. Source license inspected: AGPLv3; logos excluded by upstream |
| [chokehold EQ 560 JSFX](https://github.com/chkhld/jsfx/blob/main/Equalizer/eq_560.jsfx), [license](https://github.com/chkhld/jsfx/blob/main/LICENSE) | Ten-band SVF, fixed-storage state; also has gain-dependent Q, oversampling and coloration | Retain our original SVF/Q/sound instead of copying those musical changes. Source inspected, MIT license checked; no code/assets copied |
| [Andrew Simper's SVF analysis](https://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf) | Bounded state-space realization and bilinear frequency transform underlying the original worklet | Keep original recurrence/coefficients; derive response directly from that recurrence |
| [MEqualizer features](https://www.meldaproduction.com/MEqualizer/features?sort=lo) | Direct frequency/Q editing in a curve display | Interaction reference only; no proprietary code/assets used |

Planned controls: keep ten original bands and numeric sliders; log-frequency
point drag edits frequency/gain, selected peak-band horizontal edge handles edit
Q (0.1–10). Cuts have frequency-only points; shelves retain original fixed Q.
Show individual/combined response and effective low-rate limits. Native
`eqPeakRange/eqPeakQ` use bilinear-warped half-gain edges, not a misleading
constant-Hz width. `Equalizer::magnitude` evaluates its actual state-space
transfer function; tests compare it with rendered Float32 steady-tone DFT.
The graph, native processor integration and GUI/CLI commands remain pending;
no disconnected new product controls are exposed.

## Isolated kernel checkpoint

Initial Windows sound baseline: **480** unchanged-worklet cases, **2,968,872 stereo
frames**, **972 nonzero tail segments**, maximum **4.99755863e-8** under **1e-6**.
Includes all ten presets, every active band/gain/Q boundary, mono/absent input,
silence tails, bypass re-entry/reset and 64/128/37 processing subdivisions.
Analytical automation: **15 cases**, **184,335 stereo frames** (rerun after the
browser correction below); it is separate from actual browser evidence.

Actual WebView2: **48 comparisons**, **270,336 mono/stereo frames**, maximum
**8.57054142e-8**, **1e-6** peak / **1e-5** relative-RMS limits. Defaults/Radio/
Master Air at 8/22.05/32/44.1/48/96 kHz, and live original `smoothSetParam`
retargeting at 44.1/48/96 kHz, mono/stereo, native 64/128. This is an isolated
worklet sound oracle, not product routing or physical driver acceptance.

The actual browser initially failed smoothing at **5.62220756e-5** peak. The
original handler's `AudioParam.value` anchors from the **previous rendered
sample**, not the next sample at `currentTime`. Native retargeting and the
analytical model were corrected; the original method/worklet, PCM limits and
failing retarget sequence remain intact. Example 44.1 kHz: target +12 dB at
frame 256, retarget at 1024 anchors **3.5255067348480225**, matching age 767,
not age 768. Same-sample repeated edits retain their previous anchor.

Product CSP correctly blocked the initial worklet module load. A debugger-only
CSP bypass did not unblock the worklet either. The sound harness now serves
exactly one test fixture document via CDP Fetch in its owned WebView2 instance.
No product CSP/source changes, original bootstrap, network module fetching or
product AudioContext. The helper owns/guards port 9236 and closes its own app.

Curve acceptance: **48 frequency points** from actual Float32 steady-tone output
at 8/22.05/48/96 kHz, flat/all-active settings, maximum amplitude-response error
**2.01394e-9** under **1e-6** on Windows. **60 bandwidth ranges / 120 inverse
edge checks** cover Q 0.1/0.707/1/10, 20/1000/22000 Hz and five sample rates,
including the approved effective-frequency limit. Filter construction, validation,
configuration, reset, smoothing, rendering and curve/handle math show zero
`new/delete` in the scoped allocation guard. This is not a measured production
callback or a full-bank performance claim.

One newly introduced curve test was corrected rather than changing sound:
an ideal state-space transfer function was incorrectly asserted equal to a
unit-impulse spectrum at **1e-6**. Original tiny-state flushing makes the system
slightly nonlinear: 8 kHz, 65536-sample impulse DFT, bin 131 differed by
**1.13081e-6**. No PCM or curve tolerance was loosened. The exact full impulse/
tail is retained as an unchanged-original **1e-6 PCM** regression, while plotted
linear response is checked against six simultaneous low-level steady tones
after 65536 warm samples, at the same **1e-6** amplitude-response limit. This
retains observable flushing and adds a valid steady-state curve measurement;
it does not claim an ideal response predicts the flush floor at arbitrary levels.

Reproduction (existing caches; no packages):

```powershell
cmake --build build/windows-core --config Release --target aion_eq_tests aion_eq_probe --parallel 2
./build/windows-core/Release/aion_eq_tests.exe
node tests/EqParity.cjs build/windows-core/Release/aion_eq_probe.exe
node tests/EqAutomationParity.cjs build/windows-core/Release/aion_eq_probe.exe
./tools/TestEqWebAudio.ps1
```

Linux commands after safe committed-snapshot sync:

```sh
cmake --build build/linux --target aion_eq_tests aion_eq_probe -j2
ctest --test-dir build/linux -R '^(eq_realtime_contract|js_native_eq_kernel|js_native_eq_automation)$' --no-tests=error --output-on-failure
```

Linux results, sanitizer/curve measurements and final commit are recorded in the
current handoff after execution. Reuse prior accepted loop/source regressions:
this checkpoint does not yet change the production callback, UI, IDs or state.
