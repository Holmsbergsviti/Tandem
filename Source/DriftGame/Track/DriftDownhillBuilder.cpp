#include "Track/DriftDownhillBuilder.h"
#include "Util/ProcGeo.h"
#include "UI/DriftGraphicsSettings.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

namespace DriftDownhillLocal
{
	// Road centre line in metres {x, y, height}, designed offline: starts at the top heading +X, ends at
	// the bottom (height 0) heading +X. No two parts of the road come closer than 50 m.
	const FVector RoadPoints[] = {
		{0.0,0.0,225.8},{7.8,0.0,225.6},{15.6,0.0,225.4},{23.3,0.0,225.3},{31.1,0.0,225.1},{38.9,0.0,225.0},{46.7,0.0,224.8},{54.4,0.0,224.7},
		{62.2,0.0,224.5},{70.0,0.0,224.4},{77.8,0.0,224.2},{85.6,0.0,223.9},{93.3,0.0,223.7},{101.1,0.0,223.3},{108.9,0.0,222.9},{116.7,0.0,222.5},
		{124.4,0.0,222.0},{132.2,0.0,221.5},{140.0,0.0,221.0},{147.3,-0.4,220.4},{154.6,-1.5,219.8},{161.6,-3.4,219.2},{168.5,-6.1,218.5},{175.0,-9.4,217.9},
		{181.5,-13.1,217.2},{188.0,-16.9,216.5},{194.5,-20.6,215.9},{201.0,-24.4,215.2},{207.5,-28.1,214.5},{214.0,-31.9,213.8},{220.5,-35.6,213.2},{227.0,-39.4,212.5},
		{233.7,-42.7,211.9},{240.9,-45.0,211.2},{248.3,-46.4,210.6},{255.8,-46.7,210.0},{263.3,-46.0,209.3},{270.7,-44.3,208.7},{277.7,-41.6,208.1},{285.0,-38.2,207.4},
		{292.2,-34.8,206.6},{299.5,-31.5,205.9},{306.7,-28.1,205.2},{314.0,-24.7,204.4},{321.2,-21.3,203.7},{328.5,-17.9,202.9},{335.7,-14.5,202.2},{343.0,-11.2,201.5},
		{350.2,-7.8,200.8},{357.5,-4.8,200.1},{365.0,-2.4,199.4},{372.6,-0.7,198.7},{380.4,0.3,198.1},{388.2,0.6,197.4},{395.7,0.6,196.7},{403.2,0.6,196.0},
		{410.7,0.6,195.4},{418.2,0.6,194.7},{425.7,0.6,194.1},{433.2,0.6,193.4},{440.7,0.6,192.8},{448.2,0.6,192.3},{456.0,1.9,191.7},{462.9,5.4,191.2},
		{468.5,11.0,190.7},{472.0,17.9,190.3},{473.2,25.6,189.8},{472.0,33.4,189.4},{468.5,40.3,188.9},{462.9,45.9,188.5},{456.0,49.4,187.9},{448.2,50.6,187.4},
		{440.3,50.6,186.8},{432.4,50.6,186.2},{424.5,50.6,185.5},{416.6,50.6,184.8},{408.7,50.6,184.1},{400.7,50.6,183.4},{392.8,50.6,182.6},{384.9,50.6,181.8},
		{377.0,50.6,181.1},{369.1,50.6,180.3},{361.2,50.6,179.6},{353.2,50.6,178.8},{345.3,50.6,178.1},{337.4,50.6,177.3},{329.5,50.6,176.6},{321.6,50.6,175.8},
		{313.7,50.6,175.1},{305.7,50.6,174.3},{297.8,50.6,173.6},{289.9,50.6,172.9},{282.0,50.6,172.2},{274.1,50.6,171.5},{266.2,50.6,170.9},{258.2,50.6,170.3},
		{250.5,51.9,169.7},{243.6,55.4,169.2},{238.0,61.0,168.7},{234.5,67.9,168.3},{233.2,75.6,167.8},{234.5,83.4,167.4},{238.0,90.3,167.0},{243.6,95.9,166.5},
		{250.5,99.4,165.9},{258.2,100.6,165.4},{266.1,100.6,164.8},{274.0,100.6,164.2},{281.8,100.6,163.5},{289.7,100.6,162.8},{297.5,100.6,162.1},{305.4,100.6,161.4},
		{313.2,100.6,160.6},{321.1,100.6,159.9},{329.0,100.6,159.2},{336.8,100.6,158.4},{344.7,100.6,157.7},{352.5,100.6,156.9},{360.4,100.6,156.2},{368.2,100.6,155.4},
		{376.1,100.6,154.7},{384.0,100.6,153.9},{391.8,100.6,153.2},{399.7,100.6,152.4},{407.5,100.6,151.7},{415.4,100.6,150.9},{423.2,100.6,150.2},{431.1,100.6,149.4},
		{439.0,100.6,148.7},{446.8,100.6,148.0},{454.7,100.6,147.3},{462.5,100.6,146.7},{470.4,100.6,146.0},{478.2,100.6,145.5},{486.0,101.9,144.9},{492.9,105.4,144.4},
		{498.5,111.0,143.9},{502.0,117.9,143.4},{503.2,125.6,143.0},{502.0,133.4,142.6},{498.5,140.3,142.1},{492.9,145.9,141.6},{486.0,149.4,141.1},{478.2,150.6,140.5},
		{470.2,150.6,139.9},{462.1,150.6,139.3},{454.0,150.6,138.6},{445.9,150.6,137.9},{437.8,150.6,137.2},{429.7,150.6,136.4},{421.6,150.6,135.7},{413.5,150.6,134.9},
		{405.4,150.6,134.1},{397.3,150.6,133.3},{389.2,150.6,132.6},{381.1,150.6,131.8},{373.0,150.6,131.0},{364.9,150.6,130.3},{356.8,150.6,129.5},{348.7,150.6,128.7},
		{340.6,150.6,128.0},{332.5,150.6,127.3},{324.4,150.6,126.6},{316.3,150.6,126.0},{308.2,150.6,125.4},{300.5,151.9,124.8},{293.6,155.4,124.3},{288.0,161.0,123.8},
		{284.5,167.9,123.4},{283.2,175.6,122.9},{284.5,183.4,122.5},{288.0,190.3,122.0},{293.6,195.9,121.6},{300.5,199.4,121.0},{308.2,200.6,120.5},{316.6,200.6,119.9},
		{324.9,200.6,119.2},{333.2,200.6,118.5},{341.6,200.6,117.8},{349.9,200.6,117.0},{358.2,200.6,116.2},{366.6,200.6,115.4},{374.9,200.6,114.7},{383.2,200.6,113.9},
		{391.6,200.6,113.1},{399.9,200.6,112.4},{408.2,200.6,111.7},{416.6,201.1,110.9},{424.9,202.4,110.2},{433.0,204.6,109.5},{440.8,207.6,108.9},{448.2,211.4,108.2},
		{455.7,215.2,107.5},{463.5,218.2,106.8},{471.6,220.3,106.1},{479.9,221.6,105.4},{488.2,222.1,104.7},{496.2,222.1,104.0},{504.2,222.1,103.3},{512.2,222.1,102.6},
		{520.2,222.1,101.8},{528.2,222.1,101.1},{536.2,222.1,100.3},{544.2,222.1,99.6},{552.2,222.1,98.9},{560.2,222.1,98.2},{568.2,222.1,97.5},{576.3,222.9,96.8},
		{584.1,225.4,96.2},{591.2,229.3,95.5},{597.3,234.6,94.8},{602.3,241.1,94.2},{605.8,248.4,93.5},{608.4,255.5,92.9},{611.0,262.5,92.2},{613.5,269.5,91.6},
		{616.1,276.6,90.9},{618.7,283.6,90.2},{621.2,290.7,89.6},{623.8,297.7,88.9},{626.4,304.8,88.3},{629.9,312.3,87.6},{634.7,319.0,86.9},{640.7,324.7,86.3},
		{647.7,329.2,85.6},{655.3,332.4,85.0},{663.4,334.1,84.3},{671.7,334.3,83.7},{679.9,333.0,83.0},{687.7,330.2,82.3},{695.1,326.7,81.6},{702.5,323.3,80.9},
		{709.9,319.8,80.2},{717.3,316.3,79.5},{724.7,312.9,78.7},{732.2,309.4,77.9},{739.6,306.0,77.2},{747.0,302.5,76.4},{754.4,299.1,75.6},{761.8,295.6,74.8},
		{769.2,292.1,74.0},{776.1,289.2,73.3},{783.1,286.7,72.6},{790.3,284.6,71.9},{797.6,283.0,71.2},{805.0,281.8,70.5},{812.5,281.1,69.8},{819.9,280.9,69.1},
		{827.7,280.9,68.3},{835.5,280.9,67.6},{843.3,280.9,66.9},{851.1,280.9,66.1},{858.8,280.9,65.4},{866.6,280.9,64.6},{874.4,280.9,63.9},{882.2,280.9,63.2},
		{889.9,280.9,62.4},{897.7,280.9,61.7},{905.5,280.9,60.9},{913.3,280.9,60.2},{921.1,280.9,59.5},{928.8,280.9,58.8},{936.6,280.9,58.1},{944.4,280.9,57.4},
		{952.2,280.9,56.8},{959.9,280.9,56.2},{967.7,281.9,55.7},{974.9,284.9,55.2},{981.2,289.7,54.7},{985.9,295.9,54.2},{988.9,303.1,53.7},{989.9,310.9,53.2},
		{988.9,318.7,52.7},{985.9,325.9,52.1},{982.2,332.4,51.6},{978.4,338.9,51.0},{974.7,345.4,50.3},{970.9,351.9,49.7},{967.2,358.4,49.1},{963.4,364.9,48.4},
		{959.7,371.4,47.8},{955.9,377.9,47.1},{952.7,385.3,46.4},{951.3,393.3,45.8},{951.8,401.4,45.1},{954.1,409.2,44.5},{958.2,416.3,43.9},{963.7,422.2,43.2},
		{970.5,426.6,42.6},{978.2,429.4,41.9},{986.2,430.4,41.3},{994.6,430.4,40.6},{1002.9,430.4,39.8},{1011.2,430.4,39.1},{1019.6,430.4,38.3},{1027.9,430.4,37.6},
		{1036.2,430.4,36.8},{1044.6,430.4,36.0},{1052.9,430.4,35.2},{1061.2,430.4,34.4},{1069.6,430.4,33.7},{1077.9,430.4,32.9},{1086.2,430.4,32.2},{1094.1,430.9,31.6},
		{1101.8,432.4,30.9},{1109.2,434.9,30.2},{1116.2,438.4,29.6},{1122.8,442.8,29.0},{1128.7,447.9,28.4},{1134.6,453.1,27.7},{1141.1,457.5,27.1},{1148.1,460.9,26.5},
		{1155.6,463.5,25.8},{1163.3,465.0,25.2},{1171.1,465.5,24.5},{1179.1,465.5,23.8},{1187.1,465.5,23.1},{1195.1,465.5,22.4},{1203.1,465.5,21.6},{1211.1,465.5,20.9},
		{1219.1,465.5,20.1},{1227.1,465.5,19.4},{1235.1,465.5,18.6},{1243.1,465.5,17.8},{1251.1,465.5,17.1},{1259.1,465.5,16.3},{1267.1,465.5,15.6},{1275.1,465.5,14.9},
		{1283.1,465.5,14.1},{1291.1,465.5,13.5},{1298.5,466.1,12.8},{1305.8,467.7,12.2},{1312.8,470.5,11.6},{1319.3,474.2,11.0},{1325.1,478.9,10.4},{1330.2,484.3,9.8},
		{1334.4,490.5,9.2},{1338.6,496.7,8.6},{1343.7,502.2,8.1},{1349.5,506.8,7.5},{1356.0,510.6,6.9},{1362.9,513.3,6.3},{1370.2,514.9,5.7},{1377.7,515.5,5.2},
		{1385.9,515.5,4.7},{1394.1,515.5,4.2},{1402.2,515.5,3.8},{1410.4,515.5,3.4},{1418.6,515.5,3.0},{1426.8,515.5,2.7},{1435.0,515.5,2.5},{1443.1,515.5,2.3},
		{1451.3,515.5,2.1},{1459.5,515.5,2.0},{1467.7,515.5,1.8},{1475.9,515.5,1.6},{1484.1,515.5,1.5},{1492.2,515.5,1.3},{1500.4,515.5,1.1},{1508.6,515.5,1.0},
		{1516.8,515.5,0.8},{1525.0,515.5,0.7},{1533.1,515.5,0.5},{1541.3,515.5,0.3},{1549.5,515.5,0.2},{1557.7,515.5,0.0}
	};
	constexpr int32 NumPoints = static_cast<int32>(UE_ARRAY_COUNT(RoadPoints));

