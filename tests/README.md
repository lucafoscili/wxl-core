# Focused regression checks

The `wxl-skin-alpha-test` target executes shared production stock/custom paint
routing, caller scopes, queued identity and pixel arithmetic. The
[SkinAlpha checkpoint](../src/client/CharModel/README.md#stock-sheet-scope-correction--2-october-2026)
owns current evidence and the native checklist. Configure Win32 with `CLIENT_PATH`
empty, build only this target and run `Release/wxl-skin-alpha-test.exe`.

The original request seam fixture is isolated and read-only:

```powershell
python -B tests/check_skin_alpha_requests.py --client <12340-Wow.exe> --python-deps <existing-pefile-unicorn-directory> --output <offline-output>/native-request-seams.json
```

It executes native pool reuse/submission, with synthetic gather/lock/signal leaves;
it does not execute installed detours or render a character. It never launches or
attaches to WoW. No standalone core DLL from these checks is an install candidate.

The `wxl-story-select-test` target exercises the shared selector Performer:
clip admission/first-failure diagnostics, zero Walk metadata, the author-chosen
G1 route, bounded/clamped travel and exact return in the captured scaled basis.
`src/client/StorySelect/Performer.hpp` owns that one route (1.5 local units per
0.6-second leg); it is probe choreography, not measured natural locomotion.
The test does not execute native animation, selection callbacks or rendering.
Velora's queued story-select checkpoint owns the native verdict and test batch.

The opt-in two-resident experiment remains in `src/client/StorySelect/StorySelect.cpp`.
`WXL_STORY_RESIDENTS=0` disables its bridge without disabling G1 or capacity.
`tests/story_resident_fixture.cpp` compiles that owner into an emulator-only DLL;
`tests/check_story_residents.py` runs its row redirects and owner against supplied
12340 code. Native equipment/attachment/lighting-prefix code executes, while
model allocation, DBC/composition/render readiness and sequence leaves are
synthetic. This is not rendered equipment or native visual acceptance.

In an x86 Visual Studio developer shell, with pefile and Unicorn on Python's path:

```powershell
cl /nologo /std:c++17 /O2 /Oi /Gy /GS- /LD /MT /EHs-c- /I src tests/story_resident_fixture.cpp /Fo<offline-output>/fixture.obj /Fe<offline-output>/fixture.dll /link /NOENTRY /OPT:REF libucrt.lib
python -B tests/check_story_residents.py --client <12340-Wow.exe> --fixture <offline-output>/fixture.dll --output <offline-output>/check.json
```

The fixture omits CRT startup, security cookies and exception unwinding solely
to execute the selected exports in Unicorn. Production build flags remain intact.
Never load/install this fixture DLL into a client. A prepared composed runtime
and the owning Velora checkpoint are the route to a reviewed native candidate.

The separate [herbs + trivial quests trial](../docs/herb-quests.md) owns its
Win32 policy check and read-only exact-client/collector fixtures. Those fixtures
need supplied client files and are explicit commands, not unittest discovery.

From the repository root, with Python 3 (standard library only):

```sh
python -m unittest discover -s tests -v
```

The buffer-binding checks guard the source call boundary: both index refills
must commit an index binding, while the wide-vertex refill must only unlock its
vertex buffer. They do not execute native WoW APIs or prove renderer behavior.
Build the Win32 DLL and retest the same model in the client for that verdict.

The temporary `m2wide-beta` address experiment logs only skins with more than
65,535 vertices. Process-lifetime caps are 16 shared-index refill records,
16 instance-index refill records, 8 vertex-refill records, and 96 draw records.
Index refill records focus on sections 0 and 1 plus the last three sections
(including the boundary markers) and include the first complete
triangle only (`triangle=0` means the zero sample fields are unavailable).
`groupOrCopy` is the group number for instance refills and copy number for shared
refills; `groupVertexBase` applies only to instance refills. The latter reports
the signed 16-bit accumulator as used by the existing arithmetic.
`refillVertexStart` is the shared submesh copy's start on shared refills and the
original skin section's start on instance refills, matching each fill's operand.
Draw records separately report the actual copied section's `copyVertexStart`.

Match the draw's `boundIb` with the refill's `ib` before interpreting its index
sample. Compare emitted indices plus the observed device base against the source
section's vertex window; a nonzero section start already present in the indices
and added again by the draw is evidence of a doubled base. `override=0`,
`matched=0`, or `wideValid=0` identify unavailable/inactive address correction.
Vertex-refill records compare the buffer's stride and offset before lock and
after unlock to expose relocation. Each record flushes the existing log so it can
be read while the client remains open. Caps reset only with a new process.

The diagnostic source-contract checks guard these caps, context save/restore,
sample bounds, observation placement, and the existing draw/binding boundaries.
They do not execute the C++ hooks, validate native offsets, establish the actual
runtime index mode, or prove that rendering is correct. This is a bounded Beta
experiment; native observation is still required before choosing a correction.

The subsequent global-index gate preserves the native vertex-stream offset when
indices already name model vertices, instead of adding the section start twice.
Its source-contract check verifies that the gate owns the offset-write block;
the logs remain active for bypassed draws. Retest the same 65,536-vertex candidate:
section 1 should keep emitted indices `678,679,680` with `override=0` and unchanged
incoming/imposed offsets. Visual recovery is a separate client verdict.
This correction does not extend the range of uint16 global indices beyond vertex
65,535 or validate the separate local/group-relative path.

The subsequent shared-window candidate handles dense identity skins with more
than 65,536 vertices on the global-index, exactly-one-copy, shader-enabled shared
buffer path. Its fill checks the full dense vertex layout, low-16 starts, lookup
identity, matching source/copy triangle ranges, and each modulo-local index before
rewriting shared indices. The existing 65,536-vertex global path stays unchanged.
No file flags, bone palettes, or per-instance index fills change.

The existing 64-entry wide-skin registry records the model, skin/index identity,
vertex count, and successfully converted shared IB. A shared rebuild clears its
conversion marker before the native fill; failed custom locks or a full registry
do not activate converted draws. Instance-index notes preserve an unchanged
shared conversion. This bounded registry does not recycle entries in this Beta.

A converted draw requires the matching shared IB, the model's actual shared VB,
stride 48, base mode 0, one copy, and a valid section. It adds the section's full
vertex start to the incoming byte offset and temporarily sets local min/max
bounds. Both bounds and offset are restored afterward. An unsupported subsequent
draw of a converted IB is warned once and skipped: a native global descriptor
cannot correctly consume that already-local index payload.
This includes draws outside the existing triangle-batch context, such as an
unhandled pass or doodad draw; those paths are not supported by this candidate.

`windowed=1` on refill records describes the emitted local triangle; the separate
`converted=1` record is emitted after commit. Draw records report `converted` and
`windowed` together with the actual offset and local bounds. For the 65,539 probe,
the final marker's raw `(0,1,2)` must address vertices 65,536 through 65,538 with a
section base of 65,536, while the earlier section-1 raw `(678,679,680)` becomes
local `(0,1,2)` with base 678. Compare the matching IB records and inspect the
rendered markers; source-contract checks alone cannot establish that result.

The actual C++ window arithmetic checks are a separate explicit target:

```powershell
cmake --build build --config Release --target wxl-vertex-window-test
./build/Release/wxl-vertex-window-test.exe
```

This candidate does not add multi-copy/doodad, per-instance/local-group,
CPU-skinned rendering, or extension-provided vertex-buffer support. The picking
repair targets only a current, validated shared-window conversion, not every
skin with a large index array. A noted skin that no current certificate admits
(every skin of at most 65,536 vertices with wide triangle starts, such as the
index-only Virna, Lisa and BG3 skins, and any larger skin whose admission fails)
gets a legacy frame instead. It is matched through the live instance's skin
against the note's address and size keys; the note's skin pointer is never
followed. Its filler call records the exact section from the frame's own skin,
read only once the instance is shown to walk that skin, so the recorded section
and the index array its triangle call plans against are one skin by
construction. The triangle call keeps the two older repairs for that section's
exact stock arguments: the 66a64d5 triangle-start remap, with the incoming vertex
base unchanged, and the e9c68f6 crossing-section skip, with the same one-time
`m2native-indices: picking skips section` warning. A legacy frame never repairs
positions or chunks indices, so a wholly-above section of an uncertified skin
still picks against positions read through the 16-bit lookup. Unnoted skins, and
collision's own triangle call, run outside every geometry scope and reach the
next link unchanged.

The bridge scopes its context to the native geometry call and observes all three
native fillers. A fully prepared, certified crossing or wholly-above section uses
the existing dense CPU fill from the current bone palette as its sole fill. Native
geometry allocates its scratch positions before invoking the filler; the stock
fillers only write those positions, so their redundant truncated-lookup fill can
be skipped. Every low, legacy, unnoted or rejected fill forwards its original
arguments. The filler identifies the exact section, not the first section sharing
a truncated range. Native filters, allocation and triangle arbitration remain in
charge. This changes the successful upper-section fill-chain contract: a future
custom downstream filler with additional side effects must reconcile them before
joining this chain. The installed physics02 composition has no such filler.
A 3,072-index stack block in a separate non-inlined function submits complete local triangles with vertex base zero,
carrying currentHit and bestDepth through every chunk. No patch-owned heap
allocation is added. The chunk loop works from a snapshot of the section, triangle
start and skin taken at entry, and stops, keeping the native prefix result, if the
certificate or the frame's preparation epoch changes between chunks. An admitted
section with no indices or no vertices forwards its original call without a
warning. Any other unsupported call inside an admitted wide scope warns once per
process and never invents a hit or updates depth: a low section identified by its
filler is forwarded through the triangle-start remap with its own vertex base,
while a crossing or wholly-above section, or one that was not identified, keeps
the native no-hit result.

`test_wide_indices_picking.py` pins the gate, lifetime, argument forwarding,
section identity, native blend selection, bounds, chunking and logging. The
one-fill, one-triangle handshake both frame kinds depend on is pinned as whole
normalised bodies: the fill hook (clear pending, try the exact upper fill, return
only after complete preparation; otherwise disarm, forward and record),
the legacy section record, the start of the admitted record, the triangle hook
(read and then clear the pending fill before either frame kind is dispatched, the
full admitted match, then the empty, ready and reject dispatch), the legacy
triangle call and the reject branch. So are the predicates those bodies rely on:
the fill and triangle frame lookups (every key one conjunction), the geometry
hook (the live skin is followed only on the legacy path that set it), the legacy
frame's live skin, note match, note lookup and currency gate, and every picking
warning and record (each warning's once flag, each record's per-note bit and the
shared 24-record cap). These are text contracts: a changed body fails them, but
they do not execute the hooks. The C++ window target additionally
exercises real production modulo arithmetic, all 21,846 complete u16 section
counts, 393,216 additional wrapped index samples, array-slot membership,
bone-reference bounds, simple projection cases and the legacy-frame plan. That
plan covers the triangle-start fold, the exact stock-call match shared with
admitted frames, the crossing skip and the remap target. Its fixtures are
VirnaAoA00 (27,731 vertices; sections 2 and 3 are remapped to 66,036 and 126,036)
and Kasumi sections 59 (skipped) and 60 (remapped). Every single-argument change
to each stock call, and every other section's call, is forwarded unchanged. Neither
source contracts nor these arithmetic checks execute WoW or prove ABI, hook-chain,
pose, floating-point or picking behavior. For a portable arithmetic-only run:

```sh
c++ -std=c++17 -Isrc tests/vertex_window_test.cpp -o vertex-window-test
./vertex-window-test
```

`m2wide-beta: picking` samples at most once per low/crossing/above class per note,
under a separate 24-record process cap. `positionRepair=1` means dense positions
were submitted, not that the section was hit. `filler=0/1/2` identifies the
SSE/scalar/single-bone entry actually observed. Repeated batches and the second
native mode can test a section more than once. Restart for another capped log
sample. Under the same cap, `m2wide-beta: picking-reject` samples each reject
reason once per note: `reason` 1 certificate, mode, projection or distance at the
fill; 2 section pointer; 3 filler pairing, placement or vertex bounds; 4 triangle
range; 5 scratch or bone palette; 6 triangle arguments or a missing fill; 7 an
index outside its window; 8 a change between chunks. `remapped=1` means a low
section was forwarded through the remap. `m2wide-beta: picking-legacy` is written
once per note when a skin above 65,536 vertices picks through a legacy frame;
`certificate=1` means a conversion was published but does not match the live
instance. The one-time reject warning now names the first `reason`. The
installation line ends `unadmitted noted skins keep the triangle-start remap and
skip crossing sections`. Thread affinity and same-scene reentrancy must be
checked locally; the scoped registry link is not a lock or an asset-lifetime
pin. Native hover, wrong-place selection, movement, occlusion and rendering
remain separate checks.

### Picking fill performance - 5 October 2026

`picking_benchmark.py` exports read-only M2/SKIN geometry and exact x86 code from
12340 `Wow.exe`. Its manifest records the executable, model, skin, native capture
and generated production-body hashes. `--installed-client` resolves Tifa/Elene's
named Beta target archives through Velora's existing StormLib owner. It does not
launch or change the client. From this checkout and an x86 MSVC developer prompt:

```powershell
python -B tests/picking_benchmark.py --client <Wow.exe> --assets <velora/wow/_ssot> --installed-client <Beta-client> --output <scratch>
cl /nologo /O2 /EHsc /std:c++17 /Isrc /I<scratch> tests/picking_benchmark.cpp /Fe:<scratch>/picking_benchmark.exe
<scratch>/picking_benchmark.exe <scratch> installed-tifa installed-elene tifa kasumi ayane shadowheart luna human-female
```

The fixture executes the captured SSE/scalar/single-bone fillers, blend helpers,
affine transform and triangle predicate. Only external addresses are relocated;
native instruction bytes otherwise stay intact. It compiles the actual production
fill hook, section preparation and dense refill bodies generated from
`WideIndices.cpp`; doubles supply the scene/model boundary and certificate result.
This is not a complete client ABI or installed-hook test.

Eight meshes pass 2,056 fill comparisons over both projection modes, all three
filler entries, and identity plus a deterministic animated palette with varying
bone translations and noncommuting rotations. Successful certified upper fills
make no native filler call and produce byte-identical final positions and native hit/depth results, including a
prior nearer hit. Low/index-only and legacy fills forward. Rejected certificate,
section, mode, projection, distance, empty/malformed section, insufficient scratch,
and missing/misaligned/invalid bone-palette cases also forward exact arguments,
with the pending handshake disarmed during that call. The source contracts still
pin the real currency predicates and triangle dispatch rather than the doubles.

The measured duplicate work is the stock upper-section fill whose positions the
existing wide repair immediately replaces: 91,305 vertices for installed Tifa
(149,674 vertices / 182,078 triangles), and 39,188 for installed Elene
(94,817 / 111,932). With identity bone palettes and 100 passes, their composed fill
loops measured 2.596 -> 1.594 ms and 0.912 -> 0.620 ms respectively. The unchanged
native triangle kernels measured 1.416 ms and 1.021 ms. These timings exclude
geometry dispatch, admission/index preparation, triangle arbitration overhead
and live animated palettes; they are
offline component CPU costs, not measured game FPS. Skins below 65,537 vertices,
and upper skins without a current admission certificate, receive no saving.

A conservative triangle-bound filter remains a rejected benchmark experiment:
10,416 finite point/mask comparisons agreed with the native predicate, but timings
were slower or inconsistent. It is not in the runtime. This limited experiment
makes no equivalence claim for every nonfinite/degenerate native input. The sole
fill change does not change native triangle decisions, visibility filtering,
chunking, nearest-depth arbitration or the existing >65K crash protection.
Native acceptance and the broader low-vertex hover cost remain open evidence
questions for the delivery owner.


### Complete warm geometry dispatch - 5 October 2026

The same exporter now captures native geometry at `0x81DAF0` and the upstream
`EndHitTest` listing at `0x81DF10`, preserving their hashes in the manifest. The
fixture's `--dispatch` mode executes the captured geometry dispatcher through
actual generated registry, current-source/current-call, frame, section preparation,
fill and triangle-hook bodies. The parsed skin struct is generated from the SDK
owner, with its native size/section offset asserted. Logging output, native calls
and the CPU-feature address are boundary redirects; the actual logging budget,
visibility/material filters and hook decisions remain in the compiled bodies.
Scratch is preallocated to 65,536 positions; reaching the unavailable native
allocator aborts the fixture. Thus these are **complete warm geometry-call** costs,
not cold allocation, broad-phase scene search, actual game FPS or a whole-frame
profile. Shared-conversion admission scans immutable topology on rebuild, rather
than on each geometry call, and is not part of this warm timer.

```powershell
<scratch>/picking_benchmark.exe <scratch> --dispatch installed-tifa installed-elene tifa kasumi ayane shadowheart luna human-female
```

The executable runs five native filter checks per mesh (hidden sections, global
alpha zero, no-pick, nonzero material layer and disabled material visibility),
requiring no filler/triangle calls and preserving an existing hit/depth. Every
large fixture explicitly requires the real `PickingNote` and
`CurrentPickingSource` to admit its live header/skin/IB/source identities. A
rebuild during the first chunk preserves exactly the captured kernel's genuine
prefix; a changed preparation epoch stops the section at the same prefix. The
geometry scope restores its predecessor link. Timings use all visible material
alpha one, a synthetic fixed camera-space bone palette/view, mode zero, a fixed
point and 200 calls. This setup does not represent live Deck visibility or poses. Comparing one versus 64 registry entries bounds registry search cost.
The kernel-disabled run still executes all native dispatch, live position fill,
validation, index rebasing and currency checks; only the triangle predicate is
replaced by a no-op that preserves the current hit. A separate actual
`TestPickingSection` loop with that no-op measures the admitted topology path.

| Mesh | Warm geometry | Without triangle kernel | Validation/rebase/currency only |
| --- | ---: | ---: | ---: |
| Installed Tifa | 3.010 ms | 1.794 ms | 0.129 ms |
| Installed Elene | 1.622 ms | 0.689 ms | 0.079 ms |
| Kasumi | 1.048 ms | 0.395 ms | legacy path |
| Ayane | 0.894 ms | 0.327 ms | legacy path |
| Shadowheart | 0.970 ms | 0.356 ms | legacy path |
| Stock HumanFemale | 0.106 ms | 0.042 ms | legacy path |

All eight inputs have exactly one layer-zero, pickable batch per section;
there is no repeated section fill to remove inside a geometry call. Registry
capacity adds only small/noisy differences. The admitted topology work is about
4-5% of total geometry time; removing it cannot address the dominant crossover
cost. The first diagnostic high-mesh pass used legacy dispatch because its
boundary header count was omitted; explicit admission assertions caught that
fixture error. The table contains the corrected admitted pass.

`EndHitTest` calls geometry once per admitted scene candidate in mode zero. When
no hit is returned and the caller permits it, it repeats candidates in mode one
(normal-expanded surfaces). It stops after a genuine hit. Misses can therefore
legitimately cost two geometry passes. Actual broad-phase candidate multiplicity
and retry frequency are unmeasured here. The fixed-point timer measures per-call
work; it does not reproduce the reported worst case of walking/running while
hovering, nor establish a thermal or FPS delta.

**Defer another runtime edit.** Dominant work is current-palette skinning and
native triangle testing. A next exact-algorithm question is whether a conservative
animated section bound can reject a ray before skinning, without losing any real
hit. The SDK exposes current model/split-body/region bounds, but no verified
contract maps them to every SKIN section, including custom weights, garments and
procedural physics. Static section/model bounds do not establish that animated
coverage. Verify that contract before using an existing bound. If no such bound
exists, a live projected section bound after filling is a narrower experiment:
it can reduce triangle work while retaining fresh positions, but still needs
conservative floating-point/fallback correctness and a measured benefit. A
lower-detail picking proxy is a separate content/interaction decision: it changes
selectable silhouette/detail and needs explicit behavior acceptance; it is not an
exact optimization of the current picker. No new bounds, cache, proxy or throttle
is implemented by this diagnostic.


### Live section-bound experiment - 5 October 2026

**DROP.** `--bounds` is a benchmark-only experiment; no runtime, DLL or installed
behavior changes. It scans positions just filled for the current section and
forms a padded projected bound. Only exact identified stock section calls can
reject. Mode one, nonfinite values, nonpositive/near-plane depth, invalid local
indices, degenerate triangles and strongly ill-conditioned triangles retain the
original hook. A deliberately strict triangle condition check protects the AABB
argument from the captured kernel's x87 area epsilon and float barycentric spill.
This is a limited experiment, not proof of equivalence for every native float
input. No cached pose, hit, topology, proxy or new allocation is used.

```powershell
<scratch>/picking_benchmark.exe <scratch> --bounds installed-tifa installed-elene tifa kasumi ayane shadowheart luna human-female
```

Use a fresh named output for this experiment; retain the preceding dispatch
benchmark outputs unchanged. Six point heights, including hits and misses beyond
the model extent, are timed over 20 passes. Both before/after use the same captured
native dispatcher and kernel with the same live palette. The synthetic view and
visibility/material setup have the same limits as the warm-dispatch benchmark.

| Mesh | Original dispatcher | Bound experiment |
| --- | ---: | ---: |
| Installed Tifa | 3.032 ms | 3.303 ms |
| Installed Elene | 1.711 ms | 1.851 ms |
| Kasumi | 1.083 ms | 1.253 ms |
| Ayane | 0.897 ms | 1.087 ms |
| Shadowheart | 0.991 ms | 1.147 ms |
| Luna source | 1.361 ms | 1.509 ms |

Tifa rejected 44 sections across the six points, Kasumi 12; the other listed
meshes rejected none under the uncertainty gate. Scanning positions and checking
uncertainty erased any saving. A second focused Tifa pass was also slower
(3.012 -> 3.246 ms). The experiment agreed with original hit/depth results for
154 point/prior-hit cases per mesh across eight meshes. Tifa and stock HumanFemale
additionally pass 16 animated-palette/partial-visibility/prior-hit cases each;
small direct fixtures require the bound to defer for edge padding, NaN/infinity,
near-plane/behind, degenerate/skinny and malformed-index inputs. Correctness gates
were not loosened to obtain a speedup. Do not promote this helper into the runtime.

The next useful live evidence is geometry-call/candidate and mode-one retry
frequency while walking plus hover, compared with walking plus cursor-away. The
existing `hkHitTestGeometry` / `RunPickingGeometry` owner already sees each scoped
call and its mode. Its current picking logs are capped class/rejection samples,
not frequency/timing counters. A bounded sample there could distinguish movement
multiplying geometry checks from this fixed per-call cost exhausting the frame
budget. No instrumentation is added in this experiment. A verified conservative
bound before expensive skinning remains a separate next evidence question; no
picking-proxy behavior decision is inferred.


### Passive hover throttle boundary - 5 October 2026

**Historical investigation; superseded by the opt-in trial below.** A bounded read of the same 12340
client found two direct native origins of `PickAtScreen` (`0x4F9DA0`):

| Origin | Observed behavior |
| --- | --- |
| Frame update `0x4FA040`, call `0x4FA13A` / return `0x4FA13F` | Takes a frame delta, checks input ownership against the world frame, reads live normalized cursor coordinates, then picks into a local result with mode **1**. This is the available passive-hover origin. |
| `SetupDefaultAction` `0x4FA570`, call `0x4FA5C0` / return `0x4FA5C5` | Refreshes live cursor coordinates into frame `0x310/0x314`, picks with mode **0** into frame `0x2E0`, and records the result type at `0x2D8`. Direct action-setup callers occur at `0x5FC63F`, `0x5FC6EF`, `0x5FC7B3`, and `0x5FC7F3`. |

The generated read-only listing is
`F:/GitHub/velora-codex-hover-performance/wow/queued/features/crossover-hover-performance/caller-evidence/world-pick-callers.txt`
(SHA256 `2cc42c2bb40eb0d87ac11d3cb70a93f3469a9218e2b88fa9e59d825ac9c8938a`).
It includes client path/hash, dumpbin tool and captured address ranges; client
SHA256 is `1df8ba4be431b5396a27680e44d287c6e364b6ba34b97599deadb18541737ef6`.
This is instruction evidence, not a live call-frequency measurement or proof of
every indirect caller/action consumer. The SDK mode-zero comment was corrected;
its constant, value and behavior remain unchanged.

The existing [input owner](../src/engine/input/Input.cpp) calls
[PickCursor](../src/game/Pick.hpp) afresh for each unconsumed left/right button
down before publishing `OnWorldClick`. This supports keeping explicit picks
unthrottled, but does not establish freshness for all native actions or macros.
[MouseoverGuid](../src/game/World.hpp) simply reads the cached global GUID at
`0xBD07A0`; an `@mouseover` action could consume that state without going through
the observed default-action setup. That consumer boundary remains unknown.

The proposed initial policy is an immediate first eligible passive pick, then
repeated passive refreshes at most once per 100 ms (10 Hz). Gate only the proven
passive origin, preserving the rest of frame/UI update work and leaving unknown
query contexts unthrottled. Every actual refresh still runs the native picker
with current pose, visibility, distance and hit arbitration. Reuse only bounded
presentation/target state, not cached geometry hits or model pointers. Clear or
invalidate retained presentation when its target disappears or world/input
eligibility changes; restart the leading edge when passive hover resumes.

Before any action, including mouseover-target macros, the policy requires a fresh
full pick **and the correct native mouseover-state update** at its consumer
boundary. Refresh-on-click alone is insufficient, and a fresh WXL `WorldHit`
does not prove that the cached mouseover GUID was updated. Locating this action
boundary is the remaining implementation dependency. Switching targets, cursor
movement or animation inside the 100 ms window can briefly retain an old
highlight/tooltip; this policy does not promise instant detection of every new
target. It deliberately accepts that visual age while requiring fresh action
selection. No performance saving or live behavior has been measured for it, and
no throttle, build or installation has been produced by this investigation.

#### Bounded action-consumer follow-up

Luca subsequently accepted the brief stale-highlight tradeoff for a reversible
Beta trial. Implementation still depends on a correct action refresh; this is
a technical gap, not another approval requirement. The additional read-only
12340 capture is [native_mouseover_boundary.txt](native_mouseover_boundary.txt).
The original client hash above was rechecked and matched. These are instruction
and registration-table observations, not an executed client ABI fixture.

The eleven direct references to the low cached GUID word were classified:

| Instruction | Observed role |
| --- | --- |
| `0x404C4E` | Object diagnostic output (`Current Object Track`); reads cached selection. |
| `0x513D12` | Refreshes highlighting of existing mouseover/target objects; does not select an action target. |
| `0x51F7CC` | Prior-GUID read in native mouseover publication. |
| `0x51F7EB`, `0x51F82D` | Clears old GUID / writes new GUID in that publisher. |
| `0x5243D8` | Object-disappearance invalidation; clears matching hover through the world-frame setter. |
| `0x527F4D` | **Action consumer:** Lua `InteractUnit`, with a direct `mouseover` branch. |
| `0x60AED6` | **Selection consumer:** shared unit-token resolver's `mouseover` branch; includes suffixed-unit handling. Its callers still need action-versus-presentation coverage. |
| `0x60B3EC`, `0x60B692`, `0x60BB38` | GUID-to-unit-token representation; comparisons can return the `mouseover` literal. These instructions do not perform picking or actions. |

`InteractUnit` is registered at `0xAC8350` as name pointer `0x9FE01C`, callback
`0x527F00`. It compares its argument with the literal at `0xA02E6C`, reads both
cached GUID words at `0x527F4D` / `0x527F52`, and passes the result toward native
interaction at `0x527F9E`. It bypasses the shared token resolver (`0x60ABF0`),
whose matching prefix branch begins at `0x60AEC2` and reads at `0x60AED6`.
Two consumer hooks invoking one refresh owner are a plausible implementation;
the second consumer alone does not make the trial infeasible. Refreshing on every
mouseover-token query could also bypass the passive saving when UI scripts poll
it frequently. That frequency remains unmeasured.

The world-frame setter at `0x4F5980` takes the new GUID as two stack words
(`__thiscall`, `ret 8`). It returns early when frame `0x2C8/0x2CC` already matches;
otherwise it passes `{newLo,newHi,oldLo,oldHi}` to `0x51FB60`, which calls
`0x51F790` (`__cdecl`, four stack words). The publisher requires an active player,
clears/publishes `0xBD07A0/0xBD07A4`, and applies highlight and UI side effects.
Raw global writes would bypass this owner. Calling it with every picked GUID is
also insufficient: native frame dispatch at `0x4FA260` first chooses among
miss/targeting/object paths; its type-3 branch checks object kind and additional
eligibility before choosing the native helper. `0x4F8190` itself checks cursor
suppression (`frame+0x31C`, bit 1). A fresh hit is not that dispatch result.

**Precise remaining gap:** no callable hover-only pick-and-publish entry was
established. The observed dispatch is an internal frame continuation, requiring
EBX=frame, EBP locals for hit/type and a matching prologue/epilogue. Replaying the
whole frame update is not a safe shortcut: before picking, `0x4FA052` calls
base-frame update `0x490770`; at `0x490787..0x4907A5` that entry pushes the delta
onto Lua's stack and invokes a frame script handler, then visits child callbacks.
An action refresh could therefore invoke another action before publishing the
new target. A reentrancy guard alone would let that nested consumer see stale
state; zero delta does not suppress these calls.

Keep runtime behavior unchanged. The next bounded implementation question is a
verified native hover-only continuation with an executable ABI/reentrancy fixture,
or an evidence-backed shared refresh that retains native eligibility and publisher
semantics for all hit types. Extend the existing exact-client offline fixture
conventions: drive the proposed continuation with a fresh hit through all four
native dispatch branches and cursor/eligibility loss, asserting stack/register
restoration and new GUID publication before a nested action. This is one focused
fixture for the missing boundary, not a new test framework.
This follow-up added no policy implementation,
benchmark, DLL, installation or live test, and changes none of the large-model
picking, native fallback or physics paths. Existing benchmark build instructions
above still apply; there is no hover-throttle candidate to compose or install.

#### Executable world-only continuation

The follow-up [fixture](hover_refresh_fixture.cpp), exercised by
[check_hover_refresh.py](check_hover_refresh.py), passes 15 exact-client cases.
It supplies the original x86 frame prologue, enters native hover at `0x4FA05C`,
and identifies its supplemental call by return address. A tail detour at
`0x4FA32E` uses the native epilogue at `0x4FA368` for that supplemental call;
ordinary frame calls retain their original trampoline and frame/UI suffix.
The supplemental wrapper checks world input ownership before entering, so it
never replays the Lua-producing UI-unit branch. Normal native UI processing is
still exercised, including its fresh unit-token publication.

All four native hit-type dispatch branches, suppression, object disappearance,
special-object kind/owner rejection, null world/input/ownership and UI ownership
are covered. The native setter/publisher execute; immediately after GUID
publication the fixture executes native `InteractUnit("mouseover")` on a nested
stack and asserts that native interaction receives the published GUID. Each
case checks callee-saved registers and exact stack return. An ordinary frame
case verifies that frame prefix/suffix are retained.

The first input-loss assertion expected native GUID clearing and failed. Native
processing deliberately keeps UI-owned state when no world pick is eligible;
the corrected contract expires **our** retained world-hover state while keeping
ordinary native UI semantics. A supplemental UI refresh would introduce Lua
recursion before publication and is excluded by the ownership guard.

Geometry, object residency, cursor/tooltip leaves and Lua query leaves are
synthetic. These cases prove the scoped continuation/dispatch ABI and ordering,
not rendered selection, actual script callback behavior or live performance.
Dependencies are task-local `pefile 2024.8.26` and `unicorn 2.1.4`.

In an x86 Visual Studio developer shell:

```powershell
cl /nologo /std:c++20 /O2 /Oi /GS- /LD /MT /EHs-c- /I src tests/hover_refresh_fixture.cpp /Fo<scratch>/hover-refresh.obj /Fe<scratch>/hover-refresh.dll /link /NOENTRY /OPT:REF /FIXED /BASE:0x10000000
python -B tests/check_hover_refresh.py --client <12340-Wow.exe> --fixture <scratch>/hover-refresh.dll --python-deps <scratch>/python-deps --output <scratch>/continuation-check.json
```

#### Opt-in passive world-hover trial

**Delivered experiment failed its live verdict; do not redeploy this policy.**
The later regression investigation below supersedes the initial offline verdict.
[HoverPicking.cpp](../src/engine/input/HoverPicking.cpp) owns the one shared
refresh policy. Build with `WXL_PASSIVE_HOVER_TRIAL=ON` (default OFF); the existing
composed-runtime producer forwards `--passive-hover-trial` to that option.
`CLIENT_PATH` must be empty, so this switch cannot trigger the build's automatic
client copy. The standalone checked build was:

```powershell
cmake -S . -B build-hover-evidence/core -G "Visual Studio 17 2022" -A Win32 -DCLIENT_PATH:PATH= -DWXL_PASSIVE_HOVER_TRIAL:BOOL=ON
cmake --build build-hover-evidence/core --config Release --target WarcraftXL --parallel 8
```

Only mode 1 returning to `0x4FA13F` in the world-owned frame is eligible. Its
first object hit is immediate; subsequent retained object values can last less
than 100 ms. At the boundary the full native picker runs again. Unsigned elapsed
time handles timer wrap. Misses, terrain, unknown origins/modes, targeting and
cursor suppression stay fresh. Each reuse requires the same frame/input context,
matching published native GUID and current object residency. Retained state holds
GUID/ray values and GUI context identity, never geometry/object/skin pointers.
Native dispatch and publication still execute each normal frame.

Input/focus/world/object-destruction loss expires the retained world result.
Movement intentionally permits the approved brief visual age. Shared
`mouseover` token resolution (including native prefixes/suffixes) and
`InteractUnit` invoke one supplemental native pick/dispatch/publication before
their consumers; button-down refresh runs before input subscribers. A TLS guard
allows a nested consumer only after native publication, without recursive picks.
Supplemental refresh checks world ownership and does not replay UI Lua. Ordinary
UI processing and vanilla retained GUID behavior continue on ownership loss;
only our cache expires. Existing native picker fallback, >65K repair and physics
source are unchanged.

The fixture now includes the production owner verbatim and patches the exact
native entry points with equivalent offline trampolines. **34 cases pass**:
the original 15 continuation/dispatch cases plus compiled policy checks for the
100 ms boundary/wrap, both consumer hooks and nested actions, button/key/focus
input, cursor movement, world/UI/input-context/load/target eligibility loss,
target disappearance, external GUID change, unknown callers and uncached misses.
Stack and callee-saved registers are checked on normal frame calls as well as the
continuation. World-event coverage invokes the owner's invalidation operation;
event delivery and MinHook installation itself are not emulated.

The fixture's bounded cadence comparison drives 100 frames at 10 ms intervals:
stable eligible passive hover makes **10** full synthetic pick calls versus
**100** native passive calls. With a `mouseover` token query each frame it makes
**101** (one initial passive plus 100 forced queries). Each fresh eligible action
result seeds the passive interval, while even same-timestamp actions always pick
afresh. Presentation polling can therefore erase the saving; repeated queries
within a frame can still increase pick work. These are dispatch counts with synthetic
geometry, not elapsed CPU, rendered hit accuracy, Deck FPS or a net performance
claim. Actual Lua callback behavior and the visible highlight/action experience
need the composed Beta play test.

Replay used pristine client hash above, Python 3.11, and task-local dependencies
at `F:/GitHub/wxl-core-hover-performance/build-hover-evidence/python-deps`:
`pefile 2024.8.26`, `unicorn 2.1.4`. The fixture explicitly initializes x86 TLS.
Its `/NOENTRY` DLL link warns LNK4210 about CRT initializers; it is emulator-only,
and no DLL loader or CRT initializer is claimed by that fixture. Production DLL
uses the normal build/loader. Generated report is
`build-hover-evidence/continuation-check.json`, with exact cases and dependency
versions. The 34 existing source contracts also pass. No installation was run.

The fixture DLL is never an install candidate. The failed runtime trial was
composed through the existing runtime producer; it must not be redeployed.

#### Failed live trial: presentation amplification and repeated publication

Luca reported improved temperature but severe hover stalls (FPS around 10), and
clarified that the tooltip symptom is **one tooltip with stacking information**,
not two boxes. The [Velora hover checkpoint](../../velora/wow/queued/features/crossover-hover-performance/README.md)
owns the live verdict and recovery. No replacement runtime is produced here.
The previous offline ABI/correctness passes did not establish gameplay or callback
safety: their object header kind `0x200` bypassed native unit presentation, and
their nested consumer ran inside a forced refresh with its TLS guard already set.

The existing exact-client fixture now executes six additional baseline/trial
comparisons (**40 total cases pass**) against the unchanged compiled production
owner. The ordinary shared resolver handles presentation reads as well as actions;
it cannot identify intent from the `mouseover` token. The actual hooks and native
hover continuation show this work amplification over 100 frames at 10 ms:

| Presentation reads/frame | Baseline full picks / hover dispatches | Trial full picks / hover dispatches |
| --- | --- | --- |
| 1 | 100 / 100 | 101 / 200 |
| 4 | 100 / 100 | 401 / 500 |

Even re-seeding passive values cannot combine independent forced resolver reads.
Every such read runs the full synchronous picker and then native cursor/selection
dispatch; these extra dispatches also execute cursor reset in this fixture.
There is no cheaper query-only path inside `Refresh()`. The 100 ms interval bounds
only passive reuse, not resolver work. A genuinely expensive full pick remains
one synchronous burst whenever the interval expires. This accounts for ways to
increase or retain stalls; it does not measure Luca's real query frequency or
prove the duration/cause of his approximately 10 FPS frames.

The unit case uses native object-header kind **9**, dispatch table `0x4F837C`
(`0x4F82C6 -> 0x4F7A50`) and publisher table `0x51F9C4` (`0x51F889`). Its ordinary
frame setter `0x4F5980` calls publisher `0x51F790` through `0x51FB60`. The publisher
writes global GUID at `0x51F82D/0x51F832`, then performs native unit presentation
selection (`0x621070`) and emits event **0x142** via the real `0x81B530` wrapper.
Only after the publisher returns does the setter commit the frame GUID at
`0x4F59C8/0x4F59CF`.

At the event's Lua-dispatch leaf (`0x81AC90`), the fixture models an ordinary
presentation callback reading `mouseover`. Baseline reads the newly published
global once and does not pick. Trial `ResolveHook` calls `Refresh()` while
`g_refreshing` is false: ordinary native publication did not enter that guard.
The nested fresh pick finds the same unit; the frame still holds the old GUID,
so its setter republishes the same new GUID. **Baseline has one pick, one native
unit presentation selection and one event; trial has two of each.** The guard
limits deeper recursion during the supplemental call, but does not prevent this
first duplicate. Stack/register checks still pass: this is an ordering/side-effect
defect, not an ABI return failure.

The unit cursor helper and publisher execute native instructions; synthetic
cursor state leaves choose a simple inactive-unit path. Lua callback behavior is
explicitly injected, and tooltip rendering is not implemented. Thus repeated
native publication is demonstrated under a concrete presentation read, while
actual stacked tooltip content remains Luca's observed symptom rather than a
rendered fixture result. Native kind 17 skips this unit-event branch, but custom
model identity does not establish its object's native kind; no stock/custom
character cause is inferred from that branch difference.

Input adds another synchronous refresh before subscribers on button-down; the
existing unhandled world-click pick and native default-action pick remain fresh.
This can add click work, but it is not a demonstrated cause of passive hovering.
No interval change fixes the broad resolver or ordinary-publication reentry.

**Smallest sound next experiment, offline first:** use these same cases to test
one scoped publication guard around the native setter, preserving action freshness
and asserting one unit event through the presentation callback. Separately replace
the broad resolver refresh with a proven action dispatch context, so read-only
tooltip/unit queries never trigger picks. The missing evidence is the concrete
native spell/macro dispatch boundary that covers `@mouseover` resolution; removing
the resolver hook without that evidence would restore stale action targeting.
Keep the direct `InteractUnit` consumer and existing action coverage. Do not ship
the publication guard alone as a performance fix: four presentation reads would
still force 401 picks. A candidate needs both the single-publication check and
baseline-equivalent read-only pick counts before another live trial is useful.

Replay unchanged fixture DLL with the earlier command, setting output to
`build-hover-evidence/failed-live-diagnosis.json`; dependencies remain the task-local
path and versions above. The report names baseline/trial scenarios, counts native
side-effect calls and retains all earlier cases. No runtime source, installed DLL,
client state or live capture/input was changed for this investigation.

#### Publication guard and action-only refresh successor — 5 October 2026

**Offline candidate; default OFF, not installed or accepted.** The same
`HoverPicking.cpp` owner now scopes refresh suppression to the native setter
`0x4F5980` until its frame-GUID commit returns. The setter and publisher still
execute once normally; no GUID write, event suppression or replacement publisher
was introduced. A nested action at the native unit event sees the already
published target without recursively picking or notifying it again.

The resolver hook refreshes only mouseover arguments at three exact native
action return addresses. Every other resolver caller forwards without a pick:

| Registered action | Native resolver call → return |
| --- | --- |
| `CastSpellByID` (`0x53E060`) | `0x53E0CB → 0x53E0D0` |
| `CastSpellByName` (`0x540310`) | `0x54037B → 0x540380` |
| `UseAction` (`0x5AC000`) | `0x5AC043 → 0x5AC048` |

The shared native macro executor `0x564DB0` refreshes before its first line is
dispatched as event `0x17F`, so macro conditionals start from current world
selection without making `SecureCmdOptionParse` or tooltip reads refresh paths.
It is reached by `RunMacroText` at `0x56646D`, `RunMacro` at `0x566E9F`, and
the macro-slot wrapper at `0x566DDB`. Native `UseAction` reaches that wrapper
at `0x5ABDBC`; the fourth direct executor call is `0x563336`. The existing
direct `InteractUnit` and mouse-button refresh remain. Each macro execution
refreshes even when its text has no mouseover; it does not parse or rewrite text.
Spell execution within a macro can perform another fresh action pick.

The existing fixture pins all five action registrations and these call bytes
against the same exact 12340 executable. **47 compiled/native cases pass**:
native spell-by-name/id and action-slot functions consume the freshly published
GUID; native RunMacro/Text/slot functions enter the shared executor and dispatch
their first line after publication; nested unit-event reads, interaction and
macro execution each produce one pick/notification and restore the stack and
callee-saved registers. Existing type dispatch, eligibility loss, input/UI,
native fallback, 100 ms/wrap and fresh-action cache-seeding cases still pass.

| Ordinary reads/frame over 100 frames | Baseline picks/dispatches | Successor picks/dispatches |
| --- | --- | --- |
| 1 | 100 / 100 | 10 / 100 |
| 4 | 100 / 100 | 10 / 100 |

Replay with the earlier fixture command, using `build-hover-evidence/hover-action.dll`
and report `build-hover-evidence/hover-action-check.json`. The opt-in Win32
`WarcraftXL` target also builds with empty `CLIENT_PATH`; this standalone DLL is
compile evidence, not a retained Velora composition or install candidate.
No physics, equipment, capacity, geometry, interval or default-enable change.

Stock FrameXML corroborates the dispatch route: `SecureTemplates.lua` from
`patch-enUS-2.MPQ` (SHA256 `18e855a2…`) invokes CastSpellByName at line 339 and
RunMacro/Text at 373/378; `ChatFrame.lua` from `patch-enUS-3.MPQ` (SHA256
`2506a6ec…`) parses `/cast` options then calls CastSpellByName at 1029–1036.
Those are read-only archive observations, not an executed Lua test. Geometry,
object residency, cursor leaves, Lua arguments, macro splitting/line event and
spell execution leaves remain synthetic. Real addon callbacks outside the
post-publication unit event, hook installation, macro outcomes, rendered tooltip,
CPU/FPS/thermal behavior and the duration of each actual pick need native review.
This source candidate does not establish freshness for arbitrary direct Lua
unit-action APIs outside the admitted spell/action/macro/InteractUnit boundaries.
