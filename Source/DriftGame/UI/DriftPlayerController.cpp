#include "UI/DriftPlayerController.h"
#include "UI/DriftGraphicsSettings.h"
#include "Input/DriftKeyBindings.h"
#include "Modes/DriftGameModeBase.h"
#include "Vehicle/DriftVehiclePawn.h"
#include "Vehicle/DriftTuning.h"
#include "Vehicle/DriftStats.h"
#include "Character/DriftDriverCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"

namespace DriftMenuLocal
{
	const int32 ResolutionSteps[] = { 50, 60, 70, 80, 85, 90, 100 };
	const int32 FpsSteps[] = { 30, 45, 60, 0 };
	constexpr int32 CarSlots = 4;

	int32 MenuWrap(int32 V, int32 N) { return N > 0 ? ((V % N) + N) % N : 0; }

	int32 NearestStep(const int32* Steps, int32 N, int32 Value)
	{
		int32 Best = 0;
		for (int32 i = 1; i < N; ++i)
		{
			if (FMath::Abs(Steps[i] - Value) < FMath::Abs(Steps[Best] - Value)) Best = i;
		}
		return Best;
	}

	FString FpsName(int32 Fps) { return Fps > 0 ? FString::Printf(TEXT("%d FPS"), Fps) : FString(TEXT("UNLIMITED")); }
}