	constexpr float HalfW = 400.f;        // two narrow lanes
	constexpr float Shoulder = 150.f;     // gravel to the guardrail
	constexpr float RailOff = HalfW + Shoulder;
	constexpr float PadHalf = 2200.f;     // flat pads at both ends, 44 m square
	constexpr float PadLen = 4500.f;
	constexpr float Margin = 45000.f;     // terrain this far round the road
	constexpr float Cell = 1500.f;

	FVector P(int32 i) { return RoadPoints[FMath::Clamp(i, 0, NumPoints - 1)] * 100.f; }

	float Hash(int32 X, int32 Y)
	{
		uint32 H = static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return static_cast<float>(H & 0xFFFFu) / 65535.f;
	}
	float Noise(float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X), Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0, FY = Y - Y0;
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
		return FMath::Lerp(FMath::Lerp(Hash(X0, Y0), Hash(X0 + 1, Y0), SX), FMath::Lerp(Hash(X0, Y0 + 1), Hash(X0 + 1, Y0 + 1), SX), SY);
	}
	FVector2D Right(const FVector& Dir) { const FVector2D D = FVector2D(Dir.X, Dir.Y).GetSafeNormal(); return FVector2D(-D.Y, D.X); }

	/** The two pads: top (behind the start) and bottom (past the end), as local rectangles with a height. */
	struct FPad { FBox2D Rect; float Z; };
	FPad TopPad() { const FVector S = P(0); return { FBox2D(FVector2D(S.X - PadLen, S.Y - PadHalf), FVector2D(S.X, S.Y + PadHalf)), static_cast<float>(S.Z) }; }
	FPad BottomPad() { const FVector E = P(NumPoints - 1); return { FBox2D(FVector2D(E.X, E.Y - PadHalf), FVector2D(E.X + PadLen, E.Y + PadHalf)), static_cast<float>(E.Z) }; }
}

