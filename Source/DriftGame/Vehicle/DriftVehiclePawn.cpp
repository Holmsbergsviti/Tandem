#include "DriftVehiclePawn.h"
#include "Vehicle/DriftCarMesh.h"
#include "Character/DriftDriverCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Modes/DriftGameModeBase.h"
#include "Input/DriftKeyBindings.h"
#include "Vehicle/DriftTuning.h"
#include "Vehicle/DriftStats.h"
#include "UI/DriftGraphicsSettings.h"
#include "Track/DriftTrackBuilder.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Track/DriftScenery.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	const TCHAR* ColorMaterialPath = TEXT("/Game/Materials/M_Color.M_Color");
	const TCHAR* VertexMaterialPath = TEXT("/Game/Materials/M_VertexColor.M_VertexColor");
	const TCHAR* GlassMaterialPath = TEXT("/Game/Materials/M_Glass.M_Glass");
	const TCHAR* SmokeMaterialPath = TEXT("/Game/Materials/M_Smoke.M_Smoke");
	// Looping sounds made by Scripts/setup_materials.py from Scripts/Audio/*.wav.
	const TCHAR* EngineLowSoundPath = TEXT("/Game/Audio/S_EngineLow.S_EngineLow");   // recorded at 1800 rpm
	const TCHAR* EngineHighSoundPath = TEXT("/Game/Audio/S_EngineHigh.S_EngineHigh"); // recorded at 5000 rpm
	// The NSX's V6 loops, same rpm (Scripts/Audio/make_v6.py). Missing until setup_materials.py has run once.
	const TCHAR* EngineLowV6SoundPath = TEXT("/Game/Audio/S_EngineLowV6.S_EngineLowV6");
	const TCHAR* EngineHighV6SoundPath = TEXT("/Game/Audio/S_EngineHighV6.S_EngineHighV6");
	const TCHAR* ExplosionSoundPath = TEXT("/Game/Audio/S_Explosion.S_Explosion");
	const TCHAR* WindSoundPath = TEXT("/Game/Audio/S_Wind.S_Wind");
	const TCHAR* TyreSoundPath = TEXT("/Game/Audio/S_TyreSqueal.S_TyreSqueal");
	constexpr int32 CockpitCameraIndex = 1;
	// Right-hand drive: the driver sits at Y = +38, eye at about 110 cm above the road.
	constexpr float DriverY = 38.f;
	const FVector ShoulderL(-62.f, DriverY - 19.f, 39.f);
	const FVector ShoulderR(-62.f, DriverY + 19.f, 39.f);
	const FQuat WiperTilt(FRotator(-37.f, 0.f, 0.f)); // windscreen rake

	struct FCamMode { const TCHAR* Name; float Arm; FVector Socket; FVector Pos; float Pitch; float Fov; bool bLag; };
	const FCamMode CamModes[] = {
		{ TEXT("Chase"),     580.f, FVector(0, 0, 140), FVector(0, 0, 20),     -10.f, 92.f,  true  },
		{ TEXT("Cockpit"),   0.f,   FVector::ZeroVector, FVector(-52, 38, 57), -8.f, 84.f,  false },
		{ TEXT("Hood"),      0.f,   FVector::ZeroVector, FVector(70, 0, 44),   -3.f, 95.f,  false },
		{ TEXT("Bumper"),    0.f,   FVector::ZeroVector, FVector(216, 0, -16), -1.f, 100.f, false },
		{ TEXT("Far chase"), 900.f, FVector(0, 0, 240), FVector(0, 0, 20),     -13.f, 85.f,  true  },
	};
	constexpr float NewtonToUU = 100.f; // 1 N = 100 kg*cm/s^2
	constexpr float Gravity = 9.81f;
	constexpr float AirDensity = 1.225f;
	constexpr float DoorOpenDeg = 62.f;
	const TCHAR* WheelNames[4] = { TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR") };

	// ---- Car presets (menu > Car) ----
	// 4A-GE: peaky twin-cam, peak torque at 5200 rpm, peak power near 6600, fading to the 7600 redline.
	const FVector2D CurveAE86[] = {
		{ 800.f, 0.62f }, { 2000.f, 0.72f }, { 3000.f, 0.82f }, { 4000.f, 0.92f }, { 5200.f, 1.f },
		{ 6000.f, 0.98f }, { 6600.f, 0.94f }, { 7200.f, 0.84f }, { 7600.f, 0.76f } };
	// C30A V6 with a supercharger: fuller low down, peak torque at 5000, still pulling to the 8000 redline.
	const FVector2D CurveNSX[] = {
		{ 800.f, 0.6f }, { 2000.f, 0.74f }, { 3000.f, 0.85f }, { 4000.f, 0.94f }, { 5000.f, 1.f },
		{ 6000.f, 0.99f }, { 7000.f, 0.95f }, { 7600.f, 0.88f }, { 8000.f, 0.82f } };

	struct FCarSpec
	{
		const TCHAR* Id;
		const TCHAR* Name;
		const FVector2D* Curve; int32 CurveN; float CurveRedline;
		float Mass;
		FVector CoM;          // centre of mass, body space (cm)
		FVector Box;          // physics box half size (cm)
		float FrontX, RearX, HalfTrack;
		float WheelRadius, TyreHalfWidth;
		float SpringF, SpringR, Damper;
		float CdA;
		float Torque, Redline, Idle, Launch, Inertia;
		float Gears[5]; float Reverse, Final;
		float ClutchMax, LSDMax;
		float Brake, BrakeBias;
		float Steer;
		FLinearColor Paint;
	};
	const FCarSpec Specs[] = {
		// Toyota AE86 Sprinter Trueno, drift build: 975 kg 53/47, 2400 mm wheelbase, 4A-GE built to 330 Nm.
		{ TEXT("AE86"), TEXT("Toyota AE86  drift build"), CurveAE86, (int32)UE_ARRAY_COUNT(CurveAE86), 7600.f,
		  975.f, FVector(2.f, 0.f, -5.f), FVector(205.f, 78.f, 32.f), 115.f, -125.f, 68.f, 29.f, 9.25f,
		  24000.f, 22000.f, 1900.f, 0.63f,
		  330.f, 7600.f, 850.f, 4500.f, 0.15f,
		  { 3.587f, 2.022f, 1.384f, 1.0f, 0.861f }, 3.484f, 4.3f, 420.f, 450.f,
		  9000.f, 0.72f, 60.f, FLinearColor(0.82f, 0.82f, 0.8f) },
		// Honda NSX (NA1), supercharged drift build: 1370 kg 42/58 (mid-engine), 2530 mm wheelbase,
		// 1510 mm track, C30A V6 at 400 Nm, 8000 rpm, NSX 5-speed and final drive.
		{ TEXT("NSX"), TEXT("Honda NSX  supercharged"), CurveNSX, (int32)UE_ARRAY_COUNT(CurveNSX), 8000.f,
		  1370.f, FVector(-20.f, 0.f, -8.f), FVector(220.f, 86.f, 30.f), 126.f, -127.f, 76.f, 29.5f, 10.5f,
		  26000.f, 35000.f, 2500.f, 0.58f,
		  400.f, 8000.f, 800.f, 4500.f, 0.12f,
		  { 3.071f, 1.947f, 1.423f, 1.091f, 0.888f }, 3.0f, 4.062f, 520.f, 550.f,
		  13000.f, 0.62f, 60.f, FLinearColor(0.45f, 0.015f, 0.012f) },
	};
	constexpr int32 NumSpecs = static_cast<int32>(UE_ARRAY_COUNT(Specs));
	const FCarSpec& Spec(int32 Model) { return Specs[FMath::Clamp(Model, 0, NumSpecs - 1)]; }

	float TorqueShape(const FCarSpec& C, float Rpm, float MaxRpm)
	{
		const float RpmScale = MaxRpm / C.CurveRedline; // stretch the curve if the rev limit is tuned
		if (Rpm <= C.Curve[0].X * RpmScale) return (float)C.Curve[0].Y;
		for (int32 i = 1; i < C.CurveN; ++i)
		{
			if (Rpm <= C.Curve[i].X * RpmScale)
			{
				const float A = (float)((Rpm - C.Curve[i - 1].X * RpmScale) / ((C.Curve[i].X - C.Curve[i - 1].X) * RpmScale));
				return (float)FMath::Lerp(C.Curve[i - 1].Y, C.Curve[i].Y, (double)A);
			}
		}
		return (float)C.Curve[C.CurveN - 1].Y;
	}

	/** Moves V towards zero by at most Step, never past it (how a brake acts on a spinning wheel). */
	float MoveTowardsZero(float V, float Step)
	{
		return V > 0.f ? FMath::Max(0.f, V - Step) : FMath::Min(0.f, V + Step);
	}
}

static UStaticMeshComponent* MakePart(APawn* Owner, const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh,
	const FVector& Loc, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator)
{
	UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(Parent);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetRelativeLocation(Loc);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(Scale);
	return C;
}

static UProceduralMeshComponent* MakeProc(APawn* Owner, const TCHAR* Name, USceneComponent* Parent,
	const FVector& Loc = FVector::ZeroVector, const FRotator& Rot = FRotator::ZeroRotator)
{
	UProceduralMeshComponent* C = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
	C->SetupAttachment(Parent);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	return C;
}

static USceneComponent* MakeScene(APawn* Owner, const TCHAR* Name, USceneComponent* Parent, const FVector& Loc, const FRotator& Rot)
{
	USceneComponent* C = Owner->CreateDefaultSubobject<USceneComponent>(Name);
	C->SetupAttachment(Parent);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	return C;
}

namespace DriftHands
{
	constexpr float RimRadius = 17.5f;
	constexpr float ZoneLo[2] = { -70.f, 30.f };   // each hand's comfortable stretch of the rim
	constexpr float ZoneHi[2] = { 150.f, 250.f };
	constexpr float MaxFollowDegPerSec = 450.f;    // a faster spin slides through the palms (caster snap)
	constexpr float Spacing = 60.f;                // re-grip this far behind the other hand
	constexpr float TransitTime = 0.15f;
	constexpr float LiftCm = 7.f;                  // the crossing hand passes over, towards the driver
}

static void PlaceArm(UProceduralMeshComponent* Arm, UProceduralMeshComponent* Hand, const FVector& Shoulder, const FVector& HandPos)
{
	const FVector Dir = HandPos - Shoulder;
	const FRotator Rot = Dir.Rotation();
	Arm->SetRelativeLocationAndRotation((Shoulder + HandPos) * 0.5f, Rot);
	Arm->SetRelativeScale3D(FVector(FMath::Max(1.f, (float)Dir.Size() - 3.f), 1.f, 1.f));
	Hand->SetRelativeLocationAndRotation(HandPos, Rot);
}

