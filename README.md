# CGVM

| Draws with great difficulty.

1. Requires libPNG, SDL3.

CGVM is an experimental fantasy computer for rendering and small games.
For the next development session, read the [session handoff](doc/handoff.md).
See the [machine architecture draft](doc/machine.md) for the
proposed CPU, memory, device, timing, graphics, and input architecture. That
document distinguishes the planned machine from the current prototype.

## The end


| TODO | NOT TODO |  
+------+----------+  
| REMOVE | DEPS |  

![](./_cgvm.png)

## References and validation

- [Instruction Set Reference](doc/isa.md): implemented CPU contract and opcode table.
- [Implementation Roadmap](doc/roadmap.md): migration status and pending devices.
- `sh tests/run.sh`: headless CPU conformance tests; requires a C23 compiler.
- `bash bismilla.sh`: build the SDL demo; requires SDL3 and libPNG.

The machine core is `src/vm/machine.c`; it can be linked without SDL or libPNG.
Machine instances are large and should be allocated statically or on the heap.

- [Bus and Graphics Reference](doc/devices.md): address map, GPU registers, clock, and frame waiting.

## Interactive demo

Run `bash bismilla.sh && ./cgvm.elf` to launch Astra near Earth in a realistically
scaled Solar System. Planet positions are fixed demonstrations, not live ephemerides.

- **Arrow keys:** manual steering; **Z/X:** throttle/braking.
- **F:** cycle FLIGHT (0–9,000 km/s), SYSTEM (0–900,000 km/s), and LOCAL
  (0–10 m/s), resetting throttle. Starts in FLIGHT.
- **Tab:** cycle Sol, planets including Pluto, 13 moons, Ceres and Vesta.
- **T:** cruise to the destination, or cancel. **X** also cancels cruise.
- **M:** show/hide the system map.
- Close the window to exit and save `test.png`.

Cruise is fictional high-speed travel. It stops outside the destination and checks
the entire movement segment against intervening bodies. The map compresses orbital
spacing; the flight view uses physical body scale. Distances show explicit units.

See the [Solar System program reference](doc/solar-system.md) for source data,
travel rules, precision, and limitations. Earlier local demos remain in the source
tree and regression suite. The map marks the main asteroid belt between Mars and
Jupiter. Its kilometre-scale asteroids are now generated inside the guest from
region coordinates and a fixed seed, with a 27-region cache around the ship.
Returning to a region reproduces its rocks. The simplified sparse distribution
covers the belt without a special encounter group near Ceres.
Unnamed asteroids have no destination entries, map symbols, labels, or distant
point markers. Dark, physical-scale geometry appears when large enough on screen.
Cruise checks every crossed region neighbourhood for collisions, including rocks
outside the rendering cache. Orbital motion and smaller debris remain future work.

Planets grow through point, coarse-sphere, and detailed-sphere levels according
to their apparent size, including non-destination flybys. Cruise holds a steady
1 approximate AU/s in guest time and brakes immediately at arrival, retaining
swept collision checks. The cockpit now shows cruise speed. Labels have ten times
the previous angular range plus forward sensor lookahead; selected destinations
remain identified at any distance within the viewport.

Flight labels use solid lettering and opaque backgrounds. The selected destination
has priority, followed by bodies with larger apparent size. Labels move apart and
avoid the map; excess labels are omitted when there is no room.

The sparse belt still requires a close pass to see a rock: a 0.5 km-radius object
reaches one pixel at about 200 km with this camera, while typical neighbours are
millions of kilometres away. Faster flight does not guarantee visual encounters.
