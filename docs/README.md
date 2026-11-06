# CGVM documentation

For build instructions, controls, and headless validation, start with the
[project README](../README.md).

## Current references

- [Solar System flight](solar-system.md): Astra's world model, controls, travel,
  rendering, procedural asteroid belt, source data, and limitations.
- [Instruction set reference](isa.md): implemented CPU behavior, instruction
  encoding, opcodes, and faults.
- [Bus and graphics reference](devices.md): implemented memory map, GPU, clock,
  frame publication, and input registers.

## Architecture and planning

- [Machine architecture draft](machine.md): proposed machine design. Consult the
  ISA and bus references for the implemented contracts.
- [Implementation status and limitations](roadmap.md): current capabilities and
  unimplemented facilities.

## Earlier demonstrations

These documents describe local test scenes retained in the source tree and
regression suite. The current default program is Solar System flight.

- [Tetrahedron](tetrahedron.md): fixed-camera wireframe transforms and controls.
- [Flight](flight.md): local flight, chase camera, clipping, and named navigation.
- [Solid scene](solid-scene.md): local planet and sun, shading, and surface exclusion.
- [Navigation display](navigation.md): the local Terra/Sol cockpit and system chart.

