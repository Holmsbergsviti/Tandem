#include "Vehicle/DriftStats.h"
#include "Misc/ConfigCacheIni.h"

namespace DriftStatsLocal
{
	const TCHAR* Section = TEXT("Tandem.Stats");
	constexpr float MinAngle = 15.f, MinSpeed = 25.f, MinLength = 1.f, SpinAngle = 80.f, VerdictTime = 1.5f;

	FDriftTotals SessionT, AllT;
	bool bLoaded = false;

	// The drift going on now.
	bool bIn = false;
	float Side = 1.f, Length = 0.f, MaxAngle = 0.f, AngleTime = 0.f, LimiterTime = 0.f;
	float GearTime[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	// A finished drift waiting to see whether the car snaps the other way.
	bool bPending = false;
	float PendingSide = 1.f, PendingAge = 0.f;

	void Load()
	{
		if (bLoaded || !GConfig) return;
		bLoaded = true;
		const FString& Ini = GGameUserSettingsIni;
		GConfig->GetInt(Section, TEXT("Drifts"), AllT.Drifts, Ini);
		GConfig->GetInt(Section, TEXT("Clean"), AllT.Clean, Ini);
		GConfig->GetInt(Section, TEXT("Snapped"), AllT.Snapped, Ini);
		GConfig->GetInt(Section, TEXT("Spun"), AllT.Spun, Ini);
		GConfig->GetFloat(Section, TEXT("DriftTime"), AllT.DriftTime, Ini);
		GConfig->GetFloat(Section, TEXT("Longest"), AllT.Longest, Ini);
		GConfig->GetFloat(Section, TEXT("MaxAngle"), AllT.MaxAngle, Ini);
		GConfig->GetFloat(Section, TEXT("AngleTime"), AllT.AngleTime, Ini);
		GConfig->GetFloat(Section, TEXT("LimiterTime"), AllT.LimiterTime, Ini);
		for (int32 G = 1; G <= 5; ++G) GConfig->GetFloat(Section, *FString::Printf(TEXT("Gear%d"), G), AllT.GearTime[G], Ini);
	}

	void Save()
	{
		if (!GConfig) return;
		const FString& Ini = GGameUserSettingsIni;
		GConfig->SetInt(Section, TEXT("Drifts"), AllT.Drifts, Ini);
		GConfig->SetInt(Section, TEXT("Clean"), AllT.Clean, Ini);
		GConfig->SetInt(Section, TEXT("Snapped"), AllT.Snapped, Ini);
		GConfig->SetInt(Section, TEXT("Spun"), AllT.Spun, Ini);
		GConfig->SetFloat(Section, TEXT("DriftTime"), AllT.DriftTime, Ini);
		GConfig->SetFloat(Section, TEXT("Longest"), AllT.Longest, Ini);
		GConfig->SetFloat(Section, TEXT("MaxAngle"), AllT.MaxAngle, Ini);
		GConfig->SetFloat(Section, TEXT("AngleTime"), AllT.AngleTime, Ini);
		GConfig->SetFloat(Section, TEXT("LimiterTime"), AllT.LimiterTime, Ini);
		for (int32 G = 1; G <= 5; ++G) GConfig->SetFloat(Section, *FString::Printf(TEXT("Gear%d"), G), AllT.GearTime[G], Ini);
		GConfig->Flush(false, Ini);
	}

	/** 0 clean, 1 snapped back, 2 spun. */
	void Verdict(int32 How)
	{
		for (FDriftTotals* T : { &SessionT, &AllT })
		{
			if (How == 0) ++T->Clean; else if (How == 1) ++T->Snapped; else ++T->Spun;
		}
		Save();
	}

	void Finish()
	{
		bIn = false;
		if (Length < MinLength) return;
		for (FDriftTotals* T : { &SessionT, &AllT })
		{
			++T->Drifts;
			T->DriftTime += Length;
			T->Longest = FMath::Max(T->Longest, Length);
			T->MaxAngle = FMath::Max(T->MaxAngle, FMath::Min(MaxAngle, 90.f));
			T->AngleTime += AngleTime;
			T->LimiterTime += LimiterTime;
			for (int32 G = 1; G <= 5; ++G) T->GearTime[G] += GearTime[G];
		}
		if (MaxAngle > SpinAngle) Verdict(2);
		else { bPending = true; PendingSide = Side; PendingAge = 0.f; }
	}

	FString Pct(int32 Part, int32 Whole) { return Whole > 0 ? FString::Printf(TEXT("%d %%"), FMath::RoundToInt(100.f * Part / Whole)) : FString(TEXT("-")); }
	FString Time(float Seconds)
	{
		return Seconds >= 60.f ? FString::Printf(TEXT("%d MIN %02d S"), FMath::FloorToInt(Seconds / 60.f), FMath::FloorToInt(FMath::Fmod(Seconds, 60.f)))
			: FString::Printf(TEXT("%.0f S"), Seconds);
	}
	FString TopGear(const FDriftTotals& T)
	{
		int32 Best = 0;
		for (int32 G = 1; G <= 5; ++G) if (T.GearTime[G] > T.GearTime[Best]) Best = G;
		return Best > 0 && T.DriftTime > 0.f ? FString::Printf(TEXT("%d  (%d %%)"), Best, FMath::RoundToInt(100.f * T.GearTime[Best] / T.DriftTime)) : FString(TEXT("-"));
	}

	FString Cell(int32 Row, const FDriftTotals& T)
	{
		switch (Row)
		{
		case 0: return FString::FromInt(T.Drifts);
		case 1: return Pct(T.Clean, T.Clean + T.Snapped + T.Spun);
		case 2: return Pct(T.Snapped, T.Clean + T.Snapped + T.Spun);
		case 3: return Pct(T.Spun, T.Clean + T.Snapped + T.Spun);
		case 4: return T.Drifts > 0 ? FString::Printf(TEXT("%.1f S"), T.Longest) : FString(TEXT("-"));
		case 5: return T.Drifts > 0 ? FString::Printf(TEXT("%.1f S"), T.DriftTime / T.Drifts) : FString(TEXT("-"));
		case 6: return T.Drifts > 0 ? FString::Printf(TEXT("%.0f DEG"), T.MaxAngle) : FString(TEXT("-"));
		case 7: return T.DriftTime > 0.f ? FString::Printf(TEXT("%.0f DEG"), T.AngleTime / T.DriftTime) : FString(TEXT("-"));
		case 8: return Time(T.DriftTime);
		case 9: return T.DriftTime > 0.f ? FString::Printf(TEXT("%d %%"), FMath::RoundToInt(100.f * T.LimiterTime / T.DriftTime)) : FString(TEXT("-"));
		default: return TopGear(T);
		}
	}
}

namespace DriftStats
{
	using namespace DriftStatsLocal;

