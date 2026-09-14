# Source

The firmware runs on the H8/38606 in normal mode: 8-bit bytes, 16-bit integers
and pointers, and 32-bit longs. It uses C90 with CH38 extensions for interrupts,
sections, bitfield order and machine instructions.

## Modules and reading paths

The source retains 40 separately compiled C files. Four `h8_` modules in
`src/startup/` initialize sections, enter interrupts, reset the device, and
describe the ROM. `src/support/` holds `ir.c` and the two `lib_` helper modules.
The 32 `pw_` modules in `src/application/` cover activities, storage, and device
interfaces. `src/globals.c` defines shared storage.

Module headers generally follow the source directories and basenames under
`include/`. Shared types, records, resources, and view state remain at the
include root.
Project includes use paths from that root, such as
`#include "application/pw_pedometer.h"` and `#include "support/lib_common.h"`.
The shared `application/pw_eeprom_m95512.h` interface covers the serial bus and
single-byte programming in `pw_eeprom_m95512_bus.c`, plus bulk transfers and
mirrored-record handling in `pw_eeprom_m95512_io.c`. Its status-receipt helpers
are implemented in `lib_common.c`. That module also groups raster operations,
artwork loading, scratch allocation, step pacing and small runtime helpers.
`InstallTask` lives in `pw_home.c`; communication completion and staged-walk
activation are declared in `h8_resetprg.h`.

Each source must have a unique lowercase basename. The build discovers C files
recursively and links their objects in alphabetical basename order. Directory
paths organize navigation independently of that order. The `h8_`, `lib_`, and
`pw_` prefixes are reconstruction conventions that express the observed module
sequence. Renaming a file can change code and data placement and requires the
same complete matching checks as an implementation change. Section placement
and runtime-library handling have their own build rules.

These paths connect the modules by responsibility:

| Start here | Responsibility |
| --- | --- |
| [h8_resetprg.c](../src/startup/h8_resetprg.c), [pw_main.c](../src/application/pw_main.c), [pw_power.c](../src/application/pw_power.c) | Boot, foreground tasks, activity scheduling and low-power transitions |
| [view.h](../include/view.h), [globals.h](../include/globals.h) | View state, interrupt-shared values and workspace ownership |
| [pw_accel_bma150.c](../src/application/pw_accel_bma150.c), [pw_fourier.c](../src/application/pw_fourier.c), [pw_pedometer.c](../src/application/pw_pedometer.c) | Samples, spectrum analysis and accepted step batches |
| [lib_common.c](../src/support/lib_common.c), [pw_rtc.c](../src/application/pw_rtc.c), [save.h](../include/save.h) | Step pacing, Watts, time, counters and persistence |
| [pw_eeprom_m95512_bus.c](../src/application/pw_eeprom_m95512_bus.c), [pw_eeprom_m95512_io.c](../src/application/pw_eeprom_m95512_io.c), [records.h](../include/records.h), [resources.h](../include/resources.h) | Serial storage, repairing reads, records and resource layouts |
| [ir.c](../src/support/ir.c), [ir_state.h](../include/support/ir_state.h), [protocol.h](../include/protocol.h) | Connection, packets, peer exchange and deferred completion |
| [pw_home.c](../src/application/pw_home.c), [pw_pictogram_menu.c](../src/application/pw_pictogram_menu.c), [pw_pack.c](../src/application/pw_pack.c), [pw_trainer.c](../src/application/pw_trainer.c), [pw_local_settings.c](../src/application/pw_local_settings.c) | Navigation, inventory, trainer history and settings |
| [pw_pokeradar.c](../src/application/pw_pokeradar.c), [pw_battle.c](../src/application/pw_battle.c), [pw_dowsing.c](../src/application/pw_dowsing.c), [pw_carry_overflow.c](../src/application/pw_carry_overflow.c) | Search, battle, rewards and full-inventory replacement |
| [pw_follower_events.c](../src/application/pw_follower_events.c), [pw_follower_prompts.c](../src/application/pw_follower_prompts.c), [pw_friend.c](../src/application/pw_friend.c), [pw_cutscenes.c](../src/application/pw_cutscenes.c), [pw_diary.c](../src/application/pw_diary.c) | Social offers, peer play, presentations and the diary |
| [pw_nt7508.c](../src/application/pw_nt7508.c), [pw_builtin.c](../src/application/pw_builtin.c), [pw_buzzer.c](../src/application/pw_buzzer.c), [pw_player_input.c](../src/application/pw_player_input.c) | Drawing, resident artwork, sound and buttons |
| [pw_factory_test.c](../src/application/pw_factory_test.c), [pw_selftest.c](../src/application/pw_selftest.c), [pw_battery.c](../src/application/pw_battery.c) | Fixture operations, diagnostics and battery references |

