#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Track/DriftScenery.h"
#include "DriftDownhillBuilder.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * A 2.9 km mountain road going down the hill: 226 m of drop, sweepers and a ladder of tight hairpins
 * (25 m radius, 50 m apart), guardrails both sides, an autumn forest on the slopes and flat pads at the
 * top and the bottom to turn round on. Spawned by the game mode. The road is designed offline
 * (grades 9.5% on straights, 7.5% in bends, 5% in the hairpins, nearly flat at both ends).
 */
UCLASS()
class DRIFTGAME_API ADriftDownhillBuilder : public AActor
{
	GENERATED_BODY()
public:
	ADriftDownhillBuilder();
	virtual void BeginPlay() override;

	/** At the top, facing down the road. */
	FTransform GetStartTransform() const;
	/** Points every 20 m down the road for "reset car". */
	TArray<FTransform> GetCheckpoints() const;
	/** Graphics quality 0 low .. 3 epic: draw distances and shadows. */
	void ApplyQuality(int32 Level);

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RoadMesh;    // asphalt, shoulders, pads, markings
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> TerrainMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RailMesh;    // guardrails (solid box collision)
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RailPosts;
	FDriftScenery Scenery;

	/** The road centre line, smoothed, every ~2.6 m (local cm). */
	TArray<FVector> Centre;
	void BuildCentre();
	void Build();
};
