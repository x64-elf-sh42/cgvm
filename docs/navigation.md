# Astra Navigation Display

Status: implemented. Local test-system chart; not astronomical scale.

This earlier local-scene cockpit identifies the ship as ASTRA. For the current
default program, see [Solar System flight](solar-system.md).
The permanent cockpit panel occupies the bottom of the flight view. It groups
flight/clearance information, destination/forward-ray details, and status/controls.
The independent upper-right panel contains only the system map and map controls.
Both are rendered entirely by guest GPU commands with depth disabled. No host text or map overlay is used.

## Controls and state

Tab cycles the selected destination between Terra and Sol. M toggles the complete upper-right map panel, including its background, frame,
labels, and controls. Both use latched press transitions and acknowledge
them after processing, so held keys and host repeat do not cycle repeatedly.
Navigation state is initialized to Terra selected and chart visible.

Guest RAM 8460 contains chart visibility (0/1), and 8464 contains selected body
(1=Terra, 2=Sol). Selection does not change steering, throttle, safety checks, or
AHEAD ray selection. The lower cockpit shows selected-body center distance in whole units,
saturated at 999. It is distinct from safety-boundary clearance and ray-entry
distance. Cruise, autopilot, and destination arrival behavior are not implemented.

## Chart projection

The chart is a fixed top-down X/Z projection:

    screen_x = 820 + world_x/4
    screen_y = 280 - world_z/5

The plot covers x=770..970 and y=160..300. It therefore spans world X=-200..600
and Z=-100..600. Y altitude is omitted. The axes have different pixel scales;
this schematic is not an angle- or distance-preserving physical map. Terra and
Sol use blue and amber markers with names, and the selected marker has white
brackets. Astra uses a green marker and heading tick. The tick projects the
forward direction; it shortens when heading approaches vertical.

An off-chart ship suppresses its marker and displays OUT OF MAP. The chart does
not pan, zoom, depict asteroid positions, or clip its short heading tick at the
plot boundary. Body positions are constants matching the current guest scene.
Future dynamic bodies require feeding the same position data into map rendering.

The separate AHEAD and NEAR displays retain their collision-navigation meanings.
The map can remain hidden while the lower cockpit, destination, range, clearance,
and warnings stay visible. Tab selection remains available with the map hidden.

## Validation

Navigation tests cover destination cycling, hold/repeat behavior, map visibility,
ray intersection, nearest-body identification, and inverted-pitch steering. The
approach regression checks that the added HUD remains within the existing 60 Hz
guest frame budget. Glyphs emit horizontal spans rather than individual pixel
commands to reduce instruction overhead. GPU and machine timing are unchanged.
