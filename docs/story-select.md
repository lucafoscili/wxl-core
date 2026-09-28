# Native character roster and optional actor probe

**28 September Deck crash:** the initial capacity DLL reached Connected then
faulted at `0x84D9C4` through `GetObjectThis` and the new identity callback.
`game/Glue.hpp::MethodSelf` incorrectly called the native custom-ABI helper as
cdecl: the helper requires callback Lua state in ESI. The compiled identity
callback held roster count0 in ESI, exactly matching the captured null-state
read at0x10. The shared helper now takes `state`, sets/preserves ESI explicitly
and passes the class id on the stack with caller cleanup. Both callers pass
their original callback state. Capacity, registrations and policy are unchanged.
Velora `check_glue_abi.py` replays compiled callbacks through the exact native
object/type/index bytes: the installed DLL reproduces the captured fault; fixed
roster and optional probe callbacks pass counts0/10/50. Object-table/output
leaves remain fixture stubs. Native retest is still required; this is not a
rendering or Deck acceptance claim. Evidence/rebuild custody belongs to the
Velora selector's `CAPACITY.md` crash checkpoint.

This opt-in, offline-built experiment controls the existing selector's equipped
actor. It neither creates a model widget nor reconstructs appearance from a
character name. The local Velora owner is
`F:/GitHub/velora/wow/clients/story_select/README.md`; it owns exact binary/source
evidence, candidate composition, loader and the pending native checklist.

`src/client/StorySelect/` owns the bounded performer and single
`ModelFFX:WXLStorySelectProbe(action, token, elapsed)` method. The method only
accepts the frame registered by native `SetCharSelectModelFrame`. `inspect`
returns the selected server GUID as hex, transient roster index and status;
`begin` captures placement and starts one salute/walk/return; `step` requires
the returned generation; `stop` invalidates it and restores native Stand plus
the captured matrix. No network, saved-state or account operation is included.

The build flag is `WXL_STORY_SELECT_TRIAL=ON` (default off); it rejects nonempty
`CLIENT_PATH`. The runtime key `WXL_STORY_SELECT` defaults on only in this trial.
Its Glue control remains manual and Beta-scoped. Four boot hooks append the
method, admit only that exact callback through the existing validation seam,
and restore the actor before native selection changes or roster refresh. Every original is
forwarded. A failed fingerprint or partial hook install adds no method.

The header `src/offsets/game/StorySelect.hpp` owns fingerprints for the inspected
selector paths. `tests/check_tracking_client.py:verify` accepts an explicit
header and label to reuse its existing read-only PE inspector; its default
tracking command is unchanged. `tests/story_select_test.cpp` exercises route
arithmetic, facing, bounded deltas and drift-free return on a synthetic matrix.
Build target: `wxl-story-select-test`; none of these checks proves rendering.

AnimationData.dbc establishes Stand=0, Walk=4 and EmoteSalute=113. The live
actor must contain direct nonalias clips, with exactly one variant for each
action and a finite positive walk speed; unsupported models stay stock.
Salute duration and travel speed come from that model, and the route uses its
captured placement basis for two 0.6-second legs. This is a mechanism test,
not a grounded cathedral/ship route. Stop restores native Stand, not an arbitrary
pre-existing third-party animation override. No competing actor performer was
found in the inspected composition; integration with one would need review.

Never install the standalone core build. Velora's heel-runtime preparation
adds these fixed files to the retained portrait/wardrobe/picking composition
with independently selected capacity, story and tracking flags. Its existing DLL install/recover owner remains
the deployment boundary. No native visual verdict exists for this probe yet.

## Fifty-character capacity

`WXL_CHARACTER_CAPACITY_TRIAL=ON` independently enables the 50-character
native packet limit and `ModelFFX:WXLRosterIdentity(index)`. It does **not**
enable `WXL_STORY_SELECT_TRIAL` or the queued tracking capability. Runtime
`WXL_CHARACTER_CAPACITY` defaults on only in this build. CLIENT_PATH must be empty.

`src/offsets/game/CharacterCapacity.hpp` owns policy50, the exact native decoder
immediate at `0x464C4F` and seven decoder/allocation/bridge fingerprints. After
verification, the shared protect/write/restore helper changes only that in-memory
byte from10 to50. The executable on disk is untouched. The original unsigned
`>maximum` failure path still clears/rejects the roster before decoding entries.
The native connection vector allocates `count*0x188`; Glue appends `0x198` records.
The ten trailing DWORDs in the packet decoder are not character storage.

Identity returns GUID hex, transient roster revision, actual count and admitted
maximum. Index0 returns metadata only. The refresh hook advances the revision
before native automatic selection events, allowing Lua to retain the selected
GUID across reorder without copying rows. One boot registration owns both optional
methods and enables complete hook chains through the existing phase-level batch.
The probe's actor admission accepts the same50 boundary and still verifies its
GUID/model/index generation before any motion. Neither method bypasses native
selection, EnterWorld or confirmation callbacks.

Velora `check_capacity.py` executes exact Beta x86 decoder/vector/bridge/getter
bytes in Unicorn for0/10/11/50 and51 rejection, with packet-reader and allocation
leaves stubbed and allocation canaries. It also proves stock11 rejection.
This is strong allocation/identity evidence, not native rendering, runtime hook,
network, input, creation or Deck acceptance. Publish only the composed Velora
candidate and matching server boundary after the coordinator's bundle review.
