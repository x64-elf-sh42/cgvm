# CGVM Instruction Set Reference

Revision: 0.6. Status: implemented CPU contract.

## 1. Architectural state

The processor contains sixteen 32-bit unsigned integer registers R0–R15 and
sixteen host `double` floating-point registers F0–F15. R4 is PC. R0–R3 retain the
names BP, SP, DS, SS but have no implicit addressing or stack behavior. F0 retains
the legacy name DT; the core does not update it from host time.

Reset clears registers, flags, memory, loaded code length, and fault state.
Execution is runnable. Fetch without a loaded program faults. Loading a valid
program resets the machine, copies code and optional data, and sets PC to entry_pc.
An invalid load returns false without modifying the machine. Loading detaches
transitional graphics buffers; the caller must attach them again.

PC and branch targets are instruction indices. Code and data occupy separate
arrays: CODE_SIZE instruction words and DATA_SIZE bytes (currently 16,777,216
each). Code is not writable by guest instructions. Data accesses are byte-sized;
endianness and alignment do not affect this revision. Register-addressed bus operations are specified in section 7.

R4 reads return the current instruction index. Integer instructions that would
write R4 fault before changing architectural operands. Branch instructions modify
PC. Floating-point register F4 is an ordinary register.

## 2. Encoding

| Format | Bits 31:24 | Bits 23:20 | Bits 19:16 | Bits 15:0 |
| --- | --- | --- | --- | --- |
| RR | Opcode | Destination/left register | Source/right register | Ignored |
| RI | Opcode | Register | Immediate[19:16] | Immediate[15:0] |
| I24 | Opcode | Immediate[23:20] | Immediate[19:16] | Immediate[15:0] |
| OP | Opcode | Ignored | Ignored | Ignored |

Immediates are unsigned and zero-extended. Ignored bits do not affect execution.
Undefined opcode values fault. Opcode zero is invalid. Encodings are retained
from the prototype; mnemonic suffixes for memory transfers are historical.

## 3. Flags and arithmetic

Comparison flags are EQ=0x1000, LT=0x2000, GT=0x4000, NE=0x8000.
Comparison instructions replace these four bits and preserve all other bits.
All other instructions preserve flags, including EXIT and faults.

Integer arithmetic wraps modulo 2^32. Integer division truncates toward zero.
Division by zero faults without changing the destination. Comparisons are unsigned.
No arithmetic instruction sets carry or overflow flags.

Floating-point operations use host C double arithmetic. This revision does not
specify a portable floating-point environment, NaN payloads, or exception traps.
FCMP compares floating operands directly: NaN sets NE and clears EQ, LT, GT.
Portable floating-point reproducibility remains outside this transitional contract.

## 4. Instruction definitions

Notation: D and S denote encoded register indices; I denotes the immediate.
R and F denote integer and floating-point banks. M denotes data memory.
Unless specified otherwise, successful execution increments PC by one.
Each completed instruction consumes one execution-budget unit and one guest tick.
WAIT_FRAME and the mapped-device instructions are specified in section 7.

