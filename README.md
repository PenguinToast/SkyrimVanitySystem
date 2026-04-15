# Skyrim Vanity System

`Skyrim Vanity System` is a Skyrim SKSE mod for player-facing vanity outfit management.

This project was previously named `Skyrim Outfit System Redone`.

It lets the player keep their real equipped gear for gameplay while changing the visible appearance through a native in-game browser and variant workbench.

Requires https://github.com/PenguinToast/DynamicArmorVariants-Extended to function.

100% vibecoded with Codex, **heavily** inspired by [Modex](https://github.com/patchulidev/ModExplorerMenu) for catalog/UI.

## Features
- Snappy ImGui interface for managing armor appearance overrides
- Searchable catalog for:
  - individual armor pieces
  - outfits
  - Modex kits
- Persistent favorites system
- Preview mode for selected gear, outfits, and kits.
- Integration with Modex kits - creation from equipped gear or active overrides
- SKSE save/load for overrides.
- Modex-style theme loading, fade behavior, and smooth scrolling.

## Requirements
- [XMake](https://xmake.io) 3.0.0+
- C++23 compiler on Windows (MSVC or Clang-CL)

## Getting Started
```bash
git clone --recurse-submodules git@github.com:PenguinToast/SkyrimVanitySystem.git
cd SkyrimVanitySystem
```

If you cloned without submodules:
```bash
git submodule update --init --recursive
```

## Build
From Windows:

```bat
xmake build
```

This generates output under `build/windows/` in the project root.

From WSL, to build and deploy directly into the local test mod folder:

```bash
./scripts/build-deploy.sh
```

By default this uses `releasedbg`, so the deploy includes a `.pdb` alongside the DLL for better crash logs.

For a full clean rebuild:

```bash
./scripts/build-deploy.sh --clean
```

By default this deploys to:
`/mnt/f/games/skyrim/modlists/pt_test/mods/Skyrim Vanity System`

## Build And Package
To build a release mod archive:

```bash
./scripts/build-package.sh
```

For the full WSL build, deploy, packaging, tag, and GitHub release workflow, see
`docs/Build-Deploy-Release.md`.

This writes a zip under `dist/` named like:
`Skyrim Vanity System v1.1.1.zip`

Tagged clean builds produce a normal `X.Y.Z` archive version.
Dirty or untagged builds produce a `X.Y.Z-dev+<sha>[.dirty]` archive version.

The archive root contains the normal mod payload plus `fomod/info.xml`.
Packaging uses `releasedbg`, so the archive also includes a `.pdb` next to the DLL.

## Settings And Data
- Runtime settings are stored under:
  - `Data/SKSE/Plugins/SkyrimVanitySystem/settings.json`
- Favorites are stored separately in:
  - `Data/SKSE/Plugins/SkyrimVanitySystem/favorites.json`
- Bundled UI assets are under:
  - `Data/Interface/SkyrimVanitySystem/`

For backward compatibility, the mod will migrate or fall back to the old
`SkyrimOutfitSystemRedone` settings/interface paths when the renamed ones are
missing.

## Modex Integration
- Modex kits are loaded from:
  - `Data/Interface/Modex/user/kits`
- SVS can browse kits, preview them, and add them into the workbench.
- SVS can also create new Modex kits from equipped gear or active overrides.

## Lint And Format
```bash
./scripts/format.sh
./scripts/format.sh --check
./scripts/lint.sh
./scripts/lint.sh src/ui/Menu.cpp
```

From WSL, `format.sh` prefers Linux `clang-format` when available. `lint.sh` prefers the
Visual Studio LLVM `x64` `clang-tidy.exe` because the generic Visual Studio `Llvm/bin`
binary is a 32-bit build that crashes in this environment.

`lint.sh` also passes `/Y-` to disable MSVC PCH use during linting. xmake's generated
`compile_commands.json` points `clang-tidy` at a PCH path that does not exist, so disabling
PCH is the reliable way to lint these translation units.

## Project Generation
For Visual Studio:

```bat
xmake project -k vsxmake
```

For `clangd` or other LSP tooling:

```bat
xmake project -k compile_commands
```