ADriftDownhillBuilder::ADriftDownhillBuilder()
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
	RoadMesh = MakeMesh(TEXT("DownRoad"));
	RoadMesh->bUseComplexAsSimpleCollision = true;
	RoadMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RoadMesh->SetCastShadow(false);
	TerrainMesh = MakeMesh(TEXT("DownTerrain"));
	TerrainMesh->bUseComplexAsSimpleCollision = true;
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RailMesh = MakeMesh(TEXT("DownRails"));
	RailMesh->bUseComplexAsSimpleCollision = false; // solid blocks: a thin rail would let a fast car through
	RailMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RailPosts = DriftSceneryUtil::MakeISM(this, TEXT("DownRailPosts"), DriftSceneryUtil::Cube());
	Scenery.Create(this, TEXT("Down"));
}

void ADriftDownhillBuilder::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ADriftDownhillBuilder::BuildCentre()
{
	using namespace DriftDownhillLocal;
	// Catmull-Rom through the designed points, three steps per span: smooth hairpins without kinks.
	Centre.Reset();
	for (int32 i = 0; i + 1 < NumPoints; ++i)
	{
		const FVector P0 = P(i - 1), P1 = P(i), P2 = P(i + 1), P3 = P(i + 2);
		for (int32 k = 0; k < 3; ++k)
		{
			const float T = k / 3.f, T2 = T * T, T3 = T2 * T;
			Centre.Add(0.5f * ((2.f * P1) + (-P0 + P2) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3));
		}
	}
	Centre.Add(P(NumPoints - 1));
}

