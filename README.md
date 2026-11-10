# Drift Game

Lightweight Unreal Engine 5 drifting game for an M1 MacBook (8 GB). C++ gameplay, rendering set up for low-end hardware (no Lumen, Nanite, virtual shadow maps or ray tracing).

## Controls
| Key | Action |
| --- | --- |
| W / S | Throttle / brake (auto reverse when stopped) |
| A / D | Steer |
| Space | Handbrake |
| Q / E | Gear down / up (R and N below 1st) |
| R | Wipers |
| C | Camera, Backspace reset car, Esc pause |

Input actions and the mapping context are created in code (`ADriftVehiclePawn::BuildInput`), so there are no input assets to set up.

## First run (about 15 minutes)
1. Install the latest UE 5.x from the Epic Games Launcher and Xcode from the App Store. If your engine is not 5.6, right-click `DriftGame.uproject` > *Switch Unreal Engine version*.
2. Right-click `DriftGame.uproject` > *Generate Xcode project* (or just double-click to open and build).
3. In a separate throwaway project, create **Games > Vehicle** (C++) and **migrate** its `SportsCar` folder (skeletal mesh, physics asset, anim blueprint) into this project's `Content/Vehicles/`. The wheel bones are `PhysWheel_FL/FR/BL/BR`, which the pawn already expects.
4. In the editor create `BP_DriftCar` as a child of `DriftVehiclePawn`. Set Mesh > Skeletal Mesh Asset and Anim Class to the SportsCar ones.
5. Create a Blueprint child of `DriftGameModeBase` named `BP_DriftGameMode`, set *Default Pawn Class* to `BP_DriftCar`, and set it as the map's GameMode override (or Project Settings > Maps & Modes).
6. Create `Content/Maps/L_DriftTrack` with a floor, a PlayerStart and some walls, press Play, and drive.

## Layout
- `Source/DriftGame/Vehicle` car pawn, wheels
- `Source/DriftGame/Scoring` drift detection and score
- `Source/DriftGame/Modes` game modes (race and rally will be added here)
- `Source/DriftGame/UI` simple canvas HUD
- `Config` rendering, input, scalability settings

## Tuning
Open `DriftVehiclePawn.cpp` (constructor) and `DriftWheels.cpp`. Rear grip `FrictionForceMultiplier` lower than front = easier slides. Use the console command `p.Vehicle.DebugDrawing 1` to see forces.
