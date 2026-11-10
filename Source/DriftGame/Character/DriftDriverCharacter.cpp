#include "DriftDriverCharacter.h"
#include "Vehicle/DriftVehiclePawn.h"
#include "Util/ProcGeo.h"
#include "ProceduralMeshComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace DriverParts
{
	const TCHAR* DriverMaterialPath = TEXT("/Game/Materials/M_VertexColor.M_VertexColor");

	// Capsule is 34 x 90: feet at Z = -90, head top near +84.
	constexpr float HipZ = -4.f;
	constexpr float ShoulderZ = 48.f;

	const FLinearColor Jacket(0.012f, 0.018f, 0.045f);
	const FLinearColor Stripe(0.55f, 0.02f, 0.01f);
	const FLinearColor Jeans(0.035f, 0.06f, 0.14f);
	const FLinearColor Skin(0.52f, 0.31f, 0.2f);
	const FLinearColor Hair(0.015f, 0.01f, 0.006f);
	const FLinearColor Shoe(0.75f, 0.75f, 0.75f);
	const FLinearColor Sole(0.03f, 0.03f, 0.03f);
	const FLinearColor Eye(0.01f, 0.01f, 0.01f);

	void Box(FProcGeo& G, float X, float Y, float Z, float HX, float HY, float HZ, const FLinearColor& C)
	{
		G.AddBox(FVector(X, Y, Z), FVector(HX, HY, HZ), C);
	}

	UProceduralMeshComponent* MakePart(ACharacter* Owner, const TCHAR* Name, USceneComponent* Parent)
	{
		UProceduralMeshComponent* P = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		P->SetupAttachment(Parent);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(true);
		return P;
	}

	USceneComponent* MakePivot(ACharacter* Owner, const TCHAR* Name, USceneComponent* Parent, const FVector& Pos)
	{
		USceneComponent* S = Owner->CreateDefaultSubobject<USceneComponent>(Name);
		S->SetupAttachment(Parent);
		S->SetRelativeLocation(Pos);
		return S;
	}
}

ADriftDriverCharacter::ADriftDriverCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.f, 540.f, 0.f);
	MoveComp->MaxWalkSpeed = WalkSpeed;
	MoveComp->JumpZVelocity = 420.f;
	MoveComp->AirControl = 0.3f;
	MoveComp->BrakingDecelerationWalking = 1600.f;
	MoveComp->PushForceFactor = 30000.f; // leaning on a 975 kg car barely moves it

	USceneComponent* Root = GetCapsuleComponent();

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Root);
	CameraArm->TargetArmLength = 320.f;
	CameraArm->SocketOffset = FVector(0.f, 35.f, 50.f);
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bDoCollisionTest = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 12.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(80.f);

	BodyMesh = DriverParts::MakePart(this, TEXT("BodyMesh"), Root);
	HipL = DriverParts::MakePivot(this, TEXT("HipL"), Root, FVector(0.f, -10.f, DriverParts::HipZ));
	HipR = DriverParts::MakePivot(this, TEXT("HipR"), Root, FVector(0.f, 10.f, DriverParts::HipZ));
	ShoulderL = DriverParts::MakePivot(this, TEXT("ShoulderL"), Root, FVector(0.f, -25.f, DriverParts::ShoulderZ));
	ShoulderR = DriverParts::MakePivot(this, TEXT("ShoulderR"), Root, FVector(0.f, 25.f, DriverParts::ShoulderZ));
	LegL = DriverParts::MakePart(this, TEXT("LegL"), HipL);
	LegR = DriverParts::MakePart(this, TEXT("LegR"), HipR);
	ArmL = DriverParts::MakePart(this, TEXT("ArmL"), ShoulderL);
	ArmR = DriverParts::MakePart(this, TEXT("ArmR"), ShoulderR);
}

void ADriftDriverCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	BuildBody();
}

