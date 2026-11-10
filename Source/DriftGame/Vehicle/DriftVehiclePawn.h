#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "DriftVehiclePawn.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UAudioComponent;
class ADriftDriverCharacter;

/** Per-wheel state. Forces in newtons, speeds in m/s unless noted. */
USTRUCT()
struct FDriftWheel
{
	GENERATED_BODY()
	UPROPERTY() FVector LocalMount = FVector::ZeroVector; // suspension top, body space (cm)
	UPROPERTY() bool bFront = false;
	UPROPERTY() bool bGrounded = false;
	UPROPERTY() float Compression = 0.f;   // cm
	UPROPERTY() float Load = 0.f;          // normal force from the road, N
	UPROPERTY() float Omega = 0.f;         // wheel spin, rad/s (+ = rolling forwards)
	UPROPERTY() float SlipRatio = 0.f;     // (wheel surface speed - road speed) / road speed
	UPROPERTY() float SlipAngleDeg = 0.f;  // angle between where the tyre points and where it moves
	UPROPERTY() float Fx = 0.f;            // tyre force along the tyre (drive/brake), N
	UPROPERTY() float Fy = 0.f;            // tyre force across the tyre (cornering), N
	UPROPERTY() float SpinDeg = 0.f;
	UPROPERTY() FVector ContactPoint = FVector::ZeroVector;   // world, cm
	UPROPERTY() FVector ContactNormal = FVector::UpVector;
	UPROPERTY() FVector TyreForceWorld = FVector::ZeroVector; // N
	UPROPERTY() float SlideSpeed = 0.f;    // how fast the contact patch scrubs over the road, m/s
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Visual = nullptr;
};

/** One tyre-smoke puff (world space). */
struct FDriftSmokePuff
{
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	float Age = 0.f;
	float Life = 0.f;
	float StartSize = 0.f;
	float EndSize = 0.f;
	bool bActive = false;
};

/**
 * AE86-style car driven purely by forces. Every frame, for each wheel:
 *  - normal force from a spring + damper (so weight shifts under braking, acceleration and cornering),
 *  - tyre force from slip ratio and slip angle on a combined friction ellipse (Pacejka-style curve),
 *  - wheel spin integrated from engine/diff torque, brake torque, rolling resistance and the tyre's own reaction.
 * Plus aerodynamic drag and gravity (applied by the physics engine). Cornering (centripetal) force is the sum
 * of the tyres' sideways forces; nothing pushes the car around except these forces.
 * No driving aids: nothing trims the engine or steers for you. Two cars (AE86, NSX), picked in the menu.
 * Controls: W/S throttle+brake, A/D steer, Space handbrake, Q/E gears, Shift clutch kick, R wipers,
 * C camera, G force overlay, F get out, M last checkpoint (again: next map), Backspace reset, Esc pause.
 */