void ADriftDownhillBuilder::Build()
{
	using namespace DriftDownhillLocal;
	if (!RoadMesh || !TerrainMesh || !RailMesh) return;
	BuildCentre();
	RoadMesh->ClearAllMeshSections();
	TerrainMesh->ClearAllMeshSections();
	RailMesh->ClearAllMeshSections();
	RailPosts->ClearInstances();
	Scenery.Clear();
	const int32 N = Centre.Num();

	auto Dir = [&](int32 i) { return (Centre[FMath::Min(i + 1, N - 1)] - Centre[FMath::Max(i - 1, 0)]).GetSafeNormal(); };
	auto Edge = [&](float Off, float DZ)
	{
		TArray<FVector> Out;
		Out.Reserve(N);
		for (int32 i = 0; i < N; ++i) Out.Add(Centre[i] + FVector(Right(Dir(i)) * Off, DZ));
		return Out;
	};

	// ---- Road section 0: asphalt and the two pads ----
	FProcGeo Asphalt;
	Asphalt.Ribbon(Edge(HalfW, 3.f), Edge(-HalfW, 3.f), [](int32 i)
	{
		const float V = 0.03f + 0.006f * FMath::Sin(i * 0.37f) + 0.005f * FMath::Sin(i * 1.73f);
		return FLinearColor(V, V, V * 1.08f);
	});
	for (const FPad& Pad : { TopPad(), BottomPad() })
	{
		const FBox2D& R = Pad.Rect;
		const FLinearColor C(0.034f, 0.034f, 0.037f);
		Asphalt.Quad(Asphalt.AddVert(FVector(R.Min.X, R.Min.Y, Pad.Z + 3.f), C), Asphalt.AddVert(FVector(R.Min.X, R.Max.Y, Pad.Z + 3.f), C),
			Asphalt.AddVert(FVector(R.Max.X, R.Max.Y, Pad.Z + 3.f), C), Asphalt.AddVert(FVector(R.Max.X, R.Min.Y, Pad.Z + 3.f), C));
	}
	Asphalt.Commit(RoadMesh, 0, true);

	// ---- Road section 1: gravel shoulders, and skirts down to the ground under their edges ----
	FProcGeo Gravel;
	auto GravelC = [](int32 i) { const float V = 0.8f + 0.2f * FMath::Sin(i * 0.53f); return FLinearColor(0.11f * V, 0.1f * V, 0.085f * V); };
	Gravel.Ribbon(Edge(RailOff + 40.f, 0.f), Edge(HalfW, 2.5f), GravelC);
	Gravel.Ribbon(Edge(-HalfW, 2.5f), Edge(-RailOff - 40.f, 0.f), GravelC);
	Gravel.Ribbon(Edge(RailOff + 40.f, -80.f), Edge(RailOff + 40.f, 0.f), GravelC);   // right skirt, faces out
	Gravel.Ribbon(Edge(-RailOff - 40.f, 0.f), Edge(-RailOff - 40.f, -80.f), GravelC); // left skirt, faces out
	Gravel.Commit(RoadMesh, 1, true);

	// ---- Road section 2: markings (white edge lines, dashed centre line) ----
	FProcGeo Lines;
	const FLinearColor LineC(0.9f, 0.9f, 0.88f);
	Lines.Ribbon(Edge(HalfW - 30.f, 3.6f), Edge(HalfW - 45.f, 3.6f), [&](int32) { return LineC; });
	Lines.Ribbon(Edge(-HalfW + 45.f, 3.6f), Edge(-HalfW + 30.f, 3.6f), [&](int32) { return LineC; });
	{
		const TArray<FVector> CR = Edge(7.f, 3.6f), CL = Edge(-7.f, 3.6f);
		for (int32 i = 0; i + 1 < N; i += 2)
		{
			if ((i / 2) % 2) continue;
			Lines.Quad(Lines.AddVert(CR[i], LineC), Lines.AddVert(CR[i + 1], LineC), Lines.AddVert(CL[i + 1], LineC), Lines.AddVert(CL[i], LineC));
		}
	}
	Lines.Commit(RoadMesh, 2, false);

	// ---- Guardrails both sides: a steel band on posts, with solid blocks for collision ----
	FProcGeo Rail;
	const FLinearColor Steel(0.5f, 0.52f, 0.54f);
	Rail.Ribbon(Edge(RailOff, 80.f), Edge(RailOff, 45.f), [&](int32) { return Steel; });          // right, faces the road
	Rail.Ribbon(Edge(RailOff + 6.f, 45.f), Edge(RailOff + 6.f, 80.f), [&](int32) { return Steel * 0.7f; }); // right, back
	Rail.Ribbon(Edge(RailOff + 6.f, 80.f), Edge(RailOff, 80.f), [&](int32) { return Steel; });    // right, top
	Rail.Ribbon(Edge(-RailOff, 45.f), Edge(-RailOff, 80.f), [&](int32) { return Steel; });        // left, faces the road
	Rail.Ribbon(Edge(-RailOff - 6.f, 80.f), Edge(-RailOff - 6.f, 45.f), [&](int32) { return Steel * 0.7f; });
	Rail.Ribbon(Edge(-RailOff, 80.f), Edge(-RailOff - 6.f, 80.f), [&](int32) { return Steel; });
	Rail.Commit(RailMesh, 0, false);
	TArray<TArray<FVector>> Blocks;
	for (int32 i = 0; i + 1 < N; i += 3)
	{
		const int32 j = FMath::Min(i + 3, N - 1);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			TArray<FVector>& B = Blocks.AddDefaulted_GetRef();
			for (const int32 k : { i, j })
			{
				const FVector2D R = Right(Dir(k)) * static_cast<float>(Side);
				for (const float Off : { RailOff, RailOff + 100.f })
				{
					B.Add(Centre[k] + FVector(R * Off, -60.f));
					B.Add(Centre[k] + FVector(R * Off, 100.f));
				}
			}
		}
		// Posts every ~4 m (instanced, the blocks above do the stopping).
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const FVector D = Dir(i);
			const FVector Pos = Centre[i] + FVector(Right(D) * (static_cast<float>(Side) * (RailOff + 10.f)), 35.f);
			RailPosts->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f), Pos, FVector(0.1f, 0.12f, 0.9f)));
		}
	}
	RailMesh->SetCollisionConvexMeshes(Blocks);

	// ---- Terrain: follows the road's height near it, mountains further out ----
	FVector2D Min(TNumericLimits<float>::Max()), Max(TNumericLimits<float>::Lowest());
	for (int32 i = 0; i < NumPoints; ++i) { Min = FVector2D::Min(Min, FVector2D(P(i))); Max = FVector2D::Max(Max, FVector2D(P(i))); }
	Min -= FVector2D(Margin, Margin);
	Max += FVector2D(Margin, Margin);
	const int32 NX = FMath::CeilToInt((Max.X - Min.X) / Cell) + 1, NY = FMath::CeilToInt((Max.Y - Min.Y) / Cell) + 1;
	TArray<float> Heights;
	TArray<float> RoadDist;
	Heights.SetNumUninitialized(NX * NY);
	RoadDist.SetNumUninitialized(NX * NY);
	const FPad Pads[2] = { TopPad(), BottomPad() };
	for (int32 y = 0; y < NY; ++y)
	{
		for (int32 x = 0; x < NX; ++x)
		{
			const FVector2D Q(Min.X + x * Cell, Min.Y + y * Cell);
			// Nearest point on the road (segments of the designed line) and a smooth blend of road heights.
			float Best = TNumericLimits<float>::Max(), BestZ = 0.f;
			double WSum = 0.0, ZSum = 0.0;
			for (int32 i = 0; i < NumPoints; ++i)
			{
				const FVector A = P(i);
				const double D2 = FVector2D::DistSquared(Q, FVector2D(A));
				const double W = 1.0 / FMath::Square(D2 * 1e-6 + 1.0); // 1 / (d^2 + 1)^2, d in 10 m
				WSum += W; ZSum += W * A.Z;
				if (i + 1 < NumPoints)
				{
					const FVector B = P(i + 1);
					const FVector2D AB(B.X - A.X, B.Y - A.Y);
					const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(Q - FVector2D(A), AB) / FMath::Max(AB.SizeSquared(), 1.0)), 0.f, 1.f);
					const float D = FVector2D::Distance(Q, FVector2D(A) + AB * T);
					if (D < Best) { Best = D; BestZ = FMath::Lerp(A.Z, B.Z, T); }
				}
			}
			float Z = static_cast<float>(ZSum / WSum);
			// Ridges and valleys away from the road.
			const float Far = FMath::SmoothStep(2500.f, 30000.f, Best);
			Z += Far * (3000.f + 7000.f * Noise(Q.X / 40000.f, Q.Y / 40000.f) + 1500.f * Noise(Q.X / 9000.f + 7.f, Q.Y / 9000.f));
			// Under and right beside the road: just below it, so the asphalt and gravel lie on top.
			const float Under = BestZ - 40.f;
			Z = Best < RailOff + 200.f ? FMath::Min(Z, Under) : FMath::Lerp(Under, Z, FMath::SmoothStep(RailOff + 200.f, RailOff + 1200.f, Best));
			for (const FPad& Pad : Pads)
			{
				if (Pad.Rect.ExpandBy(600.f).IsInside(Q)) Z = FMath::Min(Z, Pad.Z - 40.f);
			}
			Heights[y * NX + x] = Z;
			RoadDist[y * NX + x] = Best;
		}
	}
	FProcGeo Terrain;
	FRandomStream Rng(9090);
	for (int32 y = 0; y < NY; ++y)
	{
		for (int32 x = 0; x < NX; ++x)
		{
			const int32 I = y * NX + x;
			const float HX0 = Heights[y * NX + FMath::Max(x - 1, 0)], HX1 = Heights[y * NX + FMath::Min(x + 1, NX - 1)];
			const float HY0 = Heights[FMath::Max(y - 1, 0) * NX + x], HY1 = Heights[FMath::Min(y + 1, NY - 1) * NX + x];
			const float Slope = FMath::Sqrt(FMath::Square(HX1 - HX0) + FMath::Square(HY1 - HY0)) / (2.f * Cell);
			const FVector2D Q(Min.X + x * Cell, Min.Y + y * Cell);
			const float Var = 0.75f + 0.5f * Noise(Q.X / 2500.f, Q.Y / 2500.f);
			const float Autumn = Noise(Q.X / 12000.f + 3.f, Q.Y / 12000.f);
			const FLinearColor Grass = FMath::Lerp(FLinearColor(0.05f, 0.11f, 0.03f), FLinearColor(0.13f, 0.09f, 0.035f), Autumn * 0.7f) * Var;
			const FLinearColor RockC(0.17f * Var, 0.16f * Var, 0.15f * Var);
			FLinearColor C = FMath::Lerp(Grass, RockC, FMath::SmoothStep(0.5f, 0.85f, Slope));
			C.A = 1.f;
			Terrain.AddVert(FVector(Q, Heights[I]), C);
		}
	}
	for (int32 y = 0; y + 1 < NY; ++y)
	{
		for (int32 x = 0; x + 1 < NX; ++x)
		{
			const int32 I = y * NX + x;
			Terrain.Quad(I, I + NX, I + NX + 1, I + 1); // faces up
		}
	}
	Terrain.Commit(TerrainMesh, 0, true);

	// ---- Forest (pines and autumn leaves), bushes by the rails, boulders on steep ground ----
	auto GroundAt = [&](float GX, float GY)
	{
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(GX), 0, NX - 2), Y0 = FMath::Clamp(FMath::FloorToInt(GY), 0, NY - 2);
		const float FX = GX - X0, FY = GY - Y0;
		const int32 I = Y0 * NX + X0;
		const float H = FMath::Lerp(FMath::Lerp(Heights[I], Heights[I + 1], FX), FMath::Lerp(Heights[I + NX], Heights[I + NX + 1], FX), FY);
		const float D = FMath::Lerp(FMath::Lerp(RoadDist[I], RoadDist[I + 1], FX), FMath::Lerp(RoadDist[I + NX], RoadDist[I + NX + 1], FX), FY);
		const float Steep = FMath::Max(FMath::Abs(Heights[I + 1] - Heights[I]), FMath::Abs(Heights[I + NX] - Heights[I])) / Cell;
		return FVector(H, D, Steep);
	};
	int32 Trees = 0, Rocks = 0, Bushes = 0;
	for (int32 Attempt = 0; Attempt < 20000 && Trees < 2200; ++Attempt)
	{
		const float GX = Rng.FRandRange(0.f, NX - 1.01f), GY = Rng.FRandRange(0.f, NY - 1.01f);
		const FVector G = GroundAt(GX, GY);
		const FVector Pos(Min.X + GX * Cell, Min.Y + GY * Cell, G.X - 15.f);
		if (G.Y < RailOff + 300.f) continue;                            // keep off the road
		if (G.Z > 0.9f)                                                  // cliff: boulders, not trees
		{
			if (Rocks < 500) { Scenery.AddRock(Rng, Pos, Rng.FRandRange(150.f, 500.f)); ++Rocks; }
			continue;
		}
		if (G.Y < RailOff + 900.f && Bushes < 500) { Scenery.AddBush(Rng, Pos); ++Bushes; continue; }
		const float Autumn = Noise(Pos.X / 12000.f + 3.f, Pos.Y / 12000.f);
		Scenery.AddTree(Rng, Pos, 0.55f, 0.25f + 0.6f * Autumn, 1.f);
		++Trees;
	}

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
	RoadMesh->SetMaterial(0, MakeMID(GroundBase, 0.75f, 0.f));
	RoadMesh->SetMaterial(1, MakeMID(GroundBase, 0.95f, 0.f));
	RoadMesh->SetMaterial(2, MakeMID(VMat, 0.5f, 0.f));
	TerrainMesh->SetMaterial(0, MakeMID(GroundBase, 0.95f, 0.f));
	RailMesh->SetMaterial(0, MakeMID(VMat, 0.35f, 0.7f));
	DriftSceneryUtil::Tint(this, RailPosts, FLinearColor(0.35f, 0.36f, 0.37f), 0.5f);
	Scenery.Paint(this);

	ApplyQuality(DriftGraphics::Quality());
}

