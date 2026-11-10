#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DriftDriverCharacter.generated.h"

class ADriftVehiclePawn;
class UCameraComponent;
class USpringArmComponent;
class USceneComponent;
class UProceduralMeshComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * The driver on foot: walks around the car in third person, opens and closes the doors (E)
 * and gets back in (F). Spawned by the car when you get out, destroyed when you get in.
 */
UCLASS()
class DRIFTGAME_API ADriftDriverCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ADriftDriverCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void SetCar(ADriftVehiclePawn* InCar);
	ADriftVehiclePawn* GetCar() const;
	/** Door within reach (0 left, 1 right) or -1. */
	int32 GetDoorInReach() const;

	UPROPERTY(EditAnywhere, Category="Driver") float WalkSpeed = 170.f;
	UPROPERTY(EditAnywhere, Category="Driver") float RunSpeed = 450.f;
	UPROPERTY(EditAnywhere, Category="Driver") float ReachDistance = 220.f;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraArm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> HipL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> HipR;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ShoulderL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ShoulderR;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> LegL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> LegR;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> ArmL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> ArmR;

	UPROPERTY() TObjectPtr<ADriftVehiclePawn> Car;

	UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY() TObjectPtr<UInputAction> IA_Move;
	UPROPERTY() TObjectPtr<UInputAction> IA_Look;
	UPROPERTY() TObjectPtr<UInputAction> IA_Jump;
	UPROPERTY() TObjectPtr<UInputAction> IA_Run;
	UPROPERTY() TObjectPtr<UInputAction> IA_Enter;
	UPROPERTY() TObjectPtr<UInputAction> IA_Door;
	UPROPERTY() TObjectPtr<UInputAction> IA_Pause;
	UPROPERTY() TObjectPtr<UInputAction> IA_Quit;

	void BuildInput();
	void BuildBody();
	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnRunStart();
	void OnRunEnd();
	void OnEnter();
	void OnDoor();
	void OnPause();
	void OnQuit();

	float WalkPhase = 0.f;
};