## Execution and shared state

Boot restores mirrored records, repairs an interrupted staged copy, initializes
the peripherals and installs the foreground task. Interrupts update counters
and latch work for that task. The quarter-second RTC interrupt requests a UI
refresh; the one-second interrupt maintains seconds separately. Several ticks
can coalesce into one pending refresh, which the foreground handles when it runs.

`MainTick` samples on every wake. A completed motion batch takes priority over
rendering; a pending refresh takes priority over deferred RTC processing.
Inactive mode suspends regular sampling and refresh timers, while later wakes
still sample for activity detection. Activity resumes motion mode for thirty
seconds. Interactive entry sets sixty seconds for the display and ninety for
motion; later button edges refresh the display timeout to ninety seconds.

`g_state` contains runtime state, including the RAM copy of `SaveData`.
`g_ui.view` overlays the active view in an 18-byte bank. Changing `g_state.view`
preserves that bank. Radar passes its encounter byte to battle; a full inventory
passes its selection to discard; IR carries its LCD readback result into
diagnostics. Initializers deliberately preserve those bytes.

`g_work` overlays motion, sound and IR storage. Audio reuses the three motion
sample arrays for a score and takes over the foreground while it plays. IR
reuses motion state and part of the scratch area. Task handoffs finish using the
old interpretation before preparing the next one.

Scratch allocation reserves consecutive bytes; callers provide initialization
and alignment. Resetting scratch rewinds the cursor. The bytes remain until
another user writes them. Peer rewards deliberately place retained records
beyond the course prefix that a nested lookup reloads. Drawing helpers similarly
reuse known portions of the backing storage. Callers must account for every
intervening write before using a retained pointer. Ordinary allocation checks
1,024 bytes and sleeps after an overflow; direct large-artwork transfers use all
1,536 bytes of the backing storage.

## From motion to steps

The main task collects signed high bytes of the accelerometer's three axes.
Each 64-sample batch passes through the FFT, accumulating approximate magnitudes
from all axes.

`MotionEstimateQ9` visits candidates in a defined order, compares relative
magnitudes and changes its rejection test according to the preceding batch.
These checks determine which bin is accepted.

The accepted bin position represents steps per batch in Q9 units: one step is
512. Fractional credit carries into later batches. `StepPacingTick` spreads the
whole-step budget across subsequent samples, credits at most one step per call
and updates hourly, daily and lifetime counters. Every twenty credited steps
can earn a Watt, up to 9,999. The arithmetic, single-credit conditions and
saturation points are part of the firmware's behavior.

RTC processing handles minutes, hours and the configured daily rollover.
The elapsed-hours counter runs through periods with an empty Pokemon slot. It
resets on a new walk or lifetime reset and stops when it or the lifetime step
count reaches 9,999,999. Rollover shifts daily history and clears the IDs of
prior peer records while preserving their other fields.

## Views and activities

Activity state includes input, update and rendering. Renderers can advance or
clamp animation counters, change a presentation stage or finish a view. State
names therefore appear across those paths. Timers explicitly distinguish input
polls, rendered calls, hardware counts and completed motion batches.

Battle separates presentation phase, player action, probability row and
response code. The same response code has different meanings for Attack and
Evade; the table and dispatch document that relationship. Dowsing separates
patch selection from the chosen course item. Inventory selectors distinguish
ordinary slots from held Pokemon and event gifts, including their index bases.

Reward persistence and its presentation can happen at different times. An IR
reward is already stored when its arrival animation begins. Some local rewards
commit on acknowledgement, while others are stored before display. Diary
appending is a separate operation; the device's cursor rotates through 23 slots
within the 24-entry stored area.

## Stored data and communication