ADriftVehiclePawn::ADriftVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = CubeF.Object;

	// Physics body: 4.1 m x 1.56 m x 0.64 m box (the AE86 is 4.2 x 1.63 m).
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetBoxExtent(FVector(205.f, 78.f, 32.f));
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetSimulatePhysics(true);
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetLinearDamping(0.f);   // all resistance comes from drag, rolling resistance and the tyres
	Body->SetAngularDamping(0.f);
	Body->BodyInstance.bUseCCD = true; // continuous collision: no tunnelling through walls at speed
	Body->BodyInstance.bOverrideMass = true;
	Body->BodyInstance.SetMassOverride(MassKg);
	Body->SetHiddenInGame(true);

	// Visual body: sculpted procedural mesh (see DriftCarMesh.cpp).
	CarBody = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CarBody"));
	CarBody->SetupAttachment(Body);
	CarBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarBody->SetRelativeLocation(FVector::ZeroVector);

	// Doors hinge at their front edge.
	DoorHingeL = MakeScene(this, TEXT("DoorHingeL"), Body, DriftCarMesh::DoorHinge(0), FRotator::ZeroRotator);
	DoorHingeR = MakeScene(this, TEXT("DoorHingeR"), Body, DriftCarMesh::DoorHinge(1), FRotator::ZeroRotator);
	DoorL = MakeProc(this, TEXT("DoorL"), DoorHingeL);
	DoorR = MakeProc(this, TEXT("DoorR"), DoorHingeR);
	DoorL->SetCastShadow(true);
	DoorR->SetCastShadow(true);

	WiperPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WiperPivot"));
	WiperPivot->SetupAttachment(Body);
	WiperPivot->SetRelativeLocation(FVector(52, 30, 38.f));
	WiperPivot->SetRelativeRotation(WiperTilt);
	WiperBlade = MakePart(this, TEXT("WiperBlade"), WiperPivot, Cube, FVector(0, -32, 0.6f), FVector(0.025f, 0.62f, 0.02f));

	// Wheels: 2400 mm wheelbase, 1355 mm track. Centre of mass sits 53/47 between the axles.
	const FVector Mounts[4] = { {115, -68, -10}, {115, 68, -10}, {-125, -68, -10}, {-125, 68, -10} };
	const TCHAR* Names[4] = { TEXT("WheelFL"), TEXT("WheelFR"), TEXT("WheelRL"), TEXT("WheelRR") };
	Wheels.SetNum(4);
	for (int32 i = 0; i < 4; ++i)
	{
		Wheels[i].LocalMount = Mounts[i];
		Wheels[i].bFront = i < 2;
		UProceduralMeshComponent* WM = CreateDefaultSubobject<UProceduralMeshComponent>(Names[i]);
		WM->SetupAttachment(Body);
		WM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WM->SetRelativeLocation(Mounts[i]);
		WM->SetRelativeScale3D(FVector(1.f, Mounts[i].Y < 0.f ? -1.f : 1.f, 1.f)); // rim faces outwards
		Wheels[i].Visual = WM;
	}

	// Cockpit: dash, seats, steering wheel, handbrake, shifter and the driver's arms.
	Interior       = MakeProc(this, TEXT("Interior"), Body);
	SteeringPivot  = MakeScene(this, TEXT("SteeringPivot"), Body, FVector(6, DriverY, 27), FRotator(-22, 180, 0));
	SteeringSpin   = MakeScene(this, TEXT("SteeringSpin"), SteeringPivot, FVector::ZeroVector, FRotator::ZeroRotator);
	SteeringWheel  = MakeProc(this, TEXT("SteeringWheel"), SteeringSpin, FVector::ZeroVector, FRotator(0, 90, 0));
	// Drift-style vertical hydraulic handbrake next to the shifter, on the driver's side of the tunnel.
	HandbrakePivot = MakeScene(this, TEXT("HandbrakePivot"), Body, FVector(-4, 14, -6), FRotator(-8, 0, 0));
	HandbrakeLever = MakeProc(this, TEXT("HandbrakeLever"), HandbrakePivot);
	ShifterPivot   = MakeScene(this, TEXT("ShifterPivot"), Body, FVector(8, 3, -4), FRotator::ZeroRotator);
	ShifterStick   = MakeProc(this, TEXT("ShifterStick"), ShifterPivot);
	HandL = MakeProc(this, TEXT("HandL"), Body);
	HandR = MakeProc(this, TEXT("HandR"), Body);
	ArmL  = MakeProc(this, TEXT("ArmL"), Body);
	ArmR  = MakeProc(this, TEXT("ArmR"), Body);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Body);
	SpringArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 650.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 120.f);
	SpringArm->bDoCollisionTest = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 7.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->SetFieldOfView(95.f);

	// Tyre smoke: a pool of soft translucent spheres living in world space (they stay where they were puffed).
	TyreSmoke = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TyreSmoke"));
	TyreSmoke->SetupAttachment(Body);
	TyreSmoke->SetUsingAbsoluteLocation(true);
	TyreSmoke->SetUsingAbsoluteRotation(true);
	TyreSmoke->SetUsingAbsoluteScale(true);
	TyreSmoke->SetStaticMesh(SphereF.Object);
	TyreSmoke->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TyreSmoke->SetCastShadow(false);
	TyreSmoke->SetNumCustomDataFloats(1); // per-puff opacity

	// Instrument needles (rev counter, speedometer), turned about X every frame.
	// Parts a crash can knock off, and the engine fire (hidden until the engine blows).
	PartFrontBumper = MakeProc(this, TEXT("PartFrontBumper"), Body, DriftCarMesh::LoosePartPivot(0, 0));
	PartRearBumper  = MakeProc(this, TEXT("PartRearBumper"), Body, DriftCarMesh::LoosePartPivot(0, 1));
	PartSpoiler     = MakeProc(this, TEXT("PartSpoiler"), Body, DriftCarMesh::LoosePartPivot(0, 2));
	for (UProceduralMeshComponent* Part : { PartFrontBumper.Get(), PartRearBumper.Get(), PartSpoiler.Get() }) Part->SetCastShadow(true);
	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Body);
	FireLight->SetLightColor(FLinearColor(1.f, 0.42f, 0.08f));
	FireLight->SetAttenuationRadius(1800.f);
	FireLight->SetCastShadows(false);
	FireLight->SetIntensity(0.f);
	FireLight->SetVisibility(false);
	FireBall = MakePart(this, TEXT("FireBall"), Body, SphereF.Object, FVector::ZeroVector, FVector(0.5f));
	FireBall->SetCastShadow(false);
	FireBall->SetVisibility(false);

	TachNeedle  = MakeProc(this, TEXT("TachNeedle"), Body, DriftCarMesh::TachCentre());
	SpeedNeedle = MakeProc(this, TEXT("SpeedNeedle"), Body, DriftCarMesh::SpeedoCentre());

	// Sound: two engine loops cross-faded by rpm, wind, tyre squeal. Pitch and volume are set every frame.
	auto MakeAudio = [this](const TCHAR* Name)
	{
		UAudioComponent* A = CreateDefaultSubobject<UAudioComponent>(Name);
		A->SetupAttachment(Body);
		A->bAutoActivate = false;
		A->bAllowSpatialization = false;
		return A;
	};
	EngineLowAudio = MakeAudio(TEXT("EngineLowAudio"));
	EngineHighAudio = MakeAudio(TEXT("EngineHighAudio"));
	WindAudio = MakeAudio(TEXT("WindAudio"));
	TyreAudio = MakeAudio(TEXT("TyreAudio"));

	// A touch of motion blur on the scenery streaming past (the car itself moves with the camera).
	Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	Camera->PostProcessSettings.MotionBlurAmount = 0.4f;
	Camera->PostProcessSettings.bOverride_MotionBlurMax = true;
	Camera->PostProcessSettings.MotionBlurMax = 3.f;
}

void ADriftVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	UAudioComponent* Comps[4] = { EngineLowAudio, EngineHighAudio, WindAudio, TyreAudio };
	const TCHAR* Paths[4] = { EngineLowSoundPath, EngineHighSoundPath, WindSoundPath, TyreSoundPath };
	for (int32 i = 0; i < 4; ++i)
	{
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, Paths[i]);
		if (!Comps[i] || !Sound) continue;
		Comps[i]->SetSound(Sound);
		Comps[i]->SetVolumeMultiplier(0.001f);
		Comps[i]->Play();
	}
	// The car picked last time (menu > Car) and the player's saved setup for it (menu > Tuning).
	FString SavedCar;
	int32 Model = 0;
	if (GConfig && GConfig->GetString(TEXT("Tandem.Car"), TEXT("Model"), SavedCar, GGameUserSettingsIni))
	{
		for (int32 i = 0; i < NumSpecs; ++i) if (SavedCar == Specs[i].Id) Model = i;
	}
	ApplyCarSpec(Model);
	DriftTuning::Load(this);
	MapToastName = TEXT("PRACTICE LOT      M: last checkpoint, again: next map");
	MapToastAge = 0.f;
}

void ADriftVehiclePawn::Paint(UStaticMeshComponent* C, const FLinearColor& Color, UMaterialInterface* Base)
{
	if (!C || !Base) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	C->SetMaterial(0, MID);
}

void ADriftVehiclePawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Body->BodyInstance.SetMassOverride(MassKg);
	Body->SetCenterOfMass(FVector(2.f, 0.f, -5.f)); // 53/47 front/rear, about 48 cm above the road

	BuildVisuals();
	ApplyCameraMode();
}

void ADriftVehiclePawn::BuildVisuals()
{
	DriftCarMesh::BuildBody(CarBody, PaintColor, DoorL, DoorR, CarModel);
	DriftCarMesh::BuildLoosePart(PartFrontBumper, CarModel, 0, PaintColor);
	DriftCarMesh::BuildLoosePart(PartRearBumper, CarModel, 1, PaintColor);
	DriftCarMesh::BuildLoosePart(PartSpoiler, CarModel, 2, PaintColor);
	for (FDriftWheel& W : Wheels) DriftCarMesh::BuildWheel(W.Visual, WheelRadius, TyreHalfWidth);
	DriftCarMesh::BuildInterior(Interior, CarModel == 1 ? 9.f : 0.f);
	Interior->SetMeshSectionVisible(2, CarModel == 0); // the AE86 has a cage; the NSX's lower roof doesn't fit it
	DriftCarMesh::BuildSteeringWheel(SteeringWheel);
	DriftCarMesh::BuildHandbrakeLever(HandbrakeLever);
	DriftCarMesh::BuildShifter(ShifterStick);
	DriftCarMesh::BuildHand(HandL);
	DriftCarMesh::BuildHand(HandR);
	DriftCarMesh::BuildArm(ArmL);
	DriftCarMesh::BuildArm(ArmR);
	DriftCarMesh::BuildNeedle(TachNeedle, 4.6f);
	DriftCarMesh::BuildNeedle(SpeedNeedle, 4.6f);

	UMaterialInterface* GlassMat = LoadObject<UMaterialInterface>(nullptr, GlassMaterialPath);
	if (UMaterialInterface* VMat = LoadObject<UMaterialInterface>(nullptr, VertexMaterialPath))
	{
		auto MakeMID = [&](float Rough, float Metal, float Emissive)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(VMat, this);
			MID->SetScalarParameterValue(TEXT("Roughness"), Rough);
			MID->SetScalarParameterValue(TEXT("Metallic"), Metal);
			MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
			return MID;
		};
		UMaterialInstanceDynamic* PaintMID = MakeMID(0.28f, 0.05f, 0.f); // solid white paint, not metallic
		UMaterialInstanceDynamic* LightMID = MakeMID(0.3f, 0.f, 6.f);
		UMaterialInstanceDynamic* TyreMID = MakeMID(0.9f, 0.f, 0.f);
		UMaterialInstanceDynamic* RimMID = MakeMID(0.25f, 0.95f, 0.f);
		UMaterialInterface* GlassOrFallback = GlassMat ? GlassMat : static_cast<UMaterialInterface*>(MakeMID(0.05f, 0.6f, 0.f));
		CarBody->SetMaterial(0, PaintMID);
		CarBody->SetMaterial(1, LightMID);
		CarBody->SetMaterial(2, GlassOrFallback);
		DoorL->SetMaterial(0, PaintMID);
		DoorL->SetMaterial(1, GlassOrFallback);
		DoorR->SetMaterial(0, PaintMID);
		DoorR->SetMaterial(1, GlassOrFallback);
		PartFrontBumper->SetMaterial(0, PaintMID);
		PartRearBumper->SetMaterial(0, PaintMID);
		PartSpoiler->SetMaterial(0, PaintMID);
		UMaterialInstanceDynamic* CabinMID = MakeMID(0.7f, 0.f, 0.f);
		UMaterialInstanceDynamic* MetalMID = MakeMID(0.35f, 0.6f, 0.f);
		Interior->SetMaterial(0, CabinMID);
		SteeringWheel->SetMaterial(0, CabinMID);
		HandbrakeLever->SetMaterial(0, MetalMID);
		ShifterStick->SetMaterial(0, MetalMID);
		HandL->SetMaterial(0, CabinMID);
		HandR->SetMaterial(0, CabinMID);
		ArmL->SetMaterial(0, CabinMID);
		ArmR->SetMaterial(0, CabinMID);
		UMaterialInstanceDynamic* GaugeMID = MakeMID(0.4f, 0.f, 2.5f); // backlit dials
		Interior->SetMaterial(1, GaugeMID);
		Interior->SetMaterial(2, MetalMID); // roll cage
		TachNeedle->SetMaterial(0, GaugeMID);
		SpeedNeedle->SetMaterial(0, GaugeMID);
		for (FDriftWheel& W : Wheels)
		{
			W.Visual->SetMaterial(0, TyreMID);
			W.Visual->SetMaterial(1, RimMID);
		}
	}
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, ColorMaterialPath))
	{
		Paint(WiperBlade, FLinearColor(0.01f, 0.01f, 0.01f), Base);
	}
	if (UMaterialInterface* SmokeMat = LoadObject<UMaterialInterface>(nullptr, SmokeMaterialPath))
	{
		TyreSmoke->SetMaterial(0, SmokeMat);
	}
}

void ADriftVehiclePawn::ApplyCameraMode()
{
	const int32 Num = UE_ARRAY_COUNT(CamModes);
	CameraIndex = ((CameraIndex % Num) + Num) % Num;
	const FCamMode& C = CamModes[CameraIndex];
	SpringArm->TargetArmLength = C.Arm;
	SpringArm->SocketOffset = C.Socket;
	SpringArm->SetRelativeLocation(C.Pos);
	SpringArm->SetRelativeRotation(FRotator(C.Pitch, 0.f, 0.f));
	SpringArm->bEnableCameraLag = C.bLag;
	SpringArm->bEnableCameraRotationLag = C.bLag;
	SpringArm->bInheritPitch = !C.bLag;
	SpringArm->bInheritRoll = !C.bLag;
	Camera->SetFieldOfView(C.Fov);
	BaseFov = C.Fov;
	CameraSwitchAge = 0.f;

	// The driver's arms only make sense from inside, with someone in the seat.
	const bool bArms = IsCockpitCamera() && bOccupied;
	if (HandL) { HandL->SetVisibility(bArms); HandR->SetVisibility(bArms); ArmL->SetVisibility(bArms); ArmR->SetVisibility(bArms); }
}

bool ADriftVehiclePawn::IsCockpitCamera() const
{
	return CameraIndex == CockpitCameraIndex;
}

