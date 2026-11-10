#include "DriftHUD.h"
#include "Vehicle/DriftVehiclePawn.h"
#include "Track/DriftTrackBuilder.h"
#include "EngineUtils.h"
#include "Character/DriftDriverCharacter.h"
#include "UI/DriftPlayerController.h"
#include "UI/DriftGraphicsSettings.h"
#include "Input/DriftKeyBindings.h"
#include "Misc/App.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "RenderUtils.h"

namespace
{
	const FLinearColor Accent(1.f, 0.42f, 0.05f);
	const FLinearColor Dim(0.55f, 0.58f, 0.62f);
	const FLinearColor Red(1.f, 0.12f, 0.1f);
}

void ADriftHUD::Text(const FString& Str, float X, float Y, const FLinearColor& Color, float Scale, bool bCenter, UFont* Font)
{
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Str), Font ? Font : GEngine->GetLargeFont(), Color);
	Item.Scale = FVector2D(Scale * S, Scale * S);
	Item.bCentreX = bCenter;
	Item.bCentreY = bCenter;
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Canvas->DrawItem(Item);
}

void ADriftHUD::Panel(float X, float Y, float W, float H, float Alpha)
{
	DrawRect(FLinearColor(0.01f, 0.012f, 0.018f, Alpha), X, Y, W, H);
	DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.9f), X, Y, 4.f * S, H);
}

