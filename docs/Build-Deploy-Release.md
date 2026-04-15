# Build, Deploy, And Release

This document describes the WSL-based workflow for:

- local builds with `scripts/build.sh`
- local deploy runs with `scripts/build-deploy.sh`
- dist package creation with `scripts/build-package.sh`
- tag-based GitHub releases with `gh`

## Prerequisites

- Run from WSL in the repo root.
- `powershell.exe` must be available in `PATH`.
- `wslpath` must be available.
- `xmake` must be installed on the Windows side, since `build.sh` invokes it through PowerShell.
- `gh` should be authenticated for release work.

## Versioning

All build flows use `scripts/version.sh`.

- If `HEAD` is clean and points at a semver tag like `v1.1.1`, the display version is `1.1.1`.
- Otherwise the display version is `<base>-dev+<shortsha>` with an additional `.dirty` suffix when the worktree is dirty.

Examples:

- tagged clean release: `1.1.1`
- untagged dev build: `1.1.1-dev+abc1234`
- dirty worktree: `1.1.1-dev+abc1234.dirty`

For real release packages, keep the worktree clean and put exactly one release tag on `HEAD`.

## Build

`build.sh` builds the native plugin through Windows `xmake`.

```bash
./scripts/build.sh
./scripts/build.sh releasedbg
./scripts/build.sh release
./scripts/build.sh debug
./scripts/build.sh --vr
./scripts/build.sh --all
./scripts/build.sh --clean --all releasedbg
```

Modes:

- `release`: optimized release build
- `debug`: debug build
- `releasedbg`: optimized build with debug info

Targets:

- default: flat build only
- `--vr`: VR build only
- `--all`: both flat and VR

Outputs:

- flat DLL: `build/runtime/flat/windows/x64/<mode>/SkyrimVanitySystem.dll`
- flat PDB when present: `build/runtime/flat/windows/x64/<mode>/SkyrimVanitySystem.pdb`
- VR DLL: `build/runtime/vr/windows/x64/<mode>/SkyrimVanitySystem.dll`
- VR PDB when present: `build/runtime/vr/windows/x64/<mode>/SkyrimVanitySystem.pdb`

Notes:

- `build.sh` retries with `xmake build -j 1` if a parallel MSVC build hits transient `D8000` / `UNKNOWN COMMAND-LINE ERROR`.
- The default target is flat-only. Use `--all` for release packaging parity.

## Build And Deploy

`build-deploy.sh` builds the plugin and copies the flat runtime payload into a target mod folder.

```bash
./scripts/build-deploy.sh
./scripts/build-deploy.sh release
./scripts/build-deploy.sh debug
./scripts/build-deploy.sh --all
./scripts/build-deploy.sh --clean releasedbg
```

Defaults:

- mode: `releasedbg`
- destination:
  `/mnt/f/games/skyrim/modlists/pt_test/mods/Skyrim Vanity System`

Override the destination with `MOD_DIR`:

```bash
MOD_DIR="/mnt/f/games/skyrim/modlists/some_profile/mods/Skyrim Vanity System" \
  ./scripts/build-deploy.sh
```

What gets deployed:

- flat `SkyrimVanitySystem.dll`
- flat `SkyrimVanitySystem.pdb` when present
- the repo `data/` tree

Notes:

- `build-deploy.sh` only deploys the flat runtime DLL into the mod folder.
- `--all` also builds VR, but the deploy step still copies only the flat plugin.
- The script will fail if the destination file is locked by Skyrim, MO2, or another process.

## Build Dist Package

`build-package.sh` creates the release zip under `dist/`.

```bash
./scripts/build-package.sh
./scripts/build-package.sh --clean
```

Output:

- `dist/Skyrim Vanity System v<version>.zip`

The package contains:

- `Data/` payload from the repo
- `fomod/`
- flat plugin under `SkyrimSE/SKSE/Plugins/`
- VR plugin under `SkyrimVR/SKSE/Plugins/`
- matching `.pdb` files when present

Important:

- The package script always builds both flat and VR via `build.sh --all`.
- The archive version string comes from `scripts/version.sh`.
- If `HEAD` is not a clean semver tag, the zip name will be a dev build name instead of a release version.
- `build-package.sh` prefers the `upstream` remote URL for `fomod/info.xml`, then falls back to `origin`.

## Release Workflow

Recommended release flow:

1. Make sure the release commit is on the intended branch.
2. Make sure the worktree is clean.
3. Optionally do a final validation build:

```bash
./scripts/build-deploy.sh
```

4. Create or move the release tag:

```bash
git tag v1.1.1
```

If you intentionally need to retarget an existing tag:

```bash
git tag -f v1.1.1 HEAD
```

5. Push the branch and tag:

```bash
git push origin main
git push origin v1.1.1
```

If you retagged an existing release tag:

```bash
git push origin -f v1.1.1
```

6. Build the release package:

```bash
./scripts/build-package.sh
```

7. Create the GitHub release:

```bash
gh release create v1.1.1 \
  -R PenguinToast/SkyrimVanitySystem \
  --title "v1.1.1" \
  --notes-file /path/to/release-notes.md
```

8. Upload the dist zip:

```bash
gh release upload v1.1.1 \
  "dist/Skyrim Vanity System v1.1.1.zip" \
  -R PenguinToast/SkyrimVanitySystem
```

If you need to replace an existing asset:

```bash
gh release upload v1.1.1 \
  "dist/Skyrim Vanity System v1.1.1.zip" \
  -R PenguinToast/SkyrimVanitySystem \
  --clobber
```

## Release Notes

A simple release notes file works well:

```md
## Changelog

- Brief summary of the main fix or feature.
- Any packaging or compatibility changes.
- Any noteworthy risks or limitations.
```

Then create the release with:

```bash
gh release create v1.1.1 \
  -R PenguinToast/SkyrimVanitySystem \
  --title "v1.1.1" \
  --notes-file release-notes.md
```

## Verification

Useful checks after publishing:

```bash
git tag --points-at HEAD
gh release view v1.1.1 -R PenguinToast/SkyrimVanitySystem --json assets,url
sha256sum "dist/Skyrim Vanity System v1.1.1.zip"
```

Verify:

- `HEAD` has the intended release tag
- the release exists on GitHub
- the zip asset is attached
- the package checksum is recorded if you need one
