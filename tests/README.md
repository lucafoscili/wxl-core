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
