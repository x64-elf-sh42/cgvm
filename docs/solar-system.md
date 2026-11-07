# Solar System Flight Program

Status: implemented. Default demonstration program. Requires ISA revision 0.6,
device revision 0.6, and the second input bank. The original fictional Terra/Sol flight program remains
available through flight() for regression tests; solar_flight() is the default.

## World model and source data

World positions use kilometres in heliocentric binary64 coordinates. Each body is
a sphere. Planet radii are half the rounded equatorial diameters in the NASA
[Planetary Fact Sheet](https://nssdc.gsfc.nasa.gov/planetary/factsheet/).
The solar mean radius follows the
[Sun Fact Sheet](https://nssdc.gsfc.nasa.gov/planetary/factsheet/sunfact.html).

| Body | Radius (km) | Nominal Sun distance (million km) |
| --- | --- | --- |
| Sol | 695700 | 0 |
| Mercury | 2439.5 | 57.9 |
| Venus | 6052 | 108.2 |
| Earth | 6378 | 149.6 |
| Mars | 3396 | 228.0 |
| Jupiter | 71492 | 778.5 |
| Saturn | 60268 | 1432.0 |
| Uranus | 25559 | 2867.0 |
| Neptune | 24764 | 4515.0 |
| Pluto | 1188.3 | 5906.4 |

Orbital angles are fixed demonstration placements, not current ephemerides.
Bodies lie in one plane, without orbital motion, obliquity, gravity, rings,
atmospheres, or orbital asteroid encounters. The program models physical scale
and system travel; it is not an orbital simulator. The local scene remains
available as a separate local-flight regression fixture.

Astra starts on the sunward side of Earth at three Earth radii from its center.
Its local position changes are calculated in binary64. Body-center minus observer
position is computed before adding local mesh offsets and projecting. This retains
sub-metre movement at outer-system coordinates without requiring a sector system
for this bounded Solar System. Mesh offsets and clipped camera vertices remain
binary32; those approximations affect mesh detail, not stored world position.

## Controls and travel

| Control | Function |
| --- | --- |
| arrow keys | Manual steering; release rock guidance if active |
| Z / X | Accelerate / brake in the selected manual flight mode |
| F | Cycle FLIGHT (0–9,000 km/s), SYSTEM (0–3,000,000 km/s), LOCAL (0–100 m/s), CRUISE; stop and cancel travel |
| G | Cycle destination group; cancel travel and reset throttle |
| Tab | Cycle destinations within the current group; cancel travel and reset throttle |
| T | Engage/cancel travel only in CRUISE mode; reject destinations already nearby |
| X during cruise | Cancel cruise immediately |
| S | Scan or rescan for the nearest asteroid within 50 million km |
| C | Clear the scan result and hide its panel |
| L | Toggle bearing hold on the scanned asteroid in a manual flight mode |
| R | Toggle rock cruise with automatic stopping; requires SYSTEM mode |
| M | Toggle the complete system-map panel |

Local flight preserves throttle on key release. Cruise follows the straight line
to the selected body's center at a maximum 1 approximate AU/s. The display unit
AU uses 149.6 million km, matching the rounded source distances. This intentionally
fictional speed is 149.6 million km/s; no relativistic or acceleration model applies.
It is separate from manual throttle. Default FLIGHT reaches 9,000 km/s;
SYSTEM reaches 3,000,000 km/s for large manual detours and belt exploration;
LOCAL provides close-approach speeds up to 100 m/s. These are fictional powered travel
ranges, not orbital mechanics. Each range has 300 throttle increments; Z/X change
one increment per update (about five seconds to full throttle at 60 updates/s).
Holding throttle steady retains it on key release. Actual manual displacement,
like cruise, scales with elapsed guest time, so rendering load does not change
the selected speed. All modes retain arrow keys steering and swept collision protection.
F advances once per press, resets throttle, and cancels cruise. Flight mode is
stored at 8484 (0=flight, 1=system, 2=local, 3=cruise). CRUISE READY is stationary
until a fresh T press engages travel; Z does not accelerate in this mode. A T
press in a manual mode only displays SELECT CRUISE MODE, preserving heading,
throttle, and the selected range. Held T does not engage when F changes modes.
Automatic obstacle routing remains unimplemented.

Before engaging cruise, the guest checks the selected body's center distance.
At or within max(five body radii, twice the arrival radius), travel is rejected
with ALREADY NEARBY, preserving position and heading. This includes reselecting
the body just reached and selecting Earth at the initial position.

Cruise holds 149.6 million km/s (1 approximate AU/s) until the final step,
which is clipped exactly to the arrival shell. There is no distance-based easing
or flyby slowdown. Braking is instantaneous and fictional. Rendered travel uses
the elapsed 6 MHz guest clock, so expensive frames do not reduce cruise speed.
The physics-only probe uses fixed 1/60-second updates. The cockpit shows actual
step distance divided by its update duration, with explicit distance units per
second. Host execution slower than guest real time can still slow presentation.

For Sol and the primary planets including Pluto, destination arrival occurs at
radius + max(20% of radius, 1000 km). Moons, Ceres, and Vesta use a 20 km minimum
margin instead of 1000 km. The final step is shortened to this distance.
Travel then stops in CRUISE READY and displays
ARRIVED. Arriving at a planet with catalogued moons opens that planet's destination
group, keeping the planet selected. Being already within the arrival zone also
exits immediately.
The ship's heading is aimed along the route using the existing 256-step angular
lookup tables; translational movement uses the continuous normalized direction.

Every proposed movement step, local or cruise, is swept against the 25 catalogue
safety spheres and procedurally generated rocks along the complete segment. Planet safety radii are physical radius + 100 km; the Sun uses +10000 km.
The earliest forward entry within the step shortens it to one km before the
boundary, cancels travel/guidance, resets local throttle, and displays ROUTE BLOCKED.
Procedural asteroids use a bounding-radius-plus-10-metre safety sphere with a
1-metre entry margin, stopping about 11 metres outside the bounding radius and
displaying ASTEROID LIMIT. Named-body margins remain unchanged.
The implementation computes perpendicular distance to the ray explicitly to avoid
subtracting nearly equal astronomical squared distances. There is no route finding:
choose another destination or change position if a body obstructs the direct route.
Valid initial positions are outside the safety spheres. Large-speed surface sliding
and planetary landing are not implemented.

All input decisions, position updates, route aiming, swept tests, clipping, shading,
and projection execute as guest instructions. Host C builds constants and encoded
program words at load time, but does not perform runtime flight or astronomy.

## Display and rendering

The lower cockpit retains Astra identification, local/cruise state, selected
body and center range, nearest safety-boundary clearance, arrival/obstruction
status, and controls. Ranges use KM, KKM (thousand km), MKM (million km), or AU
according to magnitude. Values are whole units of the indicated scale. Local
speed uses M S (metres per second). For procedural rocks, NEAR clearance is
measured from the bounding radius, with metres shown below one kilometre.

Manual throttle has a bar and percentage from 0 to 100 across all three manual
ranges. CRUISE shows READY or AUTO, and rock fly-to shows AUTO, instead of a manual percentage. SYSTEM speed
is displayed as X.XX MKM/s (million km/s), covering all 300 increments from
0.00 to 3.00; for example, 1.10 and 1.80 remain distinguishable. Other range
readouts use the whole-unit conventions above.

The independent map has 25 named destinations organized into nine groups:
PLANETS (Sol, the eight planets, and Pluto), EARTH SYSTEM, MARS SYSTEM, JUPITER
SYSTEM, SATURN SYSTEM, URANUS SYSTEM, NEPTUNE SYSTEM, PLUTO SYSTEM, and ASTEROID
BELT (Ceres and Vesta). Each planet-system list starts with the parent and then
its moons. G advances to the next group and selects its first entry; Tab wraps
within the group. Destinations remain selectable regardless of distance.

The map has radially compressed primary orbit rings. Satellite markers appear
only near their parent, using the same proximity rule as flight labels. They are
offset from the parent for readability; their chart separation is not to scale.
Its angular positions match the demonstration world; radial spacing is
piecewise compressed between nominal orbital distances. Astra's green marker uses
the same mapping. Beyond Pluto it is capped at the outer chart edge. This is a
navigation schematic, not a distance-preserving diagram. Astra's green marker
has a heading line whose length reflects horizontal heading. Group and destination
selection continue to work with the map hidden.

## Heading and belt discovery

The compact upper-left navigation strip displays actual forward heading and
pitch, including inverted flight. Headings use 000 degrees = world +Z, 090 = +X,
180 = -Z, and 270 = -X. Pitch shows UP or DN. These are world-axis bearings, not
Earth compass directions.

IN MAIN BELT requires a cylindrical radius of 2.2–3.2 AU and a height within
0.5 AU of the system plane; otherwise OUTSIDE BELT is shown. COURSE is a
horizontal tangent around Sol, intended for level flight through the belt.
Matching that heading with level pitch and adjusting it as the displayed course
changes helps follow the belt rather than crossing it radially.

Press S to perform a scan in all directions, choosing the nearest procedural
asteroid whose center is within a 50-million-km sphere. Nearby cached contacts
seed the search. When needed, the guest generates additional procedural regions
without enlarging the 27-cell rendering cache. Region bounds reject cells outside
the belt or farther away than the current best contact. The result is the nearest
rock in range, including rocks not loaded for rendering. The scanner is a
fictional instantaneous sensor, without radio-echo travel delays.

This is a pulse, not a continuous search. An open panel does not scan new areas;
NO CONTACT remains the result of the previous pulse until S is pressed again.
At full SYSTEM speed, one scan radius takes about 16.7 seconds of guest time to
traverse. Repeated scans are necessary when searching at speed. The navigation
strip reports IN MAIN BELT from physical coordinates; proximity to the map's
schematic ring is not sufficient. A long-range pulse can also find the belt's
rocks before the ship itself reaches the belt.

A successful scan locks that rock's world coordinates. Its compact result panel
shows ROCK center range (metres below one kilometre), heading, and pitch, updated as the ship moves. Match
those bearings to approach, then reduce speed as range falls; LOCAL provides
0–100 m/s for close inspection. A closer rock or a cache rebuild cannot switch
the lock. S performs a new scan; held S performs only one scan. C clears any
result and hides the panel. Outside guidance, scanning does not change flight
mode, heading, throttle, or the cruise destination. Rescanning or clearing releases
rock guidance and stops an active fly-to before changing the contact.

NO CONTACT means no asteroid was found within the scan sphere. CONTACT OUT OF
RANGE means a locked rock moved beyond 50 million km relative to the ship; the
lock is dropped and requires another S press to acquire again. Results do not
appear until a scan is requested. The scanner has no global catalogue,
destination entry, or in-space marker. All readings are calculated by guest
instructions. Bearings use the same 256 angular steps as steering.

## Rock guidance and close approaches

After scanning, L toggles bearing hold in FLIGHT, SYSTEM, or LOCAL. The guest
continually aims at the locked rock while throttle remains manual. The scanner
panel shows ALIGN MANUAL. L again releases the hold; steering or braking also releases
it. Bearing hold does not change the selected planetary destination.

Select SYSTEM and press R to cruise to the locked rock. The guest aims continuously,
using the exact normalized direction for translation and the existing angular
steps for camera orientation. ROCK CRUISE travels at 3 million km/s and handles
arrival without requiring the pilot to brake. The final movement step is clipped
to stop 50 metres outside the bounding radius; the ship switches
to LOCAL at rest and displays TARGET REACHED. A target already closer than this
is left at its current distance. There is no automatic retreat or position snap.

R again cancels rock cruise. A fresh steering, braking, or throttle press, changing
flight mode or destination/group, rescanning, or clearing the target releases it.
Cruise returns throttle to rest on interruption; Z can then accelerate manually.
Controls already held when R is pressed are ignored until release or a fresh
press, and cannot silently cancel cruise or resume movement after arrival.
L during rock cruise stops and changes to bearing hold. Losing the scan lock also
stops guidance. R outside SYSTEM only displays SELECT SYSTEM MODE; without a
contact the control displays SCAN FOR TARGET. Held L/R do not toggle repeatedly.

Guidance retains complete swept collision against named and procedural bodies.
An intervening obstacle stops it rather than finding a path around the obstacle.
The planetary cruise control T remains separate from rock guidance.

ASTEROID NEARBY warns within 10 km of a cached rock's bounding radius. ROCK VERY
CLOSE warns within 100 m. BRAKE ROCK AHEAD warns when the current flight ray
would enter a cached rock's safety sphere within five seconds at the current
travel speed. Rock cruise does not issue a manual braking prompt for its own
target. Other predicted obstacles are labelled OBSTACLE AHEAD during automatic
travel. Stationary ships receive distance warnings without a braking cue.
These warnings use nearby cached rocks; long-range guidance still uses its
retained scan lock and full-path collision checks.

Rock collision and arrival clearances refer to the bounding sphere enclosing the
irregular mesh. Actual visible faces can sit farther inward. There is no landing
or mesh-accurate contact solver.

## Geometry

Before vertex projection, guest code rejects body bounding spheres wholly outside
the near, left, right, top, or bottom view planes. Side-plane tests scale the
radius by the plane-normal length. This conservative check retains visible limbs
while preventing enormous off-screen bodies from producing out-of-range integer
screen coordinates after near-plane clipping. Triangle clipping and the CPU's
conversion-fault behavior remain unchanged.

Every visible body selects detail from approximate projected radius f*radius/depth,
independent of the selected destination:

| Projected radius | Representation |
| --- | --- |
| Below 1 pixel | One-pixel navigation marker |
| 1–16 pixels | 20-triangle icosahedral sphere |
| Above 16 pixels | 80-triangle shaded sphere |

Body radius and perspective scale remain unchanged across levels. The coarse mesh
and detailed mesh are inscribed approximations; there is no geometric morph or
crossfade, so small silhouette/material changes can remain at level boundaries.
For named catalogue bodies, markers deliberately preserve visibility below physical
pixel size; unnamed rocks are omitted below one pixel of projected radius. All bodies,
including ones passed en route to another destination, use the same rules.
Guest diagnostics at 8872+4*body contain 0=point, 1=coarse, 2=detailed, 3=culled.
Reciprocal depth uses 1,000,000,000/camera_z for distant views. When a cached rock
is within 10 km, the entire frame uses 1,000,000/camera_z so close depths fit
the integer device ABI without changing occlusion ordering. Named bodies retain
a 1 km near plane. Close rocks use a 1 m near plane; other rock views retain 1 km.
Astra's green triangle is a HUD reticle,
not a physically kilometre-sized ship or a chase-camera model. The previous local
asteroid scene and vehicle mesh are not rendered in this program. Generated belt
objects share an irregular 20-triangle template with seeded axis proportions and
radius scaling, dark grey-brown material, and object-relative solar illumination.

The solar HUD supersedes the local demo's AHEAD/NEAR gauge layout; actual movement
still uses swept checks against all bodies. There is no hidden assumption that a
selected destination is the only possible obstacle.

## Guest state and validation

- 8460: map visibility; 8464: destination ID (0=Sol, 1–8=planets in orbital order, 9=Pluto; 10–24 below).
- 8472: active planetary cruise (0/1); 8476: status (0=none, 1=arrived,
  2=route blocked, 3=select cruise mode, 4=already nearby, 5=scan for target,
  6=select system mode, 7=select manual mode, 8=target lost, 9=target reached,
  10=asteroid limit).
- 8484: flight mode (0=flight, 1=system, 2=local, 3=cruise); 8488: destination group (0–8).
- 8600–8623: binary64 position; 8704–8727: normalized movement direction.
- 8728: destination-center range; 8736: proposed/clipped step length.
- 20000 onward: 25 permanent 64-byte body records (center, radius, safety radius,
  arrival radius, schematic radius, nominal orbital distance).
- 21600: transient procedural body record (center, bounding radius, safety
  radius, three axis proportions); rendering and collision reuse this slot.
- 24000–25727: 27 cached procedural records; a zero radius denotes an empty cell.
- 10560–10583: cached region coordinates, stored as integer-valued binary64.
- 10800: cache rebuild count; 10804: rendered rock count; 10808: cells checked by
  the most recent movement sweep (counts repeated visits).
- 11900 + 4 * parent: moon-label and map-marker proximity flags.
- 12000 + 8 * parent: binary64 system visibility radii.
- 12104 + 8 * body: binary64 nearby-destination cruise exclusion radii.
- 12400: scanner contact flag; 12404/12408: rock heading/pitch angular indices.
- 12412/12416: actual heading/pitch indices; 12420: in-belt flag.
- 12424: binary64 rock center range; 12432–12455: relative rock vector.
- 12456: binary64 cylindrical Sun distance; 12488: belt-course angular index.
- 12500: scan result (0=idle, 1=locked, 2=no contact, 3=out of range).
- 12504–12527: locked rock's binary64 world coordinates; 12528: pending scan request.
- 12532: generated regions checked by the last pulse; 12536: region bounds visited.
- 12600–12647: scanner region-bound scratch.
- 12700: rock guidance (0=off, 1=bearing hold, 2=fly-to).
- 12704: asteroid proximity (0=clear, 1=nearby, 2=very close, 3=brake ahead).
- 12712: nearest cached rock's binary64 bounding-radius clearance.
- 12728: locked rock's bounding radius; 12736: fly-to speed in km/s.
- 12744: cached-rock presence flag; 12752–12791: warning/guidance scratch.
- 12808–12831: manual controls held at rock-cruise takeover, ignored until released.
- 18096: binary64 scan radius, 50,000,000 km.

Tests cover binary64 round trips and bounds faults, local precision at Neptune
scale, Mars and Neptune arrival, cancellation and held-key behavior, a movement
segment crossing Earth completely, and windowless rendering. Renderer regressions include sideways/inverted Sun
views and complete rendered trips to Venus and Neptune; projected-size tests verify
non-target Jupiter growth and bounded approach expansion; physics-only travel
tests do not substitute for these checks. Catalogue tests verify satellite
separations, an Earth-to-Pluto trip, nearby arrival at every added body, selection
wraparound, and close-up rendering of all added bodies. Navigation tests cover
explicit cruise arming, held T across mode changes, nearby-target rejection,
group membership and wrap, moon proximity boundaries, real cached-rock bearings,
a complete rendered fly-to from Ceres to a close asteroid arrival, and
inverted-flight compass readings. Scanner tests verify spherical range boundaries,
explicit acquisition, stable locks across cache transitions, rescanning, range
loss, and clearing. Distant scans from the inner, outer, and vertical belt
boundaries are compared against independent exhaustive region enumeration;
rendered tests check acquisition beyond the rendering cache. HUD tests verify
empty, half, and full throttle bars and
percentages in every manual range, plus distinct 1.10, 1.80, and 3.00 SYSTEM
readouts. Guidance tests cover bearing hold, off-axis cruise arrivals, held-input takeover,
cancellation and target loss, long updates, intervening obstacles, proximity
warnings, and the 11-metre exclusion limit. Earlier CPU and
local-demo conformance tests remain active. Input/replay timestamps, physically
accurate orbits, atmospheric flight speeds, and larger-than-system coordinates
remain future work.

## Satellites and main belt

Pluto is a primary navigation destination. Its scientific classification is dwarf
planet. Ceres is a dwarf planet; Vesta is an asteroid. These classifications do
not restrict navigation. Radii below are spherical approximations, including the
irregular small bodies. Orbital distances are nominal center-to-center distances.

| ID | Body | Parent | Radius km | Parent distance km |
| --- | --- | --- | --- | --- |
| 10 | Moon | Earth | 1737.4 | 384400 |
| 11 | Phobos | Mars | 11.1 | 9378 |
| 12 | Deimos | Mars | 6.2 | 23459 |
| 13 | Io | Jupiter | 1821.5 | 421800 |
| 14 | Europa | Jupiter | 1560.8 | 671100 |
| 15 | Ganymede | Jupiter | 2631.2 | 1070400 |
| 16 | Callisto | Jupiter | 2410.3 | 1882700 |
| 17 | Titan | Saturn | 2575 | 1221830 |
| 18 | Enceladus | Saturn | 252 | 238020 |
| 19 | Titania | Uranus | 788.9 | 436300 |
| 20 | Oberon | Uranus | 761.4 | 583500 |
| 21 | Triton | Neptune | 1353.4 | 354760 |
| 22 | Charon | Pluto | 606 | 19596 |
| 23 | Ceres | Sol | 473 | 414092800 |
| 24 | Vesta | Sol | 265 | 353056000 |

The satellite catalogue is a selected subset, not a census. Static parent-relative
positions are added to heliocentric parent positions in binary64. All bodies use
the same guest rendering, LOD, navigation and swept collision paths. Satellite and
asteroid safety shells are radius + 1 km; arrival shells are radius + max(20% of
radius, 20 km). Existing primary-body shells remain unchanged. Lighting direction
is computed from heliocentric position, including for satellites.

The chart marks the approximate main-belt annulus from 2.2 to 3.2 AU, between
Mars and Jupiter. Ceres and Vesta are rendered and selectable physical bodies.
The annulus is a chart annotation, not a continuous obstacle.

The generated belt spans 329.12–478.72 million km from Sol: 149.6 million km
wide radially. Its full vertical thickness is also 149.6 million km, extending
74.8 million km above and below the X/Z system plane. A 50-million-km scan has
a 100-million-km diameter; its radius is about 0.334 AU, or one third of the
belt's radial width.

The belt uses deterministic **regional streaming inside the guest VM**. Cubic
regions are 3 million km wide. A hash of signed region coordinates and world seed
0x41535452 drives occupancy, offsets, size, and axis proportions. Generation uses
32-bit unsigned arithmetic with high-word feedback and 24-bit random fractions;
it does not depend on host random functions, travel direction, or visit order.
The host supplies only the mesh template and constants.

Asteroids are not evenly spaced. For the current seed, a sample of 3,669 interior
rocks has a mean nearest-neighbor distance of 2.10 million km, a median of
2.14 million km, and a 95th percentile of 3.03 million km. These values describe
the generated model, not observations of the real belt. Edges and local gaps
can differ from this interior sample.

Each region has a 75% chance of proposing one asteroid. Its center is uniformly
jittered inside the cell with a 20 km boundary margin. Candidates outside the
2.2–3.2 AU annulus or beyond 0.5 AU above/below the ecliptic are rejected, as are
candidates overlapping a permanent catalogue body's safety shell. Bounding radii
are 0.5–8 km, using `0.5 + 7.5*u^8`; axis proportions range from 0.7 to 1.
The size distribution is an illustrative bias toward small rocks, not a measured
fit. Geometry and the radius-plus-10-metre safety sphere remain inside the cell.

The volume and occupancy imply approximately **1.58 million kilometre-class
objects** across the belt, before boundary and catalogue exclusions. This is an
expected population, not an enumerated count. It is chosen to roughly match
NASA's estimate of 1.1–1.9 million asteroids over one kilometre in diameter.
The regional distribution is a simplified static approximation, not a real
catalogue or orbital model. Smaller debris, asteroid families, resonance gaps,
and observed size/albedo distributions are not yet modelled. There is no special
population or density boost near Ceres. References:
[NASA asteroid facts](https://science.nasa.gov/solar-system/asteroids/facts/) and
[NASA Dawn FAQ](https://science.nasa.gov/mission/dawn/faq/).

Rendering caches the current cell and its 26 neighbours in 1,728 bytes. It rebuilds
only when the ship crosses a cell boundary. Negative coordinates use floor division.
Revisiting a region produces identical records; streaming never relocates a rock
relative to the world. A shared guest loop renders cached objects through one
scratch body and one mesh program, rather than emitting separate code per rock.
With the maximum 8 km radius and the current one-pixel visibility threshold,
visible geometry is far inside the neighbouring-cell coverage.

Collision does not rely on the rendering cache. The guest samples the complete
movement segment at intervals no larger than 1.5 million km, including both ends,
and generates each sample's 27 neighbouring cells. Those neighbourhoods cover the
segment plus all possible asteroid safety spheres. Each candidate is checked
against the original swept ray; the earliest hit clips travel and cancels cruise.
Repeated cells can be generated more than once, trading extra work for simple,
conservative coverage. There is no cell-count cutoff that silently skips part of
a long movement. Memory stays bounded; processing cost grows with path length.
Region coordinates use signed 32-bit conversion, comfortably covering the bounded
Solar System; this is not an unbounded-coordinate universe.

Rocks remain unmarked: no Tab destinations, map symbols, labels, or subpixel
point markers. Geometry appears at a projected bounding radius of one pixel or
more. The cockpit may show generic ASTEROID clearance for a nearby cached rock,
and the navigation scanner reports a local rock's bearing and center range;
neither is a global search for the nearest asteroid. Static orbital placement,
simple flat lighting, and conservative spherical collision remain approximations.

Numerical references: NASA fact sheets for the
[Moon](https://nssdc.gsfc.nasa.gov/planetary/factsheet/moonfact.html),
[Mars](https://nssdc.gsfc.nasa.gov/planetary/factsheet/marsfact.html),
[Jovian satellites](https://nssdc.gsfc.nasa.gov/planetary/factsheet/joviansatfact.html),
[Saturnian satellites](https://nssdc.gsfc.nasa.gov/planetary/factsheet/saturniansatfact.html),
[Uranian satellites](https://nssdc.gsfc.nasa.gov/planetary/factsheet/uraniansatfact.html),
[Neptunian satellites](https://nssdc.gsfc.nasa.gov/planetary/factsheet/neptuniansatfact.html),
[Pluto](https://nssdc.gsfc.nasa.gov/planetary/factsheet/plutofact.html), and
[asteroids](https://nssdc.gsfc.nasa.gov/planetary/factsheet/asteroidfact.html).
Ceres uses the rounded radius in
[NASA Ceres facts](https://science.nasa.gov/dwarf-planets/ceres/facts/);
Vesta uses half the approximate diameter in
[NASA Vesta scale reference](https://science.nasa.gov/asset/hubble/compass-and-scale-image-for-vesta/).

Composition, temperatures, materials, and an in-game reference display are not
implemented. Future catalogue fields should record their source and measurement
context, particularly surface versus atmospheric temperatures.

## Body annotations

The selected map-list name is green. Other names retain the standard blue text;
selection is not represented by a line between rows.

The flight view labels body centers whose projected radius is at least 0.025
pixel. Within a forward cone |x/z| and |y/z| <= 0.5,
named catalogue bodies are also identified out to 1,196.8 million km of camera
depth, equivalent to eight seconds of full cruise. Generated asteroids have no
annotations. Moon labels require proximity to their parent, including when the
moon is selected: the system radius is max(100,000 km, three times the orbital
distance of its outermost catalogued moon). Inside that system radius, ordinary
label visibility checks apply. Selected destinations bypass the angular and
lookahead distance thresholds, but cannot bypass the moon proximity gate.
Near-plane and screen-margin checks still apply to every label. A center-distance
readout uses KM, KKM, MKM or AU below each name.
A leader connects each name to its projected center. Labels use solid 5x7 glyphs
at 2x scale on opaque dark panels, with names above distance readouts. Selected
annotations are green; others are pale blue. The selected destination is placed
first, followed by decreasing apparent radius. Each 120x34 panel starts beside
its object, switches sides near the right edge, and moves downward to avoid
already placed labels, the navigation instruments, and the open map. If no
vertical space remains, the label is omitted for that frame. All leaders draw
before the panels so crossing lines
cannot corrupt text. Coordinates are bounded before integer conversion, and
centers behind the near plane or outside annotation margins remain omitted.
Label leaders, opaque backgrounds, and glyphs are depth-tested at their body's
camera depth. Nearer scene geometry masks their pixels, including selected-target
labels; nearer tags still cover farther geometry. Testing does not write scene
depth, so text can draw on its own background without hiding a foreground rock.
Panel priority can change as apparent sizes change; layout does
not yet have temporal smoothing.

Placement rectangles are recorded at 11000 + 16 * body (x, y, width, height);
projected centers at 11500 + 8 * body. The records are meaningful only when the
body's annotation placement diagnostic is one. Geometry can occlude some or all pixels.

Original annotation diagnostics occupy 9300 + 4 * body (0=omitted, 1=placed).
The former generated-body annotation/LOD slots are no longer used. Procedural
render-count diagnostics are at 10804; individual rocks have no persistent IDs
in the HUD.
10400 holds elapsed update time in 1/60-second units; 10408 holds the preceding
low clock word. Unsigned subtraction handles clock wrap when updates are less
than 2^31 ticks apart. Normal frame intervals are far below this limit.
All annotation projection and glyph drawing execute in the guest.

## Encounter visibility check

With the current seed, an Earth-to-Ceres arrival has its nearest cached asteroid
approximately 1.44 million km away (bounding radius about 0.65 km); it reaches a
one-pixel projected radius only at about 259 km of forward depth. At Vesta the
corresponding values are about 1.71 million km, 0.50 km, and 201 km. These are
measurements of the synthetic world, not astronomical observations. Planet-to-
planet trips and named-body arrivals therefore rarely expose any rock geometry.
At high travel speeds a near miss can also cross the visibility range between
rendered frames. Population density, physical sizes, darkness, and the absence of
rock markers remain unchanged.
