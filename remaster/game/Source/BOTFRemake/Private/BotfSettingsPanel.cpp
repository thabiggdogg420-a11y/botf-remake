#include "BotfSettingsPanel.h"
#include "BotfUiTheme.h"
#include "CoreGlobals.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformTime.h"

void SBotfSettingsPanel::Construct(const FArguments& InArgs)
{
	OnCloseDelegate = InArgs._OnClose;
	ThemeEmpire = InArgs._ThemeEmpire;
	SetCanTick(true);

	// Window Mode static options
	WindowModeOptions.Add(MakeShared<FString>(TEXT("Fullscreen")));
	WindowModeOptions.Add(MakeShared<FString>(TEXT("Windowed Fullscreen (Borderless)")));
	WindowModeOptions.Add(MakeShared<FString>(TEXT("Windowed")));

	LoadCurrentSettings();
	PopulateResolutionOptions();
	PopulateQualityOptions();
	PopulateFpsCapOptions();

	// ---- Styling only below: LCARS console themed by the active empire ----
	using BotfUi::FPalette;
	using BotfUi::EShape;
	using BotfUi::ERole;
	auto Empire = [this]() { return ThemeEmpire.Get(1); };
	auto Col = [Empire](FLinearColor FPalette::* Field)
	{
		return TAttribute<FSlateColor>::CreateLambda([Empire, Field]() { return FSlateColor(BotfUi::Palette(Empire()).*Field); });
	};
	auto Fill = [Empire, Col](EShape Shape, FLinearColor FPalette::* Field, float Width, float Height,
		const TSharedRef<SWidget>& Content, FMargin Pad) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.WidthOverride(Width > 0 ? FOptionalSize(Width) : FOptionalSize())
			.HeightOverride(Height > 0 ? FOptionalSize(Height) : FOptionalSize())
			[
				SNew(SBorder)
				.BorderImage_Lambda([Empire, Shape]() { return BotfUi::Styles().Get(Empire(), Shape); })
				.BorderBackgroundColor(Col(Field))
				.Padding(Pad)
				[ Content ]
			];
	};
	auto Bar = [Fill](EShape Shape, FLinearColor FPalette::* Field, float Width, float Height)
	{
		return Fill(Shape, Field, Width, Height, SNullWidget::NullWidget, FMargin(0));
	};
	auto Pill = [Empire](const FString& Label, TFunction<void()> OnClick, TFunction<bool()> Enabled, ERole Role)
	{
		return BotfUi::MakeButton(FText::FromString(Label), MoveTemp(OnClick), MoveTemp(Enabled), nullptr,
			Empire, Role, FMargin(20.0f, 8.0f), false, 11, FText::GetEmpty());
	};
	auto Section = [Fill, Bar, Col](const FString& Title, const TSharedRef<SWidget>& Body) -> TSharedRef<SWidget>
	{
		return Fill(EShape::Block, &FPalette::Field, 0, 0,
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Bar(EShape::CapLeft, &FPalette::Secondary, 22, 16) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Title)).Font(BotfUi::Condensed(13)).ColorAndOpacity(Col(&FPalette::Tertiary))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center) [ Bar(EShape::Block, &FPalette::Secondary, 0, 3) ]
			]
			+ SVerticalBox::Slot().AutoHeight() [ Body ],
			FMargin(16.0f, 12.0f));
	};
	auto SettingRow = [Col](const FString& Label, const TSharedRef<SWidget>& Control) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.45f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(BotfUi::Font(11)).ColorAndOpacity(Col(&FPalette::Text))
			]
			+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center) [ Control ];
	};

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.MinDesiredWidth(560.0f)
			.MaxDesiredWidth(980.0f)
			[
				SNew(SVerticalBox)

				// LCARS header: elbow, title, segmented bar
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth() [ Bar(EShape::ElbowTop, &FPalette::Primary, 64, 30) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("SYSTEM CONFIGURATION  /  SETTINGS")))
						.Font(BotfUi::Condensed(18))
						.ColorAndOpacity(Col(&FPalette::Accent))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center) [ Bar(EShape::Block, &FPalette::Secondary, 0, 10) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f) [ Bar(EShape::CapRight, &FPalette::Tertiary, 44, 30) ]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)

					// Segmented sidebar
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(22.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().FillHeight(1.0f) [ Bar(EShape::Block, &FPalette::Primary, 0, 0) ]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f) [ Bar(EShape::Block, &FPalette::Secondary, 0, 46) ]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f) [ Bar(EShape::ElbowBottom, &FPalette::Primary, 0, 28) ]
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(14.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)

						// Confirmation Banner (Video Mode Change Testing)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 12.0f)
						[
							SNew(SBox)
							.Visibility(this, &SBotfSettingsPanel::GetConfirmationBannerVisibility)
							[
								Fill(EShape::Row, &FPalette::FieldHi, 0, 0,
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
									[ Bar(EShape::Block, &FPalette::Alert, 8, 34) ]
									+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(this, &SBotfSettingsPanel::GetCountdownText)
										.Font(BotfUi::Condensed(13))
										.ColorAndOpacity(Col(&FPalette::Accent))
										.AutoWrapText(true)
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
									[ Pill(TEXT("KEEP SETTINGS"), [this]() { OnConfirmKeepClicked(); }, nullptr, ERole::Accent) ]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
									[ Pill(TEXT("REVERT NOW"), [this]() { OnConfirmRevertClicked(); }, nullptr, ERole::Alert) ],
									FMargin(14.0f, 10.0f))
							]
						]

						// Scrollable Settings Body
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SScrollBox)

							// Section: Display & Video
							+ SScrollBox::Slot()
							.Padding(0.0f, 0.0f, 0.0f, 12.0f)
							[
								SNew(SBox)
								.IsEnabled(this, &SBotfSettingsPanel::IsSettingsEditable)
								[
									Section(TEXT("DISPLAY & RESOLUTION"),
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
										[
											SettingRow(TEXT("Window Mode"),
												SAssignNew(WindowModeComboBox, STextComboBox)
												.OptionsSource(&WindowModeOptions)
												.OnSelectionChanged(this, &SBotfSettingsPanel::OnWindowModeChanged))
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
										[
											SettingRow(TEXT("Resolution"),
												SAssignNew(ResolutionComboBox, STextComboBox)
												.OptionsSource(&ResolutionOptions)
												.OnSelectionChanged(this, &SBotfSettingsPanel::OnResolutionChanged))
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
										[
											SettingRow(TEXT("Vertical Sync (VSync)"),
												SAssignNew(VSyncCheckBox, SCheckBox)
												.IsChecked_Lambda([this]() { return bStagedVSync ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
												.OnCheckStateChanged(this, &SBotfSettingsPanel::OnVSyncCheckChanged))
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("Resolution or window mode changes apply as a test: keep them within 15 seconds or they revert automatically.")))
											.Font(BotfUi::Font(10))
											.ColorAndOpacity(Col(&FPalette::Muted))
											.AutoWrapText(true)
										])
								]
							]

							// Section: Graphics & Scalability
							+ SScrollBox::Slot()
							.Padding(0.0f, 0.0f, 0.0f, 12.0f)
							[
								SNew(SBox)
								.IsEnabled(this, &SBotfSettingsPanel::IsSettingsEditable)
								[
									Section(TEXT("GRAPHICS & PERFORMANCE"),
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
										[
											SettingRow(TEXT("Overall Quality Preset"),
												SAssignNew(QualityComboBox, STextComboBox)
												.OptionsSource(&QualityOptions)
												.OnSelectionChanged(this, &SBotfSettingsPanel::OnQualityChanged))
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
										[
											SettingRow(TEXT("Max Frame Rate"),
												SAssignNew(FpsCapComboBox, STextComboBox)
												.OptionsSource(&FpsCapOptions)
												.OnSelectionChanged(this, &SBotfSettingsPanel::OnFpsCapChanged))
										])
								]
							]

							// Section: Audio Note
							+ SScrollBox::Slot()
							.Padding(0.0f, 0.0f, 0.0f, 12.0f)
							[
								Section(TEXT("AUDIO"),
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("Audio subsystem is not integrated in this prototype build. Volume and mixing channels will be configured in an upcoming milestone.")))
									.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
									.ColorAndOpacity(Col(&FPalette::Muted))
									.AutoWrapText(true))
							]
						]

						// Footer Action Buttons
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[ Pill(TEXT("< BACK"), [this]() { OnBackClicked(); }, [this]() { return IsSettingsEditable(); }, ERole::Tertiary) ]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							.Padding(8.0f, 0.0f)
							[ Bar(EShape::Block, &FPalette::Secondary, 0, 6) ]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[ Pill(TEXT("REVERT"), [this]() { OnRevertClicked(); }, [this]() { return IsRevertEnabled(); }, ERole::Secondary) ]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[ Pill(TEXT("APPLY"), [this]() { OnApplyClicked(); }, [this]() { return IsApplyEnabled(); }, ERole::Accent) ]
						]
					]
				]
			]
		]
	];

	UpdateComboBoxSelections();
}

