# SemperSupra experimental build and distribution policy

Status: draft policy for the SemperSupra OXCE fork and compile-observer work.

## Purpose

`SemperSupra/OpenXcom` is a public fork used as a reviewed build, deployment, and possible upstream-contribution surface. It is not the official OXCE repository.

Official upstream remains:

- repository: `MeridianOXC/OpenXcom`
- project: OpenXcom Extended (OXCE)

SemperSupra builds must be unmistakably distinguishable from official OXCE builds, even if an archive is renamed or copied away from GitHub.

## Build classes

Use three distinct levels.

1. **CI build** — compile/test evidence only. No claim that the output is suitable for users.
2. **Rolling experimental prerelease** — public downloadable binaries used by SemperSupra development and local-agent environments. Unsupported and explicitly unofficial.
3. **Immutable preview prerelease** — intentionally published snapshot for broader testing after the relevant parity/runtime gates have passed.

Do not create a normal GitHub Release for experimental compile-observer work. Experimental distribution uses GitHub **Pre-release** semantics only.

## Rolling experimental channel

The canonical development channel is the mutable prerelease tag:

```text
semper-exp-current
```

Release page:

```text
https://github.com/SemperSupra/OpenXcom/releases/tag/semper-exp-current
```

Machine API:

```text
https://api.github.com/repos/SemperSupra/OpenXcom/releases/tags/semper-exp-current
```

The tag is intentionally mutable. It advances to a new `semper/compile-observer` candidate only after all public gates required by the workflow succeed. For the current work those gates are:

- observer contract test;
- full Linux engine build;
- full Windows engine build.

The rolling release is an **acquisition channel**, not an immutable historical record. Every platform package includes the candidate SHA in its filename and internal provenance, and the release publishes a machine-readable `experimental-build-manifest.json` plus `SHA256SUMS.txt`.

Automated consumers should resolve the release by the exact `semper-exp-current` tag, read the manifest, select a compatible artifact, verify SHA-256, and only then execute or unpack it. Consumers should not scrape Actions run IDs or depend on Actions artifact retention.

## Artifact naming

Experimental downloadable artifacts use this form:

```text
semper-oxce-experimental-<upstream-base>-g<short-sha>-<os>-<arch>.<ext>
```

Examples:

```text
semper-oxce-experimental-8.6.5-g189795f-linux-x86_64.AppImage
semper-oxce-experimental-8.6.5-g189795f-windows-x86.zip
```

Avoid names that could be mistaken for upstream, including:

```text
OXCE-8.6.5.zip
OpenXcom-8.6.5.zip
OXCE-nightly.zip
v8.6.6
```

The `semper-` and `experimental` qualifiers are intentional provenance, not decoration.

## Executable provenance

Public experimental CI stamps the executable version metadata at build time, for example:

```text
Extended 8.6.5 (SemperSupra experimental g189795f)
```

The source baseline is not permanently rewritten solely to carry this build label. The stamp is produced by the public build workflow so a renamed archive or copied executable still identifies itself as a SemperSupra experimental build.

## BUILD-INFO.txt

Every downloadable package contains generated provenance equivalent to:

```text
UNOFFICIAL / EXPERIMENTAL BUILD

This build is produced by the SemperSupra OpenXcom fork.
It is not an official OXCE release and is not published by MeridianOXC.

Upstream project:
  MeridianOXC/OpenXcom

Upstream base:
  OXCE <version>
  <full upstream commit SHA>

SemperSupra source:
  SemperSupra/OpenXcom

Candidate commit:
  <full SemperSupra commit SHA>

Channel:
  semper-exp-current

Support:
  Do not report failures from this build to upstream OXCE unless the
  problem is independently reproduced on an official upstream build.
```

This information is generated from immutable build inputs rather than maintained manually inside an archive.

## Machine-readable manifest

The rolling prerelease publishes:

```text
experimental-build-manifest.json
```

Schema version 1 contains at least:

- channel and experimental/offical status;
- upstream repository, version, and exact upstream commit;
- SemperSupra source repository, branch, and exact candidate commit;
- release/tag URLs;
- one entry per downloadable platform artifact;
- OS and architecture compatibility;
- package kind and entrypoint;
- immutable SHA-256 for each artifact;
- direct public download URL.

