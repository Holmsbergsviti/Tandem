#include "Track/DriftCityBuilder.h"
#include "Util/ProcGeo.h"
#include "UI/DriftGraphicsSettings.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

namespace DriftCityLocal
{
	constexpr int32 NX = 5, NY = 4;              // blocks east-west, north-south
	constexpr float BlockX = 9000.f, BlockY = 7000.f;
	constexpr float Street = 1800.f;             // kerb to kerb
	constexpr float Walk = 350.f;                // sidewalk width
	constexpr float Kerb = 15.f;
	constexpr float PitchX = BlockX + Street, PitchY = BlockY + Street;
	constexpr float TotalX = NX * PitchX + Street, TotalY = NY * PitchY + Street;
	constexpr float HX = TotalX * 0.5f, HY = TotalY * 0.5f;
	constexpr float Margin = 60000.f;            // grass out to here, rising to a low horizon
	constexpr float PaintZ = 0.8f;               // road markings sit this far above the asphalt
	constexpr float LaneOffset = 450.f;          // left-hand traffic: the start and resets use the left lane

	enum class EBlock : uint8 { Buildings, Plaza, CarPark, Park };
	EBlock KindOf(int32 I, int32 J)
	{
		if (I == 2 && J == 1) return EBlock::Plaza;   // open square in the middle: drift space
		if (I == 4 && J == 3) return EBlock::CarPark;
		if (I == 1 && J == 2) return EBlock::Park;
		return EBlock::Buildings;
	}
	float StreetX(int32 I) { return -HX + Street * 0.5f + I * PitchX; } // centre line of north-south street I
	float StreetY(int32 J) { return -HY + Street * 0.5f + J * PitchY; } // centre line of east-west street J
	FBox2D BlockRect(int32 I, int32 J)
	{
		const FVector2D Min(StreetX(I) + Street * 0.5f, StreetY(J) + Street * 0.5f);
		return FBox2D(Min, Min + FVector2D(BlockX, BlockY));
	}

	float Hash(int32 X, int32 Y)
	{
		uint32 H = static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return static_cast<float>(H & 0xFFFFu) / 65535.f;
	}
	FLinearColor Grey(float V) { return FLinearColor(V, V, V * 1.06f); }

	/** Ground height outside the grid: flat near the city, rising gently far out (a horizon). */
	float GroundZ(float X, float Y)
	{
		const float D = FMath::Max3(FMath::Abs(X) - HX, FMath::Abs(Y) - HY, 0.f);
		const float Rise = FMath::SmoothStep(18000.f, Margin, D);
		return Rise * (2600.f + 900.f * FMath::Sin(X * 0.00011f + 1.3f) * FMath::Cos(Y * 0.00013f));
	}

