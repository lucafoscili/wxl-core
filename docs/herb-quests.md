# Herbs and trivial quests — bounded native trial

This is the existing WXL owner's two-hook experiment for Velora, not a new fork
or extension loader. Source: `src/client/MinimapTracking/`; native landmarks:
`src/offsets/game/MinimapTracking.hpp`. The Velora owner of exact-client findings,
candidate identity and human-led acceptance is
`F:/GitHub/velora/wow/ui/TRACKING.md`.

Build flag `WXL_HERB_QUESTS_TRIAL` defaults **off**. An enabled build refuses
`CLIENT_PATH`, so building cannot deploy. Its runtime key `WXL_HERB_QUESTS`
defaults on only inside that opt-in build. No new configuration is required.

## Behavior

The native service slot and actual tracked spell ID remain the source of truth.
`GetTrackingInfo`, the stock dropdown, native dots, aura handling and the saved
`minimapTrackedInfo` CVar are not replaced.

- Selecting TrivialQuests while the actual tracked spell is Find Herbs (2383)
  calls the native service setter without cancelling that spell.
- A Find Herbs aura update may keep an already selected TrivialQuests service.
  This exception is restricted to the inspected spell-update caller. It still
  calls the native service setter for CVar/event handling.
- Clicking selected TrivialQuests toggles it off when the tracked spell is zero
  or Find Herbs. Herbs still follows its native cast/toggle/aura feedback path.
- None and every other spell/service keep their native behavior. Other tracking
  spells still clear TrivialQuests, and other services still cancel the spell.
  The class-filtered native service index resolver is reused. No unavailable
  spell is added, no profession is granted and no server resource mask is edited.
- Both hooks must activate before either changes behavior. Incompatible code or
  a partial installation leaves the feature inactive; inspect the existing WXL log.

## Exact client evidence

Home and Beta are different executables. Both passed the capability's whole-routine
and service-table compatibility fingerprints on 2026-09-28. The fingerprint table
is the one source for runtime admission and the offline reader. It includes the
selector, setter, spell updater, class-aware resolver, menu query, marker collector,
resource/quest predicates and Lua argument seams. No claim is made for other 12340
clients. Compatibility fingerprints are not a security checksum.

The unchanged collector at `0x57F7F0` emits separate category lists. Resource objects
reach `0x6DCA90`, which tests the local player's resource mask against lock skills;
trivial quest statuses 2/4 reach `0x57BF30`, which checks service type 3. Selection
exclusion is upstream, not a single renderer branch shared by both object classes.
Spell 2383 was confirmed as Find Herbs in the supplied Home Spell.dbc.

## Focused checks

```powershell
cmake -S . -B build-tracking -A Win32 -DCLIENT_PATH:PATH=
cmake --build build-tracking --config Release --target wxl-herb-quests-test
./build-tracking/Release/wxl-herb-quests-test.exe
python tests/check_tracking_client.py <exact-Wow.exe> [<another-Wow.exe>]
python tests/tracking_native_fixture.py <exact-Wow.exe> [<another-Wow.exe>]
```

The Python tools require `pefile`; the collector fixture additionally requires
`unicorn`. The fixture executes the supplied executable's original x86 collector
and both eligibility predicates with synthetic objects, resource-mask updates and
engine-service stubs. It checks neither/quests/herbs/both and captures category-9
quest and category-8 herb admission. It does **not** launch WoW, emulate a server,
execute the WXL hooks, or prove rendered pixels, aura timing, saved state or Proton.
The C++ check exercises pair policy/exclusions, with assertions kept on in Release.

For a playable candidate, use Velora's existing composed Beta runtime preparation,
which retains portrait/wardrobe work and both picking patches. A clean core DLL
from this checkout is not a replacement for that installed runtime. No remote push,
installation, game input or Home promotion is implied by these checks.
