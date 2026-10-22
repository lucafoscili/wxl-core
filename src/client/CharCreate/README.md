# Race tables (3.3.5a / 12340)

`RaceTables.cpp` registers the `race-tables` Boot feature. `WXL_RACE_TABLES`
defaults to `1`; the existing environment/`WarcraftXL.cfg` reader accepts `0`
to disable it explicitly. `offsets/game/CharCreate.hpp` owns its sites and
capacities; `offsets/game/DB2.hpp::chrraces` owns the native table layout/loader.
Every listed instruction span is verified with StorySelect's FNV-1a-64 form
before any patch or hook registration. A mismatch logs the site and applies none.

| Patch | Operand address(es) | New value |
| --- | --- | --- |
| Preference array | `4E157D`, `4E15B5`, `4E16A3`, `4E1C3A`, `4E1E94`, `4E20EE`, `4E2127`, `4E222A` | Static 64-race x 2-sex pointer array |
| Preference memset length | `4E1C34` | `0x200` bytes |
| Preference free-loop count | `4E1E9B` | 64 races |
| DK sound / trial-loading names | `4CDA43`, `4D9EB0` | Static 64-pointer name table |
| Restriction JA displacement | `4DFAF2` (instruction `4DFAF0`) | Thunk: unsigned `race-1 > 62` goes to `4DFC32`; otherwise `fldz` then `4DFC18` |
| ChrRaces loader detour | `63AB00` | Call original, then fill new name slots |

Boot copies name entries 0..21 unchanged and leaves 22..63 null. The loader
detour fills 22..63 from each loaded row's `ClientFileString` pointer at `+0x2C`,
leaving absent rows null. It runs **after the original loader returns**, because
the native loader builds the row/index pointers before reading their string
block, and sets the loaded flag only afterward (`63AD44`). The Boot hook batch
is enabled before client startup resumes, so the fill precedes Glue/game readers.
Names borrow the native string block for its lifetime. No DBC is loaded by WXL.

The restriction switch and original handling for races 1..11 remain unchanged;
12..63 return numeric zero. Race 0 and IDs above 63 retain the error path.
Text-emote sound storage has its own [SE2 feature](../SE2/README.md).
GlueXML and client data are outside this feature.

Configure with `cmake -S . -B <temp-build> -A Win32 -DCLIENT_PATH:PATH=`.
Build `WarcraftXL` and `wxl-race-tables-test` with `--config Release`, then run
`<temp-build>/Release/wxl-race-tables-test.exe`. The synthetic Win32 fixture
exercises production verification, sparse name filling and thunk bytes/branches;
it never loads a game client. Run the existing suite with
`python -B -m unittest discover -s tests -v`.

Beta after 24 October still needs creator race/sex switching and exit/reentry,
stock/new race restriction queries, DK name/sound and trial-loading text paths,
and creation/login with actual ChrRaces/Glue data. Offline checks cannot establish
those in-client results; text-emote audio needs the SE2 feature and matching client data.
