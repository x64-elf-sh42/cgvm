# Implementation status and limitations

CGVM is an experimental virtual computer with an SDL frontend and headless
machine core. The [machine architecture draft](machine.md) describes the proposed
design; the [ISA](isa.md) and [bus reference](devices.md) define implemented
behavior.

## Implemented capabilities

- A machine instance owns processor state, code, memory, devices, and guest time.
  The core runs without SDL or libPNG.
- A fixed memory bus provides RAM, video memory, GPU registers, input registers,
  and a guest clock readout. Frame publication runs at 60 Hz against a 6 MHz
  guest clock; `WAIT_FRAME` supports frame synchronization.
- Graphics include clipped lines, filled triangles, and reciprocal-depth testing.
  Guest programs perform 3D transforms, clipping, shading, and projection.
- Input exposes held keys and latched press/release transitions, including
  focus-loss release in the SDL frontend.
- [Astra](solar-system.md) provides physical-scale Solar System flight, binary64
  world coordinates, manual flight ranges, destination cruise, a system map,
  contextual moon labels, grouped destinations, a heading/belt-course display,
  an on-demand rock scanner with a finite range and persistent contact lock,
  bearing hold, assisted rock approaches, and proximity warnings,
  deterministic regional asteroids, and swept collision checks.
  Cruise requires its own flight mode and rejects destinations already nearby.
- Headless regression tests cover CPU behavior, devices, rendering, input,
  navigation, procedural generation, and interplanetary travel. Run
  `sh tests/run.sh` from the repository root.

## Current limitations

- The device map is fixed. Dynamic device registration, an independent timer
  device, generalized event waits, and timestamped input replay are not implemented.
- The SDL frontend is a singleton adapter. Legacy graphics instructions retain
  separate caller-attached buffers; optional legacy PMC integration is not wired
  into the frontend.
- Solar System positions are static demonstrations. Orbital motion, gravity,
  atmospheric flight, and planetary landing are not simulated.
- Cruise follows a direct route and stops when obstructed; it does not find a
  path around bodies.
- The procedural belt uses a simplified sparse distribution. Smaller debris,
  measured size/albedo distributions, resonance gaps, and asteroid families are
  not modeled.
- Mesh detail changes without blending, and body-label layout has no temporal
  smoothing.

See the [Solar System reference](solar-system.md) for detailed rendering, travel,
and precision limits. These limitations do not imply a scheduled feature roadmap.