void ADriftHUD::Disc(float CX, float CY, float R, const FLinearColor& Color, int32 Segs)
{
	TArray<FCanvasUVTri> Tris;
	for (int32 i = 0; i < Segs; ++i)
	{
		const float A0 = 2.f * PI * i / Segs, A1 = 2.f * PI * (i + 1) / Segs;
		FCanvasUVTri T;
		T.V0_Pos = FVector2D(CX, CY);
		T.V1_Pos = FVector2D(CX + FMath::Cos(A0) * R, CY + FMath::Sin(A0) * R);
		T.V2_Pos = FVector2D(CX + FMath::Cos(A1) * R, CY + FMath::Sin(A1) * R);
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
		Tris.Add(T);
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ADriftHUD::Arc(float CX, float CY, float R, float A0, float A1, float Thick, const FLinearColor& Color, int32 Segs)
{
	// Filled band between R - Thick/2 and R + Thick/2, angles in degrees (0 = right, clockwise on screen).
	TArray<FCanvasUVTri> Tris;
	const float RI = R - Thick * 0.5f, RO = R + Thick * 0.5f;
	for (int32 i = 0; i < Segs; ++i)
	{
		const float T0 = FMath::DegreesToRadians(FMath::Lerp(A0, A1, (float)i / Segs));
		const float T1 = FMath::DegreesToRadians(FMath::Lerp(A0, A1, (float)(i + 1) / Segs));
		const FVector2D I0(CX + FMath::Cos(T0) * RI, CY + FMath::Sin(T0) * RI), O0(CX + FMath::Cos(T0) * RO, CY + FMath::Sin(T0) * RO);
		const FVector2D I1(CX + FMath::Cos(T1) * RI, CY + FMath::Sin(T1) * RI), O1(CX + FMath::Cos(T1) * RO, CY + FMath::Sin(T1) * RO);
		FCanvasUVTri TA, TB;
		TA.V0_Pos = I0; TA.V1_Pos = O0; TA.V2_Pos = O1;
		TB.V0_Pos = I0; TB.V1_Pos = O1; TB.V2_Pos = I1;
		TA.V0_Color = TA.V1_Color = TA.V2_Color = Color;
		TB.V0_Color = TB.V1_Color = TB.V2_Color = Color;
		Tris.Add(TA); Tris.Add(TB);
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ADriftHUD::DrawTacho(float CX, float CY, float R, float Rpm, float MaxRpm, int32 Gear, float SpeedKmh)
{
	const float Start = 135.f, Sweep = 270.f;
	const float Scale = FMath::CeilToFloat(MaxRpm / 1000.f) * 1000.f;
	const float Frac = FMath::Clamp(Rpm / Scale, 0.f, 1.f);
	const float RedFrac = (MaxRpm - 500.f) / Scale;

	Disc(CX, CY, R + 14.f * S, FLinearColor(0.f, 0.f, 0.f, 0.55f), 64);
	Arc(CX, CY, R, Start, Start + Sweep, 10.f * S, FLinearColor(1.f, 1.f, 1.f, 0.08f), 60);
	Arc(CX, CY, R, Start + Sweep * RedFrac, Start + Sweep, 10.f * S, FLinearColor(Red.R, Red.G, Red.B, 0.35f), 12);

	// Live RPM band: white, orange near the top, red at the limiter.
	FLinearColor Band = FLinearColor::White;
	if (Frac > RedFrac) Band = Red; else if (Frac > RedFrac - 0.15f) Band = Accent;
	Arc(CX, CY, R, Start, Start + Sweep * Frac, 10.f * S, Band, FMath::Max(2, FMath::RoundToInt(60 * Frac)));

	// Ticks and numbers every 1000 rpm.
	const int32 Steps = FMath::RoundToInt(Scale / 1000.f);
	for (int32 i = 0; i <= Steps; ++i)
	{
		const float A = FMath::DegreesToRadians(Start + Sweep * i / Steps);
		const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
		const bool bRedZone = (float)i / Steps >= RedFrac;
		DrawLine(CX + Dir.X * (R - 16.f * S), CY + Dir.Y * (R - 16.f * S), CX + Dir.X * (R - 30.f * S), CY + Dir.Y * (R - 30.f * S),
			bRedZone ? Red : FLinearColor::White, 3.f * S);
		Text(FString::FromInt(i), CX + Dir.X * (R - 50.f * S), CY + Dir.Y * (R - 50.f * S), bRedZone ? Red : Dim, 0.7f, true);
	}

	// Needle.
	const float NA = FMath::DegreesToRadians(Start + Sweep * Frac);
	DrawLine(CX, CY, CX + FMath::Cos(NA) * (R - 8.f * S), CY + FMath::Sin(NA) * (R - 8.f * S), Accent, 4.f * S);
	Disc(CX, CY, 9.f * S, Accent, 20);

	// Digital speed and gear.
	Text(FString::Printf(TEXT("%d"), FMath::RoundToInt(SpeedKmh)), CX, CY + R * 0.38f, FLinearColor::White, 2.3f, true);
	Text(TEXT("KM/H"), CX, CY + R * 0.62f, Dim, 0.7f, true);
	const FString GearStr = Gear < 0 ? TEXT("R") : (Gear == 0 ? TEXT("N") : FString::FromInt(Gear));
	Text(GearStr, CX, CY - R * 0.36f, Gear < 0 ? Red : Accent, 2.6f, true);
	Text(TEXT("x1000 RPM"), CX, CY - R * 0.08f, Dim, 0.55f, true);

	// Shift lights above the dial.
	const int32 Lights = 10;
	const float LW = 18.f * S, Gap = 6.f * S;
	const float LX0 = CX - (Lights * LW + (Lights - 1) * Gap) * 0.5f, LY = CY - R - 46.f * S;
	const bool bLimiter = Frac >= RedFrac;
	const bool bFlash = bLimiter && FMath::Fmod((float)GetWorld()->GetRealTimeSeconds(), 0.16f) < 0.08f;
	for (int32 i = 0; i < Lights; ++i)
	{
		const float On = 0.62f + 0.36f * i / (Lights - 1);
		FLinearColor C = i < 4 ? FLinearColor(0.1f, 0.9f, 0.2f) : (i < 7 ? FLinearColor(1.f, 0.8f, 0.05f) : Red);
		const bool bLit = bLimiter ? bFlash : (Rpm / MaxRpm >= On);
		C.A = bLit ? 1.f : 0.15f;
		DrawRect(C, LX0 + i * (LW + Gap), LY, LW, 10.f * S);
	}
}

float ADriftHUD::TextWidth(const FString& Str, float Scale) const
{
	float XL = 0.f, YL = 0.f;
	Canvas->TextSize(GEngine->GetLargeFont(), Str, XL, YL, Scale * S, Scale * S);
	return XL;
}

FString ADriftHUD::DrivingHints() const
{
	auto K = [](const TCHAR* Id) { return DriftKeys::KeyName(DriftKeys::Get(FName(Id))).ToUpper(); };
	return FString::Printf(TEXT("%s/%s gas/brake   %s part gas   %s/%s steer   %s handbrake   %s/%s gears   %s wipers   %s clutch kick   %s camera   %s forces   %s get out   %s checkpoint / next map   %s reset   ESC menu"),
		*K(TEXT("Throttle")), *K(TEXT("Brake")), *K(TEXT("PartThrottle")), *K(TEXT("SteerLeft")), *K(TEXT("SteerRight")), *K(TEXT("Handbrake")),
		*K(TEXT("GearDown")), *K(TEXT("GearUp")), *K(TEXT("Wipers")), *K(TEXT("Clutch")), *K(TEXT("Camera")),
		*K(TEXT("Forces")), *K(TEXT("Exit")), *K(TEXT("Map")), *K(TEXT("Reset")));
}

FSlateFontInfo ADriftHUD::UIFont(bool bBold, float Size, int32 Spacing) const
{
	// The engine's runtime Roboto, rasterised at the size asked for (no blurry scaling).
	FSlateFontInfo F(GEngine->GetLargeFont(), FMath::Max(1, FMath::RoundToInt(Size * S)), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	F.LetterSpacing = Spacing;
	return F;
}

float ADriftHUD::UIWidth(const FString& Str, const FSlateFontInfo& Font) const
{
	if (FSlateApplication::IsInitialized())
	{
		const auto Renderer = FSlateApplication::Get().GetRenderer();
		if (Renderer) return static_cast<float>(Renderer->GetFontMeasureService()->Measure(Str, Font).X);
	}
	return Str.Len() * Font.Size * 0.62f;
}

void ADriftHUD::UIText(const FString& Str, float X, float CY, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow)
{
	FCanvasTextItem Item(FVector2D(X, CY), FText::FromString(Str), Font, Color);
	Item.bCentreY = true;
	if (bShadow) Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.55f * Color.A), FVector2D(0.f, 1.5f * S));
	Canvas->DrawItem(Item);
}

void ADriftHUD::Gradient(float X, float Y, float GW, float GH, const FLinearColor& C0, const FLinearColor& C1, bool bHorizontal)
{
	const FVector2D TL(X, Y), TR(X + GW, Y), BR(X + GW, Y + GH), BL(X, Y + GH);
	const FLinearColor CTL = C0, CBL = bHorizontal ? C0 : C1, CTR = bHorizontal ? C1 : C0, CBR = C1;
	TArray<FCanvasUVTri> Tris;
	FCanvasUVTri T1, T2;
	T1.V0_Pos = TL; T1.V1_Pos = TR; T1.V2_Pos = BR; T1.V0_Color = CTL; T1.V1_Color = CTR; T1.V2_Color = CBR;
	T2.V0_Pos = TL; T2.V1_Pos = BR; T2.V2_Pos = BL; T2.V0_Color = CTL; T2.V1_Color = CBR; T2.V2_Color = CBL;
	Tris.Add(T1); Tris.Add(T2);
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ADriftHUD::Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
{
	TArray<FCanvasUVTri> Tris;
	FCanvasUVTri T;
	T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = C;
	T.V0_Color = T.V1_Color = T.V2_Color = Color;
	Tris.Add(T);
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ADriftHUD::Outline(float X, float Y, float BW, float BH, float Thick, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, BW, Thick);
	DrawRect(Color, X, Y + BH - Thick, BW, Thick);
	DrawRect(Color, X, Y + Thick, Thick, BH - 2.f * Thick);
	DrawRect(Color, X + BW - Thick, Y + Thick, Thick, BH - 2.f * Thick);
}

float ADriftHUD::KeyCap(float X, float CY, const FString& Label, float Alpha)
{
	const FSlateFontInfo F = UIFont(true, 11.f, 60);
	const bool bArrows = Label == TEXT("<>") || Label == TEXT("^v");
	const float HH = 13.f * S;
	const float BW = bArrows ? 40.f * S : FMath::Max(26.f * S, UIWidth(Label, F) + 18.f * S);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.08f * Alpha), X, CY - HH, BW, HH * 2.f);
	Outline(X, CY - HH, BW, HH * 2.f, FMath::Max(1.f, 1.2f * S), FLinearColor(1.f, 1.f, 1.f, 0.35f * Alpha));
	const FLinearColor C(1.f, 1.f, 1.f, 0.9f * Alpha);
	if (bArrows)
	{
		const float A = 4.5f * S, MX = X + BW * 0.5f;
		if (Label == TEXT("<>"))
		{
			Triangle(FVector2D(MX - 4.f * S, CY - A), FVector2D(MX - 4.f * S, CY + A), FVector2D(MX - 4.f * S - A * 1.3f, CY), C);
			Triangle(FVector2D(MX + 4.f * S, CY - A), FVector2D(MX + 4.f * S + A * 1.3f, CY), FVector2D(MX + 4.f * S, CY + A), C);
		}
		else
		{
			Triangle(FVector2D(MX - 4.f * S - A, CY - 1.f * S), FVector2D(MX - 4.f * S, CY - 1.f * S - A * 1.3f), FVector2D(MX - 4.f * S + A, CY - 1.f * S), C);
			Triangle(FVector2D(MX + 4.f * S - A, CY + 1.f * S), FVector2D(MX + 4.f * S + A, CY + 1.f * S), FVector2D(MX + 4.f * S, CY + 1.f * S + A * 1.3f), C);
		}
	}
	else
	{
		UIText(Label, X + (BW - UIWidth(Label, F)) * 0.5f, CY, F, C, false);
	}
	return BW;
}

void ADriftHUD::DrawMenu(ADriftPlayerController* PC)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetRealTimeSeconds()) : 0.f;
	const float RealDt = FMath::Clamp(static_cast<float>(FApp::GetDeltaTime()), 0.f, 0.1f);
	if (MenuAnimStart < 0.f) MenuAnimStart = Now;
	const float Fade = FMath::SmoothStep(0.f, 0.22f, Now - MenuAnimStart);
	const float Slide = (1.f - Fade) * -36.f * S;
	const FLinearColor Accent2(Accent.R, Accent.G, Accent.B, Fade);
	auto Fa = [Fade](FLinearColor C) { C.A *= Fade; return C; };

	// ---- Backdrop: the game stays visible on the right, the menu sits on a dark gradient on the left ----
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.28f * Fade), 0.f, 0.f, W, H);
	const FLinearColor Ink(0.006f, 0.008f, 0.014f, 0.93f * Fade);
	const float Solid = 470.f * S;
	DrawRect(Ink, 0.f, 0.f, Solid, H);
	Gradient(Solid, 0.f, FMath::Max(1.f, W * 0.62f - Solid), H, Ink, FLinearColor(Ink.R, Ink.G, Ink.B, 0.f), true);
	Gradient(0.f, H - 220.f * S, W, 220.f * S, FLinearColor(0.f, 0.f, 0.f, 0.f), FLinearColor(0.f, 0.f, 0.f, 0.55f * Fade), false);
	DrawRect(Accent2, 0.f, 0.f, 4.f * S, H); // thin accent edge

	TArray<FDriftMenuItem> Items;
	PC->GetMenuItems(Items);
	const FString Title = PC->GetMenuTitle();
	const bool bMain = PC->IsMainPage();
	const float X0 = 110.f * S + Slide;
	const float RowW = 600.f * S;

	// ---- Title block ----
	if (!bMain)
	{
		UIText(TEXT("TANDEM"), X0, 128.f * S, UIFont(true, 13.f, 450), Fa(FLinearColor(0.6f, 0.63f, 0.68f, 1.f)), false);
		const float TW = UIWidth(TEXT("TANDEM"), UIFont(true, 13.f, 450));
		Triangle(FVector2D(X0 + TW + 12.f * S, 123.f * S), FVector2D(X0 + TW + 18.f * S, 128.f * S), FVector2D(X0 + TW + 12.f * S, 133.f * S), Accent2);
	}
	UIText(Title, X0 - 3.f * S, 185.f * S, UIFont(true, bMain ? 64.f : 50.f, 260), Fa(FLinearColor::White));
	DrawRect(Accent2, X0, 236.f * S, 56.f * S, 4.f * S);
	UIText(PC->GetMenuNote(), X0 + 72.f * S, 238.f * S, UIFont(false, 14.f, 40), Fa(FLinearColor(0.72f, 0.74f, 0.78f, 1.f)), false);

	// ---- Items ----
	const float ListTop = 300.f * S;
	const float RowH = FMath::Clamp((H - 150.f * S - ListTop) / FMath::Max(1, Items.Num()), 36.f * S, 60.f * S);
	const bool bCompact = RowH < 50.f * S;
	const FSlateFontInfo LabelF = UIFont(true, bCompact ? 15.f : 19.f, 140);
	const FSlateFontInfo ValueF = UIFont(false, bCompact ? 14.f : 16.f, 60);
	const FSlateFontInfo TagF = UIFont(true, 11.f, 120);
	const int32 Sel = PC->GetSelectedItem();

	// Highlight glides to the selected row (snaps on a new page).
	const float TargetY = ListTop + Sel * RowH;
	if (Title != MenuLastTitle || MenuHighlightY < 0.f) MenuHighlightY = TargetY;
	MenuLastTitle = Title;
	MenuHighlightY = FMath::FInterpTo(MenuHighlightY, TargetY, RealDt, 20.f);
	if (Items.IsValidIndex(Sel) && Items[Sel].bEnabled)
	{
		Gradient(X0 - 24.f * S, MenuHighlightY, RowW + 24.f * S, RowH, FLinearColor(Accent.R, Accent.G, Accent.B, 0.42f * Fade),
			FLinearColor(Accent.R, Accent.G, Accent.B, 0.f), true);
		DrawRect(Accent2, X0 - 24.f * S, MenuHighlightY, 5.f * S, RowH);
	}

	TArray<FBox2D> Rects;
	const bool bBlink = FMath::Fmod(Now, 0.8f) < 0.5f;
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FDriftMenuItem& It = Items[i];
		const float Y = ListTop + i * RowH, CY = Y + RowH * 0.5f;
		const bool bSel = i == Sel && It.bEnabled;
		Rects.Add(FBox2D(FVector2D(X0 - 24.f * S, Y), FVector2D(X0 + RowW, Y + RowH)));
		if (i > 0) DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.045f * Fade), X0, Y, RowW - 20.f * S, 1.f);

		const FLinearColor LabelC = !It.bEnabled ? FLinearColor(0.36f, 0.38f, 0.42f, Fade)
			: (bSel ? FLinearColor(1.f, 1.f, 1.f, Fade) : FLinearColor(0.66f, 0.69f, 0.74f, Fade));
		UIText(It.Label.ToUpper(), X0 + (bSel ? 6.f * S : 0.f), CY, LabelF, LabelC);
		if (It.Value.IsEmpty()) continue;

		const float Right = X0 + RowW - 24.f * S;
		if (It.bTag)
		{
			// Status badge.
			const float BW = UIWidth(It.Value, TagF) + 20.f * S, BH = 22.f * S;
			const bool bLive = It.bEnabled;
			DrawRect(bLive ? Accent2 : FLinearColor(1.f, 1.f, 1.f, 0.06f * Fade), Right - BW, CY - BH * 0.5f, BW, BH);
			if (!bLive) Outline(Right - BW, CY - BH * 0.5f, BW, BH, FMath::Max(1.f, S), FLinearColor(1.f, 1.f, 1.f, 0.18f * Fade));
			UIText(It.Value, Right - BW + 10.f * S, CY, TagF, bLive ? FLinearColor(0.02f, 0.02f, 0.03f, Fade) : FLinearColor(0.5f, 0.52f, 0.56f, Fade), false);
		}
		else if (It.bKey)
		{
			// Key cap; blinks while waiting for the new key.
			const bool bWaiting = PC->IsWaitingForKey() && i == Sel;
			const float TW = UIWidth(It.Value, ValueF);
			const float BW = FMath::Max(44.f * S, TW + 24.f * S), BH = FMath::Min(RowH - 10.f * S, 30.f * S);
			const FLinearColor Edge = bWaiting ? FLinearColor(Accent.R, Accent.G, Accent.B, (bBlink ? 1.f : 0.35f) * Fade) : FLinearColor(1.f, 1.f, 1.f, (bSel ? 0.6f : 0.25f) * Fade);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, (bSel ? 0.1f : 0.05f) * Fade), Right - BW, CY - BH * 0.5f, BW, BH);
			Outline(Right - BW, CY - BH * 0.5f, BW, BH, FMath::Max(1.f, 1.3f * S), Edge);
			UIText(It.Value, Right - BW + (BW - TW) * 0.5f, CY, ValueF, bWaiting ? Accent2 : FLinearColor(1.f, 1.f, 1.f, (bSel ? 1.f : 0.8f) * Fade), false);
		}
		else
		{
			// Setting value; arrows either side when it can be changed.
			const float TW = UIWidth(It.Value, ValueF);
			const FLinearColor VC = bSel ? Accent2 : FLinearColor(0.85f, 0.87f, 0.9f, Fade);
			const float VX = Right - TW - (It.bAdjustable ? 22.f * S : 0.f);
			UIText(It.Value, VX, CY, ValueF, VC);
			if (It.bAdjustable)
			{
				const float A = 5.f * S;
				const FLinearColor AC = bSel ? Accent2 : FLinearColor(1.f, 1.f, 1.f, 0.25f * Fade);
				Triangle(FVector2D(VX - 14.f * S, CY - A), FVector2D(VX - 14.f * S, CY + A), FVector2D(VX - 14.f * S - A * 1.2f, CY), AC);
				Triangle(FVector2D(Right - 8.f * S, CY - A), FVector2D(Right - 8.f * S + A * 1.2f, CY), FVector2D(Right - 8.f * S, CY + A), AC);
			}
		}
	}
	PC->SetItemRects(Rects);

	// ---- Footer: key caps ----
	const float FY = H - 62.f * S;
	float FX = X0;
	const FSlateFontInfo HintF = UIFont(false, 13.f, 40);
	const FLinearColor HintC(0.78f, 0.8f, 0.84f, Fade);
	auto Hint = [&](const FString& Cap, const FString& What)
	{
		FX += KeyCap(FX, FY, Cap, Fade) + 10.f * S;
		UIText(What, FX, FY, HintF, HintC, false);
		FX += UIWidth(What, HintF) + 30.f * S;
	};
	if (PC->IsWaitingForKey())
	{
		Hint(TEXT("ESC"), TEXT("Cancel"));
		UIText(TEXT("Press any key or mouse button"), FX, FY, HintF, Accent2, false);
	}
	else
	{
		Hint(TEXT("^v"), TEXT("Select"));
		Hint(TEXT("ENTER"), TEXT("Choose"));
		Hint(TEXT("<>"), TEXT("Change"));
		Hint(TEXT("ESC"), bMain ? TEXT("Resume") : TEXT("Back"));
	}
}

void ADriftHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	S = Canvas->ClipY / 1080.f;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	ADriftPlayerController* MenuPC = Cast<ADriftPlayerController>(PlayerOwner);
	const bool bMenu = MenuPC && MenuPC->IsMenuOpen();

	// ---- Frame rate (menu > Graphics > Show FPS) ----
	const float FrameDt = static_cast<float>(FApp::GetDeltaTime());
	if (FrameDt > KINDA_SMALL_NUMBER) FpsSmooth = FMath::Lerp(FpsSmooth, 1.f / FrameDt, 0.05f);
	if (DriftGraphics::ShowFps())
	{
		const FString Fps = FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(FpsSmooth));
		Text(Fps, W - 24.f * S - TextWidth(Fps, 0.8f), 16.f * S, FpsSmooth < 30.f ? Red : FLinearColor(0.3f, 0.9f, 0.4f), 0.8f);
	}

	// ---- On foot: just the prompts ----
	if (const ADriftDriverCharacter* Driver = Cast<ADriftDriverCharacter>(GetOwningPawn()))
	{
		if (bMenu) { DrawMenu(MenuPC); return; }
		const int32 Door = Driver->GetDoorInReach();
		if (Door >= 0)
		{
			const ADriftVehiclePawn* ParkedCar = Driver->GetCar();
			const bool bOpen = ParkedCar && ParkedCar->IsDoorOpen(Door);
			Panel(W * 0.5f - 260.f * S, H * 0.7f - 10.f * S, 520.f * S, 56.f * S);
			Text(FString::Printf(TEXT("F  get in        E  %s door"), bOpen ? TEXT("close") : TEXT("open")),
				W * 0.5f, H * 0.7f + 18.f * S, FLinearColor::White, 1.0f, true);
		}
		Text(TEXT("WASD walk   MOUSE look   SHIFT run   SPACE jump   walk to a door to get in   ESC menu"),
			30.f * S, H - 40.f * S, FLinearColor(1.f, 1.f, 1.f, 0.7f), 0.6f);
		return;
	}

	if (!bMenu) MenuAnimStart = -1.f;
	const ADriftVehiclePawn* Car = Cast<ADriftVehiclePawn>(GetOwningPawn());
	if (!Car) return;
	if (bMenu)
	{
		DrawMenu(MenuPC);
		return;
	}

	// ---- Bottom right: tacho ----
	const float R = 150.f * S;
	DrawTacho(W - R - 60.f * S, H - R - 70.f * S, R, Car->GetRPM(), Car->MaxRPM, Car->GetGear(), Car->GetSpeedKmh());

	// ---- Camera name toast ----
	const float CamAge = Car->GetTimeSinceCameraSwitch();
	if (CamAge < 1.5f)
	{
		FLinearColor C = FLinearColor::White;
		C.A = FMath::Clamp(1.5f - CamAge, 0.f, 1.f);
		Text(FString::Printf(TEXT("CAMERA: %s"), *Car->GetCameraName().ToUpper()), W * 0.5f, H - 80.f * S, C, 1.0f, true);
	}

	// ---- Map name toast (M) ----
	const float MapAge = Car->GetTimeSinceMapSwitch();
	if (MapAge < 3.f)
	{
		FLinearColor C = Accent;
		C.A = FMath::Clamp(3.f - MapAge, 0.f, 1.f);
		Text(Car->GetMapName(), W * 0.5f, H * 0.28f, C, 1.5f, true);
	}

	// ---- Turn ladder: what the next corner is ----
	// Only one track builder exists, so use the first one found.
	if (TActorIterator<ADriftTrackBuilder> It(GetWorld()); It)
	{
		FString Hint;
		if (It->GetLadderHint(Car->GetActorLocation(), Car->GetActorForwardVector(), Hint))
		{
			Panel(W * 0.5f - 300.f * S, 50.f * S, 600.f * S, 56.f * S);
			Text(Hint, W * 0.5f, 78.f * S, Accent, 1.2f, true);
		}
	}

	// ---- "Stop to get out" toast ----
	const float ExitAge = Car->GetTimeSinceExitBlocked();
	if (ExitAge < 1.5f)
	{
		FLinearColor C = Accent;
		C.A = FMath::Clamp(1.5f - ExitAge, 0.f, 1.f);
		Text(TEXT("STOP THE CAR TO GET OUT"), W * 0.5f, H * 0.62f, C, 1.2f, true);
	}

	// ---- Left: live forces read-out (G) ----
	if (Car->IsShowingForces())
	{
		const TArray<FString> Lines = Car->GetForceReadout();
		const float LineH = 20.f * S, PX = 24.f * S, PY = 170.f * S;
		Panel(PX, PY, 1080.f * S, Lines.Num() * LineH + 16.f * S, 0.6f);
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			Text(Lines[i], PX + 14.f * S, PY + 8.f * S + i * LineH, i == 0 ? Accent : FLinearColor::White, 0.55f);
		}
	}


	// ---- Bottom left: control hints (fade out after a bit) ----
	if (GetWorld()->GetTimeSeconds() < 20.f)
	{
		Text(DrivingHints(),
			30.f * S, H - 40.f * S, FLinearColor(1.f, 1.f, 1.f, 0.7f), 0.6f);
	}
}
