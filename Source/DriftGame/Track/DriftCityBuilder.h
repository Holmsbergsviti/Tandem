#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Track/DriftScenery.h"
#include "DriftCityBuilder.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * A small downtown to drive through: a 5 x 4 grid of blocks with wide streets, 15 cm kerbs and
 * sidewalks, buildings of different heights and colours (taller in the middle), street lights, an open
 * plaza and a car park to drift on, a park with trees, and a skyline far out. Spawned by the game mode.
 * Cheap to draw: the ground and the buildings are one mesh each, lights and trees are instanced.
 */
UCLASS()
class DRIFTGAME_API ADriftCityBuilder : public AActor
{
	GENERATED_BODY()
public:
	ADriftCityBuilder();
	virtual void BeginPlay() override;

	/** On a street on the west side, facing east down it. */
	FTransform GetStartTransform() const;
	/** Points along every street for "reset car". */
	TArray<FTransform> GetCheckpoints() const;
	/** Graphics quality 0 low .. 3 epic: draw distances and shadows. */
	void ApplyQuality(int32 Level);

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> GroundMesh;   // streets, sidewalks, grass (collision)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BuildingMesh; // walls, windows, roofs (box collision)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> SkylineMesh;  // distant towers, no collision
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> LampPoles;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> LampHeads;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Cones;
	FDriftScenery Scenery;

	void Build();
};