SBotfSettingsPanel::~SBotfSettingsPanel()
{
	// Teardown must not drive the display or touch child widgets (they may already be gone, and the engine may be
	// shutting down). Only an unconfirmed video test is undone, and only in the settings object, so the staged
	// mode can never be saved by a later SaveSettings. Nothing is applied here: the persisted configuration still
	// holds the confirmed mode, which the next launch (or the next explicit apply) uses.
	if (!bIsConfirmationPending)
	{
		return;
	}
	bIsConfirmationPending = false;
	if (IsEngineExitRequested() || !GEngine)
	{
		return;
	}
	if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
	{
		Settings->SetScreenResolution(PreApplyResolution);
		Settings->SetFullscreenMode(PreApplyWindowMode);
		Settings->ScalabilityQuality = PreApplyScalabilityQuality;
		Settings->SetVSyncEnabled(bPreApplyVSync);
		Settings->SetFrameRateLimit(PreApplyFPSLimit);
	}
}

void SBotfSettingsPanel::RefreshFromRuntime()
{
	if (bIsConfirmationPending)
	{
		// Preserve backup snapshot if confirmation test is actively running
		return;
	}

	LoadCurrentSettings();
	PopulateResolutionOptions();
	PopulateQualityOptions();
	PopulateFpsCapOptions();

	if (ResolutionComboBox.IsValid())
	{
		ResolutionComboBox->RefreshOptions();
	}
	if (QualityComboBox.IsValid())
	{
		QualityComboBox->RefreshOptions();
	}
	if (FpsCapComboBox.IsValid())
	{
		FpsCapComboBox->RefreshOptions();
	}
	if (WindowModeComboBox.IsValid())
	{
		WindowModeComboBox->RefreshOptions();
	}

	UpdateComboBoxSelections();
}

