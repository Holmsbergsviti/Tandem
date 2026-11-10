#include "Track/DriftCircuitBuilder.h"
#include "Util/ProcGeo.h"
#include "UI/DriftGraphicsSettings.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

namespace DriftCircuitLocal
{
	// Centre lines in metres {x, y, height}, every 8 m, closed. Designed as straights and arcs, closed by
	// adjusting the straights, and checked for tightest radius (20 m), gaps between road parts and grade.
	const FVector ParkPoints[] = {
		{0.0,-0.0,3.9},{8.0,0.0,4.0},{16.0,0.0,4.2},{24.0,0.0,4.3},{32.0,0.0,4.4},{40.0,0.0,4.6},{48.0,0.0,4.7},{56.0,0.0,4.8},
		{64.0,0.0,4.9},{72.0,0.0,5.0},{80.0,0.0,5.1},{88.0,0.0,5.3},{96.0,0.0,5.3},{104.0,0.0,5.4},{111.9,-0.9,5.5},{119.3,-3.9,5.6},
		{125.6,-8.8,5.7},{130.5,-15.1,5.7},{133.5,-22.5,5.8},{134.5,-30.4,5.8},{135.1,-38.4,5.9},{135.7,-46.4,5.9},{136.3,-54.4,6.0},{136.9,-62.4,6.0},
		{137.5,-70.3,6.0},{138.1,-78.3,6.0},{140.4,-85.9,6.0},{145.4,-92.2,6.0},{152.2,-96.3,6.0},{159.5,-99.5,6.0},{165.4,-104.8,5.9},{169.4,-111.7,5.9},
		{171.0,-119.4,5.8},{170.1,-127.4,5.8},{166.8,-134.6,5.7},{162.8,-141.5,5.7},{158.8,-148.5,5.6},{154.8,-155.4,5.5},{150.8,-162.3,5.4},{146.8,-169.2,5.3},
		{142.8,-176.2,5.2},{138.8,-183.1,5.1},{134.8,-190.0,5.0},{130.8,-197.0,4.9},{127.1,-204.0,4.8},{124.5,-211.6,4.7},{123.4,-219.5,4.6},{123.7,-227.5,4.4},
		{125.3,-235.3,4.3},{127.9,-242.9,4.2},{128.9,-250.8,4.0},{128.0,-258.7,3.9},{125.5,-266.3,3.7},{121.2,-273.1,3.6},{115.6,-278.7,3.4},{108.8,-282.9,3.3},
		{101.2,-285.4,3.1},{93.3,-286.2,3.0},{85.4,-285.2,2.8},{77.7,-282.8,2.7},{70.1,-280.5,2.5},{62.4,-278.1,2.4},{54.8,-275.8,2.3},{47.1,-273.4,2.1},
		{39.3,-271.8,2.0},{31.5,-273.1,1.8},{24.8,-277.4,1.7},{20.4,-284.0,1.6},{18.7,-291.8,1.4},{16.3,-299.4,1.3},{11.6,-305.8,1.2},{5.0,-310.3,1.1},
		{-2.7,-312.4,1.0},{-10.6,-311.8,0.8},{-17.9,-308.7,0.7},{-23.9,-303.4,0.7},{-28.1,-296.6,0.6},{-32.1,-289.7,0.5},{-36.1,-282.8,0.4},{-40.1,-275.8,0.3},
		{-44.1,-268.9,0.3},{-48.1,-262.0,0.2},{-52.1,-255.0,0.2},{-56.1,-248.1,0.1},{-60.1,-241.2,0.1},{-64.1,-234.3,0.0},{-68.1,-227.3,0.0},{-72.2,-220.5,0.0},
		{-77.2,-214.2,0.0},{-82.9,-208.6,0.0},{-89.3,-203.9,0.0},{-96.3,-200.0,0.0},{-103.1,-195.7,0.0},{-108.8,-190.2,0.1},{-113.4,-183.6,0.1},{-116.5,-176.3,0.2},
		{-118.1,-168.5,0.2},{-118.2,-160.5,0.3},{-116.6,-152.7,0.3},{-113.6,-145.3,0.4},{-109.1,-138.7,0.5},{-103.4,-133.1,0.6},{-96.8,-128.5,0.7},{-90.2,-124.0,0.8},
		{-83.6,-119.5,0.9},{-77.0,-115.0,1.0},{-70.7,-110.1,1.1},{-65.8,-103.8,1.2},{-63.0,-96.3,1.3},{-62.4,-88.3,1.4},{-64.0,-80.5,1.6},{-67.8,-73.5,1.7},
		{-72.2,-66.8,1.8},{-74.6,-59.2,2.0},{-74.9,-51.2,2.1},{-73.1,-43.5,2.3},{-69.3,-36.4,2.4},{-63.8,-30.7,2.6},{-57.0,-26.5,2.7},{-49.5,-23.8,2.9},
		{-42.4,-20.2,3.0},{-35.7,-15.7,3.2},{-29.1,-11.2,3.3},{-22.5,-6.7,3.4},{-15.5,-2.9,3.6},{-7.9,-0.6,3.7},
	};
	const FVector RingPoints[] = {
		{0.0,-0.0,11.4},{8.0,0.0,11.6},{16.0,0.0,11.8},{24.0,0.0,12.1},{32.0,0.0,12.3},{40.0,0.0,12.5},{48.0,0.0,12.7},{56.0,0.0,12.9},
		{64.0,0.0,13.1},{72.0,0.0,13.2},{80.0,0.0,13.4},{88.0,0.0,13.5},{96.0,0.0,13.7},{104.0,0.0,13.8},{112.0,0.0,14.0},{120.0,0.0,14.1},
		{128.0,0.0,14.2},{136.0,0.0,14.3},{144.0,0.0,14.4},{152.0,0.0,14.4},{160.0,0.0,14.5},{168.0,0.0,14.6},{176.0,0.0,14.6},{184.0,0.0,14.7},
		{192.0,-0.1,14.7},{200.0,-0.7,14.7},{207.8,-2.1,14.7},{215.6,-4.2,14.7},{223.1,-6.9,14.7},{230.3,-10.3,14.7},{237.2,-14.3,14.7},{243.8,-19.0,14.7},
		{249.8,-24.2,14.6},{255.4,-29.9,14.6},{260.5,-36.1,14.6},{265.0,-42.7,14.5},{268.9,-49.6,14.4},{272.2,-56.9,14.4},{274.8,-64.5,14.3},{276.7,-72.3,14.2},
		{277.9,-80.2,14.2},{278.4,-88.2,14.1},{278.4,-96.2,14.0},{278.4,-104.2,13.9},{278.5,-112.2,13.8},{278.5,-120.2,13.7},{278.5,-128.2,13.6},{278.5,-136.2,13.5},
		{278.5,-144.2,13.4},{278.5,-152.2,13.3},{278.5,-160.2,13.2},{278.5,-168.2,13.1},{278.5,-176.2,13.0},{278.5,-184.2,12.9},{278.5,-192.2,12.8},{278.5,-200.2,12.7},
		{278.5,-208.2,12.6},{278.5,-216.2,12.6},{278.5,-224.2,12.5},{278.5,-232.2,12.4},{278.5,-240.2,12.3},{279.1,-248.1,12.2},{280.7,-256.0,12.1},{283.1,-263.6,12.1},
		{286.4,-270.9,12.0},{290.5,-277.7,11.9},{295.4,-284.1,11.8},{300.9,-289.8,11.8},{307.0,-295.1,11.7},{312.7,-300.7,11.7},{317.9,-306.7,11.6},{322.8,-313.1,11.6},
		{327.1,-319.8,11.5},{331.0,-326.8,11.5},{334.3,-334.1,11.5},{337.1,-341.6,11.4},{339.4,-349.2,11.4},{341.1,-357.0,11.4},{342.2,-365.0,11.4},{342.7,-372.9,11.4},
		{342.7,-380.9,11.4},{342.0,-388.9,11.4},{340.8,-396.8,11.4},{339.1,-404.6,11.4},{336.7,-412.3,11.4},{333.8,-419.7,11.4},{330.4,-427.0,11.4},{326.5,-433.9,11.4},
		{322.0,-440.6,11.4},{317.1,-446.9,11.4},{311.8,-452.8,11.5},{306.0,-458.4,11.5},{299.8,-463.5,11.5},{293.3,-468.1,11.5},{286.5,-472.3,11.6},{279.4,-475.9,11.6},
		{272.0,-479.1,11.6},{264.5,-481.6,11.6},{256.7,-483.7,11.7},{248.9,-485.1,11.7},{240.9,-486.0,11.7},{232.9,-486.3,11.7},{224.9,-486.3,11.7},{216.9,-486.3,11.8},
		{208.9,-486.3,11.8},{200.9,-486.3,11.8},{192.9,-486.3,11.8},{184.9,-486.3,11.8},{176.9,-486.3,11.8},{168.9,-486.3,11.8},{160.9,-486.3,11.8},{152.9,-486.3,11.7},
		{144.9,-486.3,11.7},{136.9,-486.3,11.7},{128.9,-486.3,11.7},{120.9,-486.3,11.6},{112.9,-486.3,11.6},{104.9,-486.3,11.5},{96.9,-486.3,11.5},{88.9,-486.3,11.4},
		{80.9,-486.3,11.3},{72.9,-486.3,11.2},{64.9,-486.3,11.2},{56.9,-486.3,11.1},{48.9,-486.2,11.0},{40.9,-485.8,10.9},{33.0,-484.9,10.7},{25.1,-483.6,10.6},
		{17.3,-481.8,10.5},{9.6,-479.6,10.3},{2.1,-477.0,10.2},{-5.3,-473.9,10.0},{-12.5,-470.4,9.9},{-19.5,-466.5,9.7},{-26.3,-462.2,9.5},{-32.7,-457.5,9.4},
		{-39.0,-452.5,9.2},{-44.9,-447.1,9.0},{-50.5,-441.4,8.8},{-55.7,-435.4,8.6},{-60.7,-429.1,8.4},{-65.2,-422.5,8.2},{-69.4,-415.7,7.9},{-73.8,-409.0,7.7},
		{-78.8,-402.8,7.5},{-84.4,-397.1,7.3},{-90.6,-392.0,7.0},{-97.3,-387.6,6.8},{-104.3,-383.8,6.5},{-111.7,-380.8,6.3},{-119.4,-378.5,6.1},{-127.3,-377.0,5.8},
		{-135.2,-376.3,5.6},{-143.2,-376.2,5.3},{-151.2,-375.4,5.1},{-159.1,-374.1,4.8},{-166.8,-372.1,4.6},{-174.4,-369.5,4.3},{-181.7,-366.3,4.1},{-188.8,-362.6,3.9},
		{-195.5,-358.3,3.6},{-201.9,-353.4,3.4},{-207.9,-348.1,3.2},{-213.4,-342.3,2.9},{-218.4,-336.1,2.7},{-222.9,-329.5,2.5},{-226.9,-322.6,2.3},{-230.3,-315.3,2.1},
		{-233.2,-307.9,1.9},{-235.4,-300.2,1.7},{-237.0,-292.3,1.5},{-237.9,-284.4,1.4},{-238.3,-276.4,1.2},{-238.0,-268.4,1.1},{-237.0,-260.5,0.9},{-235.5,-252.6,0.8},
		{-233.3,-244.9,0.6},{-230.6,-237.4,0.5},{-227.8,-229.9,0.4},{-225.1,-222.4,0.3},{-222.4,-214.8,0.3},{-220.5,-207.1,0.2},{-219.6,-199.1,0.1},{-219.9,-191.1,0.1},
		{-221.1,-183.3,0.0},{-223.4,-175.6,0.0},{-226.7,-168.3,0.0},{-231.0,-161.5,0.0},{-236.1,-155.4,0.0},{-242.0,-150.0,0.0},{-247.8,-144.5,0.1},{-253.0,-138.4,0.1},
		{-257.7,-131.9,0.2},{-261.7,-125.0,0.3},{-265.1,-117.8,0.3},{-267.8,-110.2,0.4},{-269.7,-102.5,0.6},{-270.9,-94.6,0.7},{-271.4,-86.6,0.8},{-271.1,-78.6,0.9},
		{-270.1,-70.7,1.1},{-268.3,-62.9,1.3},{-265.8,-55.3,1.4},{-262.6,-48.0,1.6},{-258.7,-41.0,1.8},{-254.2,-34.4,2.0},{-249.1,-28.2,2.2},{-243.4,-22.6,2.5},
		{-237.2,-17.5,2.7},{-230.6,-13.0,2.9},{-223.6,-9.2,3.2},{-216.2,-6.0,3.4},{-208.6,-3.6,3.7},{-200.8,-1.9,4.0},{-192.9,-0.9,4.2},{-184.9,-0.6,4.5},
		{-176.9,-1.2,4.8},{-169.0,-2.4,5.1},{-161.1,-3.8,5.4},{-153.2,-5.2,5.7},{-145.4,-6.6,6.0},{-137.5,-7.9,6.3},{-129.6,-9.3,6.6},{-121.7,-10.7,6.9},
		{-113.9,-12.1,7.2},{-106.0,-13.5,7.5},{-98.0,-14.5,7.8},{-90.1,-15.0,8.1},{-82.1,-15.0,8.4},{-74.1,-14.4,8.6},{-66.2,-13.3,8.9},{-58.3,-11.7,9.2},
		{-50.6,-9.6,9.5},{-43.1,-7.0,9.8},{-35.4,-4.6,10.1},{-27.6,-2.8,10.3},{-19.8,-1.4,10.6},{-11.8,-0.4,10.9},{-3.8,-0.0,11.1},
	};
	const FVector SnakePoints[] = {
		{0.0,-0.0,18.4},{8.0,0.0,18.6},{16.0,0.0,18.7},{24.0,0.0,18.8},{32.0,0.0,18.9},{40.0,0.0,19.0},{47.9,-0.8,19.1},{55.2,-4.0,19.1},
		{61.1,-9.4,19.1},{65.0,-16.3,19.1},{66.9,-24.1,19.0},{70.2,-31.3,19.0},{75.7,-37.1,18.9},{82.7,-40.9,18.9},{90.5,-42.3,18.8},{98.4,-43.4,18.7},
		{105.5,-47.0,18.6},{111.2,-52.6,18.5},{114.8,-59.7,18.4},{116.6,-67.5,18.3},{120.2,-74.6,18.2},{125.9,-80.1,18.0},{133.1,-83.6,17.9},{141.0,-84.7,17.9},
		{148.9,-85.6,17.8},{156.5,-88.1,17.7},{163.5,-92.0,17.6},{169.5,-97.2,17.6},{174.4,-103.5,17.5},{177.9,-110.7,17.5},{179.9,-118.4,17.5},{180.4,-126.4,17.5},
		{179.2,-134.3,17.5},{176.5,-141.8,17.5},{173.0,-149.0,17.6},{169.5,-156.2,17.7},{167.7,-163.9,17.8},{169.0,-171.8,17.9},{173.3,-178.4,18.0},{179.9,-182.9,18.1},
		{187.5,-185.2,18.3},{194.2,-189.5,18.4},{199.0,-195.9,18.6},{201.1,-203.6,18.8},{200.4,-211.5,19.0},{197.6,-218.9,19.2},{197.1,-226.9,19.4},{199.8,-234.3,19.7},
		{205.2,-240.2,19.9},{212.4,-243.5,20.1},{220.0,-246.0,20.4},{226.6,-250.4,20.6},{231.9,-256.4,20.8},{235.4,-263.6,21.0},{236.8,-271.4,21.3},{236.2,-279.4,21.5},
		{233.5,-286.8,21.6},{228.8,-293.4,21.8},{222.7,-298.4,22.0},{215.4,-301.7,22.1},{207.5,-303.0,22.3},{199.6,-303.8,22.4},{191.6,-304.6,22.4},{183.6,-305.4,22.5},
		{175.7,-306.2,22.5},{167.7,-307.0,22.5},{159.8,-307.8,22.5},{151.8,-308.6,22.4},{143.8,-309.4,22.3},{135.9,-310.2,22.2},{127.9,-311.0,22.1},{120.1,-312.7,21.9},
		{112.9,-316.0,21.7},{106.6,-320.9,21.4},{101.6,-327.2,21.1},{98.1,-334.3,20.8},{95.6,-341.9,20.4},{91.4,-348.7,20.0},{85.5,-354.1,19.6},{78.4,-357.8,19.1},
		{70.6,-359.4,18.7},{62.9,-361.3,18.1},{55.7,-364.9,17.6},{49.6,-370.0,17.0},{44.8,-376.3,16.4},{41.5,-383.6,15.8},{39.0,-391.2,15.2},{35.1,-398.2,14.6},
		{29.9,-404.2,13.9},{23.6,-409.1,13.2},{16.5,-412.7,12.5},{8.8,-414.8,11.9},{0.8,-415.3,11.2},{-7.1,-414.2,10.5},{-14.6,-411.5,9.8},{-21.5,-407.4,9.1},
		{-27.4,-402.1,8.4},{-32.1,-395.6,7.7},{-36.2,-388.8,7.1},{-42.3,-383.6,6.4},{-49.7,-380.9,5.8},{-57.7,-381.0,5.2},{-65.1,-384.0,4.6},{-72.7,-386.2,4.0},
		{-80.7,-385.8,3.5},{-88.1,-382.9,3.0},{-94.1,-377.7,2.6},{-98.1,-370.8,2.1},{-99.6,-363.0,1.7},{-98.8,-355.0,1.4},{-97.5,-347.1,1.1},{-96.3,-339.2,0.8},
		{-95.1,-331.3,0.5},{-93.9,-323.4,0.3},{-92.7,-315.5,0.2},{-91.5,-307.6,0.1},{-90.3,-299.7,0.0},{-89.1,-291.8,0.0},{-87.8,-283.9,0.0},{-86.6,-276.0,0.1},
		{-85.4,-268.1,0.2},{-84.2,-260.2,0.4},{-83.0,-252.2,0.5},{-81.8,-244.3,0.8},{-80.6,-236.4,1.1},{-79.4,-228.5,1.4},{-78.1,-220.6,1.7},{-76.9,-212.7,2.1},
		{-75.7,-204.8,2.5},{-74.5,-196.9,2.9},{-73.3,-189.0,3.4},{-72.1,-181.1,3.9},{-70.9,-173.2,4.4},{-69.7,-165.3,4.9},{-68.4,-157.4,5.5},{-67.7,-149.4,6.1},
		{-69.0,-141.5,6.6},{-72.4,-134.3,7.2},{-77.5,-128.2,7.8},{-83.2,-122.6,8.4},{-87.8,-116.0,9.0},{-91.2,-108.8,9.6},{-93.2,-101.1,10.3},{-93.9,-93.1,10.8},
		{-93.1,-85.2,11.4},{-90.9,-77.5,12.0},{-87.4,-70.3,12.6},{-82.7,-63.8,13.1},{-77.0,-58.3,13.7},{-70.3,-53.9,14.2},{-63.6,-49.5,14.7},{-57.6,-44.2,15.1},
		{-52.3,-38.2,15.6},{-47.9,-31.6,16.0},{-44.1,-24.6,16.4},{-39.5,-18.0,16.8},{-33.9,-12.3,17.1},{-27.5,-7.6,17.4},{-20.4,-3.9,17.7},{-12.8,-1.4,18.0},
		{-4.9,-0.2,18.2},
	};

