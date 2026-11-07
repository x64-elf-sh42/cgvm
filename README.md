# CGVM

> Draws with great difficulty.

CGVM is an experimental fantasy computer for rendering and small games. Its
default demo, Astra, is a guest-programmed flight game in a realistically scaled
Solar System. Planet positions are fixed demonstrations, not live ephemerides.

![CGVM rendering example](./_cgvm.png)

## Build and run

Requires a C23 compiler, Bash, pkg-config, SDL3, and libPNG. Run from the
repository root:

```sh
bash bismilla.sh
./cgvm.elf
```

Close the window to exit and save the final frame to `test.png`.

## Controls

| Key | Action |
| --- | --- |
| Arrow keys | Manual steering |
| Z / X | Accelerate / brake; X also cancels cruise |
| F | Cycle FLIGHT, SYSTEM, LOCAL, and CRUISE; stop and cancel travel |
| G | Switch destination group: planets, named planet systems, or asteroid belt |
| Tab | Select a destination within the current group |
| T | Engage or cancel travel in CRUISE mode; nearby destinations are blocked |
| S / C | Scan for a nearby asteroid / clear the scan result |
| L | Aim at the scanned rock; throttle and braking remain manual |
| R | In SYSTEM mode, cruise to the scanned rock and stop automatically; press again to cancel |
| M | Show or hide the system map |

The game starts in FLIGHT mode (0–9,000 km/s); SYSTEM reaches 3,000,000 km/s and
LOCAL reaches 100 m/s. Select CRUISE with F before pressing T. Cruise is fictional
high-speed travel that stops outside the destination and checks the route for
collisions. The map compresses orbital spacing; the flight view uses physical
body scale. Moon labels appear near their parent planet. The navigation panel
shows your heading and a course along the asteroid belt. Press S to scan within
50 million km; a detected rock stays locked with live bearing and range until
you clear it, rescan, or move out of range. The scan panel appears only after
a scan. Procedural asteroids are sparse and their geometry appears only at
close range.

Scanning is a pulse: leaving the panel open does not search new areas. Press S
again while exploring. The cockpit shows a throttle bar and percentage; SYSTEM
speed uses two decimal places in millions of km/s so partial throttle is visible.

For an asteroid approach, scan with S, select SYSTEM with F, then press R. Fly-to
keeps the rock centered, travels at SYSTEM speed, and stops automatically
50 metres outside its bounding radius in LOCAL mode. A fresh steering or brake
press cancels cruise; already-held controls cannot cancel it or restart flight
after arrival. Asteroid
proximity and collision-course warnings help with manual approaches, and the
manual collision stop allows about 11 metres of bounding-radius clearance.
World labels respect scene depth, so distant tags cannot cover a nearer asteroid.

See the [Solar System flight reference](docs/solar-system.md) for travel rules,
rendering, source data, and limitations.

## Validation

Run the headless regression suite with a C23 compiler; SDL3 and libPNG are not
required:

```sh
sh tests/run.sh
```

The machine core is `src/vm/machine.c`; it can be linked without SDL or libPNG.
Machine instances are large and should be allocated statically or on the heap.

## Documentation

- [Documentation index](docs/README.md): all references and earlier demo documentation.
- [Instruction set reference](docs/isa.md): implemented CPU contract and opcode table.
- [Bus and graphics reference](docs/devices.md): address map, GPU registers, clock, and input.
- [Machine architecture draft](docs/machine.md): proposed architecture; distinguishes planned facilities from implemented behavior.
- [Implementation status and limitations](docs/roadmap.md): current capabilities and unimplemented facilities.
