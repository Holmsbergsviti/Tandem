#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * Rebindable keyboard controls for driving. Saved in GameUserSettings.ini under [Tandem.Keys], so they
 * survive restarts. Gamepad buttons stay fixed. Esc always opens the menu and can't be rebound.
 */
namespace DriftKeys
{
	struct FBinding
	{
		FName Id;
		FString Label;
		FKey Default;
	};

	/** Every rebindable action, in menu order. */
	const TArray<FBinding>& All();
	/** The key bound to Id now (saved choice, else the default). */
	FKey Get(FName Id);
	/** Binds Key to Id. An action that already used Key takes Id's old key instead (swap). */
	void Set(FName Id, const FKey& Key);
	/** Back to the defaults (W A S D, Space, ...). */
	void ResetAll();
	/** Keyboard keys and mouse buttons, except Esc. */
	bool IsBindable(const FKey& Key);
	/** Short name for the screen ("W", "Space Bar", "Left Shift"). */
	FString KeyName(const FKey& Key);
}