ADriftPlayerController::ADriftPlayerController()
{
	// The menu pauses the game, and still has to read keys and the mouse while it is paused.
	bShouldPerformFullTickWhenPaused = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

bool ADriftPlayerController::Pressed(const FKey& Key) const
{
	return WasInputKeyJustPressed(Key);
}

ADriftVehiclePawn* ADriftPlayerController::FindCar() const
{
	if (ADriftVehiclePawn* Car = Cast<ADriftVehiclePawn>(GetPawn())) return Car;
	if (const ADriftDriverCharacter* Driver = Cast<ADriftDriverCharacter>(GetPawn())) return Driver->GetCar();
	if (!GetWorld()) return nullptr;
	TActorIterator<ADriftVehiclePawn> It(GetWorld());
	return It ? *It : nullptr;
}

void ADriftPlayerController::OpenMenu()
{
	if (IsMenuOpen()) return;
	GoTo(EDriftMenuPage::Main);
	SetPause(true);
	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void ADriftPlayerController::CloseMenu()
{
	if (!IsMenuOpen()) return;
	Page = EDriftMenuPage::None;
	CaptureIndex = INDEX_NONE;
	ItemRects.Reset();
	SetPause(false);
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	FlushPressedKeys(); // nothing pressed in the menu carries on into driving
}

void ADriftPlayerController::GoTo(EDriftMenuPage NewPage, int32 Select)
{
	Page = NewPage;
	CaptureIndex = INDEX_NONE;
	ItemRects.Reset();
	TArray<FDriftMenuItem> Items;
	GetMenuItems(Items);
	Selected = FMath::Clamp(Select, 0, FMath::Max(0, Items.Num() - 1));
	if (Items.IsValidIndex(Selected) && !Items[Selected].bEnabled) Move(1);
}

void ADriftPlayerController::Back()
{
	switch (Page)
	{
	case EDriftMenuPage::Main: CloseMenu(); break;
	case EDriftMenuPage::Maps: GoTo(EDriftMenuPage::Main, 1); break;
	case EDriftMenuPage::Controls: GoTo(EDriftMenuPage::Main, 2); break;
	case EDriftMenuPage::Car: GoTo(EDriftMenuPage::Main, 3); break;
	case EDriftMenuPage::Tuning: GoTo(EDriftMenuPage::Main, 4); break;
	case EDriftMenuPage::Stats: GoTo(EDriftMenuPage::Main, 5); break;
	case EDriftMenuPage::Graphics: GoTo(EDriftMenuPage::Main, 6); break;
	default: CloseMenu(); break;
	}
}

void ADriftPlayerController::Move(int32 Dir)
{
	TArray<FDriftMenuItem> Items;
	GetMenuItems(Items);
	const int32 N = Items.Num();
	if (N == 0) return;
	for (int32 Step = 0; Step < N; ++Step)
	{
		Selected = DriftMenuLocal::MenuWrap(Selected + Dir, N);
		if (Items[Selected].bEnabled) return;
	}
}

FString ADriftPlayerController::GetMenuTitle() const
{
	switch (Page)
	{
	case EDriftMenuPage::Maps: return TEXT("CHANGE MAP");
	case EDriftMenuPage::Controls: return TEXT("CONTROLS");
	case EDriftMenuPage::Car: return TEXT("CAR");
	case EDriftMenuPage::Tuning: return TEXT("TUNING");
	case EDriftMenuPage::Stats: return TEXT("STATS");
	case EDriftMenuPage::Graphics: return TEXT("GRAPHICS");
	default: return TEXT("TANDEM");
	}
}

FString ADriftPlayerController::GetMenuNote() const
{
	switch (Page)
	{
	case EDriftMenuPage::Controls:
		return IsWaitingForKey() ? TEXT("Press the new key.   ESC cancels.")
			: TEXT("Choose a control, then press its new key. Taken keys swap. Gamepad buttons are fixed.");
	case EDriftMenuPage::Car: return TEXT("Pick a car. Each one keeps its own tuning.");
	case EDriftMenuPage::Tuning: return TEXT("For the car in use. Left / right (mouse: click / right-click) changes a value; saved at once.");
	case EDriftMenuPage::Stats: return TEXT("This session  /  all time. A drift counts from 1 second at 15 degrees or more.");
	case EDriftMenuPage::Graphics: return TEXT("Weak laptop? LOW quality and a lower render resolution help the most.");
	case EDriftMenuPage::Maps: return TEXT("Puts you in the car at the start of the map.");
	default: return TEXT("Paused");
	}
}

void ADriftPlayerController::GetMenuItems(TArray<FDriftMenuItem>& Out) const
{
	Out.Reset();
	auto Add = [&Out](const FString& Label, const FString& Value = FString(), bool bEnabled = true, bool bAdjustable = false)
	{
		FDriftMenuItem& It = Out.AddDefaulted_GetRef();
		It.Label = Label; It.Value = Value; It.bEnabled = bEnabled; It.bAdjustable = bAdjustable;
	};
	switch (Page)
	{
	case EDriftMenuPage::Main:
		Add(TEXT("Resume"));
		Add(TEXT("Change map"));
		Add(TEXT("Controls"));
		Add(TEXT("Car"));
		Add(TEXT("Tuning"));
		Add(TEXT("Stats"));
		Add(TEXT("Graphics"));
		Add(TEXT("Quit game"));
		break;
	case EDriftMenuPage::Maps:
	{
		const ADriftGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADriftGameModeBase>() : nullptr;
		const ADriftVehiclePawn* Car = FindCar();
		const int32 Here = Car ? Car->GetAreaIndex() : INDEX_NONE;
		const int32 Count = GM ? GM->NumAreas() : 0;
		for (int32 i = 0; i < Count; ++i)
		{
			Add(GM->GetAreaName(i), i == Here ? FString(TEXT("CURRENT")) : FString());
			Out.Last().bTag = true;
		}
		Add(TEXT("Back"));
		break;
	}
	case EDriftMenuPage::Controls:
	{
		const TArray<DriftKeys::FBinding>& All = DriftKeys::All();
		for (int32 i = 0; i < All.Num(); ++i)
		{
			Add(All[i].Label, CaptureIndex == i ? FString(TEXT("PRESS A KEY")) : DriftKeys::KeyName(DriftKeys::Get(All[i].Id)).ToUpper());
			Out.Last().bKey = true;
		}
		Add(TEXT("Reset to defaults"));
		Add(TEXT("Back"));
		break;
	}
	case EDriftMenuPage::Car:
	{
		const ADriftVehiclePawn* Car = FindCar();
		const int32 Models = ADriftVehiclePawn::NumCarModels();
		for (int32 i = 0; i < Models; ++i)
		{
			const bool bInUse = Car && Car->GetCarModel() == i;
			Add(FString::Printf(TEXT("%s  %.0f hp"), *ADriftVehiclePawn::CarModelName(i), ADriftVehiclePawn::CarModelPeakHp(i)),
				bInUse ? FString(TEXT("IN USE")) : FString(TEXT("SELECT")), Car != nullptr);
			Out.Last().bTag = bInUse;
		}
		for (int32 i = Models; i < DriftMenuLocal::CarSlots; ++i)
		{
			Add(TEXT("Empty slot"), TEXT("COMING SOON"), false);
			Out.Last().bTag = true;
		}
		Add(TEXT("Back"));
		break;
	}
	case EDriftMenuPage::Tuning:
	{
		const ADriftVehiclePawn* Car = FindCar();
		for (int32 i = 0; i < DriftTuning::Num(); ++i) Add(DriftTuning::Label(i), DriftTuning::ValueText(i, Car), Car != nullptr, true);
		Add(TEXT("Reset to defaults"), FString(), Car != nullptr);
		Add(TEXT("Back"));
		break;
	}
	case EDriftMenuPage::Stats:
		for (int32 i = 0; i < DriftStats::NumRows(); ++i) Add(DriftStats::RowLabel(i), DriftStats::RowValue(i));
		Add(TEXT("Reset all-time stats"));
		Add(TEXT("Back"));
		break;
	case EDriftMenuPage::Graphics:
		Add(TEXT("Quality"), DriftGraphics::QualityName(DriftGraphics::Quality()), true, true);
		Add(TEXT("Render resolution"), FString::Printf(TEXT("%d %%"), DriftGraphics::ResolutionPct()), true, true);
		Add(TEXT("Frame limit"), DriftMenuLocal::FpsName(DriftGraphics::FpsLimit()), true, true);
		Add(TEXT("Show FPS"), DriftGraphics::ShowFps() ? TEXT("ON") : TEXT("OFF"), true, true);
		Add(TEXT("Back"));
		break;
	default:
		break;
	}
}

void ADriftPlayerController::Activate(int32 Index, int32 Dir)
{
	TArray<FDriftMenuItem> Items;
	GetMenuItems(Items);
	if (!Items.IsValidIndex(Index) || !Items[Index].bEnabled) return;
	Selected = Index;
	const bool bBack = Index == Items.Num() - 1 && Items[Index].Label == TEXT("Back");
	if (bBack) { Back(); return; }

	switch (Page)
	{
	case EDriftMenuPage::Main:
		switch (Index)
		{
		case 0: CloseMenu(); break;
		case 1:
		{
			const ADriftVehiclePawn* Car = FindCar();
			GoTo(EDriftMenuPage::Maps, Car ? FMath::Max(0, Car->GetAreaIndex()) : 0);
			break;
		}
		case 2: GoTo(EDriftMenuPage::Controls); break;
		case 3: GoTo(EDriftMenuPage::Car); break;
		case 4: GoTo(EDriftMenuPage::Tuning); break;
		case 5: GoTo(EDriftMenuPage::Stats); break;
		case 6: GoTo(EDriftMenuPage::Graphics); break;
		case 7: UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); break;
		default: break;
		}
		break;

	case EDriftMenuPage::Maps:
		if (ADriftVehiclePawn* Car = FindCar())
		{
			if (ADriftDriverCharacter* Driver = Cast<ADriftDriverCharacter>(GetPawn())) Car->EnterCar(Driver);
			Car->GoToArea(Index);
			CloseMenu();
		}
		break;

	case EDriftMenuPage::Controls:
		if (Index < DriftKeys::All().Num())
		{
			CaptureIndex = Index;
			CaptureFrame = GFrameCounter;
		}
		else
		{
			DriftKeys::ResetAll();
			if (ADriftVehiclePawn* Car = FindCar()) Car->ApplyKeyBindings();
		}
		break;

	case EDriftMenuPage::Car:
		if (ADriftVehiclePawn* Car = FindCar())
		{
			if (Index < ADriftVehiclePawn::NumCarModels() && Index != Car->GetCarModel()) Car->SelectCar(Index);
		}
		break;

	case EDriftMenuPage::Tuning:
		if (ADriftVehiclePawn* Car = FindCar())
		{
			if (Index < DriftTuning::Num()) DriftTuning::Step(Index, Dir, Car);
			else DriftTuning::ResetAll(Car);
		}
		break;

	case EDriftMenuPage::Stats:
		if (Index == DriftStats::NumRows()) DriftStats::ResetAllTime();
		break;

	case EDriftMenuPage::Graphics:
	{
		using namespace DriftMenuLocal;
		if (Index == 0) DriftGraphics::SetQuality(GetWorld(), MenuWrap(DriftGraphics::Quality() + Dir, 4));
		else if (Index == 1)
		{
			const int32 N = static_cast<int32>(UE_ARRAY_COUNT(ResolutionSteps));
			DriftGraphics::SetResolutionPct(ResolutionSteps[MenuWrap(NearestStep(ResolutionSteps, N, DriftGraphics::ResolutionPct()) + Dir, N)]);
		}
		else if (Index == 2)
		{
			const int32 N = static_cast<int32>(UE_ARRAY_COUNT(FpsSteps));
			int32 Cur = 0;
			for (int32 i = 0; i < N; ++i) if (FpsSteps[i] == DriftGraphics::FpsLimit()) Cur = i;
			DriftGraphics::SetFpsLimit(FpsSteps[MenuWrap(Cur + Dir, N)]);
		}
		else if (Index == 3) DriftGraphics::SetShowFps(!DriftGraphics::ShowFps());
		break;
	}

	default:
		break;
	}
}

void ADriftPlayerController::TickCapture()
{
	if (GFrameCounter == CaptureFrame) return; // the Enter or click that started it
	if (Pressed(EKeys::Escape) || Pressed(EKeys::Gamepad_FaceButton_Right) || Pressed(EKeys::Gamepad_Special_Right))
	{
		CaptureIndex = INDEX_NONE;
		return;
	}
	TArray<FKey> Keys;
	EKeys::GetAllKeys(Keys);
	for (const FKey& K : Keys)
	{
		if (!DriftKeys::IsBindable(K) || !Pressed(K)) continue;
		const TArray<DriftKeys::FBinding>& All = DriftKeys::All();
		if (All.IsValidIndex(CaptureIndex)) DriftKeys::Set(All[CaptureIndex].Id, K);
		CaptureIndex = INDEX_NONE;
		if (ADriftVehiclePawn* Car = FindCar()) Car->ApplyKeyBindings();
		return;
	}
}

void ADriftPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsMenuOpen())
	{
		if (Pressed(EKeys::Escape) || Pressed(EKeys::Gamepad_Special_Right)) OpenMenu();
		return;
	}
	if (IsWaitingForKey())
	{
		TickCapture();
		return;
	}
	if (Pressed(EKeys::Gamepad_Special_Right)) { CloseMenu(); return; }
	if (Pressed(EKeys::Escape) || Pressed(EKeys::BackSpace) || Pressed(EKeys::Gamepad_FaceButton_Right)) { Back(); return; }

	TArray<FDriftMenuItem> Items;
	GetMenuItems(Items);
	if (Pressed(EKeys::Up) || Pressed(EKeys::W) || Pressed(EKeys::Gamepad_DPad_Up) || Pressed(EKeys::Gamepad_LeftStick_Up)) Move(-1);
	if (Pressed(EKeys::Down) || Pressed(EKeys::S) || Pressed(EKeys::Gamepad_DPad_Down) || Pressed(EKeys::Gamepad_LeftStick_Down)) Move(1);
	const bool bAdjust = Items.IsValidIndex(Selected) && Items[Selected].bAdjustable;
	if (bAdjust && (Pressed(EKeys::Left) || Pressed(EKeys::A) || Pressed(EKeys::Gamepad_DPad_Left) || Pressed(EKeys::Gamepad_LeftStick_Left))) Activate(Selected, -1);
	if (bAdjust && (Pressed(EKeys::Right) || Pressed(EKeys::D) || Pressed(EKeys::Gamepad_DPad_Right) || Pressed(EKeys::Gamepad_LeftStick_Right))) Activate(Selected, 1);
	if (Pressed(EKeys::Enter) || Pressed(EKeys::SpaceBar) || Pressed(EKeys::Gamepad_FaceButton_Bottom))
	{
		Activate(Selected, 1);
		return;
	}

	// Mouse: hover selects, left click chooses (and steps a setting), right click goes back.
	float MX = 0.f, MY = 0.f;
	if (!GetMousePosition(MX, MY)) return;
	const FVector2D Mouse(MX, MY);
	int32 Hover = INDEX_NONE;
	for (int32 i = 0; i < ItemRects.Num() && i < Items.Num(); ++i)
	{
		if (ItemRects[i].IsInside(Mouse)) Hover = i;
	}
	if (!Mouse.Equals(LastMouse, 0.5f))
	{
		LastMouse = Mouse;
		if (Hover != INDEX_NONE && Items[Hover].bEnabled) Selected = Hover;
	}
	if (Pressed(EKeys::LeftMouseButton) && Hover != INDEX_NONE) Activate(Hover, 1);
	else if (Pressed(EKeys::RightMouseButton))
	{
		// On the tuning page a right click on a value turns it down; anywhere else it goes back.
		if (Page == EDriftMenuPage::Tuning && Hover != INDEX_NONE && Items[Hover].bAdjustable && Items[Hover].bEnabled) Activate(Hover, -1);
		else Back();
	}
}
