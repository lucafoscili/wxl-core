# Selected actor capability probe

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
Its Glue control remains manual and Beta-scoped. Three boot hooks append the
method, admit only that exact callback through the existing validation seam,
and restore the actor before native selection changes. Every original is
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
together with the tracking trial. Its existing DLL install/recover owner remains
the deployment boundary. No native visual verdict exists for this probe yet.