	void Update(float Dt, bool bDriving, float SpeedKmh, float SlideDeg, float Rpm, float MaxRpm, int32 Gear)
	{
		Load();
		const float Angle = FMath::Abs(SlideDeg);
		const bool bSliding = bDriving && Angle >= MinAngle && SpeedKmh >= MinSpeed;

		// Did the last drift snap the other way?
		if (bPending)
		{
			PendingAge += Dt;
			if (bDriving && SlideDeg * PendingSide <= -MinAngle && SpeedKmh >= MinSpeed) { bPending = false; Verdict(1); }
			else if (PendingAge >= VerdictTime) { bPending = false; Verdict(0); }
		}

		if (bSliding && bIn && SlideDeg * Side < 0.f) Finish(); // straight into a slide the other way
		if (bSliding)
		{
			if (!bIn)
			{
				bIn = true;
				Side = SlideDeg >= 0.f ? 1.f : -1.f;
				Length = MaxAngle = AngleTime = LimiterTime = 0.f;
				for (float& G : GearTime) G = 0.f;
			}
			Length += Dt;
			MaxAngle = FMath::Max(MaxAngle, Angle);
			AngleTime += FMath::Min(Angle, 90.f) * Dt;
			if (Rpm >= MaxRpm - 150.f) LimiterTime += Dt;
			if (Gear >= 1 && Gear <= 5) GearTime[Gear] += Dt;
		}
		else if (bIn) Finish();
	}

	const FDriftTotals& Session() { return SessionT; }
	const FDriftTotals& AllTime() { Load(); return AllT; }

	void ResetAllTime()
	{
		AllT = FDriftTotals();
		bLoaded = true;
		if (GConfig) { GConfig->EmptySection(Section, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	}

	int32 NumRows() { return 11; }

	FString RowLabel(int32 Row)
	{
		static const TCHAR* Labels[] = { TEXT("Drifts"), TEXT("Ended clean"), TEXT("Snapped back"), TEXT("Spun"), TEXT("Longest drift"),
			TEXT("Average length"), TEXT("Biggest angle"), TEXT("Average angle"), TEXT("Time drifting"), TEXT("On the rev limiter"), TEXT("Most used gear") };
		return Labels[FMath::Clamp(Row, 0, 10)];
	}

	FString RowValue(int32 Row)
	{
		Load();
		return FString::Printf(TEXT("%s     /     %s"), *Cell(Row, SessionT), *Cell(Row, AllT));
	}
}