The current supported automated-consumer targets are:

- `windows` / `x86`, marked compatible with `x86_64` Windows hosts;
- `linux` / `x86_64`, delivered as AppImage.

Unsupported host combinations must fail closed rather than selecting an unrelated binary.

## Immutable preview naming

If broader distribution or historical retention becomes useful, create a separate immutable prerelease rather than treating the rolling tag as archival history.

Recommended tag:

```text
semper-preview-<upstream-base>-<YYYYMMDD>-g<short-sha>
```

Recommended title:

```text
UNOFFICIAL — SemperSupra OXCE Preview Build
Base: OXCE <upstream-base> · g<short-sha>
```

Normal OXCE semantic-version tags remain reserved for upstream.

## Public CI boundary

Public GitHub-hosted CI may contain only material safe for public distribution, including:

- GPL engine source and SemperSupra patches;
- compiler/unit/smoke tests;
- synthetic/reference mods and test assets that are redistributable;
- generated build metadata and checksums.

Public CI must not contain or upload proprietary X-COM data, private development evidence, credentials, secrets, or licensed assets that cannot be redistributed.

Authoritative tests that require legitimate X-COM data belong on a controlled local/self-hosted execution surface.

## Pinned-base review gate

Experimental public PRs should compare against an explicit pinned upstream-base branch, not a moving upstream branch. For the current work:

```text
semper/oxce-8.6.5-base
  -> 668880bb756852f0734cfe92e438e400080a2f4d
```

The current compile-observer PR is a build/review gate only and must not be merged merely because CI passes. Runtime A/B/C parity and side-effect validation remain separate promotion gates.

## Windows CI policy

Do not use `windows-latest` for this legacy Visual Studio solution when reproducibility depends on a specific Visual Studio generation. In August 2026, `windows-latest` resolved to Windows Server 2025 with Visual Studio 2026, which broke the old OXCE nightly step that attempted to modify a Visual Studio 2022 installation.

For the compile gate, pin:

```yaml
runs-on: windows-2022
```

and compile the solution's ordinary supported `Release|Win32` configuration. `OpenXcom.2010.sln` and `OpenXcom.2010.vcxproj` define both `Release` and the legacy `Release_XP` configurations; `Release` uses the runner's default supported platform toolset, while `Release_XP` explicitly requests `v141_xp`.

The public compile gate therefore validates modern Windows compilation without dynamically installing the obsolete XP toolset. If an actual Windows-XP-compatible distribution is ever required, treat that as a separate compatibility/release job with its own maintained toolchain rather than making it a prerequisite for ordinary CI.

## Release source and generator-validator boundary

Every public binary release must remain tied to public source that is sufficient to satisfy the applicable license and rebuild the distributed binary. For this GPL fork, the public release contract is **complete corresponding source**, not a deliberately reduced implementation.

The public release tag already identifies an exact public source commit and GitHub exposes source archives for that tag. The stronger target state is to also publish an explicitly named source snapshot whose SHA-256 is recorded in `experimental-build-manifest.json` alongside the platform binaries.

The source snapshot must be generated from the exact reviewed **public** candidate commit. It must never be created by archiving the private development repository and deleting known-private paths afterward.

The public/private split is based on semantic role rather than file format. Private validation/evaluation assets may remain private when they are not required corresponding source, including comprehensive conformance corpora, adversarial/regression knowledge, proprietary-data test environments, private compatibility matrices, fuzz/evaluation knowledge, and other validator/control-plane evidence. Ordinary public build/smoke tests remain public where they are part of the public build and trust contract.

This preserves a generator-validator asymmetry:

```text
private validation/control plane
    judges reviewed implementation
             |
             v
public source candidate
    -> public CI/build
    -> platform binaries
    -> corresponding source snapshot
    -> hashes/provenance
```

A public implementation can therefore be reproducible and GPL-compliant without publishing every private asset used to decide whether candidate behavior is acceptable.

For downstream Windows distribution, Windows Package Foundry should index immutable preview/stable Windows releases and their provenance rather than becoming a source-code mirror. The mutable `semper-exp-current` channel remains primarily a development acquisition channel unless Foundry later defines an explicit experimental-feed policy.