void SBotfSettingsPanel::CancelPendingChanges()
{
	if (bIsConfirmationPending)
	{
		RevertPendingVideoMode();
		return;
	}

	StagedResolution = AppliedResolution;
	StagedWindowMode = AppliedWindowMode;
	StagedScalabilityQuality = AppliedScalabilityQuality;
	StagedQualityPreset = AppliedQualityPreset;
	bHasStagedQualityPresetChange = false;
	bStagedVSync = bAppliedVSync;
	StagedFPSLimit = AppliedFPSLimit;

	PopulateResolutionOptions();
	PopulateQualityOptions();
	PopulateFpsCapOptions();

	if (ResolutionComboBox.IsValid())
	{
		ResolutionComboBox->RefreshOptions();
	}
	if (QualityComboBox.IsValid())
	{
		QualityComboBox->RefreshOptions();
	}
	if (FpsCapComboBox.IsValid())
	{
		FpsCapComboBox->RefreshOptions();
	}

	UpdateComboBoxSelections();
}

UGameUserSettings* SBotfSettingsPanel::GetUserSettings()
{
	return GEngine ? GEngine->GetGameUserSettings() : nullptr;
}

void SBotfSettingsPanel::GetActualWindowAndResolution(FIntPoint& OutResolution, EWindowMode::Type& OutWindowMode)
{
	UGameUserSettings* Settings = GetUserSettings();
	if (Settings)
	{
		OutResolution = Settings->GetScreenResolution();
		OutWindowMode = Settings->GetFullscreenMode();
	}
	else
	{
		OutResolution = FIntPoint(1920, 1080);
		OutWindowMode = EWindowMode::WindowedFullscreen;
	}

	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewportSize2D(0.0f, 0.0f);
		GEngine->GameViewport->GetViewportSize(ViewportSize2D);
		if (ViewportSize2D.X > 0.0f && ViewportSize2D.Y > 0.0f)
		{
			OutResolution = FIntPoint(FMath::RoundToInt32(ViewportSize2D.X), FMath::RoundToInt32(ViewportSize2D.Y));
		}
		else if (GEngine->GameViewport->Viewport)
		{
			const FIntPoint VSize = GEngine->GameViewport->Viewport->GetSizeXY();
			if (VSize.X > 0 && VSize.Y > 0)
			{
				OutResolution = VSize;
			}
		}

		TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
		if (Window.IsValid())
		{
			OutWindowMode = Window->GetWindowMode();
			if (OutResolution.X <= 0 || OutResolution.Y <= 0)
			{
				const FVector2D ClientSize = Window->GetClientSizeInScreen();
				if (ClientSize.X > 0.0f && ClientSize.Y > 0.0f)
				{
					OutResolution = FIntPoint(FMath::RoundToInt32(ClientSize.X), FMath::RoundToInt32(ClientSize.Y));
				}
			}
		}
	}
}