void ADriftVehiclePawn::UpdateCockpit(float Dt)
{
	// Instrument needles.
	const float Sweep = DriftCarMesh::GaugeSweepDeg;
	TachNeedle->SetRelativeRotation(FRotator(0.f, 0.f, -0.5f * Sweep + Sweep * FMath::Clamp(RPM / 9000.f, 0.f, 1.f)));
	SpeedNeedle->SetRelativeRotation(FRotator(0.f, 0.f, -0.5f * Sweep + Sweep * FMath::Clamp(GetSpeedKmh() / 220.f, 0.f, 1.f)));

	// Steering wheel: 540 degrees lock to lock, following the front wheels (your input plus the caster
	// self-steer in a slide), smoothed so keyboard steps don't jerk it.
	const float WheelTarget = MaxSteerDeg > KINDA_SMALL_NUMBER ? SteerAngleDeg / MaxSteerDeg * WheelTurnDeg : 0.f;
	const float PrevWheel = WheelVisDeg;
	WheelVisDeg = FMath::FInterpTo(WheelVisDeg, WheelTarget, Dt, 14.f);
	const float WheelDelta = WheelVisDeg - PrevWheel;
	SteeringSpin->SetRelativeRotation(FRotator(0.f, 0.f, -WheelVisDeg));

	// Handbrake lever.
	const bool bPulling = bHandbrakeIn && !bInputLocked && bOccupied;
	HandbrakePull = FMath::FInterpTo(HandbrakePull, bPulling ? 1.f : 0.f, Dt, 14.f);
	HandbrakePivot->SetRelativeRotation(FRotator(-8.f + 30.f * HandbrakePull, 0.f, 0.f)); // top comes back towards the driver

	// H-pattern shifter: 1/2 left, 3/4 middle, 5 right, R right-back. Odd forward, even back.
	float Pitch = 0.f, Roll = 0.f;
	if (Gear < 0) { Pitch = 12.f; Roll = -15.f; }
	else if (Gear > 0)
	{
		Pitch = (Gear % 2 == 1) ? -12.f : 12.f;
		Roll = Gear <= 2 ? 10.f : (Gear <= 4 ? 0.f : -10.f);
	}
	ShifterRot = FMath::RInterpTo(ShifterRot, FRotator(Pitch, 0.f, Roll), Dt, 18.f);
	ShifterPivot->SetRelativeRotation(ShifterRot);
	ShiftTimer = FMath::Max(0.f, ShiftTimer - Dt);

	// Hands: hand over hand like a real driver. Each hand holds the rim and turns with it until it runs out
	// of comfortable reach, then lets go and re-grips behind the other hand, crossing over it. Only one hand
	// is ever off the rim. Right-hand drive: the left hand also works the shifter and handbrake.
	using namespace DriftHands;
	const bool bLeftBusy = bPulling || HandbrakePull > 0.1f || ShiftTimer > 0.f;
	if (bLeftBusy) { bLeftHandAway = true; HandT[0] = -1.f; }
	else if (!IsCockpitCamera() || !bOccupied) bLeftHandAway = false;

	auto RegripDeg = [this](int32 H)
	{
		const float Other = HandRimDeg[1 - H];
		if (HandDir[H] > 0) return FMath::Clamp(Other - Spacing, ZoneLo[H] + 20.f, ZoneHi[H] - 40.f);
		if (HandDir[H] < 0) return FMath::Clamp(Other + Spacing, ZoneLo[H] + 40.f, ZoneHi[H] - 20.f);
		return FMath::Clamp(H == 0 ? Other - 140.f : Other + 140.f, ZoneLo[H] + 20.f, ZoneHi[H] - 20.f);
	};
	for (int32 H = 0; H < 2; ++H)
	{
		if (H == 0 && bLeftHandAway) { HandDir[0] = 0; HandRimDeg[0] = RegripDeg(0); continue; }
		if (HandT[H] >= 0.f)
		{
			HandT[H] += Dt / TransitTime;
			if (HandT[H] >= 1.f) { HandRimDeg[H] = RegripDeg(H); HandT[H] = -1.f; }
			continue;
		}
		const float MaxStep = MaxFollowDegPerSec * Dt;
		HandRimDeg[H] = FMath::Clamp(HandRimDeg[H] + FMath::Clamp(WheelDelta, -MaxStep, MaxStep), ZoneLo[H] - 40.f, ZoneHi[H] + 40.f);
		const bool bOtherOff = HandT[1 - H] >= 0.f || (H == 1 && bLeftHandAway);
		if (bOtherOff || (HandRimDeg[H] >= ZoneLo[H] && HandRimDeg[H] <= ZoneHi[H])) continue;
		HandDir[H] = HandRimDeg[H] > ZoneHi[H] ? 1 : -1;
		HandFromDeg[H] = HandRimDeg[H];
		HandT[H] = 0.f;
	}

	if (!IsCockpitCamera() || !bOccupied) return;

	const FTransform BodyTM = Body->GetComponentTransform();
	const FTransform ColumnTM = SteeringPivot->GetComponentTransform();
	auto RimPoint = [&](float Deg, float Lift)
	{
		const float R = FMath::DegreesToRadians(Deg);
		return BodyTM.InverseTransformPosition(ColumnTM.TransformPosition(FVector(1.f + Lift, RimRadius * FMath::Cos(R), RimRadius * FMath::Sin(R))));
	};
	auto HandPos = [&](int32 H)
	{
		if (HandT[H] < 0.f) return RimPoint(HandRimDeg[H], 0.f);
		const float A = FMath::Clamp(HandT[H], 0.f, 1.f);
		const float E = A * A * (3.f - 2.f * A);
		return RimPoint(FMath::Lerp(HandFromDeg[H], RegripDeg(H), E), LiftCm * FMath::Sin(PI * A));
	};
	const FVector LeverTop = BodyTM.InverseTransformPosition(HandbrakePivot->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, 34.f)));
	const FVector Knob = BodyTM.InverseTransformPosition(ShifterPivot->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, 23.f)));

	FVector TargetL = HandPos(0);
	if (bPulling || HandbrakePull > 0.1f) TargetL = LeverTop;
	else if (ShiftTimer > 0.f) TargetL = Knob;
	if (bLeftHandAway || ShiftHandPos.IsNearlyZero())
	{
		ShiftHandPos = ShiftHandPos.IsNearlyZero() ? TargetL : FMath::VInterpTo(ShiftHandPos, TargetL, Dt, 22.f);
		if (!bLeftBusy && FVector::DistSquared(ShiftHandPos, TargetL) < 4.f) bLeftHandAway = false; // back on the rim
	}
	else ShiftHandPos = TargetL;

	PlaceArm(ArmL, HandL, ShoulderL, ShiftHandPos);
	PlaceArm(ArmR, HandR, ShoulderR, HandPos(1));
}

// ---------------- Doors, getting in and out ----------------

void ADriftVehiclePawn::UpdateDoors(float Dt)
{
	USceneComponent* Hinges[2] = { DoorHingeL, DoorHingeR };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (DoorCloseTimer[Side] > 0.f)
		{
			DoorCloseTimer[Side] -= Dt;
			if (DoorCloseTimer[Side] <= 0.f) bDoorOpen[Side] = false;
		}
		DoorAngle[Side] = FMath::FInterpTo(DoorAngle[Side], bDoorOpen[Side] ? DoorOpenDeg : 0.f, Dt, 6.f);
		// Left door swings out to -Y (positive yaw), right door to +Y (negative yaw).
		if (Hinges[Side]) Hinges[Side]->SetRelativeRotation(FRotator(0.f, Side == 0 ? DoorAngle[Side] : -DoorAngle[Side], 0.f));
	}
}

void ADriftVehiclePawn::ToggleDoor(int32 Side)
{
	if (Side < 0 || Side > 1) return;
	bDoorOpen[Side] = !bDoorOpen[Side];
	DoorCloseTimer[Side] = 0.f;
}

void ADriftVehiclePawn::OpenDoorBriefly(int32 Side)
{
	if (Side < 0 || Side > 1) return;
	bDoorOpen[Side] = true;
	DoorCloseTimer[Side] = 1.4f;
}

FVector ADriftVehiclePawn::GetDoorStandPoint(int32 Side) const
{
	const float Y = (Side == 0 ? -1.f : 1.f) * 150.f;
	return GetActorTransform().TransformPosition(FVector(-10.f, Y, DriftCarMesh::GroundZ + 95.f));
}

int32 ADriftVehiclePawn::GetNearestDoor(const FVector& WorldPos, float MaxDist) const
{
	int32 Best = -1;
	float BestDist = MaxDist;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVector Door = GetActorTransform().TransformPosition(FVector(-5.f, (Side == 0 ? -1.f : 1.f) * 100.f, DriftCarMesh::GroundZ + 95.f));
		const float D = FVector::Dist2D(Door, WorldPos);
		if (D < BestDist) { BestDist = D; Best = Side; }
	}
	return Best;
}

void ADriftVehiclePawn::OnExitCar()
{
	if (!bOccupied) return;
	if (GetSpeedKmh() > 8.f) { ExitBlockedAge = 0.f; return; } // stop first
	APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Facing(0.f, GetActorRotation().Yaw + 90.f, 0.f);
	ADriftDriverCharacter* Driver = World->SpawnActor<ADriftDriverCharacter>(ADriftDriverCharacter::StaticClass(), GetDoorStandPoint(DriverSide), Facing, Params);
	if (!Driver) return;
	Driver->SetCar(this);

	ThrottleIn = 0.f; BrakeIn = 0.f; SteerIn = 0.f; bHandbrakeIn = false; bPartThrottleIn = false;
	bOccupied = false;
	OpenDoorBriefly(DriverSide);
	PC->Possess(Driver);
	PC->SetControlRotation(Facing);
	ApplyCameraMode();
}

void ADriftVehiclePawn::EnterCar(ADriftDriverCharacter* Driver)
{
	if (bOccupied || !Driver) return;
	APlayerController* PC = Cast<APlayerController>(Driver->GetController());
	if (!PC) return;
	const int32 Side = GetNearestDoor(Driver->GetActorLocation(), 400.f);
	OpenDoorBriefly(Side >= 0 ? Side : DriverSide);
	bOccupied = true;
	PC->Possess(this);
	Driver->Destroy();
	ApplyCameraMode();
}

FString ADriftVehiclePawn::GetCameraName() const
{
	return CamModes[FMath::Clamp(CameraIndex, 0, (int32)UE_ARRAY_COUNT(CamModes) - 1)].Name;
}

float ADriftVehiclePawn::GetSpeedKmh() const
{
	return GetVelocity().Size() * 0.036f;
}

float ADriftVehiclePawn::CurrentRatio() const
{
	if (Gear == 0) return 0.f;
	if (Gear < 0) return -ReverseRatio * FinalDrive;
	return GearRatios.IsValidIndex(Gear - 1) ? GearRatios[Gear - 1] * FinalDrive : 0.f;
}

float ADriftVehiclePawn::EngineTorqueAt(float Rpm) const
{
	return MaxTorqueNm * TorqueShape(Spec(CarModel), Rpm, MaxRPM);
}

float ADriftVehiclePawn::GetPeakPowerHp() const
{
	float BestW = 0.f;
	for (float Rpm = 1000.f; Rpm <= MaxRPM; Rpm += 100.f) BestW = FMath::Max(BestW, EngineTorqueAt(Rpm) * Rpm * 2.f * PI / 60.f);
	return BestW / 745.7f;
}

void ADriftVehiclePawn::ApplyTuning()
{
	if (Body) Body->SetMassOverrideInKg(NAME_None, MassKg, true);
}

int32 ADriftVehiclePawn::NumCarModels() { return NumSpecs; }
FString ADriftVehiclePawn::CarModelName(int32 Model) { return Spec(Model).Name; }
FString ADriftVehiclePawn::CarModelId(int32 Model) { return Spec(Model).Id; }

float ADriftVehiclePawn::CarModelPeakHp(int32 Model)
{
	const FCarSpec& C = Spec(Model);
	float BestW = 0.f;
	for (float Rpm = 1000.f; Rpm <= C.Redline; Rpm += 100.f) BestW = FMath::Max(BestW, C.Torque * TorqueShape(C, Rpm, C.Redline) * Rpm * 2.f * PI / 60.f);
	return BestW / 745.7f;
}

void ADriftVehiclePawn::ApplyCarSpec(int32 Model)
{
	const FCarSpec& C = Spec(Model);
	const ADriftVehiclePawn* Def = GetDefault<ADriftVehiclePawn>();
	CarModel = FMath::Clamp(Model, 0, NumSpecs - 1);
	MassKg = C.Mass;
	WheelRadius = C.WheelRadius;
	TyreHalfWidth = C.TyreHalfWidth;
	SpringRate = C.SpringF;
	SpringRateRear = C.SpringR;
	DamperRate = C.Damper;
	DragCdA = C.CdA;
	MaxTorqueNm = C.Torque;
	MaxRPM = C.Redline;
	IdleRPM = C.Idle;
	LaunchRPM = C.Launch;
	EngineInertia = C.Inertia;
	GearRatios = { C.Gears[0], C.Gears[1], C.Gears[2], C.Gears[3], C.Gears[4] };
	ReverseRatio = C.Reverse;
	FinalDrive = C.Final;
	ClutchMaxNm = C.ClutchMax;
	LSDMaxLockNm = C.LSDMax;
	BrakeForce = C.Brake;
	BrakeBias = C.BrakeBias;
	MaxSteerDeg = C.Steer;
	PaintColor = C.Paint;
	// Engine sound: the V6 has its own loops. If they aren't in the build yet, the four-cylinder loops
	// are pitched up instead (a six fires three times a turn against two).
	EnginePitchScale = 1.f;
	if (EngineLowAudio && EngineHighAudio && GetWorld() && GetWorld()->IsGameWorld())
	{
		USoundBase* Low = CarModel == 1 ? LoadObject<USoundBase>(nullptr, EngineLowV6SoundPath) : nullptr;
		USoundBase* High = CarModel == 1 ? LoadObject<USoundBase>(nullptr, EngineHighV6SoundPath) : nullptr;
		if (CarModel == 1 && (!Low || !High)) EnginePitchScale = 1.3f;
		if (!Low || !High)
		{
			Low = LoadObject<USoundBase>(nullptr, EngineLowSoundPath);
			High = LoadObject<USoundBase>(nullptr, EngineHighSoundPath);
		}
		UAudioComponent* Comps[2] = { EngineLowAudio, EngineHighAudio };
		USoundBase* Sounds[2] = { Low, High };
		for (int32 i = 0; i < 2; ++i)
		{
			if (!Sounds[i] || Comps[i]->GetSound() == Sounds[i]) continue;
			Comps[i]->SetSound(Sounds[i]);
			Comps[i]->SetVolumeMultiplier(0.001f);
			Comps[i]->Play();
		}
	}
	// Tyre values everyone tunes start from the class defaults.
	TyreGrip = Def->TyreGrip;
	TyreGripRear = Def->TyreGripRear;
	SlidingFriction = Def->SlidingFriction;

	// Where the wheels sit, the physics box and the centre of mass.
	const FVector Mounts[4] = { { C.FrontX, -C.HalfTrack, -10.f }, { C.FrontX, C.HalfTrack, -10.f }, { C.RearX, -C.HalfTrack, -10.f }, { C.RearX, C.HalfTrack, -10.f } };
	for (int32 i = 0; i < Wheels.Num() && i < 4; ++i)
	{
		Wheels[i].LocalMount = Mounts[i];
		if (Wheels[i].Visual) Wheels[i].Visual->SetRelativeLocation(Mounts[i]);
	}
	if (Body)
	{
		Body->SetBoxExtent(C.Box);
		Body->SetCenterOfMass(C.CoM);
	}
	if (DoorHingeL) DoorHingeL->SetRelativeLocation(DriftCarMesh::DoorHinge(0, CarModel));
	if (DoorHingeR) DoorHingeR->SetRelativeLocation(DriftCarMesh::DoorHinge(1, CarModel));
	RepairCar();
	SetupLooseParts();
	for (int32 i = 0; i < 3 && i < LooseParts.Num(); ++i) LooseParts[i].Mesh->SetRelativeLocationAndRotation(LooseParts[i].Home, FRotator::ZeroRotator);
	BuildVisuals();
	ApplyTuning();
}

