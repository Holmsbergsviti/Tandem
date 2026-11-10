#!/bin/bash
# Builds, packages (Development, macOS: faster than Shipping) and launches Tandem as a real app.
#   bash ~/UnrealProjects/DriftGame/Scripts/play.sh          -> build + package, then play
#   bash ~/UnrealProjects/DriftGame/Scripts/play.sh --run    -> just launch the last packaged build
#   bash ~/UnrealProjects/DriftGame/Scripts/play.sh --debug  -> launch in this Terminal and show its output
set -e
UE="${UE_ROOT:-$HOME/Games/UE_5.8}/Engine"
PROJ_DIR="$(cd "$(dirname "$0")/.." && pwd)"
P="$PROJ_DIR/DriftGame.uproject"
OUT="$PROJ_DIR/Packaged"
APP="$OUT/Tandem.app"
# The complete app (binary + cooked content in Contents/UE) is the staged one.
# Development builds stage as DriftGame.app (Shipping would be DriftGame-Mac-Shipping.app).
STAGED="$PROJ_DIR/Saved/StagedBuilds/Mac/DriftGame.app"

if [ "$1" != "--run" ] && [ "$1" != "--debug" ]; then
  echo "== 1/3 Building editor code"
  "$UE/Build/BatchFiles/Mac/Build.sh" DriftGameEditor Mac Development -project="$P" -waitmutex

  echo "== 2/3 Creating materials / map extras (skips what exists)"
  "$UE/Binaries/Mac/UnrealEditor-Cmd" "$P" -ExecutePythonScript="$PROJ_DIR/Scripts/setup_materials.py" -unattended -nosplash -nullrhi >/dev/null

  echo "== 3/3 Packaging (first time takes a while: shaders are compiled and cached)"
  "$UE/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$P" -platform=Mac -clientconfig=Development \
    -build -cook -stage -pak -iostore -nop4 -utf8output -unattended
fi

# If the engine named the app differently, use the newest non-Shipping app it staged.
if [ ! -d "$STAGED" ]; then
  ALT="$(ls -td "$PROJ_DIR"/Saved/StagedBuilds/Mac/*.app 2>/dev/null | grep -v Shipping | head -n 1)"
  [ -n "$ALT" ] && STAGED="$ALT"
fi

# Copy the staged app (with its content) to Packaged/Tandem.app if it's newer or missing.
if [ ! -d "$STAGED" ]; then echo "No packaged build yet. Run without --run first."; exit 1; fi
if [ ! -d "$APP/Contents/UE" ] || [ "$STAGED" -nt "$APP" ]; then
  echo "== Copying build to $APP"
  rm -rf "$APP" "$OUT/Mac"
  mkdir -p "$OUT"
  ditto "$STAGED" "$APP"
  touch "$APP"
fi

if [ "$1" == "--debug" ]; then
  "$APP/Contents/MacOS/$(ls "$APP/Contents/MacOS" | head -n 1)"
else
  echo "== Launching Tandem"
  open "$APP"
fi
