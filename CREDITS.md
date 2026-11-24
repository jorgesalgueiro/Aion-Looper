# Credits

Aion-Looper is possible because people publish their work and let others build
on it. Thank you to the authors and contributors below. Their code retains its
own copyright and license; Aion's AGPL choice does not replace those terms.
This index accompanies, rather than substitutes for, the full license notices.

## Code used or adapted

| Project / authors | Use in Aion-Looper | Source and retained credit |
| --- | --- | --- |
| **Ascii-Looper — Jorge Salgueiro** | Original ASCII interface, interaction, presets, and migration reference; selected methods adapted for the native presentation | Preserved `resources/ui/README.md`, `resources/ui/LICENSE` and source headers. GPL-2.0 notices remain; combined-distribution permission checks remain open in [licensing](docs/LICENSING.md). |
| **JUCE — JUCE contributors / Raw Material Software** | Native audio/MIDI/application framework, DSP/platform adapters and embedded WebBrowser | [JUCE 9.0.4](https://github.com/juce-framework/JUCE/tree/9.0.4), commit `3fb5cbc0340758596dac39f7c8de47d562b4896e`; upstream `LICENSE.md` and module notices. See `cmake/Dependencies.cmake`. |
| **HiFi-LoFi — AudioFFT** | Unmodified portable FFT used by the internal convolution kernels | [FFTConvolver revision f2cdeb04](https://github.com/HiFi-LoFi/FFTConvolver/tree/f2cdeb04c42141d2caec19ca4f137398b2a76b85), MIT. [Pinned files/hashes](third_party/audiofft/README.md), [full license](third_party/audiofft/COPYING.txt). Aion supplies the scheduling/history/ownership layer; upstream FFTConvolver engines are not copied. |
| **Takuya Ooura** | Radix-4 FFT arithmetic embedded in AudioFFT | [Original FFT package](https://www.kurims.kyoto-u.ac.jp/~ooura/fft.html); copyright 1996–2001 and author permission retained in [AudioFFT/Ooura notice](third_party/notices/AudioFFT-Ooura.txt). |
| **Google / Chromium and Blink contributors** | Adapted compressor, AudioParam timeline behavior, resampling coefficients and phase conventions | [Compressor notice and pinned source](third_party/notices/WebAudio-Compressor-BSD.txt), [Chromium notice](third_party/notices/Chromium-BSD.txt), [resampling notice and pinned source](third_party/notices/WebAudio-Resampling-BSD.txt). BSD-style terms retained in source/notices; no Chromium runtime is introduced by these DSP adaptations. |
| **Advanced Micro Devices contributors — AOCL LibM** | Portable scalar adaptation of the compressor logarithm, retaining source constants/arithmetic order | [Pinned source, changes and BSD notice](third_party/notices/AMD-Compressor-Log10-BSD.txt); revision `5760c0a6a5c650aaeaee81d5d83592fc2da66207`. |

The internal stereo reverb follows Ascii-Looper's `_createReverb` control/routing
order (pre-delay, convolution, wet damping, dry/wet mix, output volume). Its
native smoothing, crossfades and Q=.5 TPT filter use Aion's existing Delay
approach and bilinear-transform equations, not copied WebAudio filter code.
Convolution continues to use the credited AudioFFT/Ooura sources above.

The internal stereo reverb follows Ascii-Looper's `_createReverb` control/routing
order (pre-delay, convolution, wet damping, dry/wet mix, output volume). Its
native smoothing, crossfades and Q=.5 TPT filter use Aion's existing Delay
approach and bilinear-transform equations, not copied WebAudio filter code.
Convolution continues to use the credited AudioFFT/Ooura sources above.

## Optional components and platform dependencies

These are not all active effects or enabled plugin formats. Retain their own
notices when used; platform/framework dependencies also include nested notices.

- **GRAME, INRIA and Faust contributors:** optional Faust 2.88.0 bytecode
  interpreter development backend, currently disabled and not routed into the
  product. LGPL-2.1-or-later and applicable architecture exceptions; see
  [Faust scope, patches and provenance](docs/FAUST.md) and
  [retained license](third_party/notices/Faust-LGPL-2.1.txt). Each future Faust
  effect implementation needs its own author/license entry.
- **free-audio / CLAP contributors:** opt-in CLAP SDK, helpers and
  [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions/tree/7adee3a1bd4684d4caa5601100e364abccff4b4d),
  pinned in `cmake/Dependencies.cmake`; retain upstream MIT notices.
- **Steinberg contributors:** optional VST3 SDK and ASIO headers bundled by JUCE;
  retain the licenses of the exact bundled revisions. No driver is supplied.
- **Microsoft:** Windows WebView2 SDK/runtime and Windows development tools;
  **WebKitGTK/GTK, ALSA, JACK and other platform contributors:** Linux browser,
  UI and audio dependencies. See [SDK inventory](docs/SDKS.md), build instructions
  and the applicable upstream licenses. These credits do not relicense SDKs.
- **SoundTouchJS contributors:** the preserved original Ascii-Looper page
  references SoundTouchJS 0.1.30. This is a legacy reference, not a claim of a
  completed native sampler/time-stretch port or a new bundled DSP dependency.
  Preserve original credits and audit the selected implementation before a port.

## Ideas studied, without copied code in the convolution slice

HiFi-LoFi's two-stage convolution was studied for partitioning concepts.
[Dragonfly Reverb](https://github.com/michaelwillis/dragonfly-reverb),
[Signalsmith Audio](https://github.com/Signalsmith-Audio/basics) and
[Freewheeling](https://github.com/free-wheeling/freewheeling) are valuable public
references. Mentioning them does not imply endorsement, inclusion of their
engines/presets, or that Aion sounds identical to them.

## Keeping credits complete

With every copied/adapted dependency, algorithm, coefficient table, preset bank,
IR/audio asset or Faust DSP: retain its author/header, record the exact source
revision and modifications, add the full license/attribution, and carry those
notices and this index into distributions. Distinguish copied code from ideas
studied. Never replace a third-party copyright with Aion's SPDX identifier.
The release dependency/asset inventory and original-asset permissions still
require the checks in [LICENSING.md](docs/LICENSING.md); this index does not certify
that those release gates have passed.