	/** Quad O, O+U, O+U+V, O+V, facing along U x V. */
	void Face(FProcGeo& G, const FVector& O, const FVector& U, const FVector& V, const FLinearColor& C)
	{
		const int32 A = G.AddVert(O, C), B = G.AddVert(O + V, C), D = G.AddVert(O + U + V, C), E = G.AddVert(O + U, C);
		G.Quad(A, B, D, E);
	}
	/** Horizontal rectangle facing up (X0 < X1, Y0 < Y1). */
	void Floor(FProcGeo& G, float X0, float Y0, float X1, float Y1, float Z, const FLinearColor& C)
	{
		Face(G, FVector(X0, Y0, Z), FVector(X1 - X0, 0.f, 0.f), FVector(0.f, Y1 - Y0, 0.f), C);
	}
	/** The four outward sides of an axis-aligned box from Z0 to Z1. */
	void Sides(FProcGeo& G, float X0, float Y0, float X1, float Y1, float Z0, float Z1, const FLinearColor& C)
	{
		const FVector Up(0.f, 0.f, Z1 - Z0);
		Face(G, FVector(X0, Y0, Z0), FVector(X1 - X0, 0.f, 0.f), Up, C); // south (-Y)
		Face(G, FVector(X1, Y1, Z0), FVector(X0 - X1, 0.f, 0.f), Up, C); // north (+Y)
		Face(G, FVector(X1, Y0, Z0), FVector(0.f, Y1 - Y0, 0.f), Up, C); // east (+X)
		Face(G, FVector(X0, Y1, Z0), FVector(0.f, Y0 - Y1, 0.f), Up, C); // west (-X)
	}
	/** Smooth surface over a rectangle with shared vertices, facing up. */
	void Surface(FProcGeo& G, float X0, float Y0, float X1, float Y1, float Cell,
		TFunctionRef<float(float, float)> ZFn, TFunctionRef<FLinearColor(float, float)> ColorFn)
	{
		const int32 NA = FMath::Max(1, FMath::CeilToInt((X1 - X0) / Cell)), NB = FMath::Max(1, FMath::CeilToInt((Y1 - Y0) / Cell));
		const int32 Base = G.Verts.Num();
		for (int32 a = 0; a <= NA; ++a)
		{
			for (int32 b = 0; b <= NB; ++b)
			{
				const float X = FMath::Lerp(X0, X1, static_cast<float>(a) / NA), Y = FMath::Lerp(Y0, Y1, static_cast<float>(b) / NB);
				G.AddVert(FVector(X, Y, ZFn(X, Y)), ColorFn(X, Y));
			}
		}
		auto V = [&](int32 a, int32 b) { return Base + a * (NB + 1) + b; };
		for (int32 a = 0; a < NA; ++a)
		{
			for (int32 b = 0; b < NB; ++b) G.Quad(V(a, b), V(a, b + 1), V(a + 1, b + 1), V(a + 1, b));
		}
	}
	/** Flat ring (annulus) on the ground, for painted circles. */
	void PaintRing(FProcGeo& G, const FVector2D& C, float R, float Width, const FLinearColor& Col)
	{
		const int32 Segs = 64;
		for (int32 k = 0; k < Segs; ++k)
		{
			const float A0 = 2.f * PI * k / Segs, A1 = 2.f * PI * (k + 1) / Segs;
			const FVector I0(C.X + FMath::Cos(A0) * (R - Width), C.Y + FMath::Sin(A0) * (R - Width), PaintZ);
			const FVector O0(C.X + FMath::Cos(A0) * R, C.Y + FMath::Sin(A0) * R, PaintZ);
			const FVector I1(C.X + FMath::Cos(A1) * (R - Width), C.Y + FMath::Sin(A1) * (R - Width), PaintZ);
			const FVector O1(C.X + FMath::Cos(A1) * R, C.Y + FMath::Sin(A1) * R, PaintZ);
			// Counter-clockwise from above: inner, outer, outer next, inner next faces up.
			G.Quad(G.AddVert(O0, Col), G.AddVert(I0, Col), G.AddVert(I1, Col), G.AddVert(O1, Col));
		}
	}

