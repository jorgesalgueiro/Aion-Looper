# Workspace location

Active Rust project: `D:\Jorge\Documents\Codex\Aion-Looper`.

- `reference/Ascii-Looper/` preserves the original repository and its Git history.
- `.reference/Ascii-Looper/` remains the original migration baseline snapshot.
- Source, local toolchains, build products, outputs and Windows packages are retained.
- `verification/aion-relocation.json` records file verification and cleanup state.
- Project files, Git history and settings were checked with SHA-256. Rebuildable
  toolchain/build caches were checked by complete file sizes and timestamps.
- Partial destination files from the interrupted earlier transfer are retained in
  `verification/relocation-partial-copies/`. A pre-existing empty Git lock and the
  original ignore file are retained in `verification/relocation-originals/`.

Run the existing build/check scripts from this directory. The local toolchain
wrapper resolves its paths relative to the project; Dropbox is no longer needed
for the relocated files.

If Windows temporarily keeps the empty old project folder open,
`scripts/finish-relocation.ps1` removes only the empty old folder and empty Codex
parent after their locks clear. It never recursively deletes files.