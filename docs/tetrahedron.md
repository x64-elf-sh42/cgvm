# Tetrahedron Demonstration Program

Status: implemented. Requires ISA/device revision 0.4.

This earlier demonstration renders a green tetrahedral wireframe from a fixed camera.
The current default program is [Solar System flight](solar-system.md).
All vertex transforms, perspective division, input handling, and distance limits
execute as guest instructions. The GPU performs screen-space line rasterization.

| Input | Operation while held |
| --- | --- |
| Left / Right | Decrease / increase yaw |
| Up / Down | Decrease / increase pitch |
| X / Z | Decrease / increase camera-relative distance |

Opposing controls cancel. Releasing controls stops changes immediately at the
next input sample. These are position/orientation controls, not acceleration or
inertial flight controls. The camera remains fixed at the origin, looking along
positive Z. The model center remains on the camera axis.

Yaw and pitch use 256 discrete steps per revolution and advance one step per
published frame. Guest RAM contains binary32 sine and cosine tables generated
from sin(2*pi*i/256) and cos(2*pi*i/256). Lookup prevents cumulative rotation drift.
Yaw is applied before pitch. The initial indices are 24 and 12 respectively.
Distance is an integer in hundredths, initially 600, clamped to [350,1200], and
changes by one per frame. Frame publication remains 60 Hz.

The model vertices are (0,1.2,0), (-1,-0.8,-0.8), (1,-0.8,-0.8), and
(0,-0.8,1.2). Every distinct vertex pair forms an edge. Projection uses
screen_x=512+400*x/z and screen_y=384-400*y/z. The demo targets the current fixed
1024x768 display. Projected coordinates truncate toward zero before line submission.

The model radius is less than 1.6 units. The minimum center distance of 3.5
therefore keeps every vertex at positive depth for all rotations. General
near-plane clipping is not implemented. Distance limits are a demo constraint,
not a GPU clipping policy. A freely moving camera or unrestricted vehicle will
require near-plane clipping before projection.

R9 and R10 hold angle indices; R11 holds distance. F0–F3 hold sine/cosine values,
F4 holds distance, and F5–F15 are transformation temporaries. Guest RAM at 4096
holds four projected signed integer (x,y) pairs. Code emission is bounded; branch
fixups remain local to the emitter. A general assembler is not required by this
program.

Headless tests compare guest projections with independent host equations, compare
initial frames across CPU budgets, exercise rotations and distance limits, and
check line clipping and conversion faults. The earlier box demo remains in the
source tree and its input/clamping tests remain active.