void SBotfSettingsPanel::LoadCurrentSettings()
{
	GetActualWindowAndResolution(AppliedResolution, AppliedWindowMode);

	if (UGameUserSettings* Settings = GetUserSettings())
	{
		AppliedScalabilityQuality = Settings->ScalabilityQuality;
		AppliedQualityPreset = Settings->GetOverallScalabilityLevel();
		bAppliedVSync = Settings->IsVSyncEnabled();
		AppliedFPSLimit = Settings->GetFrameRateLimit();
	}
	else
	{
		AppliedQualityPreset = -1;
		bAppliedVSync = false;
		AppliedFPSLimit = 0.0f;
	}

	StagedResolution = AppliedResolution;
	StagedWindowMode = AppliedWindowMode;
	StagedScalabilityQuality = AppliedScalabilityQuality;
	StagedQualityPreset = AppliedQualityPreset;
	bHasStagedQualityPresetChange = false;
	bStagedVSync = bAppliedVSync;
	StagedFPSLimit = AppliedFPSLimit;
}

void SBotfSettingsPanel::PopulateResolutionOptions()
{
	ResolutionOptions.Empty();
	ResolutionValues.Empty();

	FIntPoint DesktopRes(1920, 1080);
	if (UGameUserSettings* Settings = GetUserSettings())
	{
		DesktopRes = Settings->GetDesktopResolution();
	}
	if (DesktopRes.X <= 0 || DesktopRes.Y <= 0)
	{
		DesktopRes = FIntPoint(1920, 1080);
	}

	const TArray<FIntPoint> Presets = {
		FIntPoint(1280, 720),
		FIntPoint(1280, 800),
		FIntPoint(1366, 768),
		FIntPoint(1440, 900),
		FIntPoint(1600, 900),
		FIntPoint(1680, 1050),
		FIntPoint(1920, 1080),
		FIntPoint(1920, 1200),
		FIntPoint(2560, 1440),
		FIntPoint(2560, 1600),
		FIntPoint(3840, 2160)
	};

	TArray<FIntPoint> Candidates;
	auto AddCandidate = [&Candidates](const FIntPoint& Pt)
	{
		if (Pt.X > 0 && Pt.Y > 0 && !Candidates.Contains(Pt))
		{
			Candidates.Add(Pt);
		}
	};

	for (const FIntPoint& Res : Presets)
	{
		if (Res.X <= DesktopRes.X && Res.Y <= DesktopRes.Y)
		{
			AddCandidate(Res);
		}
	}

	AddCandidate(DesktopRes);
	AddCandidate(AppliedResolution);
	AddCandidate(StagedResolution);

	Candidates.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		const int64 PixelsA = static_cast<int64>(A.X) * A.Y;
		const int64 PixelsB = static_cast<int64>(B.X) * B.Y;
		if (PixelsA != PixelsB)
		{
			return PixelsA < PixelsB;
		}
		return A.X < B.X;
	});

	for (const FIntPoint& Res : Candidates)
	{
		ResolutionValues.Add(Res);
		FString Label = FString::Printf(TEXT("%d x %d"), Res.X, Res.Y);
		if (Res == DesktopRes)
		{
			Label += TEXT(" (Native)");
		}
		ResolutionOptions.Add(MakeShared<FString>(MoveTemp(Label)));
	}
}