	struct FBuildingStyle { FLinearColor Wall; FLinearColor Glass; FLinearColor Pillar; };
	FBuildingStyle PickStyle(FRandomStream& Rng)
	{
		static const FLinearColor Walls[] = {
			{0.32f, 0.31f, 0.29f}, {0.55f, 0.53f, 0.49f}, {0.30f, 0.085f, 0.05f}, {0.48f, 0.38f, 0.24f},
			{0.10f, 0.11f, 0.13f}, {0.70f, 0.70f, 0.68f}, {0.18f, 0.24f, 0.25f}, {0.42f, 0.2f, 0.12f} };
		static const FLinearColor Glasses[] = { {0.02f, 0.035f, 0.05f}, {0.03f, 0.045f, 0.04f}, {0.012f, 0.014f, 0.02f}, {0.05f, 0.06f, 0.08f} };
		FBuildingStyle S;
		const float Shade = Rng.FRandRange(0.85f, 1.1f);
		S.Wall = Walls[Rng.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Walls)) - 1)] * Shade;
		S.Wall.A = 1.f;
		S.Glass = Glasses[Rng.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Glasses)) - 1)];
		S.Pillar = S.Wall * 0.8f;
		S.Pillar.A = 1.f;
		return S;
	}

	/** One building: shop-front ground floor, window bands per floor, pillars, a roof with plant on it. */
	void AddBuilding(FProcGeo& Walls, FProcGeo& Glass, FProcGeo& Roofs, FRandomStream& Rng, const FBox2D& R, float Base, int32 Floors,
		TArray<TArray<FVector>>& Hulls)
	{
		const FBuildingStyle St = PickStyle(Rng);
		const float GroundH = 450.f, FloorH = 340.f;
		const float Top = Base + GroundH + Floors * FloorH;
		const float X0 = R.Min.X, Y0 = R.Min.Y, X1 = R.Max.X, Y1 = R.Max.Y;

		// Glass bands: the shop windows, then one band per floor. Wall fills everything between.
		TArray<FVector2D> GlassZ;
		GlassZ.Add(FVector2D(Base + 60.f, Base + 380.f));
		for (int32 k = 0; k < Floors; ++k)
		{
			const float Z = Base + GroundH + k * FloorH;
			GlassZ.Add(FVector2D(Z + 110.f, Z + 290.f));
		}
		float Z = Base;
		for (const FVector2D& Band : GlassZ)
		{
			Sides(Walls, X0, Y0, X1, Y1, Z, Band.X, St.Wall);
			Sides(Glass, X0, Y0, X1, Y1, Band.X, Band.Y, St.Glass);
			Z = Band.Y;
		}
		Sides(Walls, X0, Y0, X1, Y1, Z, Top, St.Wall);

		// Pillars, a little proud of the facade, from above the shops to the roof.
		const float PillarW = 45.f, Proud = 6.f, Spacing = Rng.FRandRange(320.f, 480.f);
		const float PZ0 = Base + 380.f, PH = Top - PZ0;
		auto PillarsAlong = [&](const FVector& Start, const FVector& Dir, float Len, const FVector& Out)
		{
			const int32 Count = FMath::FloorToInt(Len / Spacing);
			for (int32 k = 1; k < Count; ++k)
			{
				const FVector P = Start + Dir * (Len * k / Count - PillarW * 0.5f) + Out * Proud + FVector(0.f, 0.f, PZ0);
				Face(Walls, P, Dir * PillarW, FVector(0.f, 0.f, PH), St.Pillar);
			}
		};
		PillarsAlong(FVector(X0, Y0, 0.f), FVector(1.f, 0.f, 0.f), X1 - X0, FVector(0.f, -1.f, 0.f));
		PillarsAlong(FVector(X1, Y1, 0.f), FVector(-1.f, 0.f, 0.f), X1 - X0, FVector(0.f, 1.f, 0.f));
		PillarsAlong(FVector(X1, Y0, 0.f), FVector(0.f, 1.f, 0.f), Y1 - Y0, FVector(1.f, 0.f, 0.f));
		PillarsAlong(FVector(X0, Y1, 0.f), FVector(0.f, -1.f, 0.f), Y1 - Y0, FVector(-1.f, 0.f, 0.f));

		// Roof and a few plant boxes on it.
		Floor(Roofs, X0, Y0, X1, Y1, Top, Grey(0.09f));
		const int32 Units = Rng.RandRange(0, 3);
		for (int32 k = 0; k < Units; ++k)
		{
			const FVector2D Half(Rng.FRandRange(120.f, 300.f), Rng.FRandRange(120.f, 300.f));
			const float HZ = Rng.FRandRange(80.f, 200.f);
			const FVector2D C(Rng.FRandRange(X0 + Half.X + 100.f, FMath::Max(X0 + Half.X + 100.f, X1 - Half.X - 100.f)),
				Rng.FRandRange(Y0 + Half.Y + 100.f, FMath::Max(Y0 + Half.Y + 100.f, Y1 - Half.Y - 100.f)));
			Roofs.AddBox(FVector(C.X, C.Y, Top + HZ), FVector(Half.X, Half.Y, HZ), Grey(Rng.FRandRange(0.25f, 0.45f)));
		}

		TArray<FVector>& Hull = Hulls.AddDefaulted_GetRef();
		for (int32 c = 0; c < 8; ++c)
		{
			Hull.Add(FVector((c & 1) ? X1 : X0, (c & 2) ? Y1 : Y0, (c & 4) ? Top : Base - Kerb));
		}
	}
}