void ADriftDownhillBuilder::ApplyQuality(int32 Level)
{
	Scenery.ApplyQuality(Level);
	RailPosts->SetCullDistances(0, FMath::RoundToInt(DriftSceneryUtil::CullFor(Level, 15000.f)));
	RailPosts->SetCastShadow(Level >= 2);
	RailMesh->SetCastShadow(Level >= 1);
}

FTransform ADriftDownhillBuilder::GetStartTransform() const
{
	using namespace DriftDownhillLocal;
	const FVector A = P(2), B = P(3);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
	// Left lane (left-hand traffic), on the surface.
	const FVector Pos = A + FVector(Right(B - A) * -200.f, 83.f);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw + Yaw, 0.f), GetActorTransform().TransformPosition(Pos));
}

TArray<FTransform> ADriftDownhillBuilder::GetCheckpoints() const
{
	using namespace DriftDownhillLocal;
	TArray<FTransform> Out;
	for (int32 i = 0; i + 1 < NumPoints; i += 3) // designed points are ~7.8 m apart: about every 23 m
	{
		const FVector A = P(i), B = P(i + 1);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		Out.Add(FTransform(FRotator(0.f, GetActorRotation().Yaw + Yaw, 0.f), GetActorTransform().TransformPosition(A + FVector(0.f, 0.f, 3.f))));
	}
	return Out;
}
