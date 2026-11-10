#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

class AActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Shared roadside scenery for every map: several kinds of tree (pines in two greens, leafy trees in
 * three greens plus autumn colours, birches, dead snags), bushes and rocks. Each kind is one instanced
 * mesh, so a whole forest costs a handful of draw calls. ApplyQuality sets draw distances and shadows.
 */
struct FDriftScenery
{
	UInstancedStaticMeshComponent* Trunk = nullptr;      // brown bark
	UInstancedStaticMeshComponent* BirchTrunk = nullptr; // white bark
	UInstancedStaticMeshComponent* DeadTrunk = nullptr;  // grey, bare
	UInstancedStaticMeshComponent* PineDark = nullptr;
	UInstancedStaticMeshComponent* PineBlue = nullptr;
	UInstancedStaticMeshComponent* LeafDark = nullptr;
	UInstancedStaticMeshComponent* LeafMid = nullptr;
	UInstancedStaticMeshComponent* LeafLight = nullptr;
	UInstancedStaticMeshComponent* LeafAutumn = nullptr;
	UInstancedStaticMeshComponent* LeafYellow = nullptr;
	UInstancedStaticMeshComponent* Bush = nullptr;
	UInstancedStaticMeshComponent* Rock = nullptr;

	/** Call from the owner's constructor. Prefix keeps the component names unique per owner. */
	void Create(AActor* Owner, const TCHAR* Prefix);
	/** Colours (call at runtime, e.g. in Build: dynamic materials aren't saved). */
	void Paint(AActor* Owner);
	void Clear();

	/**
	 * A tree standing on Ground. Conifer = chance of a pine (else a leafy tree or birch).
	 * Autumn = share of leafy trees in autumn colours.
	 */
	void AddTree(FRandomStream& Rng, const FVector& Ground, float Conifer, float Autumn, float SizeScale = 1.f);
	void AddBush(FRandomStream& Rng, const FVector& Ground, float SizeScale = 1.f);
	/** A boulder, partly buried. Size ~ diameter in cm. */
	void AddRock(FRandomStream& Rng, const FVector& Ground, float Size);

	/** 0 low .. 3 epic: draw distances and which kinds cast shadows. */
	void ApplyQuality(int32 Level);

	TArray<UInstancedStaticMeshComponent*> All() const;
};

namespace DriftSceneryUtil
{
	/** Engine basic shapes (loaded once, in a constructor). */
	UStaticMesh* Cylinder();
	UStaticMesh* Cone();
	UStaticMesh* Sphere();
	UStaticMesh* Cube();
	/** Applies a flat M_Color tint to a component (runtime). */
	void Tint(AActor* Owner, class UPrimitiveComponent* Comp, const FLinearColor& Color, float Roughness = 0.8f);
	/** Instanced mesh with no collision, attached to the owner's root. Call from a constructor. */
	UInstancedStaticMeshComponent* MakeISM(AActor* Owner, const FName& Name, UStaticMesh* Mesh);
	/** Draw distance (cm) for scenery at a quality level, from a base distance at medium. */
	float CullFor(int32 Level, float MediumCm);
}
