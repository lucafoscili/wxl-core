# Native selector residents

`StorySelect.cpp` owns the Glue method, GUID/native-row checks, scoped stock
initializer redirects, loading, lighting and temporary placement. Native rows
continue to own appearance, equipment and cached model memory. `Performer.hpp`
owns clip admission and the bounded G1 Walk route, reused by resident activities.
`Trial.cmake` enables the opt-in build capability.

The existing `WXLStorySelectResidents` method retains its pair `begin`, `step`,
`salute`, `stand`, `stop`, `camera` and `camera-stop` actions. `walk` uses the
existing out/back route; mounted actors remain in Stand when Walk is requested.
`page` takes roster revision, count (1–10), then one tuple per authored page slot:
native index (1–50), 16-digit GUID, lateral, depth, activity (0 Stand / 1 Salute /
2 Walk). Coordinates are finite and bounded to ±6. The selected native row must
belong to the requested page. Slot coordinates are relative to its native origin.

The original eight replies keep their positions. Reply 9 is actual resident
count; reply 10 is the complete authored-order `guid@index;` list. Generation,
revision, selected actor and every row/model are rechecked before stepping.
Stop restores owned lighting and placement, detaches extras and restores native
camera/time before selection, refresh or native initialization proceeds. Missing
Stand refuses the group; optional Salute/Walk fall back to Stand.

Offline checks use `tests/story_resident_fixture.cpp`, compiled against this
translation unit, and `tests/check_story_residents.py`, `check_story_pages.py`
and `check_story_camera.py`. They execute original client row/equipment,
attachment and camera instructions with synthetic model/DBC/composition leaves.
Use their `--client`, `--fixture` and `--output` options. The fixture DLL must
never be installed. Build and package the actual retained composition through
Velora's `wow/outfit-authoring/mesh-workshop/native/heel-runtime/prepare.py`;
the owning checkpoint is `wow/queued/features/story-select/README.md`.

These checks cannot prove equipped rendering, art quality or frame performance.
# Living harbour candidate — 1 October 2026

Fixed pages anchor placement to authored slot zero's native origin/basis, rather
than recentering on the selected actor. Member zero still identifies the actual
selected native row; native selection/entry and cleanup retain their owners.
`scene` shares the page owner with seven values per row: index, GUID, lateral,
depth, activity (Stand/Salute/Walk), initial delay (0–60 seconds) and idle interval
(3–60 seconds). Missing optional clips remain Stand; existing `page` calls retain
their five-value protocol and original default timing.

`camera-hold` shares the admitted camera path but starts and holds its bounded
wide endpoint. It does not replay a zoom on each selection. Stop, rotation,
selection teardown, refresh, hide and entry restore native camera/time before
forwarding. The optional legacy camera trial still expires normally.
`check_story_pages.py` and `check_story_camera.py` execute these contracts through
the compiled owner; visual framing and actual available-roster coverage remain
native acceptance questions.