| Opcode | Mnemonic | Format | Operation |
| --- | --- | --- | --- |
| 0x00 | UNDEFINED | OP | Invalid opcode; fault |
| 0x01 | EXIT | OP | Halt; PC := PC + 1 |
| 0x02 | YIELD | OP | Yield to caller; PC := PC + 1 |
| 0x03 | MOV_RR | RR | R[D] := R[S] |
| 0x04 | MOV_RI | RI | R[D] := I |
| 0x05 | MOV_RM | RI | M[I] := low8(R[D]) |
| 0x06 | MOV_MR | RI | R[D] := zero_extend(M[I]) |
| 0x07 | ADD_RR | RR | R[D] := R[D] + R[S] |
| 0x08 | ADD_RI | RI | R[D] := R[D] + I |
| 0x09 | SUB_RR | RR | R[D] := R[D] - R[S] |
| 0x0A | SUB_RI | RI | R[D] := R[D] - I |
| 0x0B | MUL_RR | RR | R[D] := R[D] * R[S] |
| 0x0C | DIV_RR | RR | R[D] := R[D] / R[S] |
| 0x0D | FADD_RR | RR | F[D] := F[D] + F[S] |
| 0x0E | FSUB_RR | RR | F[D] := F[D] - F[S] |
| 0x0F | FMUL_RR | RR | F[D] := F[D] * F[S] |
| 0x10 | FDIV_RR | RR | F[D] := F[D] / F[S] |
| 0x11 | CMP_RR | RR | Compare R[D], R[S] |
| 0x12 | CMP_RI | RI | Compare R[D], I |
| 0x13 | FCMP_RR | RR | Compare F[D], F[S] |
| 0x14 | JMP | I24 | PC := I |
| 0x15 | JE | I24 | If EQ: PC := I; else PC := PC + 1 |
| 0x16 | JNE | I24 | If NE: PC := I; else PC := PC + 1 |
| 0x17 | JG | I24 | If GT and NE: PC := I; else PC := PC + 1 |
| 0x18 | JL | I24 | If LT and NE: PC := I; else PC := PC + 1 |
| 0x19 | JGE | I24 | If GT or EQ: PC := I; else PC := PC + 1 |
| 0x1A | JLE | I24 | If LT or EQ: PC := I; else PC := PC + 1 |
| 0x1B | SET_PIXEL | I24 | Write RGB I at (R5, R6); out-of-range coordinates have no effect |
| 0x1C | FB_CLEAR | I24 | Fill working framebuffer with RGB I |
| 0x1D | FB_SWAP | OP | Copy working framebuffer to presentation buffer |

## 5. Execution and faults

`cpu_step(machine, budget)` returns status and completed instruction count.
A zero budget executes nothing. Budget exhaustion leaves the processor runnable.
YIELD returns immediately, including when it consumes the last budget unit.
HALT and FAULT persist across subsequent calls; subsequent calls execute nothing.
Reset or successful load clears these terminal states.

| Fault | Condition | State |
| --- | --- | --- |
| FETCH | PC outside loaded code | PC identifies attempted fetch |
| OPCODE | Opcode zero or unassigned | PC identifies rejected instruction |
| DIV_ZERO | Integer divisor is zero | Destination unchanged |
| PC_WRITE | Integer destination is R4 | Destination unchanged |
| DEVICE | Graphics instruction without both buffers | Buffers unchanged |

Faulting instructions consume no budget unit. Earlier completed instructions
remain committed. The fault code and fault PC are recorded separately from flags.
An invalid branch target faults on the next attempted fetch; the branch itself
completes. A budget boundary may therefore occur between the branch and fault.
Fault recovery through guest software is not implemented.

## 6. Transitional graphics attachment

SET_PIXEL, FB_CLEAR, and FB_SWAP require caller-owned working and presentation
buffers. Both must contain width * height pixels, with valid non-overlapping
storage for copies. The host is responsible for validating attachment dimensions
and lifetimes. The pixel layout is three bytes in red, green, blue order. Immediate
color encoding is 0xRRGGBB.

These instructions execute synchronously and do not call host graphics APIs.
FB_SWAP completes a copy; it does not wait for a display tick or produce a device
event. The memory-mapped GPU and PRESENT lifecycle in the architectural draft
remain planned replacements.

## 7. Revision 0.2 additions

These instructions are available starting with revision 0.2. Existing opcode
values are unchanged. All completed instructions advance the guest clock by one tick.
The [Bus and Graphics Reference](devices.md) defines mappings and event ordering.

