#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DriftPlayerController.generated.h"

class ADriftVehiclePawn;

enum class EDriftMenuPage : uint8 { None, Main, Maps, Controls, Car, Tuning, Stats, Graphics };

/** One line of the menu, as the HUD draws it. */
struct FDriftMenuItem
{
	FString Label;
	FString Value;          // right-hand column: key name, setting, "IN USE"...
	bool bEnabled = true;   // greyed out and skipped when false ("coming soon")
	bool bAdjustable = false; // left/right changes the value
	bool bTag = false;      // value is a status badge ("IN USE", "CURRENT")
	bool bKey = false;      // value is a key name, drawn as a key cap
};

/**
 * Owns the in-game menu (Esc, or Start on a gamepad): resume, change map, rebind keys, car slots,
 * car tuning, graphics, quit. The game pauses while it is open. Works with the keyboard (arrows or W/S, Enter,
 * Esc to go back), the mouse (hover and click) and a gamepad (d-pad, A, B). The HUD draws it.
 */
UCLASS()
class DRIFTGAME_API ADriftPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ADriftPlayerController();
	virtual void PlayerTick(float DeltaTime) override;

	bool IsMenuOpen() const { return Page != EDriftMenuPage::None; }
	bool IsMainPage() const { return Page == EDriftMenuPage::Main; }
	FString GetMenuTitle() const;
	FString GetMenuNote() const;
	void GetMenuItems(TArray<FDriftMenuItem>& Out) const;
	int32 GetSelectedItem() const { return Selected; }
	bool IsWaitingForKey() const { return CaptureIndex != INDEX_NONE; }
	/** Screen rectangles of the items as last drawn, for the mouse. */
	void SetItemRects(const TArray<FBox2D>& Rects) { ItemRects = Rects; }

	void OpenMenu();
	void CloseMenu();

private:
	EDriftMenuPage Page = EDriftMenuPage::None;
	int32 Selected = 0;
	int32 CaptureIndex = INDEX_NONE; // control being rebound
	uint64 CaptureFrame = 0;
	TArray<FBox2D> ItemRects;
	FVector2D LastMouse = FVector2D(-1.f, -1.f);

	void GoTo(EDriftMenuPage NewPage, int32 Select = 0);
	void Back();
	void Activate(int32 Index, int32 Dir);
	void Move(int32 Dir);
	bool Pressed(const FKey& Key) const;
	void TickCapture();
	ADriftVehiclePawn* FindCar() const;
};