void ADriftVehiclePawn::SelectCar(int32 Model)
{
	ApplyCarSpec(Model);
	DriftTuning::Load(this);
	if (GConfig)
	{
		GConfig->SetString(TEXT("Tandem.Car"), TEXT("Model"), *CarModelId(CarModel), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	// Put it back on its wheels where it is (the new body may be bigger).
	if (bOccupied && GetWorld()) TeleportCar(GetActorLocation() + FVector(0.f, 0.f, 60.f), FRotator(0.f, GetActorRotation().Yaw, 0.f));
	MapToastName = Spec(CarModel).Name;
	MapToastAge = 0.f;
}

void ADriftVehiclePawn::UpdateRecorder(float Dt, float Throttle, float Brake, float Steer, bool bHandbrake)
{
	if (!bRecordDrives || !bOccupied || !GetWorld() || !GetWorld()->IsGameWorld() || Wheels.Num() != 4) return;
	if (RecPath.IsEmpty())
	{
		const FString Dir = FPaths::Combine(FPlatformProcess::UserDir(), TEXT("Tandem"), TEXT("Drives"));
		IFileManager& FM = IFileManager::Get();
		FM.MakeDirectory(*Dir, true);
		// Keep the nine newest old files (names sort by time), plus this one.
		TArray<FString> Old;
		FM.FindFiles(Old, *FPaths::Combine(Dir, TEXT("drive_*.csv")), true, false);
		Old.Sort();
		for (int32 i = 0; i < Old.Num() - 9; ++i) FM.Delete(*FPaths::Combine(Dir, Old[i]));
		RecPath = FPaths::Combine(Dir, FString::Printf(TEXT("drive_%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
		RecBuffer = TEXT("t,car,area,speed_kmh,slide_deg,yaw_dps,steer_key,front_wheels_deg,throttle,brake,handbrake,gear,rpm,")
			TEXT("g_long,g_lat,rear_wheel_kmh,rl_slip,rr_slip,fl_angle,fr_angle,rl_angle,rr_angle,fl_load,fr_load,rl_load,rr_load,x_m,y_m,z_m\n");
	}
	RecTime += Dt;
	RecSampleTimer -= Dt;
	RecFlushTimer += Dt;
	if (RecSampleTimer <= 0.f)
	{
		RecSampleTimer = 0.05f;
		const FVector Pos = GetActorLocation() * 0.01f;
		const float RearKmh = 0.5f * (Wheels[2].Omega + Wheels[3].Omega) * WheelRadius * 0.01f * 3.6f;
		RecBuffer += FString::Printf(TEXT("%.2f,%s,%d,%.1f,%.1f,%.1f,%.2f,%.1f,%.2f,%.2f,%d,%d,%.0f,%.2f,%.2f,%.1f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%.0f,%.1f,%.1f,%.1f\n"),
			RecTime, *CarModelId(CarModel), MapIndex, GetSpeedKmh(), BodySlipDeg, Body->GetPhysicsAngularVelocityInDegrees().Z,
			Steer, SteerAngleDeg, Throttle, Brake, bHandbrake ? 1 : 0, Gear, RPM, GLong, GLat, RearKmh,
			Wheels[2].SlipRatio, Wheels[3].SlipRatio, Wheels[0].SlipAngleDeg, Wheels[1].SlipAngleDeg, Wheels[2].SlipAngleDeg, Wheels[3].SlipAngleDeg,
			Wheels[0].Load, Wheels[1].Load, Wheels[2].Load, Wheels[3].Load, Pos.X, Pos.Y, Pos.Z);
	}
	if (RecFlushTimer >= 2.f && !RecBuffer.IsEmpty())
	{
		RecFlushTimer = 0.f;
		FFileHelper::SaveStringToFile(RecBuffer, *RecPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		RecBuffer.Reset();
	}
}

void ADriftVehiclePawn::UpdateCheckpoint(float Dt)
{
	// Remembers the last checkpoint the car went past while on the road, facing the way it was going.
	CheckpointReturnAge += Dt;
	CheckpointScan -= Dt;
	if (CheckpointScan > 0.f || !bOccupied || GetSpeedKmh() < 15.f) return;
	CheckpointScan = 0.2f;
	const ADriftGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADriftGameModeBase>() : nullptr;
	FTransform T;
	if (!GM || !GM->GetNearestCheckpoint(GetActorLocation(), 900.f, T)) return;
	if (FVector::DotProduct(GetVelocity(), T.GetRotation().GetForwardVector()) < 0.f)
	{
		T.SetRotation(FRotator(0.f, T.Rotator().Yaw + 180.f, 0.f).Quaternion());
	}
	// Not the one M just put the car on (it would keep sending you back to the same spot).
	if (bHaveCheckpoint && CheckpointReturnAge < 5.f && FVector::Dist2D(T.GetLocation(), LastCheckpoint.GetLocation()) < 100.f) return;
	LastCheckpoint = T;
	bHaveCheckpoint = true;
}

// ---------------- Input ----------------

void ADriftVehiclePawn::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (auto* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				BuildInput();
				Subsystem->ClearAllMappings();
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}
}

static UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* A = NewObject<UInputAction>(Outer, Name);
	A->ValueType = Type;
	return A;
}

void ADriftVehiclePawn::BuildInput()
{
	if (!IA_Throttle)
	{
		IA_Throttle  = MakeAction(this, TEXT("IA_Throttle"),  EInputActionValueType::Axis1D);
		IA_Brake     = MakeAction(this, TEXT("IA_Brake"),     EInputActionValueType::Axis1D);
		IA_PartThrottle = MakeAction(this, TEXT("IA_PartThrottle"), EInputActionValueType::Boolean);
		IA_Steer     = MakeAction(this, TEXT("IA_Steer"),     EInputActionValueType::Axis1D);
		IA_Handbrake = MakeAction(this, TEXT("IA_Handbrake"), EInputActionValueType::Boolean);
		IA_GearDown  = MakeAction(this, TEXT("IA_GearDown"),  EInputActionValueType::Boolean);
		IA_GearUp    = MakeAction(this, TEXT("IA_GearUp"),    EInputActionValueType::Boolean);
		IA_Wipers    = MakeAction(this, TEXT("IA_Wipers"),    EInputActionValueType::Boolean);
		IA_Camera    = MakeAction(this, TEXT("IA_Camera"),    EInputActionValueType::Boolean);
		IA_Reset     = MakeAction(this, TEXT("IA_Reset"),     EInputActionValueType::Boolean);
		IA_Pause     = MakeAction(this, TEXT("IA_Pause"),     EInputActionValueType::Boolean);
		IA_Pause->bTriggerWhenPaused = true;
		IA_Quit      = MakeAction(this, TEXT("IA_Quit"),      EInputActionValueType::Boolean);
		IA_Quit->bTriggerWhenPaused = true;
		IA_Clutch    = MakeAction(this, TEXT("IA_Clutch"),    EInputActionValueType::Boolean);
		IA_Exit      = MakeAction(this, TEXT("IA_Exit"),      EInputActionValueType::Boolean);
		IA_Forces    = MakeAction(this, TEXT("IA_Forces"),    EInputActionValueType::Boolean);
		IA_Map       = MakeAction(this, TEXT("IA_Map"),       EInputActionValueType::Boolean);
	}
	if (!MappingContext) BuildMappings();
}

void ADriftVehiclePawn::BuildMappings()
{
	// Keyboard keys come from the player's bindings (menu > Controls); the gamepad layout is fixed.
	// Esc (menu) is handled by the player controller, not here.
	MappingContext = NewObject<UInputMappingContext>(this);
	auto Key = [](const TCHAR* Id) { return DriftKeys::Get(FName(Id)); };
	MappingContext->MapKey(IA_Throttle, Key(TEXT("Throttle")));
	MappingContext->MapKey(IA_Brake, Key(TEXT("Brake")));
	MappingContext->MapKey(IA_PartThrottle, Key(TEXT("PartThrottle")));
	MappingContext->MapKey(IA_Steer, Key(TEXT("SteerRight")));
	FEnhancedActionKeyMapping& Left = MappingContext->MapKey(IA_Steer, Key(TEXT("SteerLeft")));
	Left.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	MappingContext->MapKey(IA_Handbrake, Key(TEXT("Handbrake")));
	MappingContext->MapKey(IA_GearDown, Key(TEXT("GearDown")));
	MappingContext->MapKey(IA_GearUp, Key(TEXT("GearUp")));
	MappingContext->MapKey(IA_Clutch, Key(TEXT("Clutch")));
	MappingContext->MapKey(IA_Camera, Key(TEXT("Camera")));
	MappingContext->MapKey(IA_Wipers, Key(TEXT("Wipers")));
	MappingContext->MapKey(IA_Forces, Key(TEXT("Forces")));
	MappingContext->MapKey(IA_Exit, Key(TEXT("Exit")));
	MappingContext->MapKey(IA_Map, Key(TEXT("Map")));
	MappingContext->MapKey(IA_Reset, Key(TEXT("Reset")));

	MappingContext->MapKey(IA_Map, EKeys::Gamepad_Special_Left);
	MappingContext->MapKey(IA_Throttle, EKeys::Gamepad_RightTriggerAxis);
	MappingContext->MapKey(IA_Brake, EKeys::Gamepad_LeftTriggerAxis);
	MappingContext->MapKey(IA_Steer, EKeys::Gamepad_LeftX);
	MappingContext->MapKey(IA_Handbrake, EKeys::Gamepad_FaceButton_Bottom);
	MappingContext->MapKey(IA_GearDown, EKeys::Gamepad_LeftShoulder);
	MappingContext->MapKey(IA_GearUp, EKeys::Gamepad_RightShoulder);
	MappingContext->MapKey(IA_Wipers, EKeys::Gamepad_FaceButton_Left);
	MappingContext->MapKey(IA_Camera, EKeys::Gamepad_FaceButton_Top);
	MappingContext->MapKey(IA_Exit, EKeys::Gamepad_FaceButton_Right);
}

void ADriftVehiclePawn::ApplyKeyBindings()
{
	if (!IA_Throttle) return; // input not set up yet: BuildInput reads the bindings when it is
	BuildMappings();
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return; // on foot: the new context goes in when you get back in
	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (auto* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

void ADriftVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	if (auto* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(IA_Throttle,  ETriggerEvent::Triggered, this, &ADriftVehiclePawn::OnThrottle);
		EIC->BindAction(IA_Throttle,  ETriggerEvent::Completed, this, &ADriftVehiclePawn::OnThrottle);
		EIC->BindAction(IA_PartThrottle, ETriggerEvent::Started, this, &ADriftVehiclePawn::OnPartThrottleStart);
		EIC->BindAction(IA_PartThrottle, ETriggerEvent::Completed, this, &ADriftVehiclePawn::OnPartThrottleEnd);
		EIC->BindAction(IA_Brake,     ETriggerEvent::Triggered, this, &ADriftVehiclePawn::OnBrake);
		EIC->BindAction(IA_Brake,     ETriggerEvent::Completed, this, &ADriftVehiclePawn::OnBrake);
		EIC->BindAction(IA_Steer,     ETriggerEvent::Triggered, this, &ADriftVehiclePawn::OnSteer);
		EIC->BindAction(IA_Steer,     ETriggerEvent::Completed, this, &ADriftVehiclePawn::OnSteer);
		EIC->BindAction(IA_Handbrake, ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnHandbrakeStart);
		EIC->BindAction(IA_Handbrake, ETriggerEvent::Completed, this, &ADriftVehiclePawn::OnHandbrakeEnd);
		EIC->BindAction(IA_GearDown,  ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnGearDown);
		EIC->BindAction(IA_GearUp,    ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnGearUp);
		EIC->BindAction(IA_Wipers,    ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnWipers);
		EIC->BindAction(IA_Camera,    ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnCameraCycle);
		EIC->BindAction(IA_Reset,     ETriggerEvent::Started,   this, &ADriftVehiclePawn::ResetCar);
		EIC->BindAction(IA_Pause,     ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnPause);
		EIC->BindAction(IA_Quit,      ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnQuit);
		EIC->BindAction(IA_Clutch,    ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnClutchKick);
		EIC->BindAction(IA_Exit,      ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnExitCar);
		EIC->BindAction(IA_Forces,    ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnToggleForces);
		EIC->BindAction(IA_Map,       ETriggerEvent::Started,   this, &ADriftVehiclePawn::OnSwitchMap);
	}
}

void ADriftVehiclePawn::OnThrottle(const FInputActionValue& V) { ThrottleIn = FMath::Clamp(V.Get<float>(), 0.f, 1.f); }
void ADriftVehiclePawn::OnPartThrottleStart() { bPartThrottleIn = true; }
void ADriftVehiclePawn::OnPartThrottleEnd()   { bPartThrottleIn = false; }
void ADriftVehiclePawn::OnBrake(const FInputActionValue& V)    { BrakeIn = FMath::Clamp(V.Get<float>(), 0.f, 1.f); }
void ADriftVehiclePawn::OnSteer(const FInputActionValue& V)    { SteerIn = FMath::Clamp(V.Get<float>(), -1.f, 1.f); }
void ADriftVehiclePawn::OnHandbrakeStart(const FInputActionValue& V) { bHandbrakeIn = true; }
void ADriftVehiclePawn::OnHandbrakeEnd(const FInputActionValue& V)   { bHandbrakeIn = false; }
void ADriftVehiclePawn::OnGearDown()
{
	// Reverse has no synchro: it only goes in when the car is (nearly) stopped, like the real T50.
	if (Gear == 0 && GetSpeedKmh() > 5.f) return;
	ShiftTo(FMath::Max(Gear - 1, -1));
}
void ADriftVehiclePawn::OnGearUp()   { ShiftTo(FMath::Min(Gear + 1, GearRatios.Num())); }

void ADriftVehiclePawn::ShiftTo(int32 NewGear)
{
	if (NewGear == Gear) return;
	// Downshifts dump the clutch (no heel-toe), everything else lets it bite gently.
	bLastShiftDown = NewGear > 0 && Gear > 0 && NewGear < Gear;
	Gear = NewGear;
	ShiftTimer = 0.4f;            // hand on the lever
	ShiftClutchTimer = ShiftClutchTime;
	ClutchEngage = 0.f;
}
void ADriftVehiclePawn::OnToggleForces() { bShowForces = !bShowForces; }

void ADriftVehiclePawn::OnWipers()
{
	bWipersOn = !bWipersOn;
	OnWipersToggled(bWipersOn);
}

void ADriftVehiclePawn::OnCameraCycle()
{
	++CameraIndex;
	ApplyCameraMode();
}

void ADriftVehiclePawn::OnClutchKick()
{
	if (Gear > 0 && !bInputLocked) ClutchKickTimer = ClutchKickTime;
}

void ADriftVehiclePawn::OnQuit()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->IsPaused())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}


void ADriftVehiclePawn::OnPause()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetPause(!PC->IsPaused());
	}
}

// ---------------- Simulation ----------------

void ADriftVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Clamp(DeltaSeconds, 0.001f, 1.f / 30.f);

	float ThrottleTarget = ThrottleIn * (bPartThrottleIn ? PartThrottle : 1.f), Brake = BrakeIn, Steer = SteerIn;
	bool bHandbrake = bHandbrakeIn;
	// Locked (countdown / finished) or nobody inside: parked on the handbrake.
	if (bInputLocked || !bOccupied) { ThrottleTarget = 0.f; Brake = 0.f; Steer = 0.f; bHandbrake = true; }

	const FVector Fwd = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	const FVector Vel = Body->GetPhysicsLinearVelocity();
	const float FwdSpeed = FVector::DotProduct(Vel, Fwd); // cm/s
	const float LatSpeed = FVector::DotProduct(Vel, Right);
	const float SpeedKmh = Vel.Size() * 0.036f;
	BodySlipDeg = SpeedKmh > 10.f ? FMath::RadiansToDegrees(FMath::Atan2(LatSpeed, FMath::Abs(FwdSpeed))) : 0.f;

	// Measured accelerations (what the driver feels), smoothed for the read-out.
	const FVector Accel = (Vel - PrevVelocity) / Dt;
	PrevVelocity = Vel;
	GLong = FMath::FInterpTo(GLong, FVector::DotProduct(Accel, Fwd) / 981.f, Dt, 6.f);
	GLat = FMath::FInterpTo(GLat, FVector::DotProduct(Accel, Right) / 981.f, Dt, 6.f);

	// No auto-reverse: shift down through N into R (Q), then W drives you backwards and S brakes.

	// Keyboard throttle is on/off; feed it in over ThrottleRiseTime like a foot would.
	const float Rise = ThrottleRiseTime > KINDA_SMALL_NUMBER ? 1.f / ThrottleRiseTime : 100.f;
	ThrottleSmooth = FMath::FInterpConstantTo(ThrottleSmooth, ThrottleTarget, Dt, ThrottleTarget > ThrottleSmooth ? Rise : 14.f);

	// After a drift has ended with the countersteer key still down, that key does nothing until it is
	// let go (otherwise it would throw the car straight into a slide the other way).
	if (bHoldSpent && (FMath::Abs(Steer) < 0.1f || Steer * HoldSide < 0.f)) bHoldSpent = false;
	if (bHoldSpent) Steer = 0.f;

	// Steering: keyboard ramp, a little slower once the car is drifting (body slip past 8 deg).
	const bool bReturning = FMath::Abs(Steer) < FMath::Abs(SteerCurrent) || Steer * SteerCurrent < 0.f;
	const float SlipAbs = FMath::Abs(BodySlipDeg);
	const float DriftBlend = FMath::Clamp((SlipAbs - 8.f) / 7.f, 0.f, 1.f);
	const float RampTime = bReturning ? SteerReturnTime : FMath::Lerp(SteerRampTime, SteerRampTimeFast, DriftBlend);
	const float Rate = RampTime > KINDA_SMALL_NUMBER ? 1.f / RampTime : 1000.f;
	SteerCurrent = FMath::FInterpConstantTo(SteerCurrent, Steer, Dt, Rate);

	// Caster: left alone, the front wheels swing round to follow the way the front axle is travelling
	// (self-aligning torque). In a slide that is the countersteer; your input steers relative to it.
	float AlignDeg = 0.f;
	if (FwdSpeed > 200.f)
	{
		const FVector FrontVel = Body->GetPhysicsLinearVelocityAtPoint(GetActorTransform().TransformPosition(FVector(Wheels[0].LocalMount.X, 0.f, 0.f)));
		AlignDeg = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(FrontVel, Right), FMath::Abs(FVector::DotProduct(FrontVel, Fwd))));
	}
	// How far a full key turns the wheels. Gripping, at speed: about the angle that gives the front tyres
	// their most cornering grip (more lock than that only scrubs them). Slow: full lock.
	float SteerLimit = MaxSteerDeg;
	const float SpeedMS = SpeedKmh / 3.6f;
	if (SpeedMS > 5.f)
	{
		const float Wheelbase = FMath::Max(1.f, (Wheels[0].LocalMount.X - Wheels[2].LocalMount.X) * 0.01f);
		const float GripAngle = FMath::RadiansToDegrees(FMath::Atan(Wheelbase * TyreGrip * Gravity / (SpeedMS * SpeedMS)));
		SteerLimit = FMath::Clamp(GripAngle + PeakSlipAngleDeg, PeakSlipAngleDeg, MaxSteerDeg);
	}
	// Self-aligning torque grows with speed: at walking pace the wheels stay where you put them (otherwise
	// a turned wheel follows its own travel direction and winds itself out to full lock). Full by 30 km/h.
	const float CasterK = FMath::SmoothStep(10.f, 30.f, SpeedKmh);
	const float Caster = CasterAlign * CasterK * AlignDeg;
	// Drifting, the key steers the front tyres relative to where they travel: into the corner tightens the
	// line, towards the slide widens it and takes angle off. Either way only up to DriftSteerRangeDeg past
	// the direction of travel, so steering never scrubs the speed off or yanks the car straight.
	const float GripTarget = Caster + SteerCurrent * SteerLimit;
	const float DriftTarget = Caster + SteerCurrent * DriftSteerRangeDeg;
	float TargetSteerDeg = FMath::Clamp(FMath::Lerp(GripTarget, DriftTarget, DriftBlend), -MaxSteerDeg, MaxSteerDeg);

	// Snap-back damping (bDriftAngleHold): the only help left. It does not pick or keep an angle. While a
	// drift's angle is FALLING, the driver's hands leave the front wheels a little short of the direction
	// of travel, in proportion to how fast it falls, so the car unwinds instead of snapping straight and
	// swinging the other way. Holding the angle is the driver's job: throttle for wheelspin, steering for
	// the line. Nothing happens while the angle is steady or growing, or on the handbrake.
	const float SlideSign = BodySlipDeg >= 0.f ? 1.f : -1.f;
	const bool bCanHold = bDriftAngleHold && CasterK > 0.5f && FwdSpeed > 200.f && !bHandbrake && !bInputLocked && bOccupied;
	if (!bHoldOn && !bHoldSpent && bCanHold && SlipAbs > 10.f)
	{
		bHoldOn = true;
		HoldSide = SlideSign;
		HoldPrevSlip = SlipAbs;
		HoldRate = 0.f;
	}
	if (bHoldOn)
	{
		const float Slip = BodySlipDeg * HoldSide;      // slide angle on the drift's side
		const float Counter = SteerCurrent * HoldSide;  // > 0 countersteer, < 0 into the corner
		HoldRate = FMath::FInterpTo(HoldRate, (Slip - HoldPrevSlip) / Dt, Dt, 12.f);
		HoldPrevSlip = Slip;
		if (!bCanHold) bHoldOn = false;
		else if (Slip < 3.f) { bHoldOn = false; bHoldSpent = Counter > 0.1f; } // the drift is over
		else
		{
			const float Damp = FMath::Clamp(-SnapDamping * HoldRate, 0.f, SnapDampMaxDeg) * FMath::SmoothStep(3.f, 10.f, Slip);
			TargetSteerDeg = FMath::Clamp(TargetSteerDeg - HoldSide * Damp, -MaxSteerDeg, MaxSteerDeg);
		}
	}
	// The rack: the front wheels turn at a finite speed (hands, caster and the rack's own inertia), so
	// they never jump to a new angle.
	const float Lag = FMath::Max(SteerLagTime, 0.001f);
	const float Want = (TargetSteerDeg - SteerAngleDeg) * FMath::Min(1.f, Dt / Lag);
	const float MaxStep = FMath::Max(SteerRateDegPerSec, 1.f) * Dt;
	SteerAngleDeg = FMath::Clamp(SteerAngleDeg + FMath::Clamp(Want, -MaxStep, MaxStep), -MaxSteerDeg, MaxSteerDeg);

	UpdateCheckpoint(DeltaSeconds);
	UpdateRecorder(Dt, ThrottleSmooth, Brake, SteerCurrent, bHandbrake);
	DriftStats::Update(Dt, bOccupied && !bInputLocked, SpeedKmh, BodySlipDeg, RPM, MaxRPM, Gear);

	SimulateWheels(Dt, ThrottleSmooth, Brake, bHandbrake);
	ClutchKickTimer = FMath::Max(0.f, ClutchKickTimer - Dt);
	ShiftClutchTimer = FMath::Max(0.f, ShiftClutchTimer - Dt);

	UpdateCockpit(Dt);
	UpdateDoors(Dt);
	UpdateDamage(Dt);
	UpdateSmoke(Dt);
	UpdateSpeedFeel(Dt);
	UpdateAudio(Dt);
	MapToastAge += DeltaSeconds;
	CameraSwitchAge += DeltaSeconds;
	ExitBlockedAge += DeltaSeconds;
