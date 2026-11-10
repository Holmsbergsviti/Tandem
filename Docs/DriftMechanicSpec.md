# Tandem: Drift Mechanic & Car Setup

Source: spec by @Vladislav, 7 Nov 2026. This file records how each point is implemented.
The car uses its own force-based physics (`ADriftVehiclePawn`), not Chaos Vehicles.

## Physics model (since 7 Nov 2026: forces only, no arcade grip tricks)
Boss asked for real forces on every part of the car. Every frame, per wheel:

| Force | How it is computed |
| --- | --- |
| Normal force | Raycast suspension: spring (N/m x compression) + damper (1900, ~0.4 critical), applied at the mount. Weight shifts by itself under braking, throttle and cornering, and the body rocks |
| Tyre force | Slip ratio and slip angle combined on one friction ellipse, Pacejka-style curve peaking at 10% slip / 8 deg. Past the peak, slow sliding keeps almost full grip and fast sliding falls to 80% (drift tyres) (rubber friction vs sliding speed, 4 m/s scale). Sliding force points against the patch's real sliding velocity. Friction mu 0.95, lower on a more heavily loaded tyre (load sensitivity 0.12) |
| Drive torque | Engine is its own flywheel (0.15 kg m^2): curve x throttle (TC trims it), pumping losses off throttle. Through gear x final x 85% and a viscous LSD (120 Nm per rad/s, max 450 Nm) |
| Clutch | Friction clutch, max 420 Nm. Slips to hold ~4500 rpm when pulling away. Down while shifting (0.12 s), bites over 0.25 s after an upshift, dumped in 0.1 s after a downshift; Shift = clutch kick (down 0.25 s, then dumped) |
| Steering | Caster: with no input the front wheels follow the front axle's travel (1.0, fully), which is the countersteer in a slide; input adds up to the grip-limited angle on top, opening to full lock as body slip goes 10 -> 25 deg |
| Brake torque | Pedal 9000 N at the road, 72% front; handbrake 1600 Nm per rear wheel and presses the clutch |
| Rolling resistance | 0.013 x normal force, as a torque on each wheel |
| Aero drag | 0.5 x 1.225 x CdA 0.63 x v^2 against the motion |
| Gravity | Applied by the physics engine at the centre of mass |
| Centripetal force | Not added: it is the sum of the tyres' sideways forces (shown as the sideways part of the resultant) |

Wheel and engine spin are integrated in 10 substeps per frame (semi-implicit) so the stiff tyre stays stable.
Why the car straightens after a handbrake drift: once the handbrake is released the rear wheels
spin back up to road speed, their slip angle drops and their sideways force grows again, which
turns the yaw back. With traction control on, wheelspin is trimmed so the rears keep grip.
TC runs every physics sub-step and trims torque in proportion to the excess slip (never below 15%), so it
behaves the same at 20 or 60 fps. Pulling away, the clutch starts biting as the revs rise (no dead time).
Offline sim, flat road, TC on: 0-30 km/h 2.0 s, 0-100 6.8 s (was 2.4 / 7.6 at 60 fps, 3.2 / 8.5 at 30 fps).

G shows the force overlay: blue = normal force per wheel, red = tyre force, purple = gravity,
yellow = resultant on the car, green = velocity, plus a numbers panel.

## Drift angle
Angle between where the car points and where it moves (signed: + = sliding right).

## Phases
| Phase | Spec | Implementation |
| --- | --- | --- |
| Initiation | Handbrake tap | Space puts 1600 Nm on each rear wheel and presses the clutch: the rears lock and slide |
| Initiation | Power-over | Only with traction control off (T): 330 Nm can spin the rears through the tyre model |
| Initiation | Feint / Scandinavian flick | Flick away then back: soft damping + load-sensitive tyres let the weight swing unsettle the rear |
| Initiation | Shift-lock | Downshift with no blip: the clutch dumps and the engine's slow crank drags the rears loose |
| Initiation | Clutch-kick | Left Shift on throttle: clutch down 0.25 s while the engine revs, then dumped; the stored revs shock the rears |
| Sustain | Throttle sets angle | Rear slip from the tyre model; more throttle = more wheelspin = more angle |
| Sustain | Countersteer | The front wheels self-align with the slide (caster). Holding the steering towards the slide catches it and keeps its angle (counter-steer hold, see the end of this file) |
| Exit | Grip returns | Rear tyres regain grip as slip falls (physics, not a timer) |

