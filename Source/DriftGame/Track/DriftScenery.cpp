#include "Track/DriftScenery.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace DriftSceneryUtil
{
	UStaticMesh* Cylinder()
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> F(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		return F.Object;
	}
	UStaticMesh* Cone()
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> F(TEXT("/Engine/BasicShapes/Cone.Cone"));
		return F.Object;
	}
	UStaticMesh* Sphere()
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> F(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		return F.Object;
	}
	UStaticMesh* Cube()
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> F(TEXT("/Engine/BasicShapes/Cube.Cube"));
		return F.Object;
	}

	void Tint(AActor* Owner, UPrimitiveComponent* Comp, const FLinearColor& Color, float Roughness)
	{
		if (!Comp) return;
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Color.M_Color")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Owner);
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			Comp->SetMaterial(0, MID);
		}
	}

	UInstancedStaticMeshComponent* MakeISM(AActor* Owner, const FName& Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Owner->GetRootComponent());
		C->SetStaticMesh(Mesh);
		C->SetMobility(EComponentMobility::Static);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	}

	float CullFor(int32 Level, float MediumCm)
	{
		static const float Mult[4] = { 0.55f, 1.f, 1.5f, 2.2f };
		return MediumCm * Mult[FMath::Clamp(Level, 0, 3)];
	}
}

void FDriftScenery::Create(AActor* Owner, const TCHAR* Prefix)
{
	auto Make = [&](const TCHAR* Name, UStaticMesh* Mesh)
	{
		return DriftSceneryUtil::MakeISM(Owner, FName(*FString::Printf(TEXT("%s%s"), Prefix, Name)), Mesh);
	};
	Trunk      = Make(TEXT("Trunk"), DriftSceneryUtil::Cylinder());
	BirchTrunk = Make(TEXT("BirchTrunk"), DriftSceneryUtil::Cylinder());
	DeadTrunk  = Make(TEXT("DeadTrunk"), DriftSceneryUtil::Cylinder());
	PineDark   = Make(TEXT("PineDark"), DriftSceneryUtil::Cone());
	PineBlue   = Make(TEXT("PineBlue"), DriftSceneryUtil::Cone());
	LeafDark   = Make(TEXT("LeafDark"), DriftSceneryUtil::Sphere());
	LeafMid    = Make(TEXT("LeafMid"), DriftSceneryUtil::Sphere());
	LeafLight  = Make(TEXT("LeafLight"), DriftSceneryUtil::Sphere());
	LeafAutumn = Make(TEXT("LeafAutumn"), DriftSceneryUtil::Sphere());
	LeafYellow = Make(TEXT("LeafYellow"), DriftSceneryUtil::Sphere());
	Bush       = Make(TEXT("Bush"), DriftSceneryUtil::Sphere());
	Rock       = Make(TEXT("Rock"), DriftSceneryUtil::Sphere());
	ApplyQuality(1);
}

TArray<UInstancedStaticMeshComponent*> FDriftScenery::All() const
{
	return { Trunk, BirchTrunk, DeadTrunk, PineDark, PineBlue, LeafDark, LeafMid, LeafLight, LeafAutumn, LeafYellow, Bush, Rock };
}

void FDriftScenery::Paint(AActor* Owner)
{
	using DriftSceneryUtil::Tint;
	Tint(Owner, Trunk,      FLinearColor(0.12f, 0.07f, 0.035f), 0.9f);
	Tint(Owner, BirchTrunk, FLinearColor(0.72f, 0.71f, 0.66f), 0.8f);
	Tint(Owner, DeadTrunk,  FLinearColor(0.22f, 0.2f, 0.18f), 0.9f);
	Tint(Owner, PineDark,   FLinearColor(0.018f, 0.065f, 0.022f), 0.85f);
	Tint(Owner, PineBlue,   FLinearColor(0.025f, 0.075f, 0.055f), 0.85f);
	Tint(Owner, LeafDark,   FLinearColor(0.03f, 0.085f, 0.02f), 0.85f);
	Tint(Owner, LeafMid,    FLinearColor(0.06f, 0.14f, 0.03f), 0.85f);
	Tint(Owner, LeafLight,  FLinearColor(0.12f, 0.2f, 0.04f), 0.85f);
	Tint(Owner, LeafAutumn, FLinearColor(0.42f, 0.1f, 0.02f), 0.85f);
	Tint(Owner, LeafYellow, FLinearColor(0.48f, 0.3f, 0.03f), 0.85f);
	Tint(Owner, Bush,       FLinearColor(0.035f, 0.075f, 0.022f), 0.9f);
	Tint(Owner, Rock,       FLinearColor(0.2f, 0.19f, 0.17f), 0.95f);
}