#if ENABLE_DRAW_DEBUG
	if (bShowForces) DrawForces();
#endif

	if (bWipersOn || WiperPhase > 0.f)
	{
		WiperPhase += Dt * 2.4f;
		if (!bWipersOn && FMath::Fmod(WiperPhase, 2.f * PI) < Dt * 2.4f) WiperPhase = 0.f;
		const float Angle = (1.f - FMath::Cos(WiperPhase)) * 0.5f * 95.f;
		WiperPivot->SetRelativeRotation(WiperTilt * FQuat(FRotator(0.f, -Angle, 0.f))); // sweep in the glass plane
	}
}

void ADriftVehiclePawn::SimulateWheels(float Dt, float Throttle, float Brake, bool bHandbrake)
{
	UWorld* World = GetWorld();
	if (!World || !Body->IsSimulatingPhysics() || Wheels.Num() != 4) return;

	const FTransform BodyTM = Body->GetComponentTransform();
	const FVector Up = BodyTM.GetUnitAxis(EAxis::Z);
	const float RayLen = SuspensionRest + WheelRadius;
	const float R = WheelRadius * 0.01f; // m
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DriftWheel), false, this);

	// ---- 1) Normal forces: each corner's spring + damper pushes the body up; the road pushes the tyre ----
	struct FContact { FVector TyreFwd, TyreRight; float VLong = 0.f, VLat = 0.f; };
	FContact Ct[4];
	for (int32 i = 0; i < 4; ++i)
	{
		FDriftWheel& W = Wheels[i];
		const FVector Start = BodyTM.TransformPosition(W.LocalMount);
		FHitResult Hit;
		W.bGrounded = World->LineTraceSingleByChannel(Hit, Start, Start - Up * RayLen, ECC_Visibility, Params);

		float WheelDrop = SuspensionRest;
		if (W.bGrounded)
		{
			const float NewCompression = RayLen - Hit.Distance;
			const float CompressionVel = (NewCompression - W.Compression) / Dt;
			W.Compression = NewCompression;
			WheelDrop = Hit.Distance - WheelRadius;
			const float SpringUU = FMath::Max(0.f, (W.bFront ? SpringRate : SpringRateRear) * W.Compression + DamperRate * CompressionVel);
			Body->AddForceAtLocation(Up * SpringUU, Start);
			W.Load = SpringUU / NewtonToUU;
			W.ContactPoint = Hit.ImpactPoint;
			W.ContactNormal = Hit.ImpactNormal;

			const FQuat SteerQ(Up, FMath::DegreesToRadians(W.bFront ? SteerAngleDeg : 0.f));
			Ct[i].TyreFwd = FVector::VectorPlaneProject(SteerQ.RotateVector(BodyTM.GetUnitAxis(EAxis::X)), Hit.ImpactNormal).GetSafeNormal();
			Ct[i].TyreRight = FVector::CrossProduct(Hit.ImpactNormal, Ct[i].TyreFwd).GetSafeNormal();
			const FVector PointVel = FVector::VectorPlaneProject(Body->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint), Hit.ImpactNormal) * 0.01f;
			Ct[i].VLong = FVector::DotProduct(PointVel, Ct[i].TyreFwd);
			Ct[i].VLat = FVector::DotProduct(PointVel, Ct[i].TyreRight);
		}
		else
		{
			W.Compression = 0.f;
			W.Load = 0.f;
		}
		if (W.Visual) W.Visual->SetRelativeLocation(W.LocalMount - FVector(0, 0, WheelDrop));
	}

	// ---- 2) Engine, clutch, gearbox: the engine is its own spinning mass, joined to the rear axle by a
	//         friction clutch that can carry at most ClutchMaxNm. Revs the engine stores get dumped into
	//         the tyres when the clutch bites (clutch kick); a downshift without a blip drags them (shift-lock).
	const float Ratio = CurrentRatio(); // signed: reverse is negative; includes the final drive
	// A blown engine is seized and out of gear: the car only coasts.
	const bool bClutchDown = Gear == 0 || bHandbrake || ClutchKickTimer > 0.f || ShiftClutchTimer > 0.f || bEngineBlown;
	if (bClutchDown)
	{
		// Kick and handbrake: the pedal is snapped back up, so the clutch bites at once on release.
		ClutchEngage = (ClutchKickTimer > 0.f || bHandbrake) ? 1.f : 0.f;
	}
	else
	{
		const float EngageTime = bLastShiftDown ? ClutchDumpTime : ClutchEngageTime;
		ClutchEngage = EngageTime > KINDA_SMALL_NUMBER ? FMath::Min(1.f, ClutchEngage + Dt / EngageTime) : 1.f;
	}
	const float ShiftLift = ShiftClutchTimer > 0.f ? 0.f : 1.f; // off the throttle while the lever moves
	const float IdleOmega = IdleRPM * 2.f * PI / 60.f;
	const float SlipRpm = IdleRPM + Throttle * (LaunchRPM - IdleRPM);
	const float SlipOmega = SlipRpm * 2.f * PI / 60.f;
	const float MaxOmega = (MaxRPM + 1500.f) * 2.f * PI / 60.f;
	const float Ie = FMath::Max(EngineInertia, 0.02f);

	// ---- 3) Brakes as torques on the wheels (no ABS: a wheel locks if the torque beats the tyre) ----
	const float BrakeFrontNm = Brake * BrakeForce * BrakeBias * 0.5f * R;
	const float BrakeRearNm = Brake * BrakeForce * (1.f - BrakeBias) * 0.5f * R + (bHandbrake ? HandbrakeTorque : 0.f);

	// ---- 4) Tyres: combined slip on a friction ellipse ----
	// Curve sin(C * atan(B * s)) peaks at s = 1 and settles at SlidingFriction for large s.
	const float MFC = 2.f - 2.f * FMath::Asin(FMath::Clamp(SlidingFriction, 0.05f, 0.99f)) / PI;
	const float MFB = FMath::Tan(PI / (2.f * MFC));
	const float TanPeak = FMath::Tan(FMath::DegreesToRadians(PeakSlipAngleDeg));
	const float FalloffSpeed = FMath::Max(SlideFalloffSpeed, 0.1f);
	const float NominalLoad = MassKg * Gravity * 0.25f;
	auto TyreForce = [&](float Grip, float Kappa, float TanAlpha, float N, float SlideSpeed, float& OutFx, float& OutFy)
	{
		const float Sx = Kappa / PeakSlipRatio;
		const float Sy = TanAlpha / TanPeak;
		const float Sigma = FMath::Max(FMath::Sqrt(Sx * Sx + Sy * Sy), 1.e-4f);
		// Load sensitivity: friction coefficient falls as the load on the tyre rises.
		const float Mu = Grip * FMath::Clamp(1.f - LoadSensitivity * (N / NominalLoad - 1.f), 0.6f, 1.2f);
		float Shape = FMath::Sin(MFC * FMath::Atan(MFB * Sigma));
		if (Sigma > 1.f)
		{
			// Past the peak the patch slides. Rubber sliding slowly keeps nearly all its grip; the drop
			// to SlidingFriction comes in as the sliding speed rises.
			const float SlidingPart = (Sigma - 1.f) / Sigma * SlideSpeed;
			const float Drop = 1.f - FMath::Exp(-SlidingPart / FalloffSpeed);
			Shape = 1.f - (1.f - Shape) * Drop;
		}
		const float F = Mu * N * Shape;
		// Direction: below the peak along the normalised slip; once sliding, against the real sliding
		// velocity of the patch (friction of a sliding body opposes its motion).
		float DX = Sx / Sigma, DY = Sy / Sigma;
		const float SlideBlend = FMath::Clamp((Sigma - 1.f) * 0.5f, 0.f, 1.f);
		if (SlideBlend > 0.f)
		{
			const float VN = FMath::Max(FMath::Sqrt(Kappa * Kappa + TanAlpha * TanAlpha), 1.e-6f);
			DX = FMath::Lerp(DX, Kappa / VN, SlideBlend);
			DY = FMath::Lerp(DY, TanAlpha / VN, SlideBlend);
			const float DN = FMath::Max(FMath::Sqrt(DX * DX + DY * DY), 1.e-6f);
			DX /= DN;
			DY /= DN;
		}
		OutFx = F * DX;
		OutFy = -F * DY; // opposes the sideways slide
	};

	// ---- 5) Wheel spin, integrated in small steps (tyre forces are stiff) ----
	constexpr int32 SubSteps = 10;
	const float H = Dt / SubSteps;
	float SumFx[4] = { 0.f, 0.f, 0.f, 0.f }, SumFy[4] = { 0.f, 0.f, 0.f, 0.f };
	float EngineNmLast = 0.f;
	bool bSlipNow = false;
	for (int32 Step = 0; Step < SubSteps; ++Step)
	{
		// Engine torque at the crank: curve x throttle, or pumping losses off throttle.
		const float EngRpm = EngineOmega * 60.f / (2.f * PI);
		float EngineNm = 0.f;
		if (Throttle * ShiftLift > 0.02f) EngineNm = EngRpm < MaxRPM ? EngineTorqueAt(EngRpm) * Throttle : 0.f; // limiter
		else EngineNm = -EngineBrakeNm * (EngRpm / MaxRPM);
		if (bEngineBlown) EngineNm = -60.f * FMath::Min(1.f, EngineOmega / 20.f); // no power; it grinds to a stop
		else if (EngineOmega < IdleOmega) EngineNm = FMath::Max(EngineNm, 40.f * (IdleOmega - EngineOmega) / 10.47f); // idle control

		// Clutch: the torque that would make engine and gearbox turn together this step, up to what it can hold.
		float ClutchNm = 0.f;
		bSlipNow = false;
		if (!bClutchDown && Ratio != 0.f)
		{
			const float GearboxOmega = 0.5f * (Wheels[2].Omega + Wheels[3].Omega) * Ratio; // crank-side speed
			const float WheelSideInertia = 2.f * (WheelInertia + H * RearTyreStiffness) / (Ratio * Ratio);
			const float LockNm = (EngineOmega + H * EngineNm / Ie - GearboxOmega) / (H * (1.f / Ie + 1.f / WheelSideInertia));
			float Capacity = ClutchMaxNm * ClutchEngage;
			if (FMath::Abs(GearboxOmega) < SlipOmega)
			{
				// Pulling away / crawling: the driver slips the clutch to hold the engine near SlipRpm (no stall).
				// The clutch starts biting as the revs come up, instead of waiting for SlipRpm first.
				const float Bite = FMath::Clamp((EngRpm - IdleRPM) / FMath::Max(SlipRpm - IdleRPM, 1.f), 0.f, 1.f);
				const float Feather = EngineTorqueAt(SlipRpm) * Throttle * Bite + Ie * FMath::Max(0.f, EngineOmega - SlipOmega) / 0.05f;
				Capacity = FMath::Min(Capacity, FMath::Clamp(Feather, 0.f, ClutchMaxNm));
			}
			ClutchNm = FMath::Clamp(LockNm, -Capacity, Capacity);
			bSlipNow = FMath::Abs(LockNm) > Capacity + 1.f;
		}
		EngineOmega = FMath::Clamp(EngineOmega + H * (EngineNm - ClutchNm) / Ie, 0.f, MaxOmega);
		EngineNmLast = EngineNm;
		const float AxleNm = ClutchNm * Ratio * DrivetrainEfficiency;

		// Limited-slip diff: locks the rears together up to LSDMaxLockNm.
		const float Lock = FMath::Clamp(LSDStiffness * (Wheels[2].Omega - Wheels[3].Omega), -LSDMaxLockNm, LSDMaxLockNm);
		const float DriveNm[4] = { 0.f, 0.f, AxleNm * 0.5f - Lock, AxleNm * 0.5f + Lock };
		float RearStiffSum = 0.f;
		for (int32 i = 0; i < 4; ++i)
		{
			FDriftWheel& W = Wheels[i];
			const float Iw = WheelInertia;
			const float BrakeNm = (i < 2 ? BrakeFrontNm : BrakeRearNm) + RollingResistance * W.Load * R;
			if (!W.bGrounded)
			{
				W.Omega = MoveTowardsZero(W.Omega + DriveNm[i] / Iw * H, BrakeNm / Iw * H);
				continue;
			}
			const float Vref = FMath::Max(FMath::Abs(Ct[i].VLong), 1.5f);
			const float TanA = Ct[i].VLat / Vref;
			float Fx0, Fy0, Fx1, Fy1;
			const float Grip = i < 2 ? TyreGrip : TyreGripRear;
			const float SlideV = FMath::Sqrt(FMath::Square(W.Omega * R - Ct[i].VLong) + FMath::Square(Ct[i].VLat));
			TyreForce(Grip, (W.Omega * R - Ct[i].VLong) / Vref, TanA, W.Load, SlideV, Fx0, Fy0);
			TyreForce(Grip, ((W.Omega + 0.5f) * R - Ct[i].VLong) / Vref, TanA, W.Load, SlideV, Fx1, Fy1);
			// Semi-implicit: include how the tyre's reaction torque changes with spin, so it stays stable.
			const float Stiff = FMath::Max(0.f, (Fx1 - Fx0) / 0.5f) * R;
			if (i >= 2) RearStiffSum += Stiff;
			const float Denom = Iw + H * Stiff;
			const float Free = W.Omega + H * (DriveNm[i] - Fx0 * R) / Denom;
			W.Omega = MoveTowardsZero(Free, H * BrakeNm / Denom);

			const float Kappa = (W.Omega * R - Ct[i].VLong) / Vref;
			float Fx, Fy;
			TyreForce(Grip, Kappa, TanA, W.Load, FMath::Sqrt(FMath::Square(W.Omega * R - Ct[i].VLong) + FMath::Square(Ct[i].VLat)), Fx, Fy);
			SumFx[i] += Fx;
			SumFy[i] += Fy;
		}
		RearTyreStiffness = 0.5f * RearStiffSum;
	}
	RPM = FMath::Min(EngineOmega * 60.f / (2.f * PI), MaxRPM + 400.f); // needle stops just past the redline
	EngineTorqueNow = EngineNmLast;
	bClutchSlipping = bSlipNow;

	// ---- 6) Apply the tyre forces at the contact patches ----
	ResultantForceN = FVector::ZeroVector;
	for (int32 i = 0; i < 4; ++i)
	{
		FDriftWheel& W = Wheels[i];
		if (!W.bGrounded)
		{
			W.Fx = W.Fy = 0.f;
			W.SlideSpeed = 0.f;
			W.TyreForceWorld = FVector::ZeroVector;
			continue;
		}
		W.SlideSpeed = FMath::Sqrt(FMath::Square(W.Omega * R - Ct[i].VLong) + FMath::Square(Ct[i].VLat));
		const float Vref = FMath::Max(FMath::Abs(Ct[i].VLong), 1.5f);
		float Fx = SumFx[i] / SubSteps, Fy = SumFy[i] / SubSteps;
		// One frame's push can't carry the contact patch past zero slip (keeps low speed calm).
		const float Mq = FMath::Max(W.Load / Gravity, 40.f);
		const float LatCap = FMath::Abs(Ct[i].VLat) * Mq / Dt;
		const float LongCap = FMath::Abs(W.Omega * R - Ct[i].VLong) * Mq / Dt + 50.f;
		Fy = FMath::Clamp(Fy, -LatCap, LatCap);
		Fx = FMath::Clamp(Fx, -LongCap, LongCap);
		W.Fx = Fx;
		W.Fy = Fy;
		W.SlipRatio = (W.Omega * R - Ct[i].VLong) / Vref;
		W.SlipAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Ct[i].VLat, Vref));
		W.TyreForceWorld = Ct[i].TyreFwd * Fx + Ct[i].TyreRight * Fy;
		Body->AddForceAtLocation(W.TyreForceWorld * NewtonToUU, W.ContactPoint);
		ResultantForceN += W.TyreForceWorld;
	}

	// Wheel visuals spin with the simulated wheel speed (a locked wheel stops).
	for (FDriftWheel& W : Wheels)
	{
		// The rim has 8 spokes (repeats every 45 deg). Turned more than ~18 deg a frame it strobes: looks
		// still or even backwards (wagon-wheel effect). Cap the drawn step so a fast wheel reads as spinning.
		const float SpinStep = FMath::Clamp(FMath::RadiansToDegrees(W.Omega) * Dt, -18.f, 18.f);
		W.SpinDeg = FMath::Fmod(W.SpinDeg + SpinStep, 360.f);
		if (W.Visual)
		{
			const FQuat SteerQ = FQuat(FRotator(0.f, W.bFront ? SteerAngleDeg : 0.f, 0.f));
			const FQuat Spin = FQuat(FRotator(-W.SpinDeg, 0.f, 0.f));
			W.Visual->SetRelativeRotation(SteerQ * Spin);
		}
	}

	// ---- 7) Aerodynamic drag: 0.5 * rho * CdA * v^2, against the motion ----
	const FVector Vel = Body->GetPhysicsLinearVelocity();
	const float SpeedMS = Vel.Size() * 0.01f;
	DragForceN = 0.5f * AirDensity * DragCdA * SpeedMS * SpeedMS;
	const FVector Drag = -Vel.GetSafeNormal() * DragForceN;
	Body->AddForce(Drag * NewtonToUU);
	ResultantForceN += Drag;
	// Gravity is applied by the physics engine (m * 9.81 N straight down at the centre of mass).
}