UCLASS(Config=Game)
class DRIFTGAME_API ADriftVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	ADriftVehiclePawn();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
		FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UFUNCTION(BlueprintImplementableEvent, Category="Drift") void OnWipersToggled(bool bNewWipersState);

	UFUNCTION(BlueprintPure, Category="Drift") float GetSpeedKmh() const;
	UFUNCTION(BlueprintPure, Category="Drift") int32 GetGear() const { return Gear; }
	UFUNCTION(BlueprintPure, Category="Drift") float GetRPM() const { return RPM; }
	UFUNCTION(BlueprintPure, Category="Drift") bool AreWipersOn() const { return bWipersOn; }
	UFUNCTION(BlueprintCallable, Category="Drift") void ResetCar();
	UFUNCTION(BlueprintPure, Category="Camera") FString GetCameraName() const;
	/** Seconds since the camera was switched (HUD shows the name briefly). */
	float GetTimeSinceCameraSwitch() const { return CameraSwitchAge; }
	/** Name of the area M last switched to, and how long ago (HUD shows it briefly). */
	FString GetMapName() const { return MapToastName; }
	/** Which drivable area the car was last sent to (menu "you are here"). */
	int32 GetAreaIndex() const { return MapIndex; }
	/** Re-reads the key bindings (after the menu changed one) and refreshes the active controls. */
	void ApplyKeyBindings();
	/** Teleports to drivable area Index (see ADriftGameModeBase::GetAreaStart) and shows its name. */
	void GoToArea(int32 Index);
	float GetTimeSinceMapSwitch() const { return MapToastAge; }

	// ---- Getting in and out, doors (side 0 = left, 1 = right / driver) ----
	static constexpr int32 DriverSide = 1;
	bool IsOccupied() const { return bOccupied; }
	void EnterCar(ADriftDriverCharacter* Driver);
	void ToggleDoor(int32 Side);
	bool IsDoorOpen(int32 Side) const { return Side >= 0 && Side < 2 && bDoorOpen[Side]; }
	/** Nearest door within MaxDist (cm) of a world position, or -1. */
	int32 GetNearestDoor(const FVector& WorldPos, float MaxDist) const;
	/** Where someone getting out of this door stands (world). */
	FVector GetDoorStandPoint(int32 Side) const;

	// ---- Force overlay (G) ----
	bool IsShowingForces() const { return bShowForces; }
	TArray<FString> GetForceReadout() const;
	float GetTimeSinceExitBlocked() const { return ExitBlockedAge; }
	/** Peak engine power with the current torque and rev limit, in hp. */
	float GetPeakPowerHp() const;
	/** Call after changing MassKg (menu > Tuning): gives the physics body its new mass. */
	void ApplyTuning();

	// ---- Cars (menu > Car). 0 = Toyota AE86, 1 = Honda NSX ----
	static int32 NumCarModels();
	static FString CarModelName(int32 Model);
	/** Short id used for saved settings ("AE86", "NSX"). */
	static FString CarModelId(int32 Model);
	static float CarModelPeakHp(int32 Model);
	int32 GetCarModel() const { return CarModel; }
	/** Swaps to another car: its body, engine, gearbox, tyres, weight and wheel positions, plus the player's
	 *  saved tuning for it. Remembered for the next start. */
	void SelectCar(int32 Model);
	/** Puts the car's built-in values back (no saved tuning). */
	void ApplyCarSpec(int32 Model);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<UProceduralMeshComponent> CarBody;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<USceneComponent> DoorHingeL;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<USceneComponent> DoorHingeR;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<UProceduralMeshComponent> DoorL;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<UProceduralMeshComponent> DoorR;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> Interior;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<USceneComponent> SteeringPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<USceneComponent> SteeringSpin;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> SteeringWheel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<USceneComponent> HandbrakePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> HandbrakeLever;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<USceneComponent> ShifterPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> ShifterStick;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> HandL;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> HandR;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> ArmL;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> ArmR;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> TachNeedle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cockpit") TObjectPtr<UProceduralMeshComponent> SpeedNeedle;
	/** Engine sound: a low-rpm and a high-rpm loop, cross-faded and pitched by the engine speed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sound") TObjectPtr<UAudioComponent> EngineLowAudio;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sound") TObjectPtr<UAudioComponent> EngineHighAudio;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sound") TObjectPtr<UAudioComponent> WindAudio;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sound") TObjectPtr<UAudioComponent> TyreAudio;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<USceneComponent> WiperPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<UStaticMeshComponent> WiperBlade;
	/** Tyre smoke puffs (world space, pooled). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Car") TObjectPtr<UInstancedStaticMeshComponent> TyreSmoke;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> Camera;

	// ---- Body and suspension (AE86: 975 kg, 53/47, 2400 mm wheelbase) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body") float MassKg = 975.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body") float SuspensionRest = 24.f;
	/** Spring rate in UE units (kg/s^2 = N/m): about 1.5 Hz ride; both ends sit level at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body") float SpringRate = 24000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body") float SpringRateRear = 22000.f;
	/** About 0.4 of critical damping: the body rolls, pitches and rebounds, so you can rock the car. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body") float DamperRate = 1900.f;
	/** Drag coefficient x frontal area, m^2 (Cd 0.35 x 1.8 m^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body") float DragCdA = 0.63f;

	// ---- Wheels and tyres (185/60R14) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float WheelRadius = 29.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float TyreHalfWidth = 9.25f;
	/** Spin inertia of one wheel + tyre + brake disc, kg m^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float WheelInertia = 0.9f;
	/** Peak friction coefficient of the front tyres on dry tarmac. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float TyreGrip = 1.f;
	/** Same for the rear tyres (TyreGrip is the fronts). Drift cars often run a harder or narrower rear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float TyreGripRear = 0.88f;
	/** Friction of a tyre sliding fast, relative to the peak. Drift tyres (and a semi-slick on a drift
	 *  car) keep more of their grip once past the peak than a road tyre does, so a slide stays steerable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float SlidingFriction = 0.9f;
	/** Slip ratio where drive/brake grip peaks (10%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float PeakSlipRatio = 0.1f;
	/** Slip angle where cornering grip peaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float PeakSlipAngleDeg = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float RollingResistance = 0.013f;
	/** A tyre starts to smoke when its contact patch scrubs faster than this, m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float SmokeSlideSpeed = 2.f;
	/** Rubber barely loses grip when it slides slowly and falls to SlidingFriction as the sliding speed
	 *  grows past this (m/s). So slow manoeuvres stay planted and fast slides carry their momentum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float SlideFalloffSpeed = 6.f;
	/** A more heavily loaded tyre has a lower friction coefficient (real tyres do). This is what makes
	 *  weight transfer unsettle the car: feints, lift-off and rocking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tyres") float LoadSensitivity = 0.12f;

	// ---- Steering ----
	/** Front wheel lock. Drift cars run an angle kit (modified knuckles and arms) for about 60 deg, so the
	 *  front wheels can still point where the car is travelling at big slip angles and catch the slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float MaxSteerDeg = 60.f;
	/** Caster: how far the front wheels swing to follow the direction the front axle is travelling when
	 *  you're not steering (a real car's self-aligning torque; this is what countersteers in a slide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float CasterAlign = 1.f;
	/** Seconds from centre to full lock on keyboard: while the car grips (any speed), and while it drifts
	 *  (body slip past 8 deg, fully by 15 deg) so a tap doesn't throw the slide around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float SteerRampTime = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float SteerRampTimeFast = 0.14f;
	/** Seconds to self-centre when the key is released. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float SteerReturnTime = 0.07f;
	/** Drifting, a full key turns the front wheels this far either side of the way they travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float DriftSteerRangeDeg = 16.f;
	/** The front wheels turn at most this fast (deg/s at the road wheels) and follow with this lag (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float SteerRateDegPerSec = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float SteerLagTime = 0.03f;
	/** Snap-back damping (the only driving help; Menu > Tuning switches it). While a drift's angle is falling
	 *  the front wheels are left short of the direction of travel by SnapDamping deg per deg/s of fall, at
	 *  most SnapDampMaxDeg, so the car unwinds instead of snapping straight. It never picks or keeps an angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") bool bDriftAngleHold = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float SnapDamping = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float SnapDampMaxDeg = 6.f;
	/** Cockpit steering wheel turns this far each way at full lock (540 deg lock to lock). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering") float WheelTurnDeg = 270.f;

	// ---- Engine and drivetrain ----
	/** Stock 4A-GE is 149 Nm / 130 PS; default is a drift-built engine (Boss found stock too slow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine") float MaxTorqueNm = 330.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine") float MaxRPM = 7600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine") float IdleRPM = 850.f;
	/** Engine speed held by a slipping clutch when pulling away at full throttle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine") float LaunchRPM = 4500.f;
	/** Crank + flywheel inertia, kg m^2. A revved engine stores energy the clutch can dump into the tyres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine") float EngineInertia = 0.15f;
	/** Engine drag torque when off throttle in gear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine") float EngineBrakeNm = 22.f;
	/** Seconds for the keyboard throttle to go from 0 to full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine") float ThrottleRiseTime = 0.1f;
	/** Throttle with the part-throttle key held (keyboard W is all or nothing; this is the half-open pedal
	 *  for balancing a drift). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine") float PartThrottle = 0.55f;
	/** Chase cameras swing round towards where the car is going by this share of the slide angle, up to
	 *  ChaseSlideMaxDeg, so you see the car sideways and the road it is sliding along. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") float ChaseSlideFollow = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") float ChaseSlideMaxDeg = 40.f;
	/** T50 5-speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") TArray<float> GearRatios = { 3.587f, 2.022f, 1.384f, 1.0f, 0.861f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float ReverseRatio = 3.484f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float FinalDrive = 4.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float DrivetrainEfficiency = 0.85f;
	/** Limited-slip diff: locking torque per rad/s of speed difference, and its maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float LSDStiffness = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float LSDMaxLockNm = 450.f;
	/** Most torque the clutch can carry before it slips, Nm at the crank (uprated clutch, ~2x the engine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float ClutchMaxNm = 420.f;
	/** Clutch kick (Left Shift): seconds the pedal is held down while the engine revs; then it's dumped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float ClutchKickTime = 0.25f;
	/** Seconds the clutch is down during a gear change, and how long it takes to bite again after an
	 *  upshift. Downshifts dump it in ClutchDumpTime with no rev-match: that's the shift-lock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float ShiftClutchTime = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float ClutchEngageTime = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drivetrain") float ClutchDumpTime = 0.1f;

	// ---- Brakes (no ABS) ----
	/** Total brake force at the road with the pedal fully down, N (about 0.95 g). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brakes") float BrakeForce = 9000.f;
	/** Share of braking on the front axle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brakes") float BrakeBias = 0.72f;
	/** Handbrake torque on each rear wheel, Nm (enough to lock them). The clutch is pressed while it's held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brakes") float HandbrakeTorque = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") bool bInputLocked = false;
	/** Writes the drive recorder files (see RecPath). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") bool bRecordDrives = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car") FLinearColor PaintColor = FLinearColor(0.82f, 0.82f, 0.8f); // white over black two-tone

private:
	void BuildInput();
	void BuildMappings();
	void OnThrottle(const FInputActionValue& V);
	void OnBrake(const FInputActionValue& V);
	void OnSteer(const FInputActionValue& V);
	void OnHandbrakeStart(const FInputActionValue& V);
	void OnHandbrakeEnd(const FInputActionValue& V);
	void OnPartThrottleStart();
	void OnPartThrottleEnd();
	void OnGearDown();
	void OnGearUp();
	void ShiftTo(int32 NewGear);
	void OnWipers();
	void OnCameraCycle();
	void OnPause();
	void UpdateCheckpoint(float Dt);
	void UpdateRecorder(float Dt, float Throttle, float Brake, float Steer, bool bHandbrake);
	void OnQuit();
	void OnClutchKick();
	void OnExitCar();
	void OnToggleForces();
	void OnSwitchMap();
	void TeleportCar(const FVector& Loc, const FRotator& Rot);
	void UpdateSpeedFeel(float Dt);
	void UpdateAudio(float Dt);

	void SimulateWheels(float Dt, float Throttle, float Brake, bool bHandbrake);
	void DrawForces() const;
	float EngineTorqueAt(float Rpm) const;
	float CurrentRatio() const;
	void Paint(UStaticMeshComponent* C, const FLinearColor& Color, UMaterialInterface* Base);
	void ApplyCameraMode();
	void UpdateCockpit(float Dt);
	void UpdateDoors(float Dt);
	bool IsCockpitCamera() const;
	void BuildVisuals();
	void OpenDoorBriefly(int32 Side);
	void UpdateSmoke(float Dt);

	static constexpr int32 MaxPuffs = 180;
	TArray<FDriftSmokePuff> Puffs;
	int32 NextPuff = 0;
	float SmokeCarry[4] = { 0.f, 0.f, 0.f, 0.f };

	UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY() TObjectPtr<UInputAction> IA_Throttle;
	UPROPERTY() TObjectPtr<UInputAction> IA_Brake;
	UPROPERTY() TObjectPtr<UInputAction> IA_PartThrottle;
	UPROPERTY() TObjectPtr<UInputAction> IA_Steer;
	UPROPERTY() TObjectPtr<UInputAction> IA_Handbrake;
	UPROPERTY() TObjectPtr<UInputAction> IA_GearDown;
	UPROPERTY() TObjectPtr<UInputAction> IA_GearUp;
	UPROPERTY() TObjectPtr<UInputAction> IA_Wipers;
	UPROPERTY() TObjectPtr<UInputAction> IA_Camera;
	UPROPERTY() TObjectPtr<UInputAction> IA_Reset;
	UPROPERTY() TObjectPtr<UInputAction> IA_Pause;
	UPROPERTY() TObjectPtr<UInputAction> IA_Quit;
	UPROPERTY() TObjectPtr<UInputAction> IA_Clutch;
	UPROPERTY() TObjectPtr<UInputAction> IA_Exit;
	UPROPERTY() TObjectPtr<UInputAction> IA_Forces;
	UPROPERTY() TObjectPtr<UInputAction> IA_Map;

	UPROPERTY() TArray<FDriftWheel> Wheels;

	float ThrottleIn = 0.f, BrakeIn = 0.f, SteerIn = 0.f, SteerCurrent = 0.f;
	float ThrottleSmooth = 0.f;
	bool bHandbrakeIn = false;
	int32 Gear = 1;      // -1 = R, 0 = N
	float RPM = 900.f;
	float EngineOmega = 89.f;     // crank speed, rad/s (850 rpm)
	float ClutchEngage = 1.f;     // 0 = pedal just released, 1 = fully biting
	float ShiftClutchTimer = 0.f;
	bool bLastShiftDown = false;
	float RearTyreStiffness = 0.f; // how hard the road resists rear wheel spin, Nm per rad/s (from the tyre model)
	float EngineTorqueNow = 0.f; // Nm at the crank this frame (negative = engine braking)
	bool bClutchSlipping = false;
	int32 CarModel = 0;
	bool bPartThrottleIn = false;
	// Drive recorder: what the car did, 20 times a second, to ~/Documents/Tandem/Drives/drive_<time>.csv
	// (for tuning the handling from real driving). The ten newest files are kept.
	FString RecPath;
	FString RecBuffer;
	float RecTime = 0.f;
	float RecSampleTimer = 0.f;
	float RecFlushTimer = 0.f;
	float ChaseSlideYaw = 0.f;       // chase camera's swing into the slide, deg
	float EnginePitchScale = 1.f;    // more cylinders fire more often at the same rpm
	// Snap-back damping (see bDriftAngleHold).
	bool bHoldOn = false;
	bool bHoldSpent = false;       // the drift ended with the countersteer key down: ignored until released
	float HoldSide = 1.f;          // +1 sliding right, -1 sliding left
	float HoldPrevSlip = 0.f;
	float HoldRate = 0.f;          // deg/s, slide angle on the held side
	// Checkpoints (M): the last one passed, facing the way the car was going.
	bool bHaveCheckpoint = false;
	FTransform LastCheckpoint;
	float CheckpointScan = 0.f;
	float CheckpointReturnAge = 100.f; // since M last put the car on a checkpoint
	float SteerAngleDeg = 0.f;
	float BodySlipDeg = 0.f;
	FVector PrevVelocity = FVector::ZeroVector;
	float GLong = 0.f, GLat = 0.f;
	float DragForceN = 0.f;
	FVector ResultantForceN = FVector::ZeroVector; // world, tyres + drag (gravity and road support cancel vertically)
	bool bWipersOn = false;
	float WiperPhase = 0.f;
	int32 CameraIndex = 0;
	float CameraSwitchAge = 100.f;
	float HandbrakePull = 0.f;
	float ClutchKickTimer = 0.f;
	float ShiftTimer = 0.f;
	FVector ShiftHandPos = FVector::ZeroVector; // left hand (right-hand drive)
	float WheelVisDeg = 0.f;                     // cockpit steering wheel angle, smoothed
	// Hands on the rim, [0] left [1] right. Angles in the column frame: 0 = 9 o'clock, 90 = 12, 180 = 3.
	float HandRimDeg[2] = { 20.f, 160.f };
	float HandT[2] = { -1.f, -1.f };            // hand-over-hand move 0..1, below 0 = holding the rim
	float HandFromDeg[2] = { 0.f, 0.f };
	int8 HandDir[2] = { 0, 0 };                 // +1 carried past the top of its range, -1 past the bottom
	bool bLeftHandAway = false;                 // left hand at the shifter or handbrake, or coming back
	FRotator ShifterRot = FRotator::ZeroRotator;
	bool bShowForces = false;
	bool bOccupied = true;
	bool bDoorOpen[2] = { false, false };
	float DoorAngle[2] = { 0.f, 0.f };
	float DoorCloseTimer[2] = { 0.f, 0.f };
	float ExitBlockedAge = 100.f;
	int32 MapIndex = 0;            // 0 practice lot, 1 high-speed loop, 2 mountain pass
	float MapToastAge = 100.f;
	FString MapToastName;
	float ShakeTime = 0.f;
	float BaseFov = 92.f;
	float WindVol = 0.f;
	float TyreVol = 0.f;
};
