# CGVM Bus and Graphics Reference

Revision: 0.5. Status: implemented.

## 1. Address space

Data addresses are unsigned 32-bit byte addresses. Instruction addresses remain
indices into separate code storage. Data transfers are little-endian. Access
widths are 1, 2, and 4 bytes; each access must be naturally aligned and contained
entirely within one mapping. Unmapped, misaligned, and prohibited accesses fault
before any transfer or device side effect. The CPU records the rejected address.

| Inclusive range | Function | Access |
| --- | --- | --- |
| 0x00000000–0x00FFFFFF | RAM | Read/write, 8/16/32 bits |
| 0x01000000–0x0123FFFF | Working video memory | Read/write, 8/16/32 bits |
| 0x02000000–0x02000047 | GPU registers | Defined registers only, 32 bits |
| 0x02001000–0x0200107F | Input registers | Defined registers only, 32 bits |
| 0x02002000–0x02002003 | Clock low word | Read-only, aligned 32 bits |

All other addresses are unmapped. Code memory is not mapped on the data bus.
The fixed map identifies the sole GPU in revision 0.2; dynamic device discovery
and version registers are not implemented. RAM and video memory reset to zero.

## 2. GPU registers

All offsets are relative to 0x02000000. Registers require aligned 32-bit accesses.
RO writes and WO reads fault. Unassigned offsets fault.

| Offset | Name | Access | Reset | Definition |
| --- | --- | --- | --- | --- |
| 0x00 | WIDTH | RO | 1024 | Pixels per row |
| 0x04 | HEIGHT | RO | 768 | Rows |
| 0x08 | STRIDE | RO | 3072 | Bytes per row |
| 0x0C | FORMAT | RO | 1 | RGB24: consecutive red, green, blue bytes |
| 0x10 | STATUS | RW | 0 | Bit 0 pending, bit 1 completion, bit 2 busy error |
| 0x14 | COMMAND | WO | — | Execute command |
| 0x18 | COLOR | RW | 0 | Bits 23:0 contain 0xRRGGBB; upper bits discarded |
| 0x1C | X | RW | 0 | Unsigned pixel x coordinate |
| 0x20 | Y | RW | 0 | Unsigned pixel y coordinate |
| 0x24 | FRAME | RO | 0 | Number of published frames, modulo 2^32 |

STATUS reads return zero in bits 31:3. Writing one to bit 1 or 2 clears that
latched status. Writing zero preserves it. Any write with other bits set faults
without changing status. Completion is also consumed by WAIT_FRAME.

| Command value | Name | Operation |
| --- | --- | --- |
| 1 | PRESENT | Snapshot working video memory and set pending |
| 2 | CLEAR | Fill working video memory with COLOR |
| 3 | PIXEL | Write COLOR at (X,Y); out-of-range coordinates have no effect |

Other command values fault. CLEAR and PIXEL complete synchronously and do not
change pending snapshots. Their cost is one guest tick, the cost of their issuing
store instruction; host execution cost is not represented by this timing model.

PRESENT permits one pending snapshot. A submission while pending sets busy error
and preserves the original snapshot; the issuing store completes normally.
At the next display boundary, the snapshot becomes the published image, pending
clears, completion sets, and FRAME increments. Completion and busy error remain
set until acknowledged. Published storage is stable until the next publication.
Working video memory can be modified immediately after an accepted submission.

## 3. Timing and waiting

The guest clock runs at 6,000,000 ticks per second. Every completed instruction
consumes one tick. Faulting instructions consume none. Display boundaries occur
at positive multiples of 100,000 ticks (60 Hz). Reset sets the clock to zero.

Instruction side effects occur before advancement of its completion tick. A
PRESENT store completing exactly at a display boundary is published at that
boundary. At each completed instruction the device clock advances before the
next instruction is fetched. Publication precedes subsequent guest observation.
There is no independent timer device in this revision. Input is defined in section 5.

