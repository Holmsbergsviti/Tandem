#include "DriftGameModeBase.h"
#include "Vehicle/DriftVehiclePawn.h"
#include "Track/DriftTrackBuilder.h"
#include "Track/DriftCityBuilder.h"
#include "Track/DriftDownhillBuilder.h"
#include "Track/DriftCircuitBuilder.h"
#include "UI/DriftHUD.h"
#include "UI/DriftPlayerController.h"
#include "UI/DriftGraphicsSettings.h"
#include "EngineUtils.h"

namespace DriftModeLocal
{
	// Where the extra maps sit relative to the main track builder (cm). The main map's terrain spans about
	// x -750..2700 m and y -1500..1350 m, so these are well clear of it and of each other.
	const FVector CityOffset(0.f, 350000.f, 0.f);
	const FVector DownhillOffset(-20000.f, -420000.f, 0.f);
	// Drift circuits, east of everything (each spans at most ~0.7 x 0.6 km plus 300 m of ground round it).
	const FVector CircuitOffsets[] = { FVector(700000.f, -250000.f, 0.f), FVector(700000.f, 150000.f, 0.f), FVector(700000.f, 550000.f, 0.f) };
}

ADriftGameModeBase::ADriftGameModeBase()
{
	DefaultPawnClass = ADriftVehiclePawn::StaticClass();
	HUDClass = ADriftHUD::StaticClass();
	PlayerControllerClass = ADriftPlayerController::StaticClass();
}

ADriftTrackBuilder* ADriftGameModeBase::FindMain() const
{
	if (!GetWorld()) return nullptr;
	TActorIterator<ADriftTrackBuilder> It(GetWorld());
	return It ? *It : nullptr;
}

void ADriftGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	ADriftTrackBuilder* Main = FindMain();
	const FVector Base = Main ? Main->GetActorLocation() : FVector::ZeroVector;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	City = GetWorld()->SpawnActor<ADriftCityBuilder>(ADriftCityBuilder::StaticClass(), FTransform(Base + DriftModeLocal::CityOffset), Params);
	Downhill = GetWorld()->SpawnActor<ADriftDownhillBuilder>(ADriftDownhillBuilder::StaticClass(), FTransform(Base + DriftModeLocal::DownhillOffset), Params);

	Circuits.Reset();
	for (int32 i = 0; i < ADriftCircuitBuilder::NumLayouts; ++i)
	{
		ADriftCircuitBuilder* C = GetWorld()->SpawnActorDeferred<ADriftCircuitBuilder>(ADriftCircuitBuilder::StaticClass(),
			FTransform(Base + DriftModeLocal::CircuitOffsets[i]), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!C) continue;
		C->Layout = i;
		C->FinishSpawning(FTransform(Base + DriftModeLocal::CircuitOffsets[i]));
		Circuits.Add(C);
	}

	Checkpoints.Reset();
	if (Main) Checkpoints.Append(Main->GetCheckpoints());
	if (City) Checkpoints.Append(City->GetCheckpoints());
	if (Downhill) Checkpoints.Append(Downhill->GetCheckpoints());
	for (const ADriftCircuitBuilder* C : Circuits) if (C) Checkpoints.Append(C->GetCheckpoints());

	// First start: benchmark this machine and pick LOW or MEDIUM. Every start: apply the saved settings.
	DriftGraphics::ApplyAtStartup(GetWorld());
}

void ADriftGameModeBase::ApplySceneryQuality(int32 Level)
{
	if (ADriftTrackBuilder* Main = FindMain()) Main->ApplyQuality(Level);
	if (City) City->ApplyQuality(Level);
	if (Downhill) Downhill->ApplyQuality(Level);
	for (ADriftCircuitBuilder* C : Circuits) if (C) C->ApplyQuality(Level);
}

FTransform ADriftGameModeBase::GetAreaStart(int32 Index, FString& OutName) const
{
	if (Index == 4 && City)
	{
		OutName = TEXT("CITY STREETS");
		return City->GetStartTransform();
	}
	if (Index == 5 && Downhill)
	{
		OutName = TEXT("DOWNHILL RUN");
		return Downhill->GetStartTransform();
	}
	if (Index >= 6 && Circuits.IsValidIndex(Index - 6) && Circuits[Index - 6])
	{
		OutName = Circuits[Index - 6]->GetDisplayName();
		return Circuits[Index - 6]->GetStartTransform();
	}
	if (const ADriftTrackBuilder* Main = FindMain()) return Main->GetAreaStart(FMath::Clamp(Index, 0, 3), OutName);
	OutName = TEXT("START");
	return FTransform::Identity;
}

FString ADriftGameModeBase::GetAreaName(int32 Index) const
{
	FString Name;
	GetAreaStart(Index, Name);
	return Name;
}

FTransform ADriftGameModeBase::GetRespawnTransform(const FVector& Near) const
{
	int32 Best = INDEX_NONE;
	double BestDist = TNumericLimits<double>::Max();
	for (int32 i = 0; i < Checkpoints.Num(); ++i)
	{
		const double D = FVector::DistSquared2D(Checkpoints[i].GetLocation(), Near);
		if (D < BestDist) { BestDist = D; Best = i; }
	}
	if (Best == INDEX_NONE) return FTransform::Identity;
	FTransform T = Checkpoints[Best];
	T.AddToTranslation(FVector(0, 0, 100.f));
	return T;
}

bool ADriftGameModeBase::GetNearestCheckpoint(const FVector& Near, float MaxDist, FTransform& Out) const
{
	int32 Best = INDEX_NONE;
	double BestDist = FMath::Square(static_cast<double>(MaxDist));
	for (int32 i = 0; i < Checkpoints.Num(); ++i)
	{
		const double D = FVector::DistSquared2D(Checkpoints[i].GetLocation(), Near);
		if (D < BestDist && FMath::Abs(Checkpoints[i].GetLocation().Z - Near.Z) < 1500.f) { BestDist = D; Best = i; }
	}
	if (Best == INDEX_NONE) return false;
	Out = Checkpoints[Best];
	return true;
}

APawn* ADriftGameModeBase::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	if (const ADriftTrackBuilder* Main = FindMain())
	{
		return SpawnDefaultPawnAtTransform(NewPlayer, Main->GetStartTransform());
	}
	return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
}