ADriftCityBuilder::ADriftCityBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	// Everything static (spawned once, never moves): static children need a static parent to attach.
	USceneComponent* RootC = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootC->SetMobility(EComponentMobility::Static);
	SetRootComponent(RootC);

	auto MakeMesh = [&](const TCHAR* Name)
	{
		UProceduralMeshComponent* M = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		M->SetupAttachment(RootC);
		M->SetMobility(EComponentMobility::Static);
		return M;
	};
	GroundMesh = MakeMesh(TEXT("CityGround"));
	GroundMesh->bUseComplexAsSimpleCollision = true; // the wheels ride on the real surface, kerbs included
	GroundMesh->SetCollisionProfileName(TEXT("BlockAll"));
	GroundMesh->SetCastShadow(false);
	BuildingMesh = MakeMesh(TEXT("CityBuildings"));
	BuildingMesh->bUseComplexAsSimpleCollision = false; // solid boxes (see Build), not a thin shell
	BuildingMesh->SetCollisionProfileName(TEXT("BlockAll"));
	SkylineMesh = MakeMesh(TEXT("CitySkyline"));
	SkylineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkylineMesh->SetCastShadow(false);

	LampPoles = DriftSceneryUtil::MakeISM(this, TEXT("CityLampPoles"), DriftSceneryUtil::Cylinder());
	LampPoles->SetCollisionProfileName(TEXT("BlockAll")); // you can hit a lamp post
	LampHeads = DriftSceneryUtil::MakeISM(this, TEXT("CityLampHeads"), DriftSceneryUtil::Cube());
	Cones = DriftSceneryUtil::MakeISM(this, TEXT("CityCones"), DriftSceneryUtil::Cone());
	Scenery.Create(this, TEXT("City"));
}