WAIT_FRAME increments PC and consumes one tick. If completion is already pending,
it acknowledges completion and returns runnable. Otherwise it returns CPU_WAIT.
`cpu_step` does not advance a waiting processor. `machine_step` advances a waiting
machine to the next display boundary if necessary, acknowledges completion, and
resumes execution. If no completion exists, the machine remains waiting. A zero
budget neither executes instructions nor advances a waiting machine.

Host presentation is throttled against a monotonic clock and uploads only newly
published frames. Host delay may cause uneven presentation or skipped displayed
frames; it does not alter guest deadlines. No hard real-time guarantee is made.
Headless execution uses the same tick and publication rules without sleeping.

## 4. Host interface and compatibility

Machine-owned video, submitted, and published arrays implement the GPU. A
synchronous frontend reads published storage before advancing the machine again.
An asynchronous frontend requires its own ownership protocol.

Legacy SET_PIXEL, FB_CLEAR, and FB_SWAP retain their caller-attached buffer
semantics. They do not access the mapped GPU. The box demo uses mapped GPU
commands exclusively. Other legacy programs require migration to obtain timed
presentation through the current frontend.

The current register map and command dispatch are fixed. Adding another device
requires a non-overlapping range, explicit read/write dispatch, a register
reference, and access/reset/side-effect tests. A dynamic registration API is not
implemented.

## 5. Input device (revision 0.3)

The input device occupies 0x02001000–0x0200107F. All defined registers require
aligned 32-bit accesses. Other widths, unassigned offsets, and access-permission
violations cause BUS faults without side effects. All registers reset to zero.

Key identifiers are Up=0, Left=1, Down=2, Right=3. For identifier k in [0,3]:

| Offset | Access | Function |
| --- | --- | --- |
| 0x00 + 4*k | RO | Held state: 1 down, 0 up |
| 0x20 + 4*k | RO | Latched press transition: 0 or 1 |
| 0x40 + 4*k | RO | Latched release transition: 0 or 1 |
| 0x60 + 4*k | WO | Bit 0 acknowledges press; bit 1 acknowledges release |

Acknowledgement writes containing bits other than 0 or 1 fault. Zero preserves
both latches. Transitions coalesce: multiple presses before acknowledgement remain
one pending press. Press and release can both be pending. Reads do not acknowledge
transitions. Repeated host key-down events while already held do not create presses.

The host invokes machine_key between machine execution calls; the update is
visible to the next guest read, including after a frame wait. Interactive arrival
time is assigned at the current guest boundary and is not deterministic across
CPU batch sizes. A future replay queue must assign explicit guest timestamps.
Scripted callers can inject at controlled boundaries. Input does not wake
WAIT_FRAME. Focus loss releases all held keys, preserving release transitions.

The SDL adapter maps physical Up/Left/Down/Right scancodes to machine identifiers. The box
demo polls held state once per completed frame. Opposing keys cancel on each axis;
diagonal movement applies both axes. Movement is three pixels per frame per axis
(180 pixels/second at 60 Hz). The 100x100 outline is clamped to x in [0,WIDTH-100]
and y in [0,HEIGHT-100]. Coordinates and clamping are guest code, not host policy.
The demo requires dimensions of at least 100x100, queries WIDTH/HEIGHT at startup,
and runs until the host window is closed. Closing the window is normal termination
and exports the working image to test.png.

## 6. Revision 0.4 line rasterization and input extension

GPU offsets 0x28 (X2) and 0x2C (Y2) are read/write 32-bit endpoint registers,
reset to zero. The GPU register mapping therefore ends at 0x0200002F.
COMMAND=4 (LINE) draws from (X,Y) to (X2,Y2) using COLOR. LINE interprets endpoints
as signed two's-complement 32-bit integers; PIXEL retains unsigned coordinates.

