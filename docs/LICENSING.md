# Licensing and provenance

The project owner has selected **AGPL-3.0-only** for project-owned Aion-Looper
code. See the repository's [LICENSE](../LICENSE) for the GNU Affero General
Public License version 3. This grant covers code the owner has authority to
license; it does not change third-party code, asset or SDK licenses.

## Why AGPL-3.0

The pinned JUCE 9.0.4 modules are available under AGPLv3 or a separate JUCE EULA.
Using the AGPL route fits the project's free/open-source goal without assuming a
commercial JUCE license. GPL-2.0-only would not be compatible with that route.

Recipients may use, study, modify and redistribute the covered software under
the license. Distribution requires the corresponding source and applicable
notices; modified software supporting remote network interaction also has the
source-offer requirement in section 13. Neither free pricing nor open-source
licensing removes dependency, source-delivery or notice obligations.

Primary sources:

- [Pinned JUCE license](https://github.com/juce-framework/JUCE/blob/9.0.4/LICENSE.md)
- [GNU AGPL version 3](https://www.gnu.org/licenses/agpl-3.0.en.html)
- [GNU license compatibility](https://www.gnu.org/licenses/license-compatibility.en.html)

## Original assets and remaining release checks

- `resources/ui` retains the original Ascii-Looper GPL-2.0 notices and `LICENSE`.
  The extracted methods in `resources/native/loop-view.js` retain their provenance.
  Reference files are unchanged. GPL-2.0-only does not automatically become AGPLv3:
  confirm copyright ownership/permission for any necessary additional grant or a
  compatible separation before claiming the combined application is cleared.
  The user's selection above does not authorize relicensing anyone else's work.
- CLAP extensions are upstream MIT-licensed; retain their notices.
- Applicable JUCE, CLAP SDK/helpers, Windows VST3, ASIO, WebView2 and platform SDK
  notices and redistribution terms still apply. Any later effect/time-stretch
  dependencies require their own compatibility review.
- Local preview package rules include the project AGPL text and applicable
  dependency notices under `share/licenses`. Notice inclusion is not a completed
  rights/dependency audit or a certification of release readiness.

Active Linux delivery is Standalone/LV2; Windows is Standalone/VST3. CLAP is
opt-in/deferred and its notices apply when included. Signing,
installer work and dependency compatibility remain distribution acceptance gates.
