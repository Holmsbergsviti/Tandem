#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Track/DriftScenery.h"
#include "DriftCircuitBuilder.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * A closed drift circuit (laps forever, no timing): wide asphalt with red and white kerbs, gravel run-off,
 * guardrails, rolling ground and trees. Which one is set by Layout before BeginPlay (the game mode spawns
 * them deferred). Layouts are designed offline (Docs/DriftMechanicSpec.md, "More tracks"):
 *   0 DRIFT PARK     1.0 km, linked corners of every kind, 20 m tightest radius
 *   1 SWEEPER RING   1.9 km, long flowing sweepers (60 m and more) for long drifts
 *   2 SNAKE HILL     1.3 km, chains of S-bends over hills, 20 m tightest radius
 */
UCLASS()
class DRIFTGAME_API ADriftCircuitBuilder : public AActor
{
	GENERATED_BODY()
public:
	ADriftCircuitBuilder();
	virtual void BeginPlay() override;

	static constexpr int32 NumLayouts = 3;
	int32 Layout = 0;

	FString GetDisplayName() const;
	/** On the start straight, facing along the lap. */
	FTransform GetStartTransform() const;
	/** Points about every 24 m round the lap, facing along it. */
	TArray<FTransform> GetCheckpoints() const;
	void ApplyQuality(int32 Level);

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RoadMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> TerrainMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RailMesh;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RailPosts;
	FDriftScenery Scenery;

	/** Smoothed centre line (local cm), closed: the last point is not repeated. */
	TArray<FVector> Centre;
	void Build();
};
