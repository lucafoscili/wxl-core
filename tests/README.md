# Focused regression checks

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
native fillers, forwarding their original arguments. The filler identifies the
exact section, not the first section sharing a truncated range. Crossing and
wholly-above sections get dense CPU positions from the current bone palette;
lower sections keep their native positions. Native filters, allocation and
triangle arbitration remain in charge. A 3,072-index stack block in a separate
non-inlined function submits complete local triangles with vertex base zero,
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
normalised bodies: the fill hook (clear the pending fill, forward, then record),
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
