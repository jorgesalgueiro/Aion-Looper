# Development workflow

Owner-approved policy, 28 October 2026. Preserve the working implementation and
acceptance evidence; reduce redundant build/validation work rather than weaken
sound, timing, state compatibility or real-time safety.

## Active format matrix

| Platform | Active | Deferred |
| --- | --- | --- |
| Windows | Standalone + VST3; REAPER | CLAP (opt-in) |
| Linux | Standalone + LV2; Ardour | CLAP (opt-in); Linux VST3 removed |
| macOS | Future Standalone + AU | Other desktop formats and current acceptance |
| iOS | Future Standalone + AUv3 | Current implementation/device acceptance |

`windows` and `linux` configure presets explicitly disable CLAP, including in
older caches. Add `-DAION_BUILD_CLAP=ON` only for intentional CLAP work; existing
extension code, IDs, mapping behavior and historical evidence remain. A cached
CLAP validator path does not enable/register its gate when CLAP is off. New
non-preset builds also default CLAP off. Future macOS defaults omit VST3/AUv3;
iOS retains its AUv3 target. No format needs a separate DSP engine.

## Daily build and test loop

1. Read the current handoff and relevant code/tests once; define a small complete
   slice with observable acceptance and a focused test list.
2. For DSP, build the affected core/probe targets and run selected reference/audio
   tests. For UI/IPC, incrementally build the Standalone and affected test host or
   processor, and run the relevant actual pointer/keyboard/PCM checks. Fix new
   failures before increasing scope.
3. At feature completion, run the relevant Windows/Linux regression set and
   unchanged-JS, connected-audio and interaction acceptance, including 64/128.
   Changes to host routing/state/transport also require the selected VST3/LV2 host
   checks. Report device/DAW/platform checks that remain unavailable separately.
4. At integration/release gates, run the applicable selected-format validators,
   sanitizer/stress/long-session tests and platform checks. Build a package only
   after its acceptance passes and a package is needed.

Do not run the whole audio suite for a documentation edit, repeatedly configure
unchanged dependencies, or rebuild all wrappers after every small fix. Do not
remove failing cases, loosen tolerances or hide unavailable validation. Keep
regressions that guard observable sound/control/state behavior and ownership.
Internal browser graph topology need not be identical if a simpler native path
has demonstrated the same required behavior. Explain implementation-coupled test
changes and retain equivalent or stronger observable regression evidence.

Once configured, Standalone-only incremental builds are:

```powershell
cmake --build --preset windows-dev --parallel 2
```

```sh
cmake --build --preset linux-dev --parallel 2
```

Use explicit `--target aion_core_tests`, `aion_reference_probe`,
`aion_midi_processor_tests` or the affected targets as needed, then run a focused
CTest selection with `--no-tests=error --output-on-failure`. List tests with
`ctest --test-dir BUILD_DIR -N`; do not assume an empty selection passed.
The normal `windows`/`linux` build presets still build their selected delivery
formats and default build targets when that coverage is needed.

## Native WSL workspace

Windows editing/publishing checkout:
`D:\Jorge\Documents\Codex\Aion-Looper`.
Linux source, dependencies and generated files:
`/home/jorge/projects/Aion-Looper`.

Keep Linux builds off `/mnt/d`. Microsoft recommends keeping files in the same
filesystem as the tools for performance:
https://learn.microsoft.com/en-us/windows/wsl/filesystems.
Use a separate Git checkout, not a symlink, shared build directory or copied
Windows CMake cache. No Windows checkout or existing build output is deleted.

Commit a coherent snapshot locally in Windows first (a push is not needed), then:

```powershell
wsl -d Ubuntu --exec bash /mnt/d/Jorge/Documents/Codex/Aion-Looper/tools/SetupWslWorkspace.sh /mnt/d/Jorge/Documents/Codex/Aion-Looper
```