void SBotfSettingsPanel::PopulateQualityOptions()
{
	QualityOptions.Empty();
	QualityValues.Empty();

	const TCHAR* PresetNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	for (int32 i = 0; i <= 4; ++i)
	{
		QualityOptions.Add(MakeShared<FString>(PresetNames[i]));
		QualityValues.Add(i);
	}

	// Truthfully expose custom quality if current/staged is custom
	if (StagedQualityPreset == -1)
	{
		QualityOptions.Add(MakeShared<FString>(TEXT("Custom")));
		QualityValues.Add(-1);
	}
}

void SBotfSettingsPanel::PopulateFpsCapOptions()
{
	FpsCapOptions.Empty();
	FpsCapValues.Empty();

	const float StandardCaps[] = { 30.0f, 60.0f, 120.0f, 0.0f };
	const TCHAR* StandardLabels[] = { TEXT("30 FPS"), TEXT("60 FPS"), TEXT("120 FPS"), TEXT("Unlimited") };

	bool bFoundCurrent = false;
	for (int32 i = 0; i < 4; ++i)
	{
		FpsCapOptions.Add(MakeShared<FString>(StandardLabels[i]));
		FpsCapValues.Add(StandardCaps[i]);

		if (FMath::IsNearlyEqual(StandardCaps[i], StagedFPSLimit, 0.5f))
		{
			bFoundCurrent = true;
		}
	}

	// Retain custom FPS cap (e.g. 75, 144) without silently replacing it
	if (!bFoundCurrent)
	{
		FpsCapOptions.Add(MakeShared<FString>(FString::Printf(TEXT("%d FPS (Custom)"), FMath::RoundToInt32(StagedFPSLimit))));
		FpsCapValues.Add(StagedFPSLimit);
	}
}

void SBotfSettingsPanel::UpdateComboBoxSelections()
{
	bIsUpdatingUI = true;

	// Window mode
	int32 WindowModeIndex = 1; // Default borderless
	if (StagedWindowMode == EWindowMode::Fullscreen)
	{
		WindowModeIndex = 0;
	}
	else if (StagedWindowMode == EWindowMode::WindowedFullscreen)
	{
		WindowModeIndex = 1;
	}
	else if (StagedWindowMode == EWindowMode::Windowed)
	{
		WindowModeIndex = 2;
	}

	if (WindowModeComboBox.IsValid() && WindowModeOptions.IsValidIndex(WindowModeIndex))
	{
		WindowModeComboBox->SetSelectedItem(WindowModeOptions[WindowModeIndex]);
	}

	// Resolution
	if (ResolutionComboBox.IsValid())
	{
		int32 FoundResIndex = INDEX_NONE;
		for (int32 i = 0; i < ResolutionValues.Num(); ++i)
		{
			if (ResolutionValues[i] == StagedResolution)
			{
				FoundResIndex = i;
				break;
			}
		}

		if (FoundResIndex != INDEX_NONE && ResolutionOptions.IsValidIndex(FoundResIndex))
		{
			ResolutionComboBox->SetSelectedItem(ResolutionOptions[FoundResIndex]);
		}
	}

	// Graphics quality
	if (QualityComboBox.IsValid())
	{
		int32 FoundQualityIndex = INDEX_NONE;
		for (int32 i = 0; i < QualityValues.Num(); ++i)
		{
			if (QualityValues[i] == StagedQualityPreset)
			{
				FoundQualityIndex = i;
				break;
			}
		}

		if (FoundQualityIndex != INDEX_NONE && QualityOptions.IsValidIndex(FoundQualityIndex))
		{
			QualityComboBox->SetSelectedItem(QualityOptions[FoundQualityIndex]);
		}
	}

	// FPS Cap
	if (FpsCapComboBox.IsValid())
	{
		int32 FoundFpsIndex = INDEX_NONE;
		for (int32 i = 0; i < FpsCapValues.Num(); ++i)
		{
			if (FMath::IsNearlyEqual(FpsCapValues[i], StagedFPSLimit, 0.5f))
			{
				FoundFpsIndex = i;
				break;
			}
		}

		if (FoundFpsIndex != INDEX_NONE && FpsCapOptions.IsValidIndex(FoundFpsIndex))
		{
			FpsCapComboBox->SetSelectedItem(FpsCapOptions[FoundFpsIndex]);
		}
	}

	bIsUpdatingUI = false;
}

