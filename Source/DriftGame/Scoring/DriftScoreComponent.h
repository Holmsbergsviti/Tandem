#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DriftScoreComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDriftBanked, float, BankedPoints);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpinOut);

/**
 * Drift detection and scoring (see Docs/DriftMechanicSpec.md).
 * Drift angle = angle between where the car points and where it moves.
 * Points/sec = angle x speed x combo x proximity bonus. Combo grows by chaining drifts and by
 * transitions (left slide into right slide). A wall hit or a spin-out breaks the combo.
 */
UCLASS(ClassGroup=(Drift), meta=(BlueprintSpawnableComponent))
class DRIFTGAME_API UDriftScoreComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDriftScoreComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") float MinDriftAngle = 15.f;
	/** ~36 km/h, as in the spec (1000 cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") float MinDriftSpeedKmh = 36.f;
	/** Beyond this angle the car has spun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") float SpinOutAngle = 115.f;
	/** Seconds of grip driving before the chain is banked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") float ChainGraceTime = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") int32 MaxCombo = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") float PointsScale = 0.015f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") float ProximityDistance = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift") float ProximityBonus = 1.5f;

	UPROPERTY(BlueprintReadOnly, Category="Drift") float TotalScore = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Drift") float ChainScore = 0.f;
	/** Combo multiplier. */
	UPROPERTY(BlueprintReadOnly, Category="Drift") int32 Multiplier = 1;
	/** Unsigned drift angle in degrees. */
	UPROPERTY(BlueprintReadOnly, Category="Drift") float CurrentAngle = 0.f;
	/** Signed: + = sliding to the right. */
	UPROPERTY(BlueprintReadOnly, Category="Drift") float SignedAngle = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Drift") bool bDrifting = false;
	UPROPERTY(BlueprintReadOnly, Category="Drift") bool bNearWall = false;
	/** Last callout ("TRANSITION!", "CLOSE!", "SPIN OUT") and its age, for the HUD. */
	UPROPERTY(BlueprintReadOnly, Category="Drift") FString EventText;
	UPROPERTY(BlueprintReadOnly, Category="Drift") float EventAge = 100.f;

	UPROPERTY(BlueprintAssignable, Category="Drift") FOnDriftBanked OnDriftBanked;
	UPROPERTY(BlueprintAssignable, Category="Drift") FOnSpinOut OnSpinOut;

	UFUNCTION(BlueprintCallable, Category="Drift") void BreakChain();
	UFUNCTION(BlueprintCallable, Category="Drift") void BankChain();
	bool IsChainActive() const { return bChainActive; }

private:
	void Callout(const TCHAR* Text);
	bool CheckProximity() const;

	float GraceRemaining = 0.f;
	float CloseCalloutCooldown = 0.f;
	int32 LastSide = 0;
	bool bChainActive = false;
};
