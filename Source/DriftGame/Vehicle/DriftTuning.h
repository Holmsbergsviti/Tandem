#pragma once

#include "CoreMinimal.h"

class ADriftVehiclePawn;

/**
 * Car setup the player changes in the menu (Tuning), saved per car in GameUserSettings.ini under
 * [Tandem.Tuning.<car>] (AE86, NSX). Each entry is one of the car's own physical values (engine torque, rev limit, tyre friction, ...), so a
 * change works through the same force model as everything else. Defaults are each car's built-in values.
 */
namespace DriftTuning
{
	int32 Num();
	FString Label(int32 Index);
	/** The value as the menu shows it ("290 HP   330 NM", "0.95"). */
	FString ValueText(int32 Index, const ADriftVehiclePawn* Car);
	/** One step up (Dir > 0) or down, kept inside the setting's range; applied to the car and saved. */
	void Step(int32 Index, int32 Dir, ADriftVehiclePawn* Car);
	/** Back to the built-in values; forgets everything saved. */
	void ResetAll(ADriftVehiclePawn* Car);
	/** Puts the saved setup on the car (when it spawns). */
	void Load(ADriftVehiclePawn* Car);
}