LINE clips the segment to [0,WIDTH-1] x [0,HEIGHT-1] using Liang–Barsky parameter
clipping in host double arithmetic. Clipped endpoints are rounded by adding 0.5
and truncating. An integer Bresenham walk includes both endpoints; a zero-length
visible segment draws one pixel. The walk uses dx=abs(x1-x0), dy=abs(y1-y0),
error=dx-dy; x advances when 2*error > -dy, y when 2*error < dx, using the same
pre-update error for both decisions. Fully outside segments have no effect.
Extreme endpoint values are clipped before rasterization to bound host work.
LINE completes synchronously in one guest tick and modifies only working video
memory. Pending and published snapshots are unchanged.

Input identifiers append Z=4 and X=5. The register formulas in section 5 apply
for k in [0,5], with the same permissions and acknowledgement semantics. The
mapping extends through 0x02001077; gaps remain unmapped. SDL uses physical Z/X
scancodes. The original four key identifiers retain their values.

## 7. Revision 0.5 filled triangles and reciprocal depth

The GPU register range extends through 0x02000047. New registers are aligned
32-bit read/write registers, reset to zero:

| Offset | Name | Meaning |
| --- | --- | --- |
| 0x30 | X3 | Signed third screen-space x coordinate |
| 0x34 | Y3 | Signed third screen-space y coordinate |
| 0x38 | Q1 | Unsigned reciprocal depth for first vertex |
| 0x3C | Q2 | Unsigned reciprocal depth for second vertex |
| 0x40 | Q3 | Unsigned reciprocal depth for third vertex |
| 0x44 | DEPTH | 0 disables depth testing/writes; 1 enables both |

DEPTH writes other than 0 or 1 fault. COMMAND=5 (TRIANGLE) fills the triangle
(X,Y), (X2,Y2), (X3,Y3) using COLOR. Either winding is accepted; zero-area triangles
have no effect. The rasterizer clamps the bounding box to the viewport before
iteration. Samples are at pixel centers (x+0.5,y+0.5). Barycentric weights must
all be nonnegative. There is no top-left ownership rule; exact shared-edge ties
are resolved by submission order and the strict depth comparison.

Reciprocal depth is interpolated linearly across screen space. A sample passes
when Q is strictly greater than the stored depth. The application chooses the
scale; the flight program uses floor(1,000,000/camera_z). Zero depth does not pass
an initially cleared depth buffer. Depth is internal GPU state, not bus-mapped.
CLEAR resets it to zero along with the working color image. PRESENT copies color
only. With depth disabled, commands overwrite color without changing depth.

LINE also supports DEPTH, interpolating Q1/Q2 along the clipped screen segment.
PIXEL remains an unconditional color write; its behavior is unchanged. HUD lines
use disabled depth. TRIANGLE costs one guest tick, independent of covered area;
this models an accelerated device operation, not host rasterization time.

## 8. Navigation input extension

Input identifiers append M=6 and TAB=7, retaining all previous IDs. Defined
per-key registers extend to 0x0200107F. Held, pressed, released, and acknowledgement
semantics are unchanged. The frontend maps physical M and Tab scancodes. These
keys have no special host-side UI behavior; the guest implements their actions.

## 9. Second input bank

The original eight keys retain their addresses. Key T has identifier 8 and occupies
slot 0 of bank 1 at INPUT_BASE+0x100. Each bank has eight slots and the same four
register blocks: HELD +0x00, PRESSED +0x20, RELEASED +0x40, ACK +0x60. T occupies slot 0; F (identifier 9) occupies slot 1. Other slots fault. The gaps between banks remain
unmapped. SDL T/F events update this device state; cruise semantics belong to the
guest, not the frontend.

## Guest clock readout

`CLOCK_BASE` (0x02002000) returns the low 32 bits of the 6 MHz guest tick counter.
Reads observe time before the reading instruction completes. The register resets
to zero and wraps modulo 2^32; unsigned subtraction measures intervals shorter
than one wrap. Writes, other widths, and adjacent registers fault. This readout
does not add timer events, interrupts, or generalized waits.