bool SBotfSettingsPanel::HasUnappliedChanges() const
{
	const bool bVideoDiff = (StagedResolution != AppliedResolution) || (StagedWindowMode != AppliedWindowMode);
	const bool bQualityDiff = bHasStagedQualityPresetChange
		? (StagedQualityPreset != AppliedQualityPreset)
		: !(StagedScalabilityQuality == AppliedScalabilityQuality);
	const bool bVSyncDiff = (bStagedVSync != bAppliedVSync);
	const bool bFpsDiff = !FMath::IsNearlyEqual(StagedFPSLimit, AppliedFPSLimit, 0.5f);

	return bVideoDiff || bQualityDiff || bVSyncDiff || bFpsDiff;
}

void SBotfSettingsPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bIsConfirmationPending)
	{
		const double CurrentSeconds = FPlatformTime::Seconds();
		if (CurrentSeconds >= ConfirmationDeadlineSeconds)
		{
			RevertPendingVideoMode();
		}
	}
}

FReply SBotfSettingsPanel::OnApplyClicked()
{
	UGameUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		return FReply::Handled();
	}

	// Snapshot actual current game-window mode and drawable resolution before video Apply
	FIntPoint ActualCurrentRes;
	EWindowMode::Type ActualCurrentWindowMode;
	GetActualWindowAndResolution(ActualCurrentRes, ActualCurrentWindowMode);

	const bool bVideoChanged = (StagedResolution != ActualCurrentRes) || (StagedWindowMode != ActualCurrentWindowMode);

	if (bVideoChanged)
	{
		// Snapshot prior state for safe revert
		PreApplyResolution = ActualCurrentRes;
		PreApplyWindowMode = ActualCurrentWindowMode;
		PreApplyScalabilityQuality = Settings->ScalabilityQuality;
		PreApplyQualityPreset = Settings->GetOverallScalabilityLevel();
		bPreApplyVSync = Settings->IsVSyncEnabled();
		PreApplyFPSLimit = Settings->GetFrameRateLimit();

		// Apply staged video and non-resolution adjustments
		Settings->SetScreenResolution(StagedResolution);
		Settings->SetFullscreenMode(StagedWindowMode);

		if (bHasStagedQualityPresetChange && StagedQualityPreset >= 0)
		{
			Settings->SetOverallScalabilityLevel(StagedQualityPreset);
			StagedScalabilityQuality = Settings->ScalabilityQuality;
		}
		else
		{
			Settings->ScalabilityQuality = StagedScalabilityQuality;
		}

		Settings->SetVSyncEnabled(bStagedVSync);
		Settings->SetFrameRateLimit(StagedFPSLimit);

		// ApplyResolutionSettings + ApplyNonResolutionSettings WITHOUT SaveSettings or ConfirmVideoMode
		Settings->ApplyResolutionSettings(false);
		Settings->ApplyNonResolutionSettings();

		bIsConfirmationPending = true;
		ConfirmationDeadlineSeconds = FPlatformTime::Seconds() + ConfirmationTimeoutDuration;
	}
	else
	{
		// Video unchanged: reconcile Settings screen resolution and window mode
		// to the already running live display so SaveSettings persists actual running values
		Settings->SetScreenResolution(ActualCurrentRes);
		Settings->SetFullscreenMode(ActualCurrentWindowMode);

		if (bHasStagedQualityPresetChange && StagedQualityPreset >= 0)
		{
			Settings->SetOverallScalabilityLevel(StagedQualityPreset);
			StagedScalabilityQuality = Settings->ScalabilityQuality;
			AppliedQualityPreset = StagedQualityPreset;
		}
		else
		{
			Settings->ScalabilityQuality = StagedScalabilityQuality;
		}

		Settings->SetVSyncEnabled(bStagedVSync);
		Settings->SetFrameRateLimit(StagedFPSLimit);

		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();

		// Keep applied/staged state consistent with the reconciled runtime state
		AppliedResolution = ActualCurrentRes;
		AppliedWindowMode = ActualCurrentWindowMode;
		StagedResolution = ActualCurrentRes;
		StagedWindowMode = ActualCurrentWindowMode;

		AppliedScalabilityQuality = StagedScalabilityQuality;
		AppliedQualityPreset = Settings->GetOverallScalabilityLevel();
		bAppliedVSync = bStagedVSync;
		AppliedFPSLimit = StagedFPSLimit;
		bHasStagedQualityPresetChange = false;
	}

	return FReply::Handled();
}