void ADriftDriverCharacter::BuildBody()
{
	// Torso, head and hair: a simple blocky driver in a dark racing jacket and jeans.
	FProcGeo B;
	DriverParts::Box(B, 0.f, 0.f, 0.f, 11.f, 17.f, 8.f, DriverParts::Jeans);           // pelvis
	DriverParts::Box(B, 0.f, 0.f, 30.f, 12.f, 19.f, 23.f, DriverParts::Jacket);        // chest
	DriverParts::Box(B, 12.2f, 0.f, 30.f, 0.4f, 3.f, 22.f, DriverParts::Stripe);       // zip stripe
	DriverParts::Box(B, 0.f, 0.f, 55.f, 10.f, 14.f, 3.f, DriverParts::Jacket);         // collar
	DriverParts::Box(B, 0.f, 0.f, 60.f, 5.f, 5.f, 4.f, DriverParts::Skin);             // neck
	DriverParts::Box(B, 1.f, 0.f, 73.f, 10.f, 9.f, 11.f, DriverParts::Skin);           // head
	DriverParts::Box(B, -0.5f, 0.f, 83.f, 11.f, 10.f, 3.f, DriverParts::Hair);         // hair top
	DriverParts::Box(B, -6.f, 0.f, 76.f, 6.f, 10.f, 8.f, DriverParts::Hair);           // hair back
	DriverParts::Box(B, 11.f, -4.f, 75.f, 0.5f, 1.6f, 1.1f, DriverParts::Eye);
	DriverParts::Box(B, 11.f, 4.f, 75.f, 0.5f, 1.6f, 1.1f, DriverParts::Eye);
	B.Commit(BodyMesh, 0, false);

	// A leg hangs from its hip pivot down to the ground (pivot at -4, feet at -90).
	FProcGeo L;
	DriverParts::Box(L, 0.f, 0.f, -40.f, 7.5f, 7.5f, 37.f, DriverParts::Jeans);
	DriverParts::Box(L, 4.f, 0.f, -82.f, 12.f, 6.5f, 5.f, DriverParts::Shoe);
	DriverParts::Box(L, 4.f, 0.f, -86.f, 12.5f, 7.f, 1.2f, DriverParts::Sole);
	L.Commit(LegL, 0, false);
	L.Commit(LegR, 0, false);

	// An arm hangs from its shoulder pivot.
	FProcGeo A;
	DriverParts::Box(A, 0.f, 0.f, -19.f, 6.f, 6.f, 19.f, DriverParts::Jacket);
	DriverParts::Box(A, 0.f, 0.f, -36.f, 6.3f, 6.3f, 2.f, DriverParts::Stripe);       // cuff
	DriverParts::Box(A, 0.f, 0.f, -43.f, 4.f, 4.f, 5.f, DriverParts::Skin);           // hand
	A.Commit(ArmL, 0, false);
	A.Commit(ArmR, 0, false);

	if (UMaterialInterface* VMat = LoadObject<UMaterialInterface>(nullptr, DriverParts::DriverMaterialPath))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(VMat, this);
		MID->SetScalarParameterValue(TEXT("Roughness"), 0.75f);
		MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		MID->SetScalarParameterValue(TEXT("Emissive"), 0.f);
		for (UProceduralMeshComponent* Part : { BodyMesh.Get(), LegL.Get(), LegR.Get(), ArmL.Get(), ArmR.Get() })
		{
			Part->SetMaterial(0, MID);
		}
	}
}

void ADriftDriverCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Walk cycle: legs and arms swing opposite each other, faster and wider the faster you go.
	const float Speed = GetVelocity().Size2D();
	const bool bAir = GetCharacterMovement()->IsFalling();
	WalkPhase = FMath::Fmod(WalkPhase + DeltaSeconds * Speed * 0.035f, 2.f * PI);
	const float Amp = bAir ? 0.f : FMath::Clamp(Speed / 320.f, 0.f, 1.f) * 38.f;
	const float Swing = FMath::Sin(WalkPhase) * Amp;
	const float LegAir = bAir ? 18.f : 0.f;

	HipL->SetRelativeRotation(FRotator(Swing + LegAir, 0.f, 0.f));
	HipR->SetRelativeRotation(FRotator(-Swing - LegAir * 0.4f, 0.f, 0.f));
	ShoulderL->SetRelativeRotation(FRotator(-Swing * 0.8f, 0.f, bAir ? -25.f : -4.f));
	ShoulderR->SetRelativeRotation(FRotator(Swing * 0.8f, 0.f, bAir ? 25.f : 4.f));
}

void ADriftDriverCharacter::SetCar(ADriftVehiclePawn* InCar) { Car = InCar; }
ADriftVehiclePawn* ADriftDriverCharacter::GetCar() const { return Car.Get(); }

int32 ADriftDriverCharacter::GetDoorInReach() const
{
	return Car ? Car->GetNearestDoor(GetActorLocation(), ReachDistance) : -1;
}

// ---------------- Input ----------------

void ADriftDriverCharacter::PawnClientRestart()
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

static UInputAction* MakeDriverAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* A = NewObject<UInputAction>(Outer, Name);
	A->ValueType = Type;
	return A;
}