void ADriftCityBuilder::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ADriftCityBuilder::Build()
{
	using namespace DriftCityLocal;
	if (!GroundMesh || !BuildingMesh || !SkylineMesh) return;
	GroundMesh->ClearAllMeshSections();
	BuildingMesh->ClearAllMeshSections();
	SkylineMesh->ClearAllMeshSections();
	LampPoles->ClearInstances();
	LampHeads->ClearInstances();
	Cones->ClearInstances();
	Scenery.Clear();
	FRandomStream Rng(4242);

	// ---- Ground section 0: asphalt over the whole grid (streets, plaza, car park) ----
	FProcGeo Asphalt;
	Surface(Asphalt, -HX, -HY, HX, HY, 2000.f, [](float, float) { return 0.f; }, [](float X, float Y)
	{
		const float N = Hash(FMath::FloorToInt(X / 700.f), FMath::FloorToInt(Y / 700.f));
		return Grey(0.026f + 0.01f * N);
	});
	Asphalt.Commit(GroundMesh, 0, true);

	// ---- Ground section 1: grass all round, rising to a low horizon; the park lawn ----
	FProcGeo Grass;
	auto GrassColor = [](float X, float Y)
	{
		const float V = 0.8f + 0.35f * Hash(FMath::FloorToInt(X / 1500.f), FMath::FloorToInt(Y / 1500.f));
		return FLinearColor(0.05f * V, 0.11f * V, 0.03f * V);
	};
	const float OX = HX + Margin, OY = HY + Margin;
	auto GrassZ = [](float X, float Y) { return GroundZ(X, Y); };
	Surface(Grass, -OX, -OY, OX, -HY, 2500.f, GrassZ, GrassColor); // south
	Surface(Grass, -OX, HY, OX, OY, 2500.f, GrassZ, GrassColor);   // north
	Surface(Grass, -OX, -HY, -HX, HY, 2500.f, GrassZ, GrassColor); // west
	Surface(Grass, HX, -HY, OX, HY, 2500.f, GrassZ, GrassColor);   // east

	// ---- Blocks ----
	FProcGeo Concrete, Paint, Walls, Glass, Roofs;
	TArray<TArray<FVector>> Hulls;
	const FLinearColor Slab = Grey(0.3f), KerbC = Grey(0.42f), White(0.85f, 0.85f, 0.83f);
	auto AddLamp = [&](const FVector2D& P, float Yaw)
	{
		LampPoles->AddInstance(FTransform(FRotator::ZeroRotator, FVector(P, Kerb + 375.f), FVector(0.14f, 0.14f, 7.5f)));
		const FVector Arm = FRotator(0.f, Yaw, 0.f).Vector();
		LampHeads->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P, Kerb + 745.f) + Arm * 55.f, FVector(1.1f, 0.32f, 0.12f)));
	};
	for (int32 I = 0; I < NX; ++I)
	{
		for (int32 J = 0; J < NY; ++J)
		{
			const FBox2D B = BlockRect(I, J);
			const EBlock Kind = KindOf(I, J);
			const FVector2D Mid = B.GetCenter();

			if (Kind == EBlock::Plaza)
			{
				// Open square at street level: a painted circle to drift round and cones at the corners.
				PaintRing(Paint, Mid, 1600.f, 20.f, White);
				PaintRing(Paint, Mid, 500.f, 20.f, White);
				for (int32 k = 0; k < 4; ++k)
				{
					const FVector2D Corner((k & 1) ? B.Max.X - 400.f : B.Min.X + 400.f, (k & 2) ? B.Max.Y - 400.f : B.Min.Y + 400.f);
					Cones->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Corner, 35.f), FVector(0.45f, 0.45f, 0.7f)));
				}
				for (int32 k = 0; k < 8; ++k)
				{
					const float A = 2.f * PI * k / 8.f;
					Cones->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Mid + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 380.f, 35.f), FVector(0.45f, 0.45f, 0.7f)));
				}
				continue;
			}
			if (Kind == EBlock::CarPark)
			{
				// Painted bays in two double rows, at street level.
				for (int32 Row = 0; Row < 2; ++Row)
				{
					const float YC = FMath::Lerp(B.Min.Y, B.Max.Y, Row == 0 ? 0.3f : 0.7f);
					Floor(Paint, B.Min.X + 600.f, YC - 8.f, B.Max.X - 600.f, YC + 8.f, PaintZ, White);
					for (float X = B.Min.X + 600.f; X <= B.Max.X - 600.f; X += 260.f)
					{
						Floor(Paint, X - 7.f, YC - 500.f, X + 7.f, YC + 500.f, PaintZ, White);
					}
				}
				continue;
			}

			// Raised block: slab with a kerb all round.
			Floor(Concrete, B.Min.X, B.Min.Y, B.Max.X, B.Max.Y, Kerb, Slab);
			Sides(Concrete, B.Min.X, B.Min.Y, B.Max.X, B.Max.Y, 0.f, Kerb, KerbC);

			// Street lights along the sidewalk, arms over the road; small trees between them.
			const float LampIn = 60.f;
			auto Edge = [&](const FVector2D& From, const FVector2D& To, float ArmYaw, const FVector2D& Inward)
			{
				const float Len = FVector2D::Distance(From, To);
				const FVector2D Dir = (To - From) / Len;
				for (float D = 800.f; D <= Len - 800.f; D += 2600.f)
				{
					AddLamp(From + Dir * D + Inward * LampIn, ArmYaw);
					if (Kind == EBlock::Buildings && D + 1300.f <= Len - 800.f)
					{
						Scenery.AddTree(Rng, FVector(From + Dir * (D + 1300.f) + Inward * (Walk * 0.5f), Kerb), 0.f, 0.15f, 0.55f);
					}
				}
			};
			Edge(FVector2D(B.Min.X, B.Min.Y), FVector2D(B.Max.X, B.Min.Y), -90.f, FVector2D(0.f, 1.f));  // south side
			Edge(FVector2D(B.Max.X, B.Max.Y), FVector2D(B.Min.X, B.Max.Y), 90.f, FVector2D(0.f, -1.f));  // north side
			Edge(FVector2D(B.Max.X, B.Min.Y), FVector2D(B.Max.X, B.Max.Y), 0.f, FVector2D(-1.f, 0.f));   // east side
			Edge(FVector2D(B.Min.X, B.Max.Y), FVector2D(B.Min.X, B.Min.Y), 180.f, FVector2D(1.f, 0.f));  // west side

			const FBox2D Inner(B.Min + FVector2D(Walk, Walk), B.Max - FVector2D(Walk, Walk));
			if (Kind == EBlock::Park)
			{
				// Lawn with two crossing paths, trees and bushes.
				Surface(Grass, Inner.Min.X, Inner.Min.Y, Inner.Max.X, Inner.Max.Y, 1500.f, [](float, float) { return Kerb + 1.f; }, GrassColor);
				Floor(Concrete, Inner.Min.X, Mid.Y - 150.f, Inner.Max.X, Mid.Y + 150.f, Kerb + 2.f, Grey(0.36f));
				Floor(Concrete, Mid.X - 150.f, Inner.Min.Y, Mid.X + 150.f, Inner.Max.Y, Kerb + 2.5f, Grey(0.36f));
				for (int32 k = 0; k < 140; ++k)
				{
					const FVector2D P(Rng.FRandRange(Inner.Min.X + 300.f, Inner.Max.X - 300.f), Rng.FRandRange(Inner.Min.Y + 300.f, Inner.Max.Y - 300.f));
					if (FMath::Abs(P.X - Mid.X) < 500.f || FMath::Abs(P.Y - Mid.Y) < 500.f) continue; // keep the paths clear
					if (k % 3 == 0) Scenery.AddBush(Rng, FVector(P, Kerb));
					else Scenery.AddTree(Rng, FVector(P, Kerb), 0.25f, 0.3f, 1.f);
				}
				continue;
			}

			// Buildings: the block split into lots, taller towards the middle of town.
			const float Central = 1.f - FMath::Clamp(static_cast<float>(Mid.Size()) / HX, 0.f, 1.f);
			const int32 LotsX = Rng.RandRange(1, 3), LotsY = Rng.RandRange(1, 2);
			for (int32 a = 0; a < LotsX; ++a)
			{
				for (int32 b = 0; b < LotsY; ++b)
				{
					const float LX0 = FMath::Lerp(Inner.Min.X, Inner.Max.X, static_cast<float>(a) / LotsX);
					const float LX1 = FMath::Lerp(Inner.Min.X, Inner.Max.X, static_cast<float>(a + 1) / LotsX);
					const float LY0 = FMath::Lerp(Inner.Min.Y, Inner.Max.Y, static_cast<float>(b) / LotsY);
					const float LY1 = FMath::Lerp(Inner.Min.Y, Inner.Max.Y, static_cast<float>(b + 1) / LotsY);
					const float G = Rng.FRandRange(60.f, 260.f);
					const FBox2D Lot(FVector2D(LX0 + G, LY0 + G), FVector2D(LX1 - G, LY1 - G));
					const int32 Lo = 2 + FMath::RoundToInt(5.f * Central), Hi = 5 + FMath::RoundToInt(15.f * Central);
					AddBuilding(Walls, Glass, Roofs, Rng, Lot, Kerb, Rng.RandRange(Lo, Hi), Hulls);
				}
			}
		}
	}
	Concrete.Commit(GroundMesh, 2, true);
	Grass.Commit(GroundMesh, 1, true);

	// ---- Road markings: dashed centre lines and zebra crossings at every junction ----
	auto Dashes = [&](const FVector2D& From, const FVector2D& To)
	{
		const float Len = FVector2D::Distance(From, To);
		const FVector2D Dir = (To - From) / Len, Side(-Dir.Y, Dir.X);
		for (float D = 0.f; D + 300.f <= Len; D += 600.f)
		{
			const FVector2D A = From + Dir * D, B = From + Dir * (D + 300.f);
			const FVector2D Lo(FMath::Min(A.X, B.X) - FMath::Abs(Side.X) * 8.f, FMath::Min(A.Y, B.Y) - FMath::Abs(Side.Y) * 8.f);
			const FVector2D Hi(FMath::Max(A.X, B.X) + FMath::Abs(Side.X) * 8.f, FMath::Max(A.Y, B.Y) + FMath::Abs(Side.Y) * 8.f);
			Floor(Paint, Lo.X, Lo.Y, Hi.X, Hi.Y, PaintZ, White);
		}
	};
	const float Clear = Street * 0.5f + 700.f; // markings stop short of the junctions
	for (int32 J = 0; J <= NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const float Y = StreetY(J), XA = StreetX(I) + Clear, XB = StreetX(I + 1) - Clear;
			Dashes(FVector2D(XA, Y), FVector2D(XB, Y));
			for (int32 End = 0; End < 2; ++End) // zebra: stripes along the street, across its width
			{
				const float X0 = End == 0 ? StreetX(I) + Street * 0.5f + 150.f : StreetX(I + 1) - Street * 0.5f - 450.f;
				for (float S = -Street * 0.5f + 150.f; S < Street * 0.5f - 100.f; S += 110.f)
				{
					Floor(Paint, X0, Y + S, X0 + 300.f, Y + S + 55.f, PaintZ, White);
				}
			}
		}
	}
	for (int32 I = 0; I <= NX; ++I)
	{
		for (int32 J = 0; J < NY; ++J)
		{
			const float X = StreetX(I), YA = StreetY(J) + Clear, YB = StreetY(J + 1) - Clear;
			Dashes(FVector2D(X, YA), FVector2D(X, YB));
			for (int32 End = 0; End < 2; ++End)
			{
				const float Y0 = End == 0 ? StreetY(J) + Street * 0.5f + 150.f : StreetY(J + 1) - Street * 0.5f - 450.f;
				for (float S = -Street * 0.5f + 150.f; S < Street * 0.5f - 100.f; S += 110.f)
				{
					Floor(Paint, X + S, Y0, X + S + 55.f, Y0 + 300.f, PaintZ, White);
				}
			}
		}
	}
	Paint.Commit(GroundMesh, 3, false);

	Walls.Commit(BuildingMesh, 0, false);
	Glass.Commit(BuildingMesh, 1, false);
	Roofs.Commit(BuildingMesh, 2, false);
	BuildingMesh->SetCollisionConvexMeshes(Hulls); // all at once: each separate add re-cooks every hull

	// ---- Skyline: plain towers far out across the fields ----
	FProcGeo Sky;
	for (int32 k = 0, Tries = 0; k < 70 && Tries < 2000; ++Tries)
	{
		const float X = Rng.FRandRange(-OX + 8000.f, OX - 8000.f), Y = Rng.FRandRange(-OY + 8000.f, OY - 8000.f);
		const float D = FMath::Max(FMath::Abs(X) - HX, FMath::Abs(Y) - HY);
		if (D < 26000.f || D > 46000.f) continue;
		const float WX = Rng.FRandRange(1200.f, 3000.f), WY = Rng.FRandRange(1200.f, 3000.f), H = Rng.FRandRange(3500.f, 14000.f);
		const float Z0 = GroundZ(X, Y) - 300.f;
		const FLinearColor C = Grey(Rng.FRandRange(0.12f, 0.22f)) * FLinearColor(0.92f, 0.97f, 1.1f);
		Sides(Sky, X - WX, Y - WY, X + WX, Y + WY, Z0, Z0 + H, C);
		Floor(Sky, X - WX, Y - WY, X + WX, Y + WY, Z0 + H, C);
		++k;
	}
	Sky.Commit(SkylineMesh, 0, false);

	// ---- Materials (runtime: dynamic materials aren't saved) ----
	UMaterialInterface* VMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_VertexColor.M_VertexColor"));
	UMaterialInterface* GMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Ground.M_Ground"));
	auto MakeMID = [&](UMaterialInterface* Base, float Rough, float Metal) -> UMaterialInterface*
	{
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetScalarParameterValue(TEXT("Roughness"), Rough);
		MID->SetScalarParameterValue(TEXT("Metallic"), Metal);
		MID->SetScalarParameterValue(TEXT("Emissive"), 0.f);
		return MID;
	};
	UMaterialInterface* GroundBase = GMat ? GMat : VMat;
	GroundMesh->SetMaterial(0, MakeMID(GroundBase, 0.75f, 0.f));
	GroundMesh->SetMaterial(1, MakeMID(GroundBase, 0.95f, 0.f));
	GroundMesh->SetMaterial(2, MakeMID(VMat, 0.85f, 0.f));
	GroundMesh->SetMaterial(3, MakeMID(VMat, 0.5f, 0.f));
	BuildingMesh->SetMaterial(0, MakeMID(VMat, 0.85f, 0.f));
	BuildingMesh->SetMaterial(1, MakeMID(VMat, 0.08f, 0.6f)); // glass: shiny, reflects the sky
	BuildingMesh->SetMaterial(2, MakeMID(VMat, 0.9f, 0.f));
	SkylineMesh->SetMaterial(0, MakeMID(VMat, 0.9f, 0.f));
	DriftSceneryUtil::Tint(this, LampPoles, Grey(0.2f), 0.5f);
	DriftSceneryUtil::Tint(this, LampHeads, Grey(0.12f), 0.4f);
	DriftSceneryUtil::Tint(this, Cones, FLinearColor(0.9f, 0.25f, 0.02f), 0.6f);
	Scenery.Paint(this);

	ApplyQuality(DriftGraphics::Quality());
}

