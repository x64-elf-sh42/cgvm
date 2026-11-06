# CGVM Machine Architecture

Revision: 0.1 — Draft

Implemented bus and GPU behavior is specified in the [Bus and Graphics Reference](devices.md).
Implemented CPU behavior is specified in the [Instruction Set Reference](isa.md).
Where this draft describes future facilities, the ISA reference defines current
execution semantics.

Status: proposed architecture, not an implemented hardware specification. The
current prototype remains the implementation reference until the implementation milestones are completed. Numeric addresses, register layouts, and timing constants must be
specified and tested before guest programs depend on them.

## Scope

CGVM defines a virtual computer for graphics and interactive applications. The
architecture comprises a processor, memory, a device bus, graphics, timers, and
input. Physical cycle accuracy is outside the scope of this specification.

Requirements describe the proposed architecture, not current implementation
behavior. Unassigned addresses, encodings, and timing constants are TBD and must
not be treated as stable ABI values. See [Implementation Roadmap](roadmap.md)
for implementation status and migration criteria.

The machine core must run without SDL, a window, or PNG output. A host frontend
translates input and displays frames. A headless frontend runs the same machine
for tests and benchmarks.

## Components and ownership

| Component | Owns | Responsibilities |
| --- | --- | --- |
| Machine | CPU, bus, RAM, devices, guest clock | Reset, loading, scheduling, stop/fault reporting |
| CPU | Registers, PC, flags, execution state | Execute instructions through the bus |
| Bus | Address mappings | Route and validate RAM and device accesses |
| GPU | Video memory, completed frame, presentation status | Accept guest rendering and publish frames |
| Timer | Deadlines and pending events | Wake the CPU at guest-time deadlines |
| Input | Guest key state and pending transitions | Receive timestamped input from the frontend |
| Frontend | Window, host events, presentation resources | Translate host input, display frames, throttle execution |

The initial implementation uses one host thread. Devices are separate logical components, not separate
threads. The frontend must not mutate CPU registers or device internals directly.
No CPU instruction may call SDL. PNG export is a frontend/debugging facility.

## CPU and memory

The initial processor uses 32-bit instruction words, 16 integer registers, and
the existing floating-point register bank. Before freezing the ISA, document each
instruction's operands, flags, PC effect, fault cases, and execution cost.

RAM is byte-addressed. Introduce explicit 8-, 16-, and 32-bit loads/stores with
register-based addressing, so programs can address the full machine rather than
only an immediate field's range. Proposed data representation is little-endian;
unaligned multi-byte accesses fault. Instruction fetch and branch target units
must be settled explicitly during the CPU extraction: the prototype indexes a
separate instruction array, and must not silently switch to byte addresses.

Unsigned integer arithmetic wraps at 32 bits. Division by zero, invalid opcodes,
invalid fetches, and unmapped or forbidden accesses produce a machine fault,
including the PC and offending operation/address. Define signed operations
separately. Floating-point ordering and NaN behavior need explicit ISA entries.

Distinguish these execution results:

- Budget exhausted: return to the scheduler; CPU remains runnable.
- Yield: return to the scheduler without becoming blocked.
- Wait: block on specified device events; avoid a lost wakeup by checking pending
  events before blocking.
- Halt: guest execution ends normally.
- Fault: guest execution stops with diagnostic state.

Do not reuse the current unsigned instruction-count sentinel as this interface.

## Bus and devices

The initial device interface uses memory-mapped I/O. RAM, video memory, and device
registers occupy non-overlapping ranges. The existing I/O-port experiment remains
prototype code; it is not a second required guest device interface.

Ordinary RAM accesses and device accesses share guest load/store instructions.
Device registers have explicit access widths and permissions. Side effects happen
once per valid access; an invalid access must not partially execute a command.
The bus validates the entire access range, including overflow and boundaries.

Each device provides reset, register read/write, and time advancement behavior.
Devices expose pending event bits for polling or CPU waits. Interrupt handlers
are deferred until there is a concrete use case.