FReply SBotfSettingsPanel::OnRevertClicked()
{
	CancelPendingChanges();
	return FReply::Handled();
}

FReply SBotfSettingsPanel::OnBackClicked()
{
	CancelPendingChanges();
	OnCloseDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SBotfSettingsPanel::OnConfirmKeepClicked()
{
	UGameUserSettings* Settings = GetUserSettings();
	if (Settings)
	{
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
	}

	bIsConfirmationPending = false;
	AppliedResolution = StagedResolution;
	AppliedWindowMode = StagedWindowMode;
	// Baseline from the live settings object so a Custom quality stays Custom (-1) after Keep
	AppliedScalabilityQuality = Settings ? Settings->ScalabilityQuality : StagedScalabilityQuality;
	AppliedQualityPreset = Settings ? Settings->GetOverallScalabilityLevel() : StagedQualityPreset;
	StagedScalabilityQuality = AppliedScalabilityQuality;
	StagedQualityPreset = AppliedQualityPreset;
	bAppliedVSync = bStagedVSync;
	AppliedFPSLimit = StagedFPSLimit;
	bHasStagedQualityPresetChange = false;

	return FReply::Handled();
}

FReply SBotfSettingsPanel::OnConfirmRevertClicked()
{
	RevertPendingVideoMode();
	return FReply::Handled();
}

void SBotfSettingsPanel::RevertPendingVideoMode()
{
	if (UGameUserSettings* Settings = GetUserSettings())
	{
		Settings->SetScreenResolution(PreApplyResolution);
		Settings->SetFullscreenMode(PreApplyWindowMode);
		Settings->ScalabilityQuality = PreApplyScalabilityQuality;
		Settings->SetVSyncEnabled(bPreApplyVSync);
		Settings->SetFrameRateLimit(PreApplyFPSLimit);

		// Restore prior full snapshot without persisting unconfirmed video mode
		Settings->ApplyResolutionSettings(false);
		Settings->ApplyNonResolutionSettings();
	}

	bIsConfirmationPending = false;

	StagedResolution = PreApplyResolution;
	StagedWindowMode = PreApplyWindowMode;
	StagedScalabilityQuality = PreApplyScalabilityQuality;
	StagedQualityPreset = PreApplyQualityPreset;
	bHasStagedQualityPresetChange = false;
	bStagedVSync = bPreApplyVSync;
	StagedFPSLimit = PreApplyFPSLimit;

	AppliedResolution = PreApplyResolution;
	AppliedWindowMode = PreApplyWindowMode;
	AppliedScalabilityQuality = PreApplyScalabilityQuality;
	AppliedQualityPreset = PreApplyQualityPreset;
	bAppliedVSync = bPreApplyVSync;
	AppliedFPSLimit = PreApplyFPSLimit;

	PopulateResolutionOptions();
	PopulateQualityOptions();
	PopulateFpsCapOptions();

	if (ResolutionComboBox.IsValid())
	{
		ResolutionComboBox->RefreshOptions();
	}
	if (QualityComboBox.IsValid())
	{
		QualityComboBox->RefreshOptions();
	}
	if (FpsCapComboBox.IsValid())
	{
		FpsCapComboBox->RefreshOptions();
	}
	if (WindowModeComboBox.IsValid())
	{
		WindowModeComboBox->RefreshOptions();
	}

	UpdateComboBoxSelections();
}

void SBotfSettingsPanel::OnWindowModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (bIsUpdatingUI || SelectInfo == ESelectInfo::Direct || !NewSelection.IsValid())
	{
		return;
	}

	if (*NewSelection == TEXT("Fullscreen"))
	{
		StagedWindowMode = EWindowMode::Fullscreen;
	}
	else if (*NewSelection == TEXT("Windowed Fullscreen (Borderless)"))
	{
		StagedWindowMode = EWindowMode::WindowedFullscreen;
	}
	else if (*NewSelection == TEXT("Windowed"))
	{
		StagedWindowMode = EWindowMode::Windowed;
	}
}