void ADriftDriverCharacter::BuildInput()
{
	if (MappingContext) return;
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_OnFoot"));

	IA_Move  = MakeDriverAction(this, TEXT("IA_Move"),  EInputActionValueType::Axis2D);
	IA_Look  = MakeDriverAction(this, TEXT("IA_Look"),  EInputActionValueType::Axis2D);
	IA_Jump  = MakeDriverAction(this, TEXT("IA_Jump"),  EInputActionValueType::Boolean);
	IA_Run   = MakeDriverAction(this, TEXT("IA_Run"),   EInputActionValueType::Boolean);
	IA_Enter = MakeDriverAction(this, TEXT("IA_Enter"), EInputActionValueType::Boolean);
	IA_Door  = MakeDriverAction(this, TEXT("IA_Door"),  EInputActionValueType::Boolean);
	IA_Pause = MakeDriverAction(this, TEXT("IA_Pause"), EInputActionValueType::Boolean);
	IA_Pause->bTriggerWhenPaused = true;
	IA_Quit  = MakeDriverAction(this, TEXT("IA_Quit"),  EInputActionValueType::Boolean);
	IA_Quit->bTriggerWhenPaused = true;

	// WASD into one 2D axis: X = right, Y = forward.
	FEnhancedActionKeyMapping& Fwd = MappingContext->MapKey(IA_Move, EKeys::W);
	UInputModifierSwizzleAxis* SwizzleW = NewObject<UInputModifierSwizzleAxis>(MappingContext);
	SwizzleW->Order = EInputAxisSwizzle::YXZ;
	Fwd.Modifiers.Add(SwizzleW);

	FEnhancedActionKeyMapping& Back = MappingContext->MapKey(IA_Move, EKeys::S);
	UInputModifierSwizzleAxis* SwizzleS = NewObject<UInputModifierSwizzleAxis>(MappingContext);
	SwizzleS->Order = EInputAxisSwizzle::YXZ;
	Back.Modifiers.Add(SwizzleS);
	Back.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));

	MappingContext->MapKey(IA_Move, EKeys::D);
	FEnhancedActionKeyMapping& Left = MappingContext->MapKey(IA_Move, EKeys::A);
	Left.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));

	MappingContext->MapKey(IA_Look, EKeys::Mouse2D);
	MappingContext->MapKey(IA_Jump, EKeys::SpaceBar);
	MappingContext->MapKey(IA_Run, EKeys::LeftShift);
	MappingContext->MapKey(IA_Enter, EKeys::F);
	MappingContext->MapKey(IA_Door, EKeys::E);
	// Esc (menu) is handled by the player controller.
}

void ADriftDriverCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(IA_Move,  ETriggerEvent::Triggered, this, &ADriftDriverCharacter::OnMove);
		EIC->BindAction(IA_Look,  ETriggerEvent::Triggered, this, &ADriftDriverCharacter::OnLook);
		EIC->BindAction(IA_Jump,  ETriggerEvent::Started,   this, &ACharacter::Jump);
		EIC->BindAction(IA_Jump,  ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EIC->BindAction(IA_Run,   ETriggerEvent::Started,   this, &ADriftDriverCharacter::OnRunStart);
		EIC->BindAction(IA_Run,   ETriggerEvent::Completed, this, &ADriftDriverCharacter::OnRunEnd);
		EIC->BindAction(IA_Enter, ETriggerEvent::Started,   this, &ADriftDriverCharacter::OnEnter);
		EIC->BindAction(IA_Door,  ETriggerEvent::Started,   this, &ADriftDriverCharacter::OnDoor);
		EIC->BindAction(IA_Pause, ETriggerEvent::Started,   this, &ADriftDriverCharacter::OnPause);
		EIC->BindAction(IA_Quit,  ETriggerEvent::Started,   this, &ADriftDriverCharacter::OnQuit);
	}
}

void ADriftDriverCharacter::OnMove(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();
	if (!Controller) return;
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(Yaw.Vector(), (float)In.Y);
	AddMovementInput(FRotator(0.f, Yaw.Yaw + 90.f, 0.f).Vector(), (float)In.X);
}

void ADriftDriverCharacter::OnLook(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();
	AddControllerYawInput((float)In.X);
	AddControllerPitchInput((float)-In.Y);
}

void ADriftDriverCharacter::OnRunStart() { GetCharacterMovement()->MaxWalkSpeed = RunSpeed; }
void ADriftDriverCharacter::OnRunEnd()   { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed; }

void ADriftDriverCharacter::OnEnter()
{
	if (Car && GetDoorInReach() >= 0)
	{
		Car->EnterCar(this); // destroys this character
	}
}

void ADriftDriverCharacter::OnDoor()
{
	const int32 Side = GetDoorInReach();
	if (Car && Side >= 0) Car->ToggleDoor(Side);
}

void ADriftDriverCharacter::OnPause()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetPause(!PC->IsPaused());
	}
}

void ADriftDriverCharacter::OnQuit()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->IsPaused())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}
