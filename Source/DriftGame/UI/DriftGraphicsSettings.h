#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Graphics options from the menu, saved in GameUserSettings.ini under [Tandem.Graphics].
 * Quality is the engine's overall scalability preset plus the maps' scenery draw distances and shadows.
 * On the very first start the machine is benchmarked and gets LOW or MEDIUM (never more), so weak
 * laptops start out smooth; the player can raise it in the menu.
 */
namespace DriftGraphics
{
	int32 Quality();          // 0 low, 1 medium, 2 high, 3 epic
	int32 ResolutionPct();    // 3D render resolution, 50..100 % of the window
	int32 FpsLimit();         // 0 = unlimited
	bool ShowFps();

	void SetQuality(UWorld* World, int32 Level);
	void SetResolutionPct(int32 Pct);
	void SetFpsLimit(int32 Fps);
	void SetShowFps(bool bShow);

	/** First start: benchmark and pick a level. Every start: apply the saved settings. */
	void ApplyAtStartup(UWorld* World);

	FString QualityName(int32 Level);

	// ---- Sound switches (same menu page), saved under [Tandem.Sound]. Default: everything on. ----
	enum ESound : int32 { Engine = 0, Tyres, Wind, Crash, NumSounds };
	const TCHAR* SoundLabel(int32 Which);
	bool SoundOn(int32 Which);
	void SetSoundOn(int32 Which, bool bOn);
}