void FDriftScenery::Clear()
{
	for (UInstancedStaticMeshComponent* C : All()) if (C) C->ClearInstances();
}

void FDriftScenery::AddTree(FRandomStream& Rng, const FVector& Ground, float Conifer, float Autumn, float SizeScale)
{
	if (!Trunk) return;
	const float S = Rng.FRandRange(0.75f, 1.6f) * SizeScale;
	const float Yaw = Rng.FRandRange(0.f, 360.f);
	const float Kind = Rng.FRand();
	if (Kind < 0.03f)
	{
		// Dead snag: a bare grey trunk, leaning a little, with one broken branch.
		const float H = 520.f * S;
		const FRotator Lean(Rng.FRandRange(-6.f, 6.f), Yaw, Rng.FRandRange(-6.f, 6.f));
		DeadTrunk->AddInstance(FTransform(Lean, Ground + FVector(0.f, 0.f, H * 0.5f - 30.f), FVector(0.3f * S, 0.3f * S, H / 100.f)));
		DeadTrunk->AddInstance(FTransform(FRotator(45.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, H * 0.6f), FVector(0.12f * S, 0.12f * S, 1.6f * S)));
		return;
	}
	if (Rng.FRand() < Conifer)
	{
		// Pine or spruce: a trunk and one or two stacked cones.
		UInstancedStaticMeshComponent* Crown = Rng.FRand() < 0.6f ? PineDark : PineBlue;
		const float TrunkH = 260.f * S;
		Trunk->AddInstance(FTransform(FRotator::ZeroRotator, Ground + FVector(0.f, 0.f, TrunkH * 0.5f - 20.f), FVector(0.3f * S, 0.3f * S, TrunkH / 100.f)));
		if (Rng.FRand() < 0.5f)
		{
			const float H = Rng.FRandRange(600.f, 760.f) * S, D = Rng.FRandRange(280.f, 360.f) * S;
			Crown->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, 180.f * S + H * 0.5f), FVector(D / 100.f, D / 100.f, H / 100.f)));
		}
		else
		{
			const float H0 = 460.f * S, D0 = Rng.FRandRange(360.f, 420.f) * S;
			const float H1 = 420.f * S, D1 = D0 * 0.68f;
			Crown->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, 170.f * S + H0 * 0.5f), FVector(D0 / 100.f, D0 / 100.f, H0 / 100.f)));
			Crown->AddInstance(FTransform(FRotator(0.f, Yaw + 30.f, 0.f), Ground + FVector(0.f, 0.f, 440.f * S + H1 * 0.5f), FVector(D1 / 100.f, D1 / 100.f, H1 / 100.f)));
		}
		return;
	}
	if (Kind > 0.85f)
	{
		// Birch: thin white trunk, small light crown.
		const float H = Rng.FRandRange(560.f, 700.f) * S;
		BirchTrunk->AddInstance(FTransform(FRotator(Rng.FRandRange(-3.f, 3.f), Yaw, 0.f), Ground + FVector(0.f, 0.f, H * 0.5f - 20.f), FVector(0.2f * S, 0.2f * S, H / 100.f)));
		UInstancedStaticMeshComponent* Leaves = Rng.FRand() < Autumn ? LeafYellow : LeafLight;
		Leaves->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, H * 0.78f), FVector(2.3f * S, 2.1f * S, 3.6f * S)));
		return;
	}
	// Leafy tree: trunk and a crown of one to three overlapping blobs.
	UInstancedStaticMeshComponent* Leaves = LeafMid;
	const float C = Rng.FRand();
	if (Rng.FRand() < Autumn) Leaves = C < 0.5f ? LeafAutumn : LeafYellow;
	else Leaves = C < 0.4f ? LeafDark : (C < 0.8f ? LeafMid : LeafLight);
	const float TrunkH = 330.f * S;
	Trunk->AddInstance(FTransform(FRotator::ZeroRotator, Ground + FVector(0.f, 0.f, TrunkH * 0.5f - 20.f), FVector(0.36f * S, 0.36f * S, TrunkH / 100.f)));
	const float D = Rng.FRandRange(380.f, 480.f) * S;
	Leaves->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, TrunkH + D * 0.3f), FVector(D / 100.f, D / 100.f, D * 0.85f / 100.f)));
	const int32 Extra = Rng.RandRange(0, 2);
	for (int32 k = 0; k < Extra; ++k)
	{
		const float A = FMath::DegreesToRadians(Yaw + 140.f * k);
		const FVector Off(FMath::Cos(A) * D * 0.35f, FMath::Sin(A) * D * 0.35f, Rng.FRandRange(-0.1f, 0.25f) * D);
		const float D2 = D * Rng.FRandRange(0.55f, 0.75f);
		Leaves->AddInstance(FTransform(FRotator::ZeroRotator, Ground + FVector(0.f, 0.f, TrunkH + D * 0.3f) + Off, FVector(D2 / 100.f, D2 / 100.f, D2 * 0.85f / 100.f)));
	}
}

