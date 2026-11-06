# Flight Demonstration Program

Status: implemented. Requires ISA revision 0.5 and device revision 0.4.

This reference describes the earlier local test scene. The current default program
is [Solar System flight](solar-system.md).

## Controls and state

| Input | Operation |
| --- | --- |
| Left / Right | Decrease / increase yaw |
| Up / Down | Decrease / increase pitch (Up pitches upward) |
| Z | Accelerate forward at 2 world units/s² |
| X | Decelerate at 2 world units/s² |

Opposing controls cancel. Forward speed is clamped to [0,10] world units/s.
Releasing throttle keys preserves speed. No reverse motion or inertia-based turning is implemented. The solid-body
exclusion guard described in the solid-scene reference corrects inward motion
at planet and sun boundaries while preserving throttle and sideways movement; asteroids remain non-colliding. Focus loss releases
keys but does not reset speed.

The simulation updates once per 60 Hz publication. R9 and R10 contain angular
indices in [0,255]. Each held steering key changes its angle by one step per
update, wrapping at a revolution. R11 contains speed in units of 1/30 world
unit/s, clamped to [0,300]. Each throttle update changes R11 by one. Position
advances by R11/1800 world units along the current forward direction per update.
Thus acceleration from rest to maximum speed takes 300 updates (5 seconds).

Position is stored as three binary32 values at guest addresses 8192, 8196, and
8200. Input handling, acceleration, limits, movement, transforms, clipping, and
projection all execute in guest code. The host generates no runtime flight state.

## Coordinate systems and camera

World axes are X right, Y up, Z forward at initial orientation. For yaw a and
pitch b, the forward vector is (sin(a)*cos(b), -sin(b), cos(a)*cos(b)). Positive
pitch indices therefore point downward; Up decrements pitch. Movement immediately
follows the current heading without sideways drift.

For relative world position (x,y,z), camera coordinates are:

    camera_x = x*cos(a) - z*sin(a)
    intermediate_z = x*sin(a) + z*cos(a)
    camera_y = intermediate_z*sin(b) + y*cos(b)
    camera_z = intermediate_z*cos(b) - y*sin(b) + 6

The camera follows heading and position rigidly, six units behind the reference
position. There is no camera smoothing or banking. The green tetrahedron is a
camera-relative visual marker, offset below center to preserve forward visibility;
it is not independently simulated geometry or a collision body.

Perspective uses focal length 400 pixels and center (512,384). Edges are clipped
against camera_z=0.5 before perspective division. If both endpoints are behind the
plane, the edge is rejected. If one is behind, linear interpolation replaces that
endpoint with its near-plane intersection. Edges with both depths greater than
120 are rejected; this is depth rejection, not spherical range culling or exact
far-plane clipping. The GPU clips projected lines to the viewport.

## Scene

The field consists of 24 stationary tetrahedral wireframes with seeded positions,
scales, and orientations. Initial centers occupy X in [-24,24], Y in [-14,14],
and Z in [10,90]. The seed is 7001. Scene values are stored as binary32 constants
in flight_data.h. Regenerate from the repository root with:

    python3 scripts/generate_flight_data.py

The field is finite. Objects are not recycled or generated during flight. Leaving
the field exposes empty space; turning back preserves the original world layout.
There are no textures, hidden-line removal, collision, or procedural sectors.
The current display format is fixed at 1024x768.

The lower-left gauge has a 300-pixel rail. Its green length equals R11, indicating
0–10 units/s. The green vehicle remains visible while nearby objects cross the
camera plane. All frames use the existing 60 Hz publication contract.

## Validation

Headless tests check repeatable output across CPU budgets, idle stability, speed
bounds, coasting displacement, braking, opposing controls, directional movement,
and camera-plane crossings. A constructed crossing edge checks the exact projected
near-plane intersection. ASan/UBSan cover the flight integration path. Previous
CPU, box, and tetrahedron tests remain active.

The local scene includes the planet and sun described in the
[Solid Scene Demonstration](solid-scene.md). The earlier wireframe-only description
above applies to the asteroid field; the GPU depth-tests its lines against
the solid bodies.

## Named navigation and steering

The test planet is named Terra and the test star Sol. These names identify the
fictional local test bodies; their sizes and positions are not astronomical data.

AHEAD reports the first intersection of the vehicle's forward ray with a body's
safety sphere. The ray originates at vehicle position, not the chase camera, and
uses the current commanded heading, regardless of throttle. For relative center
c and unit forward f, the guest computes b=dot(c,f) and
h=b*b-dot(c,c)+radius*radius. A candidate with b<=0 or h<0 is rejected. Otherwise
the entry distance is max(0,b-sqrt(h)); the smaller accepted distance wins. Exact
ties retain Terra. The scene assumes the vehicle starts outside both spheres.
The trig lookup approximates a unit vector to binary32 precision; near-tangent
classifications inherit that precision.

AHEAD CLEAR means neither named safety sphere intersects this ray. It does not
certify clearance from asteroids or arbitrary geometry. Displayed distances are
whole world units, saturated at 999. They describe ray travel to the safety
boundary, not surface altitude or center distance. The ray calculation executes
in guest code; there is no host raycasting helper.

NEAR identifies the body used by the existing clearance gauge: minimum radial
distance to either safety sphere. It remains active even when the ship points
away. PROXIMITY includes that name. SURFACE LIMIT names the body responsible for
the latest contact while its half-second indicator is active.

Guest navigation state: 8412 stores nearest body ID, 8416 latest contact body ID,
8420 the ahead entry distance, and R15 the ahead ID (0=none, 1=Terra, 2=Sol).
The no-hit distance sentinel is 999999; it is not shown as a valid hit.
Addresses 8432–8447 contain forward-vector and ray temporaries.

Yaw controls compensate for inverted pitch: when cos(pitch)<0, Left/Right reverse their
world-yaw increments so their screen-relative effect remains consistent. Pitch
still permits full loops. This remains a yaw/pitch camera, not a quaternion-based
flight model; at exact vertical orientation world-yaw has no directional leverage.