`eeprom_map.h` owns serial-storage addresses. `save.h`, `records.h` and
`resources.h` describe the corresponding layouts. Resource expressions use
pointer syntax to calculate EEPROM addresses for the driver. Transfer spans
can contain more than one logical object or extend beyond the image being
displayed.

Mirrored reads repair a bad copy, or fill both copies with `FF` when neither
checksum validates. When both validate, the buffer holds the primary copy.
The repair decision compares their additive checksums: unequal sums trigger a
backup rewrite from primary. Status receipt checks and battery-reference checks
also perform the I/O and repair described by their interfaces.

Native H8 counters and console-produced little-endian fields coexist. Stored
fields with an `Le` suffix retain little-endian bytes during forwarding and
equality checks. Numeric consumers decode them where needed. Course item
lookup returns an encoded ID suitable for copying directly into inventory.

`PersistentReset` clears pairing, session state, return inventory, weekly steps
and peer history. Its arguments control event and lifetime clearing independently.

The IR layer separates handshake role, peer-status role and bulk phase.
Both peers eventually send and receive. Packet handlers may queue a foreground
completion action, which takes priority over a later error. Walk activation
sets a recovery marker around the staged page copies; diary, counter and
inventory changes after clearing the marker are outside that recovery span.

## Graphics and sound

Resident artwork is extracted once into six NCG files and a BMP font sheet.
The build converts these editable files into C arrays; missing images use
original placeholders. See the [artwork guide](../assets/README.md).
EEPROM artwork arrives from the console separately.
A raster page covers eight rows and stores the high then low bit plane for
each column. Small Pokemon animations contain two 32×24 frames; large animations
contain two 64×48 frames.

The display has two drawing banks. Helpers distinguish page numbers from pixel
coordinates. Horizontal animation deliberately extends beyond the visible
96 columns; the driver transmits the requested width. Vertical page addresses
must stay within the bank. Name frames and message rules also write fixed
regions around their artwork, as described in the helper interfaces.
A blank transfer sprite still follows its arrival/departure animation.

EEPROM score descriptors hold a little-endian offset, byte length and checksum.
Loading a score replaces the motion samples in shared RAM. A borrowed score's
storage must stay valid until playback ends and satisfy the command sequencing
and preceding-record requirements in `score.h` and `pw_buzzer.h`. Repeat commands
return to the shared RAM score.

Notes hold duration units and timer-table indices. The worker and Timer W
interrupt cooperate on duration, separation and legato. The rest index reaches
past the pitch table into adjacent resident artwork; the shared storage
declaration includes the full accessed span.

## Conventions and matching

Use concise PascalCase functions and types, lowerCamelCase locals and fields,
`g_` globals, uppercase constants, and two-space indentation. Function braces
start on the next line; control braces stay on the same line. The project
selected these conventions using embedded C guidance from the period.

Names express meaning and useful units. Comments explain relationships and
constraints that the code alone cannot make clear. Keep ordinary coordinates,
indices and numeric tables simple when their meaning is already clear.

Changes must preserve full retail identity under both supported compiler
suites. Integer promotions, signed shifts, access widths, volatile reads,
bitfield order, alignment, expression grouping and local lifetimes matter.
Call order also preserves shared random-number consumption and workspace
ownership. Run the [formatter, linter and complete builds](build.md).

The target assumptions are visible in scalar and layout declarations, drivers,
interrupts and build configuration. One narrow return constraint remains:
`EepromWriteByte` passes the last sampled event byte through the CH38 return
register without an ordinary C return expression. Its callers consistently
forward or inspect the whole event byte, including its clock and UI flags.
A compiler port must make that return explicit and review the other target
assumptions. The qualified suites are CH38 6.02.01 and 6.02.02.

## Remaining uncertainties

- The IR-request event has no ordinary setter in this firmware.
- `pendingStepsQ9` is consumed and cleared without an established nonzero
  producer; the motion batch's `resetOnlyByte` is only cleared.
- The threshold-test `resetWord` is initialized without an established consumer.
- Preserved fields keep their observed initialization and transfer behavior
  where their meaning remains unresolved, including the event-item prefix,
  leading walk/peer words and the separately cleared Pokemon appearance bit.

Byte identity proves the executable result. Field meanings and the frequency
with which a physical device encounters a path require separate evidence.