Every new device must document:

1. Its purpose, version, address range, and discovery mechanism.
2. Each register's offset, width, permissions, reset value, and side effects.
3. Valid commands, ordering, completion, and busy/error behavior.
4. Events, acknowledgement, and whether events remain pending until acknowledged.
5. Memory ownership, access timing, and reset/teardown behavior.
6. A minimal guest example and headless conformance tests.

Reserve the numeric memory map and device identity/version registers in the bus
milestone. Check mappings for overlap at machine creation. Changing a guest-visible
contract requires updating its documentation and compatibility policy together.

## Time and scheduling

Guest time is represented by an integer clock with a specified frequency. CPU instructions consume
defined guest ticks; the initial simple instructions can have uniform cost.
This is a scheduling model, not a simulation of electrical cycles. Host execution
speed must not change guest timer deadlines or animation speed.

The machine executes bounded CPU batches, stopping at the next device deadline,
then advances devices and resolves events. If the CPU is waiting, advance directly
to the next scheduled event instead of spinning. Bound frontend service intervals
so an infinite guest loop cannot prevent window input or shutdown.

Interactive execution is throttled against a monotonic host clock. Headless runs
can advance guest time without sleeping. Input is applied at defined guest-time
boundaries; recording those events enables deterministic replay. Specify ordering
for events sharing a timestamp before implementing replay tests.

## GPU and frame lifecycle

The first GPU is a framebuffer device with explicit presentation. Guests can
query dimensions, stride, and pixel format. Retain the current resolution for
the initial migration; do not hard-code it in new guest programs.

Proposed initial lifecycle:

1. Guest writes its working framebuffer in video memory.
2. A PRESENT register write snapshots that framebuffer into device-owned memory
   and marks presentation pending.
3. At the next guest display tick, the snapshot becomes the published frame and
   a completion event is set. Pending status clears.
4. The frontend displays the published frame when serviced. It may skip obsolete
   published frames without altering guest time or completion events.

Allow one pending submission initially. A PRESENT while pending is rejected with
documented device status; it never silently overwrites the pending snapshot.
The guest may continue modifying working video memory after an accepted snapshot.
Published pixels remain stable until replaced at a later display tick. A synchronous
frontend consumes them before further machine advancement; a future asynchronous
frontend must acquire its own snapshot or explicit ownership.

This intentionally starts with a copy for clear ownership. Buffer swapping,
dirty rectangles, and packed pixel optimizations can follow measurements without
changing guest-visible semantics. Freeze pixel format and presentation registers
in the GPU milestone. Clear, line, rectangle, and sprite commands are potential
GPU extensions, not prerequisites for the first version.

## Input device

Expose machine-defined key identifiers, independent of SDL key codes. Provide
current key state for held movement and latched press/release transitions so short
taps survive between guest polls. Document how transitions are acknowledged;
focus loss releases held keys. Apply input even while the CPU waits.

## Reference conventions

Guest denotes software executing on the virtual processor. Host denotes the
implementation environment. Architectural state is state observable by guest
software. Implementation details are not architectural guarantees.

Every instruction reference must contain the following fields:

| Field | Required content |
| --- | --- |
| Mnemonic and encoding | Opcode value, bit layout, operand fields |
| Operands | Widths, signedness, immediate extension, address units |
| Operation | Pseudocode, result width, program-counter update |
| Flags | Flags read, modified, preserved, or undefined |
| Exceptions | Fault conditions, fault precision, restart behavior |
| Timing | Guest-tick cost and blocking behavior |
| Availability | Architecture revision introducing the instruction |

Device references must include register bit layouts, reset values, reserved-bit
behavior, access permissions, side effects, and event acknowledgement semantics.
Unspecified behavior must be marked TBD. Reserved behavior and undefined behavior
must be identified separately. Examples are informative unless stated otherwise.
Changes to guest-visible behavior require corresponding reference and conformance
test updates.
