#include "Input/DriftKeyBindings.h"
#include "Misc/ConfigCacheIni.h"

namespace DriftKeysLocal
{
	const TCHAR* Section = TEXT("Tandem.Keys");

	FKey Saved(const FName Id, const FKey& Default)
	{
		FString Str;
		if (GConfig && GConfig->GetString(Section, *Id.ToString(), Str, GGameUserSettingsIni))
		{
			const FKey K{ FName(*Str) };
			if (K.IsValid()) return K;
		}
		return Default;
	}
}

namespace DriftKeys
{
	const TArray<FBinding>& All()
	{
		static const TArray<FBinding> List = {
			{ FName(TEXT("Throttle")),   TEXT("Accelerate"),        EKeys::W },
			{ FName(TEXT("Brake")),      TEXT("Brake"),             EKeys::S },
			{ FName(TEXT("SteerLeft")),  TEXT("Steer left"),        EKeys::A },
			{ FName(TEXT("SteerRight")), TEXT("Steer right"),       EKeys::D },
			{ FName(TEXT("Handbrake")),  TEXT("Handbrake"),         EKeys::SpaceBar },
			{ FName(TEXT("GearDown")),   TEXT("Shift down"),        EKeys::Q },
			{ FName(TEXT("GearUp")),     TEXT("Shift up"),          EKeys::E },
			{ FName(TEXT("Clutch")),     TEXT("Clutch kick"),       EKeys::LeftShift },
			{ FName(TEXT("Camera")),     TEXT("Change camera"),     EKeys::C },
			{ FName(TEXT("Wipers")),     TEXT("Wipers"),            EKeys::R },
			{ FName(TEXT("Forces")),     TEXT("Forces read-out"),   EKeys::G },
			{ FName(TEXT("Exit")),       TEXT("Get out / in"),      EKeys::F },
			{ FName(TEXT("Map")),        TEXT("Checkpoint / next map"), EKeys::M },
			{ FName(TEXT("Reset")),      TEXT("Reset car"),         EKeys::BackSpace },
		};
		return List;
	}

	FKey Get(FName Id)
	{
		for (const FBinding& B : All())
		{
			if (B.Id == Id) return DriftKeysLocal::Saved(B.Id, B.Default);
		}
		return EKeys::Invalid;
	}

	void Set(FName Id, const FKey& Key)
	{
		if (!GConfig || !IsBindable(Key)) return;
		const FKey Old = Get(Id);
		for (const FBinding& B : All())
		{
			if (B.Id != Id && Get(B.Id) == Key)
			{
				GConfig->SetString(DriftKeysLocal::Section, *B.Id.ToString(), *Old.GetFName().ToString(), GGameUserSettingsIni);
			}
		}
		GConfig->SetString(DriftKeysLocal::Section, *Id.ToString(), *Key.GetFName().ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void ResetAll()
	{
		if (!GConfig) return;
		GConfig->EmptySection(DriftKeysLocal::Section, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	bool IsBindable(const FKey& Key)
	{
		return Key.IsValid() && !Key.IsGamepadKey() && !Key.IsTouch() && !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D()
			&& Key != EKeys::Escape && Key != EKeys::AnyKey;
	}

	FString KeyName(const FKey& Key)
	{
		return Key.GetDisplayName(false).ToString();
	}
}