| Opcode | Mnemonic | Format | Operation |
| --- | --- | --- | --- |
| 0x1F | LOAD8 | RR | R[D] := zero_extend(bus8[R[S]]) |
| 0x20 | LOAD16 | RR | R[D] := zero_extend(bus16[R[S]]) |
| 0x21 | LOAD32 | RR | R[D] := bus32[R[S]] |
| 0x22 | STORE8 | RR | bus8[R[S]] := low8(R[D]) |
| 0x23 | STORE16 | RR | bus16[R[S]] := low16(R[D]) |
| 0x24 | STORE32 | RR | bus32[R[S]] := R[D] |
| 0x25 | LUI | RI | R[D] := I << 12 |
| 0x26 | WAIT_FRAME | OP | Acknowledge pending GPU completion or wait |

These instructions preserve flags. Loads and LUI targeting R4 cause PC_WRITE.
Invalid bus transfers cause BUS, preserve destination and device state, and record
fault_address. Stores may read R4. Bits 15:0 of RR encodings remain ignored.
LUI permits construction of a full address with a subsequent ADD_RI of its low
12 bits. Legacy immediate byte transfers still address RAM directly.

WAIT_FRAME advances PC before waiting. It returns CPU_WAIT if completion was
absent at instruction entry; otherwise it consumes completion and returns runnable
(CPU_BUDGET). It returns to the caller in either case. A completion generated at
its retirement tick remains available to the scheduler. This instruction is a
GPU-specific wait; generalized event masks remain unassigned.

## 8. Revision 0.4 floating-point transfers

| Opcode | Mnemonic | Format | Operation |
| --- | --- | --- | --- |
| 0x27 | FLOAD32 | RR | F[D] := binary32 value read through bus at R[S], widened to host double |
| 0x28 | FMOV_RR | RR | F[D] := F[S] |
| 0x29 | ITOF | RR | F[D] := signed_two_complement_32(R[S]) |
| 0x2A | FTOI | RR | R[D] := signed32(truncate_toward_zero(F[S])) |

Flags are preserved. Each completed instruction costs one tick. FLOAD32 follows
32-bit bus access validation and faults with BUS before changing F[D] on failure.
The host must support IEEE-style binary32 (32-bit storage, radix 2, 24-bit mantissa).
Memory bytes use the bus little-endian convention. Floating-point register index
4 is legal. FTOI targeting integer R4 faults with PC_WRITE.

FTOI accepts only finite values in [-2147483648,2147483647], before truncation.
Other inputs cause CONVERSION, preserving destination and PC and consuming no
tick. Negative integer results are encoded in two's complement. ITOF interprets
its integer source as signed; integer arithmetic elsewhere remains unsigned.

## 9. Revision 0.5 floating-point store

| Opcode | Mnemonic | Format | Operation |
| --- | --- | --- | --- |
| 0x2B | FSTORE32 | RR | bus32[R[S]] := binary32(F[D]) |

FSTORE32 narrows the source to host binary32 and writes its bits through the bus
in little-endian order. Flags and floating-point source are preserved. The
instruction costs one guest tick on success. Non-finite inputs or magnitudes
larger than FLT_MAX cause CONVERSION before bus access. Invalid destination
accesses cause BUS and record fault_address. No state is written on either fault.
Signed zero and subnormals follow the host floating-point environment. Rounding
follows the host conversion; cross-host bit-identical floating point is not a
contract of this revision. D identifies a floating register, so F4 is legal.

## 10. Revision 0.6 binary64 memory transfers

| Opcode | Mnemonic | Format | Operation |
| --- | --- | --- | --- |
| 0x2C | FLOAD64 | RR | F[D] := binary64 RAM value at R[S] |
| 0x2D | FSTORE64 | RR | RAM binary64 at R[S] := F[D] |

Transfers preserve all floating-point bits and use little-endian byte order.
They are RAM-only, require 8-byte alignment, and validate all eight bytes before
any access. Invalid addresses fault with BUS and record fault_address. Device and
video mappings are not supported by these instructions. F4 is legal. Flags are
preserved and each successful instruction consumes one tick. Faulting instructions
consume none. Hosts require 8-byte binary64 double representation.
