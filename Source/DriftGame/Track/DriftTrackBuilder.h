#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Track/DriftScenery.h"
#include "DriftTrackBuilder.generated.h"

class USplineComponent;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;

/**
 * Builds a 5.8 km narrow mountain road (touge) from a spline: hairpins, esses, climbs, crests and
 * rough patches; grass shoulders and concrete barriers that follow the slopes; hilly terrain around it
 * with trees. Road and terrain have real collision, so the car drives on the actual surface.
 */
UCLASS()
class DRIFTGAME_API ADriftTrackBuilder : public AActor
{
	GENERATED_BODY()
public:
	ADriftTrackBuilder();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Track") TObjectPtr<USplineComponent> Spline;

	/** Two narrow lanes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") float RoadWidth = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") float SegmentLength = 250.f;
	/** Grass shoulder between the asphalt edge and the barrier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") float RunOff = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") float WallHeight = 90.f;
	/** Spacing of the points "reset car" can put you back on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") float CheckpointSpacing = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") int32 TreeCount = 1400;
	/** Road elevation scale (1 = up to ~15% grades, 69 m from lowest to highest point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") float HillScale = 1.f;
	/** Height of the small bumps in the asphalt, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") float BumpHeight = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain") float TerrainCell = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain") float TerrainMargin = 40000.f;
	/** Height of the hills and valleys away from the road, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain") float HillHeight = 3500.f;

	UFUNCTION(BlueprintCallable, Category="Track") FTransform GetStartTransform() const;
	UFUNCTION(BlueprintCallable, Category="Track") TArray<FTransform> GetCheckpoints() const;
	/** Drivable areas M cycles through: 0 practice lot, 1 high-speed loop, 2 mountain pass, 3 turn ladder. */
	static constexpr int32 NumAreas = 4;
	FTransform GetAreaStart(int32 Index, FString& OutName) const;
	/** On the turn ladder: the next corner in the direction the car faces ("RIGHT 35 deg in 60 m"). */
	bool GetLadderHint(const FVector& WorldPos, const FVector& WorldFwd, FString& OutText) const;
	/** Height of the road centre (local, cm) at a distance along the spline. */
	float RoadHeightAt(float Distance) const;
	/** Graphics quality 0 low .. 3 epic: scenery draw distances and shadows. */
	void ApplyQuality(int32 Level);

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> TerrainMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RoadMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> WallMesh;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> TreeTrunks;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> TreeCrowns;
	/** Marker posts along the high-speed loop and cones round the practice lot: things to see rush past. */
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Posts;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> PostTops;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Cones;
	/** Mixed forest, bushes and rocks (several tree kinds and colours). */
	FDriftScenery Scenery;

	void FillSpline();
	void Build();
	void Tint(UPrimitiveComponent* Comp, const FLinearColor& Color);
};
