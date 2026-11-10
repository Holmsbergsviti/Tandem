#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DriftGameModeBase.generated.h"

class ADriftTrackBuilder;
class ADriftCityBuilder;
class ADriftDownhillBuilder;
class ADriftCircuitBuilder;

/**
 * Free driving: no laps, timers or countdown (Boss: "nobody cares about that").
 * Spawns the extra maps (city, downhill) next to the main track builder, knows every map's start for
 * the menu and M, and every map's checkpoints so "reset car" can put you back on the road.
 */
UCLASS()
class DRIFTGAME_API ADriftGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADriftGameModeBase();
	virtual void BeginPlay() override;
	/** Puts the car at the start of the road (on its surface), whatever the map's PlayerStart says. */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	/** Where "Reset car" puts the car: the nearest point on any map's road, facing along it. */
	UFUNCTION(BlueprintCallable, Category="Track") FTransform GetRespawnTransform(const FVector& Near) const;

	/** Nearest checkpoint within MaxDist (cm, flat distance) of a point, or false. */
	bool GetNearestCheckpoint(const FVector& Near, float MaxDist, FTransform& Out) const;

	/** Drivable areas in menu and M order: practice lot, high-speed loop, mountain pass, turn ladder, city,
	 *  downhill, then the drift circuits (park, sweeper ring, snake hill). */
	int32 NumAreas() const { return 6 + Circuits.Num(); }
	FTransform GetAreaStart(int32 Index, FString& OutName) const;
	FString GetAreaName(int32 Index) const;
	/** Graphics quality 0 low .. 3 epic on every map: scenery draw distances and shadows. */
	void ApplySceneryQuality(int32 Level);

private:
	TArray<FTransform> Checkpoints;
	UPROPERTY() TObjectPtr<ADriftCityBuilder> City;
	UPROPERTY() TObjectPtr<ADriftDownhillBuilder> Downhill;
	UPROPERTY() TArray<TObjectPtr<ADriftCircuitBuilder>> Circuits;
	ADriftTrackBuilder* FindMain() const;
};
