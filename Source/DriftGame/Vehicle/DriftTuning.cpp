#include "Vehicle/DriftTuning.h"
#include "Vehicle/DriftVehiclePawn.h"
#include "Misc/ConfigCacheIni.h"

namespace DriftTuningLocal
{
	/** Each car keeps its own setup: [Tandem.Tuning.AE86], [Tandem.Tuning.NSX]. */
	FString SectionFor(const ADriftVehiclePawn* Car) { return FString(TEXT("Tandem.Tuning.")) + ADriftVehiclePawn::CarModelId(Car->GetCarModel()); }

	enum class EShow : uint8 { Power, Rpm, Ratio, Grip, Percent, Degrees, Kilograms };

	struct FParam
	{
		const TCHAR* Key;
		const TCHAR* Label;
		float ADriftVehiclePawn::* Field;
		float Min, Max, Step;
		EShow Show;
	};

	const FParam Params[] = {
		{ TEXT("MaxTorqueNm"),     TEXT("Engine power"),        &ADriftVehiclePawn::MaxTorqueNm,     100.f,  700.f,   10.f,  EShow::Power },
		{ TEXT("MaxRPM"),          TEXT("Rev limit"),           &ADriftVehiclePawn::MaxRPM,          6000.f, 10000.f, 200.f, EShow::Rpm },
		{ TEXT("FinalDrive"),      TEXT("Final drive"),         &ADriftVehiclePawn::FinalDrive,      3.5f,   5.5f,    0.1f,  EShow::Ratio },
		{ TEXT("TyreGrip"),        TEXT("Front tyre grip"),     &ADriftVehiclePawn::TyreGrip,        0.6f,   1.4f,    0.05f, EShow::Grip },
		{ TEXT("TyreGripRear"),    TEXT("Rear tyre grip"),      &ADriftVehiclePawn::TyreGripRear,    0.6f,   1.4f,    0.05f, EShow::Grip },
		{ TEXT("SlidingFriction"), TEXT("Grip while sliding"),  &ADriftVehiclePawn::SlidingFriction, 0.5f,   0.95f,   0.05f, EShow::Percent },
		{ TEXT("MaxSteerDeg"),     TEXT("Steering lock"),       &ADriftVehiclePawn::MaxSteerDeg,     30.f,   70.f,    5.f,   EShow::Degrees },
		{ TEXT("MassKg"),          TEXT("Weight"),              &ADriftVehiclePawn::MassKg,          700.f,  1700.f,  25.f,  EShow::Kilograms },
	};
	const int32 NumParams = static_cast<int32>(UE_ARRAY_COUNT(Params));

	void Save()
	{
		if (GConfig) GConfig->Flush(false, GGameUserSettingsIni);
	}
}

namespace DriftTuning
{
	using namespace DriftTuningLocal;

	const TCHAR* HoldKey = TEXT("DriftAngleHold");

	int32 Num() { return NumParams + 1; } // the values, then the drift angle hold switch

	FString Label(int32 Index)
	{
		if (Index >= 0 && Index < NumParams) return Params[Index].Label;
		return TEXT("Snap-back damping");
	}

	FString ValueText(int32 Index, const ADriftVehiclePawn* Car)
	{
		if (!Car) return FString();
		if (Index < 0 || Index >= NumParams) return Car->bDriftAngleHold ? TEXT("ON") : TEXT("OFF");
		const FParam& P = Params[Index];
		const float V = Car->*P.Field;
		switch (P.Show)
		{
		case EShow::Power:     return FString::Printf(TEXT("%.0f HP   %.0f NM"), Car->GetPeakPowerHp(), V);
		case EShow::Rpm:       return FString::Printf(TEXT("%.0f RPM"), V);
		case EShow::Ratio:     return FString::Printf(TEXT("%.2f"), V);
		case EShow::Grip:      return FString::Printf(TEXT("%.2f"), V);
		case EShow::Percent:   return FString::Printf(TEXT("%.0f %%"), V * 100.f);
		case EShow::Degrees:   return FString::Printf(TEXT("%.0f DEG"), V);
		case EShow::Kilograms: return FString::Printf(TEXT("%.0f KG"), V);
		default:               return FString::SanitizeFloat(V);
		}
	}

	void Step(int32 Index, int32 Dir, ADriftVehiclePawn* Car)
	{
		if (!Car || !GConfig) return;
		if (Index < 0 || Index >= NumParams)
		{
			Car->bDriftAngleHold = !Car->bDriftAngleHold;
			GConfig->SetInt(*SectionFor(Car), HoldKey, Car->bDriftAngleHold ? 1 : 0, GGameUserSettingsIni);
			Save();
			return;
		}
		const FParam& P = Params[Index];
		const float Steps = FMath::RoundToFloat((Car->*P.Field - P.Min) / P.Step) + (Dir >= 0 ? 1.f : -1.f);
		Car->*P.Field = FMath::Clamp(P.Min + Steps * P.Step, P.Min, P.Max);
		Car->ApplyTuning();
		GConfig->SetFloat(*SectionFor(Car), P.Key, Car->*P.Field, GGameUserSettingsIni);
		Save();
	}

	void ResetAll(ADriftVehiclePawn* Car)
	{
		if (!Car) return;
		Car->ApplyCarSpec(Car->GetCarModel());
		Car->bDriftAngleHold = GetDefault<ADriftVehiclePawn>()->bDriftAngleHold;
		if (GConfig)
		{
			GConfig->EmptySection(*SectionFor(Car), GGameUserSettingsIni);
			Save();
		}
	}

	void Load(ADriftVehiclePawn* Car)
	{
		if (!Car || !GConfig) return;
		for (const FParam& P : Params)
		{
			float V = 0.f;
			if (GConfig->GetFloat(*SectionFor(Car), P.Key, V, GGameUserSettingsIni)) Car->*P.Field = FMath::Clamp(V, P.Min, P.Max);
		}
		int32 Hold = 1;
		Car->bDriftAngleHold = !GConfig->GetInt(*SectionFor(Car), HoldKey, Hold, GGameUserSettingsIni) || Hold != 0;
		Car->ApplyTuning();
	}
}
