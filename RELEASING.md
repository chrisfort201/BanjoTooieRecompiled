# Fork build and release procedure

This fork does **not** auto-build or auto-release on pushes. The Windows and Linux x64 workflows are separate, both manual (`workflow_dispatch`). **A successful build is not a gameplay test or a grant of redistribution permission.** The ROM is never an asset of a build or release.

## 1. Pre-release checks

Before publishing binary archives, verify that the repository and its included dependencies and assets have the necessary redistribution permissions. **This check is not complete.** It is independent of whether CI builds successfully, and no font or artwork changes have been applied as part of the documentation update.

The inherited `assets/Suplexmentary Comic NC.ttf` contains an embedded copyright notice reading **"All rights reserved"**. No accompanying redistribution license was identified in the uploaded repository snapshot. This does **not** establish that the original maintainer lacks permission; permission could have been granted separately. In fact, the [base revision's developer notes](https://github.com/chrisfort201/BanjoTooieRecompiled/blob/9e5852541ba1436417e2c4468960e8c6de6deda5/AGENTS.md) already flag this font and the artwork under `assets/icons/` as items requiring review before a public release.

An **optional, separate font-substitution patch** has been prepared to use an existing LatoLatin font, but it is **not applied** by the documentation patch. If you choose to use it, inspect the replacement font's license terms, remove the original binary font from the tracked tree, and test launcher appearance. Substitution alone does not settle provenance of other bundled media.

Likewise, the pinned `lib/RecompFrontend` source snapshot did not contain a top-level license file. This is an item to clarify with its maintainers or through other authoritative licensing information; it is **not** a claim that redistribution is forbidden. Review the licenses of other compiled libraries and assets as applicable. Keep appropriate copyright and license notices in distribution packages.

Until the open questions are resolved, use Actions archives for development/testing rather than publishing public binary release assets. A source-only tag or draft release can record a tested commit without distributing an executable. This is a cautious release checklist, not a legal determination about upstream's releases.

Before publication, also run the camera/Breegull hook checks, unit tests and a Windows gameplay smoke test from the precise release candidate. Disclose untested platforms, and verify that no ROM data are bundled.

## 2. Confirm the exact commit

In PowerShell, from your existing clone:

```powershell
cd "$env:USERPROFILE\PycharmProjects\BanjoTooieRecompiled"
git switch main
git pull --ff-only origin main
git status -sb
git rev-parse HEAD
py tools/tests/test_camera_hooks.py
py tools/tests/test_breegull_hooks.py
```

A clean top-level worktree and matching remote are expected. The pinned submodules can appear dirty after `tools/setup_deps.py` applies required local patches; that is expected.

Record the full SHA. Do not use the branch name as a substitute for an immutable release revision.

## 3. Build Windows x64 manually

Open [Actions](https://github.com/chrisfort201/BanjoTooieRecompiled/actions), select **Build - Windows x64 (manual)**, choose branch `main`, then click **Run workflow**. The workflow checks out the exact commit, recursively fetches pinned submodules, applies dependency patches, compiles the Windows GUI executable, runs camera/Breegull tests, checks runtime assets, and uploads a ZIP as a **temporary Actions artifact** (retained for three days).

Download the artifact and extract the inner `BanjoTooieRecompiled-main-windows-x64.zip`. Verify it contains the EXE, SDL2/DXC DLLs and `assets/`. A Windows gameplay smoke test is still recommended on the exact release candidate. A GitHub Actions artifact is **not yet** a permanent release.

## 4. Build Linux x64 manually (optional, NOT PLAYTESTED)

On [Actions](https://github.com/chrisfort201/BanjoTooieRecompiled/actions), run **Build - Linux x64 (manual)** for the **same commit**. It adapts the upstream project's Linux job: Ubuntu 24.04, Clang, Ninja, SDL2, FreeType, D-Bus portal libraries, dependency patches, hook/unit tests, ELF/`ldd` checks, and a `tar.gz` package with a working-directory `run.sh` wrapper.

The tarball is a **CI compile/package result only**. No Linux execution, game input, graphics or full gameplay has been verified for the fork unless a person actually tests it. A green result must be labeled **Linux x64 — built, not run or playtested**. If it fails, publish no Linux package and keep the failure report for future work. The separate workflow runs only on demand; do not re-enable upstream's automatic push-to-release workflow.

The **Flatpak** files in `flatpak/` come from upstream, including their upstream application identity. A release-grade fork Flatpak would require reviewing that identity and packaging separately. The fork does not currently claim to provide or test Flatpak. macOS is not one of this upstream project's present official release assets.

## 5. Verify provenance, assets and checksums

For each workflow run, read the **commit SHA** on the GitHub Actions run page. Do not combine a Windows archive from one revision with a Linux archive from another. Check the archive listing and search for the user's ROM or copyrighted unlicensed files before publication. Example PowerShell commands (adapt archive paths to where you downloaded them):

```powershell
Get-FileHash '.\BanjoTooieRecompiled-main-windows-x64.zip' -Algorithm SHA256
Get-FileHash '.\BanjoTooieRecompiled-linux-x64.tar.gz' -Algorithm SHA256
```

The Linux checksum command applies only if a Linux archive was built. Include both full commit SHA and hashes in release notes. The GitHub release's source tarball is automatically generated; it is **not** a replacement for a review of bundled assets and dependency permissions.

## 6. Create a manual GitHub Release (only after rights review)

Open [New release](https://github.com/chrisfort201/BanjoTooieRecompiled/releases/new). Choose **Create new tag** with a fork-specific, non-upstream-implying name such as `fork-2026-11-22-rc1`, targeting the recorded `main` commit (do not simply accept an outdated dropdown default). Give it a title such as `Camera and Breegull controls — experimental build 1`, attach the **inner Windows ZIP** and optional Linux `.tar.gz`, then **mark it as a pre-release**.

Use release notes similar to:

> Unofficial feature fork of Vidanox/BanjoTooieRecompiled; not an official upstream release.
> Source commit: `<full SHA>`.
> Windows x64: camera and Breegull controls exercised in gameplay; full Breegull section not comprehensively tested.
> Linux x64 (if attached): CI-built/linked/packaged; **never launched or playtested** for this fork.
> Includes optional third-person camera settings, Modern Breegull twin-stick controls, R-held aiming input, vertical inversion/speed, and avoidance of unwanted movement-triggered pitch leveling.
> Requires your own legitimately obtained Banjo-Tooie USA v1.0 ROM; no ROM included.
> Refer to upstream for newer official fixes.
> Checksums: `<SHA-256 per asset>`.

Publish only after verifying the uploaded assets and notes. A GitHub Release asset persists beyond Actions artifact expiry and can be linked directly from the fork README. GitHub's `releases/latest` page can behave differently for pre-releases, so link to **the actual release page/tag** rather than promising that `latest` resolves to a pre-release.

## 7. After release

The already-merged `modern-breegull-controls` branch has been deleted; its commits remain available through `main`. Document any new Windows/Linux gameplay results in a subsequent commit and release instead of silently changing existing release binaries. The separate upstream [camera discussion](https://github.com/Vidanox/BanjoTooieRecompiled/issues/23) does not automatically change when this fork merges branches. For Breegull, consider a separate upstream issue/PR, with a clear note that the fork currently contains both enhancements and that independent upstream contributions would need to be separated and tested.
