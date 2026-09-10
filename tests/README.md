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
Index refill records focus on sections 0 and 1 and include the first complete
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