	struct FDef
	{
		const TCHAR* Name;
		const FVector* Pts;
		int32 Num;
		float HalfW;      // asphalt half width, cm
		float Shoulder;   // run-off to the guardrail, cm
		float HillAmp;    // height of the hills away from the road, cm
		float Conifer, Autumn;
		int32 Seed;
	};
	const FDef Defs[ADriftCircuitBuilder::NumLayouts] = {
		{ TEXT("DRIFT PARK"),   ParkPoints,  (int32)UE_ARRAY_COUNT(ParkPoints),  700.f, 600.f, 2500.f, 0.3f, 0.2f, 4141 },
		{ TEXT("SWEEPER RING"), RingPoints,  (int32)UE_ARRAY_COUNT(RingPoints),  800.f, 800.f, 4000.f, 0.5f, 0.1f, 5252 },
		{ TEXT("SNAKE HILL"),   SnakePoints, (int32)UE_ARRAY_COUNT(SnakePoints), 600.f, 500.f, 6000.f, 0.7f, 0.5f, 6363 },
	};
	constexpr float Margin = 30000.f;
	constexpr float Cell = 1500.f;

	const FDef& Def(int32 Layout) { return Defs[FMath::Clamp(Layout, 0, ADriftCircuitBuilder::NumLayouts - 1)]; }
	FVector P(const FDef& D, int32 i) { return D.Pts[((i % D.Num) + D.Num) % D.Num] * 100.f; }

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
}