The helper clones/fast-forwards committed `main`, configures a local `windows`
remote and a GitHub `origin`, and reuses pristine pinned JUCE/reference Git caches
as independent native checkouts. Cache checks account for Windows Git CRLF
normalization and reproduce the reference checkout bytes without changing the
source. Transfers use Git protocol instead of direct NTFS object-directory copies.
It refuses dirty, divergent or unrelated existing destinations; it never resets, force-updates or deletes them. It warns
about uncommitted Windows work, which is not copied. Source/dependency/reference
files are read only. CLAP is deferred, so its cache is fetched normally only if
explicitly enabled later. Do not edit and publish competing histories from both
checkouts: Windows is the source/publishing owner and WSL is the Linux build mirror.
If Linux has valuable local work, preserve it and reconcile explicitly before sync.

Inside Ubuntu:

```sh
cd /home/jorge/projects/Aion-Looper
cmake --preset linux \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/.deps/JUCE" \
  -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build --preset linux-dev --parallel 2
# When the plugin needs checking:
cmake --build build/linux --target AionLooper_LV2 --parallel 2
```

Use that JUCE override only after the helper has copied the cache; for a fresh
online checkout omit it and let pinned FetchContent acquire dependencies. Do not
configure on every build. Native CLI commands run under the same Linux user/OS
as the Linux app; they cannot control the Windows app through WSL mailboxes.

For a JUCE-free baseline:

```sh
cmake --preset core
cmake --build --preset core --parallel 2
ctest --preset core -R 'original_ui_bytes|standalone_clock_generation|vst3_program_routing' --no-tests=error
```

These focused checks are infrastructure smoke tests, not completed DSP parity.
For timing comparisons, keep source revision, CMake flags and build type equal;
separate first dependency/helper setup from warmed configure/build times.
Record paths, commands and elapsed times in the handoff; do not claim a build-time
speedup from configure-only measurements.

## Stable scope

Keep JUCE + offline embedded ASCII UI. No Qt/Tauri/CEF switch now. Default product
workspace: 10 loops, 10 drones, 10 samplers; preserve expanded native capacity and
stable IDs. All new feature ports need shared GUI/CLI access. AGPL-3.0-only covers
project-owned code; third-party licenses and asset-permission checks remain.

## Measured storage comparison — 28 October 2026

One matched CMake comparison at source `f0c1264`, pinned JUCE 9.0.4, Release,
Standalone/LV2 enabled, CLAP/VST3 disabled and BUILD_TESTING=OFF:

| Configure phase | Windows-mounted `/mnt/d` | Native WSL `/home/jorge/projects` |
| --- | --- | --- |
| Fresh build-tree setup, including JUCE helper compilation | 288.21 s | 46.22 s |
| Subsequent warmed configure | 176.15 s | 1.87 s |

This run's warmed configure was about 94x faster. This is configure-only evidence,
not a repeated statistical study, full application build benchmark or runtime
performance claim. Keep configure calls out of unchanged incremental loops.
Commands and source/JUCE revisions are in the local ignored evidence file
`build/wsl-configure-benchmark.json`; both trees use `build/workflow-benchmark`.
The initial storage transfer also exposed a missing historical CMakeLists.txt Git
blob; `git fetch --refetch origin main` restored it and connectivity fsck passed.
That repair changed no working files or branch history. Reference caches required
explicit CRLF normalization detection; native exact-asset checks passed afterward.

The new native Linux Standalone-only build preset and LV2 target both built
successfully after setup (first app build 280.47s, LV2 12.39s). Windows
Standalone/VST3 builds passed too. Focused infrastructure checks passed 4/4 on
Windows and 3/3 in native WSL, including original asset identity. These are
workflow checks, not a new full DSP/DAW/device acceptance run. Dirty-mirror and
unrelated-destination probes both refused changes while preserving files/HEAD.

A subsequent native Standalone incremental no-op build took 0.75s (/usr/bin/time);
the Windows incremental build took 5.05s including its command startup. These
single runs are separate observations, not matched full-build benchmarks.
