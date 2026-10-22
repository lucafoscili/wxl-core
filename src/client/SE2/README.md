# Text-emote sounds (3.3.5a / 12340)

The existing sound owner, `Sound.cpp`, registers `emote-sounds` at Boot, before
native DBC loading or lookup allocation. `WXL_EMOTE_SOUNDS=1` is the default;
the existing environment/`WarcraftXL.cfg` reader accepts `0` to disable it.
This switch is independent of `race-tables` and the normal sound event hooks.
`offsets/engine/Sound.hpp::emotesounds` owns all expected bytes and operands.

The native `TEXTEMOTELOOKUP` record becomes 128 DWORD sound slots (`0x200`
bytes), followed by the emote ID at `+0x200`, for a stride of `0x204`. Indexing
stays `4 * (2 * race + sex)`. All three unsigned race bounds become 64; the
sex bounds remain 2. EmotesTextSound supplies the sounds, including absent
slots remaining zero. This feature does not create or substitute DBC rows.

Every span below is compared byte for byte before writing anything. A mismatch
logs one error naming the site and returns failure. All ten operands publish
in one `mem::WithWritable` window; failed unprotect writes nothing. The native
allocator, pointer index, initialization order and cleanup remain the owners
of the storage. No hooks, replacement allocator or record migration are needed.

## Closed record-layout consumers

Statically traced on 22 October 2026 in the supplied 3.3.5.12340 reference
executable. Its `.wxl` section is present; this is evidence for the observed
instruction bytes, not a claim that the whole image is pristine. No client
code or data is included here beyond bounded verification spans.

| Site / purpose | Instruction VA | Original verify bytes | Operand VA / replacement |
| --- | --- | --- | --- |
| `allocationSize`: realloc and allocation byte count | `4CEA11` | `69 F6 B4 00 00 00` | `4CEA13` / `0x204` |
| `copyDwords`: relocation copy, including `rep movsd` | `4CEA6A` | `B9 2D 00 00 00 8B F8 F3 A5` | `4CEA6B` / `0x81` DWORDs |
| `copyStride`: relocation loop step | `4CEA76` | `81 C2 B4 00 00 00` | `4CEA78` / `0x204` |
| `lookupRace`: first reader, sex guard and slot read | `4CEB4E` | `83 FA 16 73 10 8B 4D 10 83 F9 02 73 08 8D 14 51 8B 04 90` | `4CEB50` / `0x40` (byte) |
| `soundRace`: sound-processing reader, same indexing | `4CEB8E` | `83 FA 16 73 10 8B 4D 10 83 F9 02 73 08 8D 14 51 8B 04 90` | `4CEB90` / `0x40` (byte) |
| `appendStride`: address of next record | `4CEC1C` | `69 F6 B4 00 00 00` | `4CEC1E` / `0x204` |
| `zeroSounds`: memset length | `4CEC2D` | `68 B0 00 00 00` | `4CEC2E` / `0x200` |
| `zeroEmoteId`: clear trailing ID | `4CEC35` | `C7 86 B0 00 00 00 00 00 00 00` | `4CEC37` / `0x200` |
| `storeEmoteId`: assign trailing ID | `4CED9D` | `89 90 B0 00 00 00` | `4CED9F` / `0x200` |
| `fillRace`: DBC race guard, sex guard and slot write | `4CEDAF` | `83 F9 16 73 19 8B 46 0C 83 F8 02 73 11 8B 76 10 8D 04 48 8B 0F 89 34 81` | `4CEDB1` / `0x40` (byte) |

All replacements are DWORD operands except the three race-bound bytes. The
record copy count is **129 DWORDs**, including the ID, not 128 or 516 DWORDs.
There is no read of the trailing ID other than the relocation copy. The two
public readers return scalar sound IDs; record pointers do not leave the
lookup through them.

Closure followed the record array at `B4ADE4` (capacity/count/data/chunk size)
and the separate emote-ID pointer array at `B4ADF4`. Whole-image literal
searches for every address in and around those globals found only the paths
below. Direct-call and function-address searches also found no other record
allocator/append consumers. The unaligned `4CEA00` literal at `A725B1` is a
cross-item match in scalar `.rdata`, not a function pointer.

| Unchanged path | Evidence / why no layout operand needs changing |
| --- | --- |
| `4CEBE0` append -> `4CEA00` allocator | Only allocator call: `4CEC12: E8 E9 FD FF FF`. Append uses the patched stride and zeroing above. |
| `4CECB0` initialization -> `4CEBE0` append | Only append call: `4CED93: E8 48 FE FF FF`. `4CED98: 89 07` publishes the returned record into the ID index; this is its only pointer escape. |
| Record capacity/chunk count | `4CED4C: 89 35 F0 AD B4 00` sets the growth chunk to the counted emote-ID runs before appending. The first allocation covers those runs, so index pointers stay stable during filling. `7F7980` is a shared count-only growth helper. |
| Emote-ID pointer index | `4CEC60` -> `4CEAA0` resizes DWORD pointers, not records. `4CEAB7: 8D 3C 85 00 00 00 00` allocates `count*4`; `4CECF1: 8D 14 8D 00 00 00 00` supplies the index memset length. Both remain correct. |
| Pointer-index readers | `4CEB3E` / `4CEB7E`: `8B 0D FC AD B4 00`, then `8B 04 81` at `4CEB44` / `4CEB84`. Fill gets the same base at `4CED7F`. These load record pointers; slot readers are verified above. |
| Normal cleanup `4CEDF0` | `4CEDF0: A1 EC AD B4 00`, `4CEE0A: A1 FC AD B4 00`; frees both whole buffers and resets both containers. No record walk or byte count. |
| Static destructors `9D4390` / `9D43B0` | `A1 EC AD B4 00` / `A1 FC AD B4 00`; also free whole buffers, with no stride or element destructor. |
| Entry points | Initializer called at `4C8317`; readers called at `747066` and `6DDB6A`. Neither reader caller imposes a race-22 bound. |

## Offline checks and client check

Configure with `cmake -S . -B <temp-build> -A Win32 -DCLIENT_PATH:PATH=`.
Build `WarcraftXL`, `wxl-race-tables-test` and `wxl-emote-sounds-test` with
`--config Release`, then run the two `Release/*-test.exe` executables.
Run the existing suite with `python -B -m unittest discover -s tests -v`.
Keep the build outside the checkout; do not use the deploying `build.ps1`.

The emote fixture uses synthetic memory and independently recorded verification
instructions. It exercises disabled installation, every verification-byte
mismatch, failed write-window acquisition, and the exact successful operand
changes. Bounded x86 fragments exercise the expanded allocation/copy/ID layout
and both readers plus the fill path at races 21/22/23/63/64 and sexes 0/1/2,
with adjacent-record canaries. It does not load a client or any DBC/audio assets.

Only an in-client check with matching EmotesTextSound/SoundEntries and audio
assets can establish audible `/cheer` and `/laugh` for races 22/23 (both sexes),
stock-race preservation, race-63 behavior with fixture data, and restart with
`WXL_EMOTE_SOUNDS=0`. Offline verification is not an audio verdict.