void ADriftCityBuilder::ApplyQuality(int32 Level)
{
	Scenery.ApplyQuality(Level);
	const int32 LampCull = FMath::RoundToInt(DriftSceneryUtil::CullFor(Level, 30000.f));
	LampPoles->SetCullDistances(0, LampCull);
	LampHeads->SetCullDistances(0, LampCull);
	Cones->SetCullDistances(0, FMath::RoundToInt(DriftSceneryUtil::CullFor(Level, 20000.f)));
	LampPoles->SetCastShadow(Level >= 2);
	LampHeads->SetCastShadow(Level >= 2);
	Cones->SetCastShadow(Level >= 2);
	BuildingMesh->SetCastShadow(Level >= 1);
}

FTransform ADriftCityBuilder::GetStartTransform() const
{
	using namespace DriftCityLocal;
	const FVector P(StreetX(0) + 1500.f, StreetY(2) - LaneOffset, 83.f);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), GetActorTransform().TransformPosition(P));
}

TArray<FTransform> ADriftCityBuilder::GetCheckpoints() const
{
	using namespace DriftCityLocal;
	TArray<FTransform> Out;
	const FTransform& TM = GetActorTransform();
	const float Yaw = GetActorRotation().Yaw;
	for (int32 J = 0; J <= NY; ++J)
	{
		for (float X = -HX + 1000.f; X <= HX - 1000.f; X += 5000.f)
		{
			Out.Add(FTransform(FRotator(0.f, Yaw, 0.f), TM.TransformPosition(FVector(X, StreetY(J) - LaneOffset, 3.f))));
		}
	}
	for (int32 I = 0; I <= NX; ++I)
	{
		for (float Y = -HY + 1000.f; Y <= HY - 1000.f; Y += 5000.f)
		{
			Out.Add(FTransform(FRotator(0.f, Yaw + 90.f, 0.f), TM.TransformPosition(FVector(StreetX(I) + LaneOffset, Y, 3.f))));
		}
	}
	return Out;
}
