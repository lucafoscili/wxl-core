# Character appearance hooks

`SkinAlpha.cpp` owns custom-body sheet alpha and native paint routing.
`SkinAlpha.hpp` owns the shared routing, caller scope, queued identity and pixel
arithmetic exercised by `tests/skin_alpha_test.cpp`. `CustomBody.hpp` resolves the
component's model stem; the existing path policy treats `Character\` models,
including stock-derived personal models, as stock. `DonorHair.cpp` separately
owns hair visibility under helmets.

## Stock sheet scope correction — 2 October 2026

**Source/offline correction checked; not installed and not visually accepted.**
Luca/Claude reported black stock skin showing through clothing in Beta, forwarding
`5f21312` as the suspected origin. That visual report and its breadth were not
independently measured here. Source inspection confirmed two unintended paths:
the base hook cleared alpha for a stock source with alpha bits, and the overlay
hook used Snapshot/Restore for noncustom sheets. It also incorrectly treated
every ARGB sheet as custom. These defects support the correction; they do not
establish native causality for every reported character.

Both paint hooks now route stock and unidentified sheets through exactly one
ordinary native call. They preserve its arguments and resulting bytes, without
reading the region/resolution or running ClearAlpha, Snapshot or TwoPass. Source
texture alpha does not select the custom path. An identified custom ARGB sheet
still clears base alpha and uses the existing TwoPass overlay algorithm; an
already-opaque custom region keeps its one-paint fast path. The existing custom
DXT1-to-ARGB switch and its in-flight request guard are unchanged.

### Why the caller scope changed

Pristine 12340 code confirms that component reset copies the native global format
to component `+0x14` (`0x004EFC5F` / `0x004EFC6C`). Initialization stores its input
format to `0x00B6B85C` at `0x004F1DD0`; only its compressed/threaded branch replaces
that with DXT1 at `0x004F1DE6`. Stock can therefore use ARGB too. Section walks now
require **both body identity and ARGB**, rather than format alone.

Queued requests contain format and textures, but no model identity. Native
submission `0x004F1790` allocates/reuses the request at `0x004F1794`, copies the
component's format at `0x004F17A2`, gathers source textures, then publishes under
the queue lock and signals the worker. Allocation `0x004F10E0` has this single
direct native caller. The worker invokes request paint at `0x004F13F8`.

Two narrow hooks in this same owner capture the submission's custom/ARGB decision
at allocation, before publication. A mutex protects the request-address marks;
no live component/model pointer crosses threads. Paint consumes the mark and
also requires the copied request format to remain ARGB. Unknown requests stay
native. Cancellation may bypass paint, so every allocation/reuse first removes
any stale mark before capturing the new owner. Cancelled marks are bounded by
the native process request pool; this feature neither allocates nor frees native
requests. Nested scopes replace the current decision even for stock, then restore
their predecessor. Native compression choices and queue/retirement remain owned
by the original client.

### Offline evidence and next batch

Fresh `build/skin-alpha-scope`, Visual Studio 2022 **Win32**, MSVC **19.38.33135**,
SDK **10.0.22621.0**, `CLIENT_PATH` empty:

- `wxl-skin-alpha-test` Release: **78 checks passed**. Shared production routing
  covers stock alpha 0/8 with compressed/uncompressed classification, one-call
  forwarding and every native result byte, lazy sheet access, custom base clear,
  half-coverage colour, untouched skin, opaque/invalid-region fallback, stock-derived
  and unidentified models, nested scope restoration, cross-thread identity,
  cancellation/reuse and consumed/rejected marks. Existing arithmetic stays checked.
- `tests/check_skin_alpha_requests.py`: **4 isolated native cases passed** using
  unmodified 12340 allocation/submission instructions, ARGB/DXT1 and clean/dirty
  gathers. Native pool reset, returned request, format copy before gather, publication
  after gather, signal order, stack and callee-saved registers were exercised.
  Locks, signal and gather leaf are synthetic; production hook execution is not
  established by this fixture. Report: `build/skin-alpha-scope/native-request-seams.json`.
- Compile-only `WarcraftXL.vcxproj` `ClCompile` succeeded, including `SkinAlpha.cpp`.
  MSBuild's selected-file property did not restrict this target, so it compiled
  the canonical DLL translation units. No DLL was linked. C4530 exception-unwind
  warnings remain in the existing build flags; this is not exception-path evidence.

The first configure failed because a manually forced toolset version selected a
missing version-specific props path. Removing that override let the installed
toolchain configure normally. No compiler installation or system setting changed.

This is a source-ready next-batch handoff, not a deployable composed DLL. Prepare
and build the next integrated candidate through Velora's existing heel-runtime
canonical-source owner, preserving its retained overlays/options. No environment
toggle, client restart, install, publication, feed/service/SQL, asset, Home or
progress operation occurred. The current Beta play session remains unchanged.

Minimal native checklist after the separately reviewed next batch:

1. Inspect an existing equipped stock character in selector and world: natural
   skin colour and clothing coverage, then one equipment change/recomposition.
2. Inspect an existing custom overlay wearer: own skin visible in uncovered areas,
   unchanged opaque armour and clean partially covered edges, then recomposition.
3. Switch stock → custom → stock, including a rapid interrupted request: no carried
   transparency, black skin or stale coverage. Record Luca's actual verdict.

Ayane's heel alignment/hollow surfaces and donor body/material/export work remain
their separate lane. This correction does not claim to fix those reports.