void ADriftVehiclePawn::EmitPuff(const FVector& Pos, const FVector& Vel, float Life, float StartSize, float EndSize)
{
	if (Puffs.Num() != MaxPuffs) return; // pool not built yet
	FDriftSmokePuff& P = Puffs[NextPuff];
	NextPuff = (NextPuff + 1) % MaxPuffs;
	P.bActive = true;
	P.Age = 0.f;
	P.Life = Life;
	P.Pos = Pos;
	P.Vel = Vel;
	P.StartSize = StartSize;
	P.EndSize = EndSize;
}

void ADriftVehiclePawn::UpdateSmoke(float Dt)
{
	if (!TyreSmoke) return;
	if (Puffs.Num() != MaxPuffs)
	{
		Puffs.SetNum(MaxPuffs);
		TyreSmoke->ClearInstances();
		for (int32 i = 0; i < MaxPuffs; ++i)
		{
			TyreSmoke->AddInstance(FTransform(FQuat::Identity, GetActorLocation(), FVector(0.001f)), true);
			TyreSmoke->SetCustomDataValue(i, 0, 0.f, false);
		}
	}

	// (Engine smoke and blast puffs are added by EmitPuff from UpdateDamage, into the same pool.)
	// Emit: each tyre puffs in proportion to how fast it is scrubbing (wheelspin, lock-up or sideways).
	const FVector CarVel = GetVelocity();
	for (int32 w = 0; w < Wheels.Num() && w < 4; ++w)
	{
		const FDriftWheel& W = Wheels[w];
		const float Excess = W.bGrounded ? W.SlideSpeed - SmokeSlideSpeed : 0.f;
		if (Excess <= 0.f) { SmokeCarry[w] = 0.f; continue; }
		SmokeCarry[w] += Dt * FMath::Min(Excess * 7.f, 40.f); // puffs per second
		while (SmokeCarry[w] >= 1.f)
		{
			SmokeCarry[w] -= 1.f;
			FDriftSmokePuff& P = Puffs[NextPuff];
			NextPuff = (NextPuff + 1) % MaxPuffs;
			const float Heavy = FMath::Clamp(Excess / 12.f, 0.f, 1.f);
			P.bActive = true;
			P.Age = 0.f;
			P.Life = FMath::FRandRange(1.6f, 2.6f) * (0.7f + 0.6f * Heavy);
			P.Pos = W.ContactPoint + FVector(0.f, 0.f, 22.f) + FMath::VRand() * 10.f;
			P.Vel = CarVel * 0.2f + FMath::VRand() * 60.f + FVector(0.f, 0.f, 40.f);
			P.StartSize = 40.f;
			P.EndSize = FMath::FRandRange(220.f, 320.f) * (0.6f + 0.6f * Heavy);
		}
	}

	// Simulate and draw: puffs slow down in the air, drift upwards, grow and fade.
	TArray<FTransform> Xf;
	Xf.SetNum(MaxPuffs);
	const float Drag = FMath::Exp(-1.8f * Dt);
	for (int32 i = 0; i < MaxPuffs; ++i)
	{
		FDriftSmokePuff& P = Puffs[i];
		float Alpha = 0.f;
		float Size = 0.1f;
		if (P.bActive)
		{
			P.Age += Dt;
			if (P.Age >= P.Life) P.bActive = false;
			else
			{
				P.Vel = P.Vel * Drag + FVector(0.f, 0.f, 25.f * Dt);
				P.Pos += P.Vel * Dt;
				const float T = P.Age / P.Life;
				Size = FMath::Lerp(P.StartSize, P.EndSize, FMath::Sqrt(T));
				Alpha = 0.45f * FMath::Min(1.f, T * 8.f) * FMath::Square(1.f - T);
			}
		}
		// The engine sphere is 100 cm across.
		Xf[i] = FTransform(FQuat::Identity, P.Pos, FVector(P.bActive ? Size / 100.f : 0.001f));
		TyreSmoke->SetCustomDataValue(i, 0, Alpha, false);
	}
	TyreSmoke->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
}