ADriftCircuitBuilder::ADriftCircuitBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
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
	RoadMesh = MakeMesh(TEXT("CircuitRoad"));
	RoadMesh->bUseComplexAsSimpleCollision = true;
	RoadMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RoadMesh->SetCastShadow(false);
	TerrainMesh = MakeMesh(TEXT("CircuitTerrain"));
	TerrainMesh->bUseComplexAsSimpleCollision = true;
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RailMesh = MakeMesh(TEXT("CircuitRails"));
	RailMesh->bUseComplexAsSimpleCollision = false; // solid blocks: a thin rail would let a fast car through
	RailMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RailPosts = DriftSceneryUtil::MakeISM(this, TEXT("CircuitRailPosts"), DriftSceneryUtil::Cube());
	Scenery.Create(this, TEXT("Circuit"));
}

void ADriftCircuitBuilder::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

FString ADriftCircuitBuilder::GetDisplayName() const { return DriftCircuitLocal::Def(Layout).Name; }

void ADriftCircuitBuilder::Build()
{
	using namespace DriftCircuitLocal;
	if (!RoadMesh || !TerrainMesh || !RailMesh) return;
	const FDef& D = Def(Layout);
	const float HalfW = D.HalfW, RailOff = D.HalfW + D.Shoulder;

	// Closed Catmull-Rom through the designed points, three steps per span.
	Centre.Reset();
	for (int32 i = 0; i < D.Num; ++i)
	{
		const FVector P0 = P(D, i - 1), P1 = P(D, i), P2 = P(D, i + 1), P3 = P(D, i + 2);
		for (int32 k = 0; k < 3; ++k)
		{
			const float T = k / 3.f, T2 = T * T, T3 = T2 * T;
			Centre.Add(0.5f * ((2.f * P1) + (-P0 + P2) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3));
		}
	}
	RoadMesh->ClearAllMeshSections();
	TerrainMesh->ClearAllMeshSections();
	RailMesh->ClearAllMeshSections();
	RailPosts->ClearInstances();
	Scenery.Clear();
	const int32 M = Centre.Num();
	const int32 N = M + 1; // ribbons repeat the first point to close the loop
	auto C = [&](int32 i) { return Centre[((i % M) + M) % M]; };
	auto Dir = [&](int32 i) { return (C(i + 1) - C(i - 1)).GetSafeNormal(); };
	auto Edge = [&](float Off, float DZ)
	{
		TArray<FVector> Out;
		Out.Reserve(N);
		for (int32 i = 0; i < N; ++i) Out.Add(C(i) + FVector(Right(Dir(i)) * Off, DZ));
		return Out;
	};

	// ---- Road section 0: asphalt ----
	FProcGeo Asphalt;
	Asphalt.Ribbon(Edge(HalfW, 3.f), Edge(-HalfW, 3.f), [](int32 i)
	{
		const float V = 0.032f + 0.006f * FMath::Sin(i * 0.37f) + 0.004f * FMath::Sin(i * 1.73f);
		return FLinearColor(V, V, V * 1.06f);
	});
	Asphalt.Commit(RoadMesh, 0, true);

	// ---- Road section 1: run-off gravel and skirts ----
	FProcGeo Gravel;
	auto GravelC = [](int32 i) { const float V = 0.8f + 0.2f * FMath::Sin(i * 0.53f); return FLinearColor(0.12f * V, 0.11f * V, 0.09f * V); };
	Gravel.Ribbon(Edge(RailOff + 40.f, 0.f), Edge(HalfW + 100.f, 2.5f), GravelC);
	Gravel.Ribbon(Edge(-HalfW - 100.f, 2.5f), Edge(-RailOff - 40.f, 0.f), GravelC);
	Gravel.Ribbon(Edge(RailOff + 40.f, -80.f), Edge(RailOff + 40.f, 0.f), GravelC);
	Gravel.Ribbon(Edge(-RailOff - 40.f, 0.f), Edge(-RailOff - 40.f, -80.f), GravelC);
	Gravel.Commit(RoadMesh, 1, true);

	// ---- Road section 2: kerbs (red / white, 1 m wide, every ~2.6 m) and white edge lines ----
	FProcGeo Kerb;
	const FLinearColor Red(0.35f, 0.012f, 0.008f), White(0.8f, 0.8f, 0.78f);
	auto KerbC = [&](int32 i) { return ((i / 1) % 2) ? Red : White; };
	{
		const TArray<FVector> RO = Edge(HalfW + 100.f, 4.f), RI = Edge(HalfW, 4.f), LI = Edge(-HalfW, 4.f), LO = Edge(-HalfW - 100.f, 4.f);
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const FLinearColor KC = KerbC(i);
			Kerb.Quad(Kerb.AddVert(RO[i], KC), Kerb.AddVert(RO[i + 1], KC), Kerb.AddVert(RI[i + 1], KC), Kerb.AddVert(RI[i], KC));
			Kerb.Quad(Kerb.AddVert(LI[i], KC), Kerb.AddVert(LI[i + 1], KC), Kerb.AddVert(LO[i + 1], KC), Kerb.AddVert(LO[i], KC));
		}
		Kerb.Ribbon(Edge(HalfW - 25.f, 3.6f), Edge(HalfW - 40.f, 3.6f), [&](int32) { return White; });
		Kerb.Ribbon(Edge(-HalfW + 40.f, 3.6f), Edge(-HalfW + 25.f, 3.6f), [&](int32) { return White; });
		// Start / finish line across the road.
		const FVector S0 = C(6), S1 = C(7);
		const FVector2D R0 = Right(Dir(6)), R1 = Right(Dir(7));
		Kerb.Quad(Kerb.AddVert(S0 + FVector(R0 * HalfW, 3.7f), White), Kerb.AddVert(S1 + FVector(R1 * HalfW, 3.7f), White),
			Kerb.AddVert(S1 + FVector(R1 * -HalfW, 3.7f), White), Kerb.AddVert(S0 + FVector(R0 * -HalfW, 3.7f), White));
	}
	Kerb.Commit(RoadMesh, 2, false);

	// ---- Guardrails both sides ----
	FProcGeo Rail;
	const FLinearColor Steel(0.5f, 0.52f, 0.54f);
	Rail.Ribbon(Edge(RailOff, 80.f), Edge(RailOff, 45.f), [&](int32) { return Steel; });
	Rail.Ribbon(Edge(RailOff + 6.f, 45.f), Edge(RailOff + 6.f, 80.f), [&](int32) { return Steel * 0.7f; });
	Rail.Ribbon(Edge(RailOff + 6.f, 80.f), Edge(RailOff, 80.f), [&](int32) { return Steel; });
	Rail.Ribbon(Edge(-RailOff, 45.f), Edge(-RailOff, 80.f), [&](int32) { return Steel; });
	Rail.Ribbon(Edge(-RailOff - 6.f, 80.f), Edge(-RailOff - 6.f, 45.f), [&](int32) { return Steel * 0.7f; });
	Rail.Ribbon(Edge(-RailOff, 80.f), Edge(-RailOff - 6.f, 80.f), [&](int32) { return Steel; });
	Rail.Commit(RailMesh, 0, false);
	TArray<TArray<FVector>> Blocks;
	for (int32 i = 0; i < M; i += 3)
	{
		const int32 j = i + 3;
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			TArray<FVector>& B = Blocks.AddDefaulted_GetRef();
			for (const int32 k : { i, j })
			{
				const FVector2D R = Right(Dir(k)) * static_cast<float>(Side);
				for (const float Off : { RailOff, RailOff + 100.f })
				{
					B.Add(C(k) + FVector(R * Off, -60.f));
					B.Add(C(k) + FVector(R * Off, 100.f));
				}
			}
			const FVector Dk = Dir(i);
			const FVector Pos = C(i) + FVector(Right(Dk) * (static_cast<float>(Side) * (RailOff + 10.f)), 35.f);
			RailPosts->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dk.Y, Dk.X)), 0.f), Pos, FVector(0.1f, 0.12f, 0.9f)));
		}
	}
	RailMesh->SetCollisionConvexMeshes(Blocks);

	// ---- Terrain: follows the road near it, rolling hills further out ----
	FVector2D Min(TNumericLimits<float>::Max()), Max(TNumericLimits<float>::Lowest());
	for (int32 i = 0; i < D.Num; ++i) { Min = FVector2D::Min(Min, FVector2D(P(D, i))); Max = FVector2D::Max(Max, FVector2D(P(D, i))); }
	Min -= FVector2D(Margin, Margin);
	Max += FVector2D(Margin, Margin);
	const int32 NX = FMath::CeilToInt((Max.X - Min.X) / Cell) + 1, NY = FMath::CeilToInt((Max.Y - Min.Y) / Cell) + 1;
	TArray<float> Heights, RoadDist;
	Heights.SetNumUninitialized(NX * NY);
	RoadDist.SetNumUninitialized(NX * NY);
	for (int32 y = 0; y < NY; ++y)
	{
		for (int32 x = 0; x < NX; ++x)
		{
			const FVector2D Q(Min.X + x * Cell, Min.Y + y * Cell);
			float Best = TNumericLimits<float>::Max(), BestZ = 0.f;
			double WSum = 0.0, ZSum = 0.0;
			for (int32 i = 0; i < D.Num; ++i)
			{
				const FVector A = P(D, i), B = P(D, i + 1);
				const double D2 = FVector2D::DistSquared(Q, FVector2D(A));
				const double W = 1.0 / FMath::Square(D2 * 1e-6 + 1.0);
				WSum += W; ZSum += W * A.Z;
				const FVector2D AB(B.X - A.X, B.Y - A.Y);
				const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(Q - FVector2D(A), AB) / FMath::Max(AB.SizeSquared(), 1.0)), 0.f, 1.f);
				const float Dist = FVector2D::Distance(Q, FVector2D(A) + AB * T);
				if (Dist < Best) { Best = Dist; BestZ = FMath::Lerp(A.Z, B.Z, T); }
			}
			float Z = static_cast<float>(ZSum / WSum);
			const float Far = FMath::SmoothStep(3000.f, 20000.f, Best);
			Z += Far * (D.HillAmp * Noise(Q.X / 30000.f + D.Seed, Q.Y / 30000.f) + 0.25f * D.HillAmp * Noise(Q.X / 8000.f + 7.f, Q.Y / 8000.f));
			const float Under = BestZ - 40.f;
			Z = Best < RailOff + 200.f ? FMath::Min(Z, Under) : FMath::Lerp(Under, Z, FMath::SmoothStep(RailOff + 200.f, RailOff + 1500.f, Best));
			Heights[y * NX + x] = Z;
			RoadDist[y * NX + x] = Best;
		}
	}
	FProcGeo Terrain;
	FRandomStream Rng(D.Seed);
	for (int32 y = 0; y < NY; ++y)
	{
		for (int32 x = 0; x < NX; ++x)
		{
			const int32 I = y * NX + x;
			const FVector2D Q(Min.X + x * Cell, Min.Y + y * Cell);
			const float Var = 0.75f + 0.5f * Noise(Q.X / 2500.f, Q.Y / 2500.f);
			const float Dry = Noise(Q.X / 12000.f + 3.f, Q.Y / 12000.f);
			FLinearColor Col = FMath::Lerp(FLinearColor(0.05f, 0.12f, 0.03f), FLinearColor(0.12f, 0.11f, 0.05f), Dry * (0.4f + D.Autumn)) * Var;
			Col.A = 1.f;
			Terrain.AddVert(FVector(Q, Heights[I]), Col);
		}
	}
	for (int32 y = 0; y + 1 < NY; ++y)
	{
		for (int32 x = 0; x + 1 < NX; ++x)
		{
			const int32 I = y * NX + x;
			Terrain.Quad(I, I + NX, I + NX + 1, I + 1);
		}
	}
	Terrain.Commit(TerrainMesh, 0, true);

	// ---- Trees and bushes outside the rails ----
	auto GroundAt = [&](float GX, float GY)
	{
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(GX), 0, NX - 2), Y0 = FMath::Clamp(FMath::FloorToInt(GY), 0, NY - 2);
		const float FX = GX - X0, FY = GY - Y0;
		const int32 I = Y0 * NX + X0;
		const float H = FMath::Lerp(FMath::Lerp(Heights[I], Heights[I + 1], FX), FMath::Lerp(Heights[I + NX], Heights[I + NX + 1], FX), FY);
		const float Dist = FMath::Lerp(FMath::Lerp(RoadDist[I], RoadDist[I + 1], FX), FMath::Lerp(RoadDist[I + NX], RoadDist[I + NX + 1], FX), FY);
		return FVector2D(H, Dist);
	};
	int32 Trees = 0, Bushes = 0;
	for (int32 Attempt = 0; Attempt < 12000 && Trees < 1200; ++Attempt)
	{
		const float GX = Rng.FRandRange(0.f, NX - 1.01f), GY = Rng.FRandRange(0.f, NY - 1.01f);
		const FVector2D G = GroundAt(GX, GY);
		const FVector Pos(Min.X + GX * Cell, Min.Y + GY * Cell, G.X - 15.f);
		if (G.Y < RailOff + 600.f) continue;
		if (G.Y < RailOff + 1500.f) { if (Bushes < 400) { Scenery.AddBush(Rng, Pos); ++Bushes; } continue; }
		Scenery.AddTree(Rng, Pos, D.Conifer, D.Autumn, 1.f);
		++Trees;
	}

	// ---- Materials ----
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