## Getting out, doors
F (car stopped, under 8 km/h) gets out on the driver's side (right-hand drive). On foot: WASD walk,
mouse look, Shift run, Space jump, E opens/closes the nearest door, F at a door gets back in.
The car stays parked on the handbrake while empty.

## Scoring, laps, races
Removed at Boss's request (scoring 6 Nov, laps/countdown/timer 7 Nov 2026): Tandem is free driving.
Backspace resets the car onto the nearest point of the track.

## Car setup (spec -> this game)
Modelled on the Toyota AE86 Sprinter Trueno GT-APEX.

| Real AE86 | Value here |
| --- | --- |
| RWD, optional LSD | RWD with viscous LSD |
| 4A-GE 1.6 twin-cam: 130 PS @ 6600, 149 Nm @ 5200, ~7600 redline | Same curve shape, but a drift-built 330 Nm (Boss found stock, then 210 Nm, too weak); clutch slip to 4500 rpm on launch |
| T50 5-speed 3.587 / 2.022 / 1.384 / 1.000 / 0.861, R 3.484, final 4.3 | Same |
| Curb weight 940-975 kg, about 53/47 | 975 kg, CoM (2, 0, -5) = 53/47, ~48 cm high |
| 4205 x 1625 x 1335 mm, wheelbase 2400, track ~1355 | Body mesh to scale; axles at +115 / -125 cm, track 136 cm |
| 185/60R14 | Wheel radius 29 cm, width 18.5 cm, mu 0.95, wheel inertia 0.9 kg m^2 |
| Soft 80s springs | 24000 / 22000 N/m (about 1.5 Hz), damper 2600, 24 cm travel |
| Steering lock ~37 deg stock | 60 deg (angle kit), limited to the grip angle at speed while gripping, full lock available in a drift |
| Brakes, no ABS | 9000 N total, 72% front (about 0.95 g) |
| Drag Cd ~0.35, frontal ~1.8 m^2 | CdA 0.63 m^2 |

All values are `UPROPERTY`s on `DriftVehiclePawn`, so they can be tuned in the editor.

## Map (7 Nov 2026)
Boss asked for a much bigger, narrower map with hills, slopes, bumps and sharper turns. The track is now a
5.8 km mountain road (touge) built by `ADriftTrackBuilder`: 8 m wide, 1.5 m grass shoulders, concrete
barriers; 9 hairpins and 28 bends tighter than 25 m radius; 69 m from lowest to highest point, grades up
to 15%, crests and rough bumpy stretches (`HillScale`, `BumpHeight`). Hilly terrain with trees surrounds it.
Road, shoulders and terrain have real collision. The car always spawns at the start of the road.

## Practice lot (7 Nov 2026)

A dead-flat 300 x 240 m asphalt lot on the left of the start line (local X -150..150 m, Y -300..-60 m,
Z 0), no bumps. The left barrier opens for 20 m at the start and a 55 m slip road joins the lot to the
pass. Painted marks: a 12 m donut circle, a figure eight of two 20 m circles, a 35 m sweeper and a border
line. The car starts on the lot facing +X; Backspace on the lot resets you on the lot.

## High-speed loop (7 Nov 2026)

A flat 1.9 km loop south of the practice lot (`TrackParts::FastPoints`, metres): a 550 m straight whose
north edge is the lot's south edge, a 110 m-radius 180, a fast 160 m esse and two 90 m corners. 12 m
wide, no barriers, flat grass run-off (~30 m) all round, painted red/white kerbs through the bends.
Made for drifting at about 100 km/h (3rd gear; initiate with a clutch kick, feint or handbrake).

## Speed feel, sound, interior, map switch (7 Nov 2026)