void ADriftVehiclePawn::DrawForces() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World) return;
	const float Scale = 0.025f; // cm of arrow per newton
	for (const FDriftWheel& W : Wheels)
	{
		if (!W.bGrounded) continue;
		const FVector P = W.ContactPoint + W.ContactNormal * 3.f;
		DrawDebugDirectionalArrow(World, P, P + W.ContactNormal * W.Load * Scale, 12.f, FColor(60, 140, 255), false, -1.f, 0, 2.f);
		DrawDebugDirectionalArrow(World, P, P + W.TyreForceWorld * Scale, 12.f, FColor(255, 60, 40), false, -1.f, 0, 2.f);
	}
	const FVector CoM = Body->GetCenterOfMass();
	DrawDebugDirectionalArrow(World, CoM, CoM - FVector(0, 0, MassKg * Gravity * Scale), 16.f, FColor(190, 80, 255), false, -1.f, 0, 2.f);
	DrawDebugDirectionalArrow(World, CoM, CoM + ResultantForceN * Scale, 16.f, FColor(255, 220, 40), false, -1.f, 0, 3.f);
	DrawDebugDirectionalArrow(World, CoM + FVector(0, 0, 30), CoM + FVector(0, 0, 30) + GetVelocity() * 0.05f, 16.f, FColor(60, 230, 90), false, -1.f, 0, 2.f);
#endif
}

TArray<FString> ADriftVehiclePawn::GetForceReadout() const
{
	TArray<FString> L;
	const FVector Fwd = GetActorForwardVector(), Right = GetActorRightVector();
	const float ResLong = FVector::DotProduct(ResultantForceN, Fwd), ResLat = FVector::DotProduct(ResultantForceN, Right);
	L.Add(TEXT("FORCES  (G to hide)   blue normal  red tyre  purple gravity  yellow resultant  green velocity"));
	L.Add(FString::Printf(TEXT("Speed %.0f km/h   Accel %+.2f g   Lateral %+.2f g   Slide angle %.0f deg   Front wheels %+.0f deg"),
		GetSpeedKmh(), GLong, GLat, BodySlipDeg, SteerAngleDeg));
	L.Add(FString::Printf(TEXT("Gravity %.0f N   Drag %.0f N   Resultant: forward %+.0f N, sideways %+.0f N (centripetal)"),
		MassKg * Gravity, DragForceN, ResLong, ResLat));
	L.Add(FString::Printf(TEXT("Engine %.0f rpm  %+.0f Nm  gear %d  clutch %s"),
		RPM, EngineTorqueNow, Gear, (Gear == 0 || bHandbrakeIn || ClutchKickTimer > 0.f || ShiftClutchTimer > 0.f) ? TEXT("down") : (bClutchSlipping ? TEXT("slipping") : TEXT("locked"))));
	for (int32 i = 0; i < Wheels.Num() && i < 4; ++i)
	{
		const FDriftWheel& W = Wheels[i];
		L.Add(FString::Printf(TEXT("%s  normal %5.0f N   drive/brake %+6.0f N   side %+6.0f N   slip %+4.0f%%   angle %+5.1f deg"),
			WheelNames[i], W.Load, W.Fx, W.Fy, W.SlipRatio * 100.f, W.SlipAngleDeg));
	}
	return L;
}

void ADriftVehiclePawn::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
	FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	// How hard: the speed the hit took off the car (impulse / mass). Landing on the wheels' side doesn't count.
	if (!bDamage || FMath::Abs(HitNormal.Z) > 0.6f) return;
	TakeHit(HitLocation, HitNormal, static_cast<float>(NormalImpulse.Size()) / FMath::Max(MassKg, 1.f) * 0.01f);
}

// ---------------- Crash damage ----------------

FVector ADriftVehiclePawn::EngineLocalPos() const
{
	// AE86: engine in the nose. NSX: behind the seats.
	return CarModel == 1 ? FVector(-105.f, 0.f, 12.f) : FVector(150.f, 0.f, 12.f);
}

void ADriftVehiclePawn::SetupLooseParts()
{
	// Weakest first on each end, so they come off one after another: bumper, then spoiler.
	LooseParts.Reset();
	auto Add = [this](UProceduralMeshComponent* Mesh, USceneComponent* Parent, const FVector& Home, int32 Zone, float Strength)
	{
		FDriftLoosePart& P = LooseParts.AddDefaulted_GetRef();
		P.Mesh = Mesh; P.HomeParent = Parent; P.Home = Home; P.Zone = Zone; P.Strength = Strength;
	};
	Add(PartFrontBumper, Body, DriftCarMesh::LoosePartPivot(CarModel, 0), 0, 7.f);
	Add(PartRearBumper, Body, DriftCarMesh::LoosePartPivot(CarModel, 1), 1, 7.f);
	Add(PartSpoiler, Body, DriftCarMesh::LoosePartPivot(CarModel, 2), 1, 14.f);
	Add(DoorL, DoorHingeL, FVector::ZeroVector, 2, 9.f);
	Add(DoorR, DoorHingeR, FVector::ZeroVector, 3, 9.f);
}

void ADriftVehiclePawn::TakeHit(const FVector& WorldPoint, const FVector& WorldNormal, float HitSpeed)
{
	if (HitCooldown > 0.f || HitSpeed < DamageMinHit || LooseParts.Num() == 0) return;
	HitCooldown = 0.35f; // one crash is one hit, however many contact reports it makes

	// Which end or side was hit.
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	const FVector Ext = Body->GetUnscaledBoxExtent();
	const float FX = static_cast<float>(Local.X / Ext.X), FY = static_cast<float>(Local.Y / Ext.Y);
	const int32 Zone = FMath::Abs(FX) >= FMath::Abs(FY) ? (FX > 0.f ? 0 : 1) : (FY < 0.f ? 2 : 3);

	// Every part on that side takes the knock; at most one comes off per crash (the weakest that has had enough).
	FDriftLoosePart* Off = nullptr;
	for (FDriftLoosePart& P : LooseParts)
	{
		if (P.bGone || P.Zone != Zone) continue;
		P.Damage += HitSpeed;
		if (P.Damage >= P.Strength && (!Off || P.Strength < Off->Strength)) Off = &P;
	}
	if (Off) KnockOff(*Off, WorldNormal * 150.f);

	// The engine: worn down by knocks on its end of the car, blown by one hard one.
	const int32 EngineZone = CarModel == 1 ? 1 : 0;
	if (Zone == EngineZone && !bEngineBlown)
	{
		EngineDamage += HitSpeed;
		if (HitSpeed >= EngineBlowHit || EngineDamage >= EngineStrength) BlowEngine();
	}
}

void ADriftVehiclePawn::KnockOff(FDriftLoosePart& Part, const FVector& Push)
{
	if (Part.bGone || !Part.Mesh) return;
	Part.bGone = true;
	Part.bResting = false;
	const FVector Pos = Part.Mesh->GetComponentLocation();
	Part.Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	// It leaves with the car's speed at that point, plus the shove of the crash and a bit of tumble.
	Part.Vel = Body->GetPhysicsLinearVelocityAtPoint(Pos) * 0.7f + Push + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(180.f, 320.f));
	Part.SpinAxis = FMath::VRand();
	Part.SpinDegPerSec = FMath::FRandRange(180.f, 420.f);
	if (!DriftGraphics::SoundOn(DriftGraphics::Crash)) return;
	if (USoundBase* Crunch = LoadObject<USoundBase>(nullptr, ExplosionSoundPath)) UGameplayStatics::PlaySound2D(this, Crunch, 0.3f, 2.2f);
}