void FDriftScenery::AddBush(FRandomStream& Rng, const FVector& Ground, float SizeScale)
{
	if (!Bush) return;
	const float S = Rng.FRandRange(0.7f, 1.5f) * SizeScale;
	const float Yaw = Rng.FRandRange(0.f, 360.f);
	Bush->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, 30.f * S), FVector(1.7f * S, 1.3f * S, 0.95f * S)));
	if (Rng.FRand() < 0.5f)
	{
		const float A = FMath::DegreesToRadians(Yaw);
		Bush->AddInstance(FTransform(FRotator(0.f, Yaw + 50.f, 0.f), Ground + FVector(FMath::Cos(A) * 90.f * S, FMath::Sin(A) * 90.f * S, 20.f * S), FVector(1.1f * S, 1.f * S, 0.7f * S)));
	}
}

void FDriftScenery::AddRock(FRandomStream& Rng, const FVector& Ground, float Size)
{
	if (!Rock) return;
	const FVector Scale(Size / 100.f * Rng.FRandRange(0.8f, 1.3f), Size / 100.f * Rng.FRandRange(0.6f, 1.1f), Size / 100.f * Rng.FRandRange(0.35f, 0.7f));
	const FRotator Rot(Rng.FRandRange(-15.f, 15.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-15.f, 15.f));
	Rock->AddInstance(FTransform(Rot, Ground + FVector(0.f, 0.f, Scale.Z * 100.f * 0.2f), Scale));
}

void FDriftScenery::ApplyQuality(int32 Level)
{
	const int32 TreeCull = FMath::RoundToInt(DriftSceneryUtil::CullFor(Level, 40000.f));
	const int32 SmallCull = FMath::RoundToInt(DriftSceneryUtil::CullFor(Level, 18000.f));
	const TArray<UInstancedStaticMeshComponent*> Small = { Bush, Rock };
	for (UInstancedStaticMeshComponent* C : All())
	{
		if (!C) continue;
		const bool bSmall = Small.Contains(C);
		C->SetCullDistances(0, bSmall ? SmallCull : TreeCull);
		C->SetCastShadow(bSmall ? Level >= 2 : Level >= 1);
	}
}