void SBotfSettingsPanel::OnResolutionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (bIsUpdatingUI || SelectInfo == ESelectInfo::Direct || !NewSelection.IsValid())
	{
		return;
	}

	const int32 SelectedIndex = ResolutionOptions.IndexOfByKey(NewSelection);
	if (SelectedIndex != INDEX_NONE && ResolutionValues.IsValidIndex(SelectedIndex))
	{
		StagedResolution = ResolutionValues[SelectedIndex];
	}
}

void SBotfSettingsPanel::OnQualityChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (bIsUpdatingUI || SelectInfo == ESelectInfo::Direct || !NewSelection.IsValid())
	{
		return;
	}

	const int32 SelectedIndex = QualityOptions.IndexOfByKey(NewSelection);
	if (SelectedIndex != INDEX_NONE && QualityValues.IsValidIndex(SelectedIndex))
	{
		const int32 ChosenPreset = QualityValues[SelectedIndex];
		if (ChosenPreset >= 0)
		{
			StagedQualityPreset = ChosenPreset;
			bHasStagedQualityPresetChange = true;
		}
		else
		{
			// "Custom" is only listed while the applied quality is custom: re-selecting it
			// restores the applied per-group levels instead of silently keeping a staged preset.
			StagedQualityPreset = AppliedQualityPreset;
			StagedScalabilityQuality = AppliedScalabilityQuality;
			bHasStagedQualityPresetChange = false;
		}
	}
}

void SBotfSettingsPanel::OnFpsCapChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (bIsUpdatingUI || SelectInfo == ESelectInfo::Direct || !NewSelection.IsValid())
	{
		return;
	}

	const int32 SelectedIndex = FpsCapOptions.IndexOfByKey(NewSelection);
	if (SelectedIndex != INDEX_NONE && FpsCapValues.IsValidIndex(SelectedIndex))
	{
		StagedFPSLimit = FpsCapValues[SelectedIndex];
	}
}

void SBotfSettingsPanel::OnVSyncCheckChanged(ECheckBoxState NewState)
{
	bStagedVSync = (NewState == ECheckBoxState::Checked);
}

FText SBotfSettingsPanel::GetCountdownText() const
{
	if (!bIsConfirmationPending)
	{
		return FText::GetEmpty();
	}

	const double RemainingSeconds = FMath::Max(0.0, ConfirmationDeadlineSeconds - FPlatformTime::Seconds());
	return FText::FromString(FString::Printf(TEXT("DISPLAY MODE CHANGED: Keep new display settings? Reverting in %d seconds..."), FMath::CeilToInt32(static_cast<float>(RemainingSeconds))));
}

EVisibility SBotfSettingsPanel::GetConfirmationBannerVisibility() const
{
	return bIsConfirmationPending ? EVisibility::Visible : EVisibility::Collapsed;
}