void ADriftVehiclePawn::BlowEngine()
{
	if (bEngineBlown) return;
	bEngineBlown = true;
	FireAge = 0.f;
	EngineDamage = EngineStrength;
	const FVector EnginePos = GetActorTransform().TransformPosition(EngineLocalPos());

	// The blast: a kick upwards under the engine, and whatever is still bolted to that end goes flying.
	Body->AddImpulseAtLocation(GetActorUpVector() * MassKg * 220.f, EnginePos);
	const int32 EngineZone = CarModel == 1 ? 1 : 0;
	for (FDriftLoosePart& P : LooseParts)
	{
		if (!P.bGone && P.Zone == EngineZone && P.Mesh) KnockOff(P, (P.Mesh->GetComponentLocation() - EnginePos).GetSafeNormal() * 500.f + FVector(0.f, 0.f, 300.f));
	}
	for (int32 i = 0; i < 45; ++i)
	{
		EmitPuff(EnginePos + FMath::VRand() * 40.f, GetVelocity() * 0.3f + FMath::VRand() * 420.f + FVector(0.f, 0.f, 260.f), FMath::FRandRange(2.f, 3.5f), 80.f, FMath::FRandRange(350.f, 600.f));
	}
	FireLight->SetRelativeLocation(EngineLocalPos() + FVector(0.f, 0.f, 45.f));
	FireLight->SetVisibility(true);
	FireBall->SetRelativeLocation(EngineLocalPos() + FVector(0.f, 0.f, 30.f));
	FireBall->SetVisibility(true);
	DriftSceneryUtil::Tint(this, FireBall, FLinearColor(6.f, 1.6f, 0.15f), 1.f);
	if (DriftGraphics::SoundOn(DriftGraphics::Crash))
	{
		if (USoundBase* Boom = LoadObject<USoundBase>(nullptr, ExplosionSoundPath)) UGameplayStatics::PlaySound2D(this, Boom, 1.f, 1.f);
	}
	MapToastName = TEXT("ENGINE BLOWN      BACKSPACE or M: repaired car");
	MapToastAge = 0.f;
}

void ADriftVehiclePawn::UpdateDamage(float Dt)
{
	HitCooldown = FMath::Max(0.f, HitCooldown - Dt);

	// Parts that came off: they fly, drop, bounce and come to rest on the ground.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DriftDebris), false, this);
	for (FDriftLoosePart& P : LooseParts)
	{
		if (!P.bGone || P.bResting || !P.Mesh) continue;
		P.Vel.Z -= 980.f * Dt;
		FVector Pos = P.Mesh->GetComponentLocation() + P.Vel * Dt;
		FQuat Rot = FQuat(P.SpinAxis, FMath::DegreesToRadians(P.SpinDegPerSec * Dt)) * P.Mesh->GetComponentQuat();
		FHitResult Hit;
		if (P.Vel.Z < 0.f && GetWorld()->LineTraceSingleByChannel(Hit, Pos + FVector(0.f, 0.f, 80.f), Pos - FVector(0.f, 0.f, 12.f), ECC_Visibility, Params))
		{
			Pos.Z = Hit.ImpactPoint.Z + 12.f;
			P.Vel = FVector(P.Vel.X * 0.6f, P.Vel.Y * 0.6f, -P.Vel.Z * 0.3f);
			P.SpinDegPerSec *= 0.5f;
			if (P.Vel.Size() < 120.f)
			{
				P.bResting = true;
				Rot = FRotator(0.f, Rot.Rotator().Yaw, 0.f).Quaternion(); // lying flat
			}
		}
		P.Mesh->SetWorldLocationAndRotation(Pos, Rot);
	}

	// A knocked-about engine smokes (more as it gets worse); a blown one burns.
	const FVector EnginePos = GetActorTransform().TransformPosition(EngineLocalPos() + FVector(0.f, 0.f, 30.f));
	const float Hurt = 1.f - GetEngineHealth();
	const float PuffsPerSec = bEngineBlown ? 14.f : (Hurt > 0.4f ? 2.f + 8.f * (Hurt - 0.4f) / 0.6f : 0.f);
	EngineSmokeCarry += PuffsPerSec * Dt;
	while (EngineSmokeCarry >= 1.f)
	{
		EngineSmokeCarry -= 1.f;
		EmitPuff(EnginePos + FMath::VRand() * 25.f, GetVelocity() * 0.4f + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(120.f, 220.f)),
			FMath::FRandRange(1.8f, 3.f), 50.f, bEngineBlown ? FMath::FRandRange(300.f, 480.f) : FMath::FRandRange(160.f, 260.f));
	}
	if (bEngineBlown)
	{
		FireAge += Dt;
		const float Flash = 400000.f * FMath::Exp(-FireAge / 0.25f);                       // the blast
		const float Flicker = 0.6f + 0.4f * FMath::PerlinNoise1D(FireAge * 9.f);          // the fire after it
		FireLight->SetIntensity(Flash + 30000.f * Flicker);
		const float Ball = 0.35f + 0.25f * Flicker + 1.6f * FMath::Exp(-FireAge / 0.2f);
		FireBall->SetRelativeScale3D(FVector(Ball, Ball, Ball * 1.2f));
	}
}

void ADriftVehiclePawn::RepairCar()
{
	for (FDriftLoosePart& P : LooseParts)
	{
		P.Damage = 0.f;
		P.bResting = false;
		if (!P.bGone || !P.Mesh || !P.HomeParent) continue;
		P.bGone = false;
		P.Mesh->AttachToComponent(P.HomeParent, FAttachmentTransformRules::KeepRelativeTransform);
		P.Mesh->SetRelativeLocationAndRotation(P.Home, FRotator::ZeroRotator);
	}
	EngineDamage = 0.f;
	bEngineBlown = false;
	EngineSmokeCarry = 0.f;
	if (FireLight) { FireLight->SetIntensity(0.f); FireLight->SetVisibility(false); }
	if (FireBall) FireBall->SetVisibility(false);
}

void ADriftVehiclePawn::ResetCar()
{
	if (!bOccupied) return;
	FVector Loc = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	FRotator Rot(0.f, GetActorRotation().Yaw, 0.f);
	if (const ADriftGameModeBase* GM = GetWorld()->GetAuthGameMode<ADriftGameModeBase>())
	{
		const FTransform T = GM->GetRespawnTransform(GetActorLocation());
		if (!T.Equals(FTransform::Identity)) { Loc = T.GetLocation(); Rot = T.Rotator(); }
	}
	TeleportCar(Loc, Rot);
}

void ADriftVehiclePawn::OnSwitchMap()
{
	// M: back to the last checkpoint passed. Pressed again straight away (still sitting on it): next map.
	if (!bOccupied) return;
	const bool bJustReturned = CheckpointReturnAge < 4.f && FVector::Dist2D(GetActorLocation(), LastCheckpoint.GetLocation()) < 1500.f;
	if (bHaveCheckpoint && !bJustReturned)
	{
		const FTransform T = LastCheckpoint;
		TeleportCar(T.GetLocation() + FVector(0.f, 0.f, 100.f), T.Rotator());
		CheckpointReturnAge = 0.f;
		MapToastName = TEXT("CHECKPOINT      M again: next map");
		MapToastAge = 0.f;
		return;
	}
	GoToArea(MapIndex + 1);
}

void ADriftVehiclePawn::GoToArea(int32 Index)
{
	const ADriftGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADriftGameModeBase>() : nullptr;
	if (!GM) return;
	const int32 Count = GM->NumAreas();
	if (Count <= 0) return;
	MapIndex = ((Index % Count) + Count) % Count;
	bHaveCheckpoint = false;
	CheckpointReturnAge = 100.f;
	bHoldOn = bHoldSpent = false;
	const FTransform Start = GM->GetAreaStart(MapIndex, MapToastName);
	TeleportCar(Start.GetLocation(), Start.Rotator());
	MapToastAge = 0.f;
}

void ADriftVehiclePawn::UpdateSpeedFeel(float Dt)
{
	// What tells the eye it's going fast: the view widens and the camera picks up road buzz, more in a slide.
	const float SpeedMS = Body->GetPhysicsLinearVelocity().Size() / 100.f;
	const float K = FMath::Clamp(SpeedMS / 45.f, 0.f, 1.f); // full effect at ~160 km/h
	const bool bOutside = CamModes[CameraIndex].bLag;    // chase cameras
	Camera->SetFieldOfView(BaseFov + (bOutside ? 14.f : 9.f) * K * K * (3.f - 2.f * K));

	// Chase cameras swing towards the direction of travel in a slide (the car turns under the camera).
	const float WantYaw = (bOutside && bOccupied) ? FMath::Clamp(BodySlipDeg * ChaseSlideFollow, -ChaseSlideMaxDeg, ChaseSlideMaxDeg) : 0.f;
	ChaseSlideYaw = FMath::FInterpTo(ChaseSlideYaw, WantYaw, Dt, 3.5f);
	if (bOutside) SpringArm->SetRelativeRotation(FRotator(CamModes[CameraIndex].Pitch, ChaseSlideYaw, 0.f));

	ShakeTime += Dt;
	float Slide = 0.f;
	for (const FDriftWheel& W : Wheels) if (W.bGrounded) Slide = FMath::Max(Slide, W.SlideSpeed);
	const float SlideK = FMath::Clamp((Slide - 2.f) / 8.f, 0.f, 1.f);
	const float Amp = bOccupied ? (bOutside ? 1.2f : 0.45f) * (0.1f + K * K + 0.6f * SlideK) : 0.f; // cm
	const float Tm = ShakeTime;
	const FVector Off(0.f, FMath::PerlinNoise1D(Tm * 23.f) * Amp, FMath::PerlinNoise1D(Tm * 31.f + 7.3f) * Amp);
	const FRotator Jit(FMath::PerlinNoise1D(Tm * 19.f + 3.1f) * 0.15f * Amp, FMath::PerlinNoise1D(Tm * 13.f + 5.7f) * 0.1f * Amp, 0.f);
	Camera->SetRelativeLocationAndRotation(Off, Jit);
}

void ADriftVehiclePawn::UpdateAudio(float Dt)
{
	if (!EngineLowAudio || !EngineHighAudio || !WindAudio || !TyreAudio || !EngineLowAudio->IsPlaying()) return;
	const float Master = bOccupied ? 1.f : 0.4f;

	// Engine: low loop below ~3000 rpm, high loop above, each pitched from the rpm it was made at.
	const float Fade = FMath::Clamp((RPM - 2800.f) / 900.f, 0.f, 1.f);
	const float Load = (bEngineBlown || !DriftGraphics::SoundOn(DriftGraphics::Engine)) ? 0.f : 0.5f + 0.5f * ThrottleSmooth;
	EngineLowAudio->SetPitchMultiplier(FMath::Clamp(RPM / 1800.f * EnginePitchScale, 0.45f, 2.4f));
	EngineHighAudio->SetPitchMultiplier(FMath::Clamp(RPM / 5000.f * EnginePitchScale, 0.5f, 2.4f));
	EngineLowAudio->SetVolumeMultiplier(FMath::Max(0.001f, 0.7f * (1.f - Fade) * Load * Master));
	EngineHighAudio->SetVolumeMultiplier(FMath::Max(0.001f, 0.8f * Fade * Load * Master));

	// Wind grows with the square of the speed; quieter from inside the cabin.
	const float SpeedMS = Body->GetPhysicsLinearVelocity().Size() / 100.f;
	const bool bInside = !CamModes[CameraIndex].bLag;
	WindVol = FMath::FInterpTo(WindVol, FMath::Clamp(FMath::Square(SpeedMS / 40.f), 0.f, 1.2f) * (bInside ? 0.45f : 0.7f), Dt, 4.f);
	WindAudio->SetVolumeMultiplier(FMath::Max(0.001f, WindVol * Master * (DriftGraphics::SoundOn(DriftGraphics::Wind) ? 1.f : 0.f)));
	WindAudio->SetPitchMultiplier(FMath::Clamp(0.75f + SpeedMS / 70.f, 0.75f, 1.8f));

	// Tyres squeal as their contact patches scrub.
	float Slide = 0.f;
	for (const FDriftWheel& W : Wheels) if (W.bGrounded) Slide = FMath::Max(Slide, W.SlideSpeed);
	const float Want = FMath::SmoothStep(1.5f, 6.f, Slide) * 0.65f;
	TyreVol = FMath::FInterpTo(TyreVol, Want, Dt, Want > TyreVol ? 14.f : 6.f);
	TyreAudio->SetVolumeMultiplier(FMath::Max(0.001f, TyreVol * Master * (DriftGraphics::SoundOn(DriftGraphics::Tyres) ? 1.f : 0.f)));
	TyreAudio->SetPitchMultiplier(0.92f + 0.12f * FMath::Clamp((Slide - 3.f) / 10.f, 0.f, 1.f));
}

void ADriftVehiclePawn::TeleportCar(const FVector& Loc, const FRotator& Rot)
{
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	RepairCar(); // reset, checkpoint and map change all hand you a whole car
	HitCooldown = 0.5f;
	for (FDriftWheel& W : Wheels) W.Omega = 0.f;
	PrevVelocity = FVector::ZeroVector;
	Gear = 1;
	EngineOmega = IdleRPM * 2.f * PI / 60.f;
	ClutchEngage = 1.f;
	ShiftClutchTimer = 0.f;
}
