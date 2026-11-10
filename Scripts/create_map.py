# Creates /Game/Maps/L_DriftTrack: track builder, player start, sun, sky.
# Run:  UnrealEditor-Cmd DriftGame.uproject -ExecutePythonScript="<this file>" -unattended -nosplash
import unreal

MAP = "/Game/Maps/L_DriftTrack"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if not unreal.EditorAssetLibrary.does_directory_exist("/Game/Maps"):
    unreal.EditorAssetLibrary.make_directory("/Game/Maps")

les.new_level(MAP)

builder_cls = unreal.load_class(None, "/Script/DriftGame.DriftTrackBuilder")
builder = eas.spawn_actor_from_class(builder_cls, unreal.Vector(0, 0, 0))
builder.set_actor_label("DriftTrack")
start = builder.get_start_transform()
rot = start.rotation.rotator()
eas.spawn_actor_from_class(unreal.PlayerStart, start.translation, rot).set_actor_label("PlayerStart")

sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(0, -40, -30))
sun.set_actor_label("Sun")
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000))
sky.set_actor_label("SkyLight")
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))

les.save_current_level()
unreal.log("DriftGame: created " + MAP)
