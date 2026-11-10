#pragma once

#include "CoreMinimal.h"

/**
 * Drift statistics shown in the menu (Stats): how many drifts, how they ended, how long and how sideways,
 * and how the engine was used in them. Not a score: nothing is rewarded. Counted live while driving, for
 * this session and for all time (saved in GameUserSettings.ini under [Tandem.Stats]).
 *
 * A drift is 1 second or more with the car sliding at 15 degrees or more above 25 km/h. It ends
 *   SPUN         if the slide went past 80 degrees,
 *   SNAPPED BACK if within 1.5 s the car was sliding 15 degrees or more the other way,
 *   CLEAN        otherwise.
 */
struct FDriftTotals
{
	int32 Drifts = 0, Clean = 0, Snapped = 0, Spun = 0;
	float DriftTime = 0.f;    // s, in counted drifts
	float Longest = 0.f;      // s
	float MaxAngle = 0.f;     // deg
	float AngleTime = 0.f;    // deg * s, for the average angle
	float LimiterTime = 0.f;  // s of drifting on the rev limiter
	float GearTime[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }; // s of drifting in gears 1..5 (index 0 unused)
};

namespace DriftStats
{
	/** Call every frame with the car's state. */
	void Update(float Dt, bool bDriving, float SpeedKmh, float SlideDeg, float Rpm, float MaxRpm, int32 Gear);
	const FDriftTotals& Session();
	const FDriftTotals& AllTime();
	void ResetAllTime();

	/** The rows for the menu: label, then "session / all time". */
	int32 NumRows();
	FString RowLabel(int32 Row);
	FString RowValue(int32 Row);
}
