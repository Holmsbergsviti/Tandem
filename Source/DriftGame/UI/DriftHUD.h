#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DriftHUD.generated.h"

class UFont;
class ADriftPlayerController;

/**
 * Racing dashboard drawn on the canvas: rev dial with redline, shift lights, digital speed,
 * big gear, traction control light, drift score panel with combo, lap/time panel,
 * countdown, camera name toast and a results card.
 */
UCLASS()
class DRIFTGAME_API ADriftHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

private:
	float S = 1.f; // UI scale (1 at 1080p)
	float FpsSmooth = 60.f;

	void DrawMenu(ADriftPlayerController* PC);
	// Menu drawing helpers (Slate fonts: crisp at any size).
	struct FSlateFontInfo UIFont(bool bBold, float Size, int32 Spacing = 0) const;
	float UIWidth(const FString& Str, const struct FSlateFontInfo& Font) const;
	void UIText(const FString& Str, float X, float CY, const struct FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = true);
	void Gradient(float X, float Y, float GW, float GH, const FLinearColor& C0, const FLinearColor& C1, bool bHorizontal);
	void Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color);
	void Outline(float X, float Y, float BW, float BH, float Thick, const FLinearColor& Color);
	/** A key cap with a label (or arrows: "<>" left/right, "^v" up/down). Returns its width. */
	float KeyCap(float X, float CY, const FString& Label, float Alpha);
	float MenuAnimStart = -1.f;
	float MenuHighlightY = -1.f;
	FString MenuLastTitle;
	/** Bottom-left driving hints, with the player's own key bindings. */
	FString DrivingHints() const;
	float TextWidth(const FString& Str, float Scale) const;

	void Text(const FString& Str, float X, float Y, const FLinearColor& Color, float Scale, bool bCenter = false, UFont* Font = nullptr);
	void Panel(float X, float Y, float W, float H, float Alpha = 0.55f);
	void Disc(float CX, float CY, float R, const FLinearColor& Color, int32 Segs = 48);
	void Arc(float CX, float CY, float R, float A0, float A1, float Thick, const FLinearColor& Color, int32 Segs = 40);
	void DrawTacho(float CX, float CY, float R, float Rpm, float MaxRpm, int32 Gear, float SpeedKmh);
};