Boss: "I feel I'm travelling 1 m/s". What now tells the eye and ear how fast you go:
- Field of view widens with speed (+14 deg chase, +9 deg inside at ~160 km/h); the camera picks up road
  buzz (Perlin jitter, stronger at speed and in slides); light motion blur (amount 0.4).
- `M_Ground` material: vertex colour x world-space grain (35 cm, 3 octaves) x 7 m patches, faded out with
  distance. Used on asphalt, shoulders, the lot, the loop and the terrain (falls back to M_VertexColor).
- Marker posts every 16 m along the high-speed loop, cones round the lot plus a slalom line, dashed
  centre line on the loop.
- Sound (Scripts/Audio/*.wav, synthesised, imported to /Game/Audio by setup_materials.py): engine loops
  made at 1800 and 5000 rpm, cross-faded 2800-3700 rpm and pitched by rpm; wind ~ speed^2; tyre squeal
  from the contact-patch slide speed (1.5-6 m/s).
- Interior: roll cage (main hoop, A-pillar legs, roof rails, X door bars, rear stays, harness bar,
  diagonal), lit rev counter and speedometer with moving needles, red 4-point harnesses, mirror, vents,
  radio display.
- M cycles practice lot -> high-speed loop -> mountain pass (teleports the car; HUD shows the name).

## Tyre smoke and wheel spin (7 Nov 2026)
A tyre smokes when its contact patch scrubs over the road faster than 3 m/s (wheelspin, lock-up or
sliding sideways); more slip, more and bigger puffs (`SmokeSlideSpeed`, material `M_Smoke`). The drawn
wheel rotation is capped at 18 deg per frame: the 8-spoke rim otherwise strobes (wagon-wheel effect)
and a fast-spinning wheel looked stopped. With traction control on (T) the rears are held near grip,
so for a proper smoky drift switch TC off.

## Turn ladder (fourth area, M)

A flat, barrier-free practice road west of the mountain pass (start at about (-450, -1000) m, heading +X), 3.4 km long and 10 m wide.
Corners get sharper step by step: 5 deg right then 5 deg left, then 10 deg left and 10 deg right, and so on in 5 deg steps up to 90 deg (each angle once in each direction, alternating which side comes first).
All arcs are 45 m radius with 50 m straights between them, so only the angle changes. There is a 150 m run-up and a 120 m run-out, with a 35 m turning pad beyond each end.
Red/white kerbs mark each corner; the HUD panel at the top shows the next corner's direction, angle and distance (`ADriftTrackBuilder::GetLadderHint`), in whichever direction you drive.
Data lives in `TrackParts::ProgPoints` / `ProgTurns` in `Track/DriftTrackBuilder.cpp`.

## Accelerating out of corners (TC on)

Turning and accelerating share the same tyre grip (friction circle). Two changes so a held key plus W still drives the car forward when TC is on:
- **Cornering TC.** Besides the straight-line wheelspin check (kappa past `TCSlipTarget`, proportional), TC now watches the driven tyres' combined slip, sqrt((kappa/PeakSlipRatio)^2 + (tan(alpha)/tan(PeakSlipAngle))^2). While the tyre is also cornering, that is held under `TCCombinedSlipLimit` = 1.2 by an integral cut (`TCCombinedGain` 15/s, floor 10%, given back at 3/s). The engine gets the lower of the two factors. Straight-line launch is unchanged (0-100 km/h 6.8 s in the sim).
- **Steering share.** `GripSteerShare` / `GripSteerSlipShare` are both 1 (full angle). They were 0.7 / 0.3 to leave front grip for the throttle, but Boss found A/D unresponsive in normal driving. Held full lock plus full throttle still pulls out of corners up to about 60 km/h; above that it can spin, as it should.

## Steering feel and the cockpit wheel

- **Keyboard ramp.** Centre to full lock takes `SteerRampTime` = 0.12 s whenever the car grips, at any speed. Only while drifting (body slip past 8 deg, fully by 15 deg) does it slow to `SteerRampTimeFast` = 0.3 s. Releasing a key always centres in `SteerReturnTime` = 0.09 s. (Slowing with speed made normal driving feel dead, so it is gone.)
- **Wheel rotation.** The cockpit wheel turns `WheelTurnDeg` = 270 deg each way (540 lock to lock), following the actual front-wheel angle, so caster self-steer in a slide shows on the wheel. It is smoothed (about 70 ms) so keyboard steps don't jerk it.
- **Hand over hand.** Each hand holds the rim and turns with it until it leaves its comfortable stretch (left hand 8 o'clock to 2 o'clock, right hand 10 to 4, roughly). It then lets go and re-grips 60 deg behind the other hand, crossing over it lifted towards the driver (0.15 s). Only one hand is ever off the rim. If the wheel spins faster than 450 deg/s (a caster snap), it slides through the palms instead of dragging the arms. The left hand still goes to the shifter and handbrake and comes back to a grip 140 deg from the right hand.
Sim, full key + W held from 45 km/h in 2nd: before, 51 -> 43 -> 22 km/h (spun out); now 50 -> 52 -> 55 km/h with under 5 deg of slip.

## Pulling away with the wheels turned

Two low-speed fixes (Boss: from 0 with the wheels turned it wouldn't pass 8 km/h):
- **Caster fades in with speed.** The wheels' self-aligning swing towards the front axle's travel direction (`CasterAlign`) is scaled by SmoothStep(10, 30 km/h). Self-aligning torque is weak at walking pace, and with full caster a turned wheel followed its own travel direction and wound itself out to full lock. Above 30 km/h (all drifting) nothing changes.
- **TC tells diff geometry from wheelspin.** In a tight circle the LSD ties the rear wheels together, so the inner one always turns faster than its patch moves. Below 20 km/h TC now judges the axle as a whole (mean wheel speed against mean ground speed, for both the spin and the combined-slip checks), blending to per-wheel by 40 km/h.
Sim, from 0 with 30% steering held: before, stuck at 10.7 km/h (wheels wound to 45 deg); now 24 km/h at 2 s and 54 km/h at 6 s. At full 45-degree lock the circle is about 3 m in radius, so the car tops out near 12-13 km/h there (was 9).

## Menu, maps and graphics (2026-11-07)

- **Esc menu** (or Start on a gamepad), drawn by the HUD and run by `ADriftPlayerController`. It pauses the game. Pages: Resume, Change map, Controls, Car, Graphics, Quit game. Keyboard (arrows or W/S, Enter, Esc back), mouse (hover, click, right-click back) and gamepad (d-pad, A, B) all work.
- **Controls**: every driving key can be rebound (`Input/DriftKeyBindings`). They are saved in GameUserSettings.ini `[Tandem.Keys]`, and a key that's already taken swaps. Gamepad buttons are fixed, and Esc always opens the menu. The bottom-left hints show your own keys.
- **Car**: the AE86 plus three "coming soon" slots for later cars.
- **Graphics** (`UI/DriftGraphicsSettings`, saved in `[Tandem.Graphics]`): Quality LOW/MEDIUM/HIGH/EPIC (the engine scalability preset plus scenery draw distances and shadows), render resolution 50-100%, frame limit 30/45/60/unlimited, FPS counter. On the first start the machine is benchmarked and gets LOW or MEDIUM, never more.
- **Maps** (M cycles, or menu > Change map): practice lot, high-speed loop, mountain pass, turn ladder, **city streets** (`ADriftCityBuilder`, spawned 3.5 km north: a 5 x 4 block grid, 18 m streets, 15 cm kerbs, buildings taller towards the centre, an open plaza and a car park to drift on, a park), **downhill run** (`ADriftDownhillBuilder`, 4.2 km south: 2.9 km, 226 m drop, hairpin ladder of 25 m radius, guardrails, autumn forest, pads at both ends).
- **Scenery** (`Track/DriftScenery`): every map shares instanced pines, leafy trees in several greens and autumn colours, birches, dead snags, bushes and rocks. Each kind is one instanced mesh, so it stays cheap on weak laptops.

## Drift control (2026-11-07)

Boss: "control over the car during drifting is bad". Cause: with 45 deg lock, 85% caster follow and road-tyre sliding grip (68%), the front wheels could not point along the car's travel once the slide passed about 45 deg, and at speed the input was capped at the grip angle (about 12 deg at 70 km/h) even mid-drift, so a slide could not be caught and turned into a spin.

Changes, each one a real drift-car feature:
- **Angle kit: 60 deg lock** (`MaxSteerDeg`). The cockpit wheel still turns 270 deg each way at full lock.
- **Caster follow 1.0** (`CasterAlign`): the self-aligning torque lets the fronts swing fully to the travel direction.
- **Sliding grip 80%** (`SlidingFriction`): drift tyres keep more grip past the peak.
- **Full countersteer in a drift**: the grip-angle limit on input fades out as body slip goes 10 -> 25 deg.

Sim, keyboard bot after a handbrake entry, seconds held in a drift out of 15 (spins): before 3.7 (3) / 8.4 (3) / 8.5 (3) for three driving styles; after 2.9 (0) / 9.5 (0) / 12.5 (0).

## No aids, drift steering, NSX, checkpoints, circuits (2026-11-10)
Boss: make drifting easier, delete every system that controls or stabilises the car, let a drift follow any
corner (long or very twisty), more forward push while drifting, front wheels should not turn instantly,
add a Honda NSX, M first returns to the last checkpoint, more tracks.

Removed: traction control (code, T key, HUD light) and the counter-steer hold assist from earlier the same
day (it fixed the slide angle, so steering could not change the drift's line). Nothing trims the engine or
steers for the driver now.

Steering (`ADriftVehiclePawn::Tick`):
- Caster still turns the fronts to follow the front axle's travel (self-aligning torque, physical).
- Gripping: a full key asks for the grip-limited angle at speed, full lock when slow (a key-to-angle map).
- Drifting (slide 8 -> 15 deg blends in): the key turns the fronts up to `DriftSteerRangeDeg` (12) either
  side of the direction they travel. Into the corner tightens the drift's line, towards the slide widens it
  and takes angle off; it can no longer scrub the speed away or yank the car straight.
- Rack: the road wheels turn at most 300 deg/s with a 0.05 s lag (`SteerRateDegPerSec`, `SteerLagTime`).
Tyres: sliding grip 0.8 -> 0.9 of peak, and the drop to it now spreads over 6 m/s of sliding speed (was 4),
so a spinning rear keeps more force, which is more push forward in a drift.
Not checked by driving: an offline sim with a crude scripted keyboard driver could not hold a drift with
either the old or new settings (its driver was too simple), so the values are reasoned, not measured.

Cars (menu > Car, saved as `[Tandem.Car] Model`; tuning is per car in `[Tandem.Tuning.<id>]`):
| | AE86 drift build | NSX supercharged |
| --- | --- | --- |
| Mass, balance | 975 kg, 53/47 | 1370 kg, 42/58 (mid-engine) |
| Wheelbase / track | 2400 / 1355 mm | 2530 / 1510 mm |
| Engine | 4A-GE built, 330 Nm, 7600 rpm | C30A V6 + supercharger, 400 Nm, 8000 rpm |
| Gears / final | T50 3.587 2.022 1.384 1.0 0.861 / 4.3 | 3.071 1.947 1.423 1.091 0.888 / 4.062 |
| Springs F/R, damper | 24000 / 22000, 1900 | 26000 / 35000, 2500 |
| Brakes | 9000 N, 72% front | 13000 N, 62% front |
The NSX body (`DriftCarMesh`, model 1) is a low wedge with pop-up lamps, a cab-forward canopy with a black
roof, side intakes and a full-width tail lamp bar. It is 1.26 m tall (real 1.17 m) so the shared cabin
fits; its interior has no roll cage.

M: back to the last checkpoint the car passed (within 9 m of a road checkpoint, above 15 km/h), facing the
way it was going. Pressed again within 4 s while still there: next map.

More tracks (`Track/DriftCircuitBuilder.*`, closed loops east of the other maps, M and menu areas 7-9):
DRIFT PARK 1.0 km (linked corners, 14 m wide), SWEEPER RING 1.9 km (long sweepers of 60 m radius and
more, 16 m wide), SNAKE HILL 1.3 km (chains of S-bends over hills, 12 m wide). Red/white kerbs, gravel
run-off, guardrails. Designed as straights and arcs, closed by adjusting the straights, checked for
tightest radius 20 m, at least 56 m between separate parts of the road, grade under 9%.

Menu > Tuning: engine power (shown as hp), rev limit, final drive, front and rear tyre grip, grip while
sliding, steering lock, weight, reset to the car's defaults.

### Drift angle hold (later the same day)
Boss, after driving: in a drift, the countersteer key should not swing the car the other way at once, and
with only W held the car should not rotate back against the drift. So a hold came back, scoped to that:
no key keeps the angle the drift settled at (at least 12 deg); the countersteer key lowers the kept angle
at 12 deg/s so the drift unwinds gently; the key into the corner is always the driver's own steering, and
the angle it reaches is held when released. Off on the handbrake; ends when the slide is under 3 deg.
It only sets the front wheels' angle (PID on slide angle -> front slip angle, within the tyre's peak).
Menu > Tuning > Drift angle hold switches it (per car, default ON).

### "Satisfying, not easier" (10 Nov, evening)
Boss: the point was a more satisfying drift, not an easier one; picked "the car drifts by itself", "the
drift feels weak" and "controls feel numb". So:
- The angle hold is gone. What is left is snap-back damping only: while the drift angle is falling the
  fronts sit short of the travel direction by 0.2 deg per deg/s of fall (max 6 deg). It never picks or
  keeps an angle; throttle and steering do. Tuning switch renamed "Snap-back damping".
- Stronger slides: front grip 1.0, rear 0.88 (power-over and angle on throttle), drift steering range
  12 -> 16 deg, smoke from 2 m/s of scrub and denser.
- Sharper controls: key-to-lock 0.08 s (0.14 s drifting, was 0.3), rack 450 deg/s with 0.03 s lag,
  throttle rise 0.1 s (was 0.25) and faster release.
Still reasoned values, not driven.

### Camera, part throttle, NSX and circuit fixes (10 Nov, late)
- Chase cameras swing towards the direction of travel by 65% of the slide angle (max 40 deg, smoothed).
- Part throttle: hold Left Ctrl (rebindable) with W for 55% throttle, to balance a drift on the keyboard.
- NSX: rear-view mirror lowered 9 cm (it poked through the lower roof); engine loops pitched 1.3x (V6).
  Not seen on screen: the long cowl ahead of the dash and the shared seats may still look off.
- Circuits checked from their built geometry (no folded kerbs or rails, 29 m or more between rails of
  separate road parts, turn per 2.7 m step under 9 deg). Sweeper Ring and Snake Hill had a 4-5 m seam step
  that made a short 9-11% bump; the seam point is removed.
- The project is now a git repository (first commit c5078f7, work on branch drift-feel).

### V6 sound, drive recorder, cleanup (10 Nov, night)
- NSX engine sound: `Scripts/Audio/make_v6.py` synthesises two V6 loops (1800 and 5000 rpm, firing note
  90 and 250 Hz, level matched to the four-cylinder loops); `setup_materials.py` imports them as
  `S_EngineLowV6` / `S_EngineHighV6`. Until they are in a build the NSX uses the old loops pitched 1.3x.
  Checked by numbers only (level, clean loop seam, spectrum), not by ear.
- Drive recorder: `~/Documents/Tandem/Drives/drive_<time>.csv`, 20 rows a second while someone is in the
  car (speed, slide angle, yaw rate, steering key and front wheel angle, throttle, brake, handbrake, gear,
  rpm, g, rear wheel speed, wheel slip and loads, position). Ten newest files kept. `bRecordDrives` turns it off.
- Removed the unused scoring component and the empty DriftWheels files; README rewritten.
