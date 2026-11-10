#include "UI/DriftGraphicsSettings.h"
#include "Modes/DriftGameModeBase.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

namespace DriftGraphicsLocal
{
	const TCHAR* Section = TEXT("Tandem.Graphics");

	int32 GetInt(const TCHAR* Key, int32 Default)
	{
		int32 V = Default;
		if (GConfig) GConfig->GetInt(Section, Key, V, GGameUserSettingsIni);
		return V;
	}
	void SetInt(const TCHAR* Key, int32 V)
	{
		if (!GConfig) return;
		GConfig->SetInt(Section, Key, V, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void ApplyResolution(int32 Pct)
	{
		// After the quality preset, which sets its own screen percentage.
		if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
		{
			CV->Set(static_cast<float>(FMath::Clamp(Pct, 50, 100)), ECVF_SetByGameSetting);
		}
	}

	void ApplyEngine(int32 Level, int32 Fps)
	{
		if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
		{
			GS->SetOverallScalabilityLevel(FMath::Clamp(Level, 0, 3));
			GS->SetFrameRateLimit(Fps > 0 ? static_cast<float>(Fps) : 0.f);
			// Not ApplySettings: that would also re-apply window mode and resolution.
			GS->ApplyNonResolutionSettings();
			GS->SaveSettings();
		}
	}

	void ApplyScenery(UWorld* World, int32 Level)
	{
		if (!World) return;
		if (ADriftGameModeBase* GM = World->GetAuthGameMode<ADriftGameModeBase>()) GM->ApplySceneryQuality(Level);
	}
}

namespace DriftGraphics
{
	int32 Quality()       { return FMath::Clamp(DriftGraphicsLocal::GetInt(TEXT("Quality"), 1), 0, 3); }
	int32 ResolutionPct() { return FMath::Clamp(DriftGraphicsLocal::GetInt(TEXT("ResolutionPct"), 85), 50, 100); }
	int32 FpsLimit()      { return FMath::Max(0, DriftGraphicsLocal::GetInt(TEXT("FpsLimit"), 60)); }
	bool ShowFps()        { return DriftGraphicsLocal::GetInt(TEXT("ShowFps"), 0) != 0; }

	void SetQuality(UWorld* World, int32 Level)
	{
		Level = FMath::Clamp(Level, 0, 3);
		DriftGraphicsLocal::SetInt(TEXT("Quality"), Level);
		DriftGraphicsLocal::ApplyEngine(Level, FpsLimit());
		DriftGraphicsLocal::ApplyResolution(ResolutionPct());
		DriftGraphicsLocal::ApplyScenery(World, Level);
	}

	void SetResolutionPct(int32 Pct)
	{
		Pct = FMath::Clamp(Pct, 50, 100);
		DriftGraphicsLocal::SetInt(TEXT("ResolutionPct"), Pct);
		DriftGraphicsLocal::ApplyResolution(Pct);
	}

	void SetFpsLimit(int32 Fps)
	{
		DriftGraphicsLocal::SetInt(TEXT("FpsLimit"), FMath::Max(0, Fps));
		if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
		{
			GS->SetFrameRateLimit(Fps > 0 ? static_cast<float>(Fps) : 0.f);
			GS->ApplyNonResolutionSettings();
			GS->SaveSettings();
		}
	}

	void SetShowFps(bool bShow) { DriftGraphicsLocal::SetInt(TEXT("ShowFps"), bShow ? 1 : 0); }

	void ApplyAtStartup(UWorld* World)
	{
		int32 Saved = -1;
		const bool bHaveSaved = GConfig && GConfig->GetInt(DriftGraphicsLocal::Section, TEXT("Quality"), Saved, GGameUserSettingsIni);
		if (!bHaveSaved)
		{
			// First start: let the engine measure this machine, then stay at medium at most.
			int32 Level = 1;
			if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
			{
				// The benchmark only measures (it fills in the suggested levels); ApplyEngine below applies.
				GS->RunHardwareBenchmark();
				const int32 Measured = GS->GetOverallScalabilityLevel();
				const float Gpu = GS->GetLastGPUBenchmarkResult();
				Level = Measured >= 0 ? FMath::Clamp(Measured, 0, 1) : (Gpu >= 100.f ? 1 : 0);
			}
			DriftGraphicsLocal::SetInt(TEXT("Quality"), Level);
			DriftGraphicsLocal::SetInt(TEXT("ResolutionPct"), Level == 0 ? 70 : 85);
			DriftGraphicsLocal::SetInt(TEXT("FpsLimit"), 60);
		}
		const int32 Level = Quality();
		DriftGraphicsLocal::ApplyEngine(Level, FpsLimit());
		DriftGraphicsLocal::ApplyResolution(ResolutionPct());
		DriftGraphicsLocal::ApplyScenery(World, Level);
	}

	FString QualityName(int32 Level)
	{
		static const TCHAR* Names[4] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };
		return Names[FMath::Clamp(Level, 0, 3)];
	}

	namespace SoundLocal
	{
		const TCHAR* Section = TEXT("Tandem.Sound");
		const TCHAR* Keys[NumSounds] = { TEXT("Engine"), TEXT("Tyres"), TEXT("Wind"), TEXT("Crash") };
		const TCHAR* Labels[NumSounds] = { TEXT("Engine sound"), TEXT("Tyre squeal"), TEXT("Wind noise"), TEXT("Crash and explosion") };
		int8 Cached[NumSounds] = { -1, -1, -1, -1 }; // read once: the car asks every frame
	}

	const TCHAR* SoundLabel(int32 Which) { return SoundLocal::Labels[FMath::Clamp(Which, 0, NumSounds - 1)]; }

	bool SoundOn(int32 Which)
	{
		if (Which < 0 || Which >= NumSounds) return true;
		if (SoundLocal::Cached[Which] < 0)
		{
			int32 V = 1;
			if (GConfig) GConfig->GetInt(SoundLocal::Section, SoundLocal::Keys[Which], V, GGameUserSettingsIni);
			SoundLocal::Cached[Which] = V != 0 ? 1 : 0;
		}
		return SoundLocal::Cached[Which] != 0;
	}

	void SetSoundOn(int32 Which, bool bOn)
	{
		if (Which < 0 || Which >= NumSounds) return;
		SoundLocal::Cached[Which] = bOn ? 1 : 0;
		if (!GConfig) return;
		GConfig->SetInt(SoundLocal::Section, SoundLocal::Keys[Which], bOn ? 1 : 0, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