void ADriftCircuitBuilder::ApplyQuality(int32 Level)
{
	Scenery.ApplyQuality(Level);
	RailPosts->SetCullDistances(0, FMath::RoundToInt(DriftSceneryUtil::CullFor(Level, 15000.f)));
	RailPosts->SetCastShadow(Level >= 2);
	RailMesh->SetCastShadow(Level >= 1);
}

FTransform ADriftCircuitBuilder::GetStartTransform() const
{
	using namespace DriftCircuitLocal;
	const FDef& D = Def(Layout);
	const FVector A = P(D, 1), B = P(D, 2);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
	const FVector Pos = A + FVector(0.f, 0.f, 83.f);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw + Yaw, 0.f), GetActorTransform().TransformPosition(Pos));
}

TArray<FTransform> ADriftCircuitBuilder::GetCheckpoints() const
{
	using namespace DriftCircuitLocal;
	const FDef& D = Def(Layout);
	TArray<FTransform> Out;
	for (int32 i = 0; i < D.Num; i += 3)
	{
		const FVector A = P(D, i), B = P(D, i + 1);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		Out.Add(FTransform(FRotator(0.f, GetActorRotation().Yaw + Yaw, 0.f), GetActorTransform().TransformPosition(A + FVector(0.f, 0.f, 3.f))));
	}
	return Out;
}
