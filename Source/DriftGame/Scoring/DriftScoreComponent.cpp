#include "DriftScoreComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UDriftScoreComponent::UDriftScoreComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDriftScoreComponent::Callout(const TCHAR* Text)
{
	EventText = Text;
	EventAge = 0.f;
}

bool UDriftScoreComponent::CheckProximity() const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DriftProximity), false, Owner);
	const FVector Origin = Owner->GetActorLocation() + FVector(0, 0, 20.f);
	const FVector F = Owner->GetActorForwardVector(), R = Owner->GetActorRightVector();
	const FVector Dirs[] = { R, -R, -F, (-F + R).GetSafeNormal(), (-F - R).GetSafeNormal(), (F + R).GetSafeNormal(), (F - R).GetSafeNormal() };
	// Car half-width ~90 cm, half-length ~230 cm: measure gap from the body, not the centre.
	const float Extra[] = { 90.f, 90.f, 230.f, 160.f, 160.f, 160.f, 160.f };
	for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Dirs); ++i)
	{
		FHitResult Hit;
		const FVector End = Origin + Dirs[i] * (Extra[i] + ProximityDistance);
		if (World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params) && FMath::Abs(Hit.ImpactNormal.Z) < 0.5f)
		{
			return true;
		}
	}
	return false;
}

void UDriftScoreComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AActor* Owner = GetOwner();
	if (!Owner) return;

	EventAge += DeltaTime;
	CloseCalloutCooldown = FMath::Max(0.f, CloseCalloutCooldown - DeltaTime);

	FVector Vel = Owner->GetVelocity();
	Vel.Z = 0.f;
	const float SpeedKmh = Vel.Size() * 0.036f;
	const FVector Fwd = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Owner->GetActorRightVector().GetSafeNormal2D();
	const FVector VelDir = Vel.GetSafeNormal2D();

	SignedAngle = 0.f;
	if (SpeedKmh > 5.f)
	{
		const float Dot = FMath::Clamp((float)FVector::DotProduct(Fwd, VelDir), -1.f, 1.f);
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));
		SignedAngle = FVector::DotProduct(Right, VelDir) >= 0.f ? Angle : -Angle;
	}
	CurrentAngle = FMath::Abs(SignedAngle);

	// Spin-out: too much angle, or big angle with almost no speed.
	if (bChainActive && (CurrentAngle > SpinOutAngle || (SpeedKmh < 15.f && CurrentAngle > 50.f)))
	{
		BreakChain();
		OnSpinOut.Broadcast();
	}

	bDrifting = SpeedKmh >= MinDriftSpeedKmh && CurrentAngle >= MinDriftAngle && CurrentAngle <= SpinOutAngle;

	if (bDrifting)
	{
		const int32 Side = SignedAngle > 0.f ? 1 : -1;
		if (!bChainActive)
		{
			bChainActive = true;
			Multiplier = 1;
		}
		else if (GraceRemaining < ChainGraceTime - 0.05f && Side == LastSide)
		{
			// Re-entered a drift during the grace window: linked drift.
			Multiplier = FMath::Min(Multiplier + 1, MaxCombo);
		}
		if (LastSide != 0 && Side != LastSide)
		{
			// Left slide into right slide.
			Multiplier = FMath::Min(Multiplier + 1, MaxCombo);
		}
		LastSide = Side;
		GraceRemaining = ChainGraceTime;

		const float Prox = bNearWall ? ProximityBonus : 1.f;
		ChainScore += CurrentAngle * SpeedKmh * DeltaTime * PointsScale * Multiplier * Prox;
	}
	else if (bChainActive)
	{
		GraceRemaining -= DeltaTime;
		if (GraceRemaining <= 0.f) BankChain();
	}
}

void UDriftScoreComponent::BankChain()
{
	if (!bChainActive) return;
	TotalScore += ChainScore;
	OnDriftBanked.Broadcast(ChainScore);
	ChainScore = 0.f; Multiplier = 1; LastSide = 0; bChainActive = false;
}

void UDriftScoreComponent::BreakChain()
{
	ChainScore = 0.f; Multiplier = 1; LastSide = 0; bChainActive = false;
}
