# Tandem

A drift game for an M1 MacBook (8 GB), made in Unreal Engine 5.8 with C++ only. Free driving: no
scores, laps or timers. The car is simulated with real forces (springs, a tyre model, engine, clutch,
gearbox, diff, drag); nothing pushes it around except those.

## Build and play
```bash
bash Scripts/play.sh          # build, package (Development) and play
bash Scripts/play.sh --run    # play the last packaged build
```
If `--run` seems to do nothing, an old copy is stuck: `pkill -f Tandem.app; pkill -f DriftGame`.

## Controls (rebind in Esc > Controls)
| Key | Action |
| --- | --- |
| W / S | Throttle / brake |
| Left Ctrl (hold) | Part throttle, with W |
| A / D | Steer |
| Space | Handbrake |
| Q / E | Gear down / up (N and R below 1st) |
| Left Shift | Clutch kick |
| C | Camera |
| G | Force overlay |
| F | Get out / in |
| M | Back to the last checkpoint; again: next map |
| Backspace | Reset the car onto the road |
| R | Wipers |
| Esc | Menu: maps, controls, car, tuning, graphics |

## What is in it
- Cars: Toyota AE86 drift build and Honda NSX (Esc > Car). Each keeps its own tuning (Esc > Tuning).
- Maps: practice lot, high-speed loop, mountain pass, turn ladder, city, downhill, and three circuits
  (Drift Park, Sweeper Ring, Snake Hill).
- No traction control or steering assists. The one help is snap-back damping, which can be switched off
  in Tuning.
- Crash damage: bumpers, spoiler and doors come off one at a time; a hard hit on the engine's end blows the
  engine. Backspace, M or a map change gives you a repaired car.
- Drive recorder: every drive is saved to `~/Library/Containers/com.tandem.game/Data/Documents/Tandem/Drives/drive_<time>.csv` (the packaged app is sandboxed) (20 rows a second:
  speed, slide angle, steering, throttle, wheel slip, ...), for tuning the handling. The ten newest are kept.

## Layout
- `Source/DriftGame/Vehicle` the car: physics, cockpit, sound (`DriftVehiclePawn`), body meshes
  (`DriftCarMesh`), tuning menu values (`DriftTuning`)
- `Source/DriftGame/Track` map builders (everything is generated in code) and shared scenery
- `Source/DriftGame/UI` HUD, pause menu, graphics settings
- `Source/DriftGame/Input` key bindings
- `Source/DriftGame/Modes` game mode: spawns the maps, knows starts and checkpoints
- `Source/DriftGame/Character` the driver on foot
- `Scripts` `play.sh`, material/sound/map setup run by it, `Audio/make_v6.py` (NSX engine sound)
- `Docs/DriftMechanicSpec.md` how every part of the handling works and why it changed
