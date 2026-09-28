#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "GenericPlatform/GenericWindow.h"
#include "Scalability.h"

class UGameUserSettings;
class STextComboBox;
class STextBlock;
class SButton;
class SCheckBox;
class SScrollBox;

/**
 * Standalone Slate settings panel for BOTF Remake.
 * Provides video, graphics, and frame rate controls backed by UGameUserSettings
 * with staged changes, confirmation countdown, and auto-revert safety.
 */
class SBotfSettingsPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBotfSettingsPanel)
		: _OnClose()
		, _ThemeEmpire(1)
	{}
		/** Callback triggered when the Back/Close button is clicked */
		SLATE_EVENT(FSimpleDelegate, OnClose)
		/** Empire id (1-5) whose LCARS palette styles the panel; presentation only */
		SLATE_ATTRIBUTE(int32, ThemeEmpire)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SBotfSettingsPanel() override;

	/** Called by outer menu shell whenever entering the settings view.
	 *  Synchronizes state with the actual live viewport and user settings. */
	void RefreshFromRuntime();

	/** Reverts any pending video test and discards unapplied staged adjustments.
	 *  Safe to call when leaving the settings surface (e.g. Escape or switcher tab change). */
	void CancelPendingChanges();

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Reads live settings and populates staged variables */
	void LoadCurrentSettings();

	/** Populates resolution options bounded by the primary desktop resolution */
	void PopulateResolutionOptions();

	/** Populates quality options including Custom/Cinematic */
	void PopulateQualityOptions();

	/** Populates FPS cap presets including custom limits */
	void PopulateFpsCapOptions();

	/** Helper to retrieve UGameUserSettings safely */
	static UGameUserSettings* GetUserSettings();

	/** Snapshots actual current viewport resolution and window mode (via GEngine->GameViewport/SWindow) */
	static void GetActualWindowAndResolution(FIntPoint& OutResolution, EWindowMode::Type& OutWindowMode);

	/** Event handlers */
	FReply OnApplyClicked();
	FReply OnRevertClicked();
	FReply OnBackClicked();
	FReply OnConfirmKeepClicked();
	FReply OnConfirmRevertClicked();

	void OnWindowModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnResolutionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnQualityChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnFpsCapChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnVSyncCheckChanged(ECheckBoxState NewState);

	/** Reverts unconfirmed video mode changes back to PreApply snapshot */
	void RevertPendingVideoMode();

	/** Helpers for UI state */
	bool HasUnappliedChanges() const;
	bool IsSettingsEditable() const { return !bIsConfirmationPending; }
	bool IsApplyEnabled() const { return HasUnappliedChanges() && !bIsConfirmationPending; }
	bool IsRevertEnabled() const { return HasUnappliedChanges() && !bIsConfirmationPending; }

	FText GetCountdownText() const;
	EVisibility GetConfirmationBannerVisibility() const;

	void UpdateComboBoxSelections();

private:
	FSimpleDelegate OnCloseDelegate;

	// Presentation only: never read by settings state functions
	TAttribute<int32> ThemeEmpire;

	// Applied baseline snapshot
	FIntPoint AppliedResolution;
	EWindowMode::Type AppliedWindowMode;
	Scalability::FQualityLevels AppliedScalabilityQuality;
	int32 AppliedQualityPreset = -1;
	bool bAppliedVSync = false;
	float AppliedFPSLimit = 0.0f;

	// Staged user adjustments
	FIntPoint StagedResolution;
	EWindowMode::Type StagedWindowMode;
	Scalability::FQualityLevels StagedScalabilityQuality;
	int32 StagedQualityPreset = -1;
	bool bHasStagedQualityPresetChange = false;
	bool bStagedVSync = false;
	float StagedFPSLimit = 0.0f;

	// Snapshot preserved specifically for the 15-second test revert
	FIntPoint PreApplyResolution;
	EWindowMode::Type PreApplyWindowMode;
	Scalability::FQualityLevels PreApplyScalabilityQuality;
	int32 PreApplyQualityPreset = -1;
	bool bPreApplyVSync = false;
	float PreApplyFPSLimit = 0.0f;

	// Confirmation state
	bool bIsConfirmationPending = false;
	double ConfirmationDeadlineSeconds = 0.0;
	static constexpr double ConfirmationTimeoutDuration = 15.0;

	// Guard against programmatic SetSelectedItem callbacks mutating staged values
	bool bIsUpdatingUI = false;

	// Dropdown item options
	TArray<TSharedPtr<FString>> WindowModeOptions;
	TArray<TSharedPtr<FString>> ResolutionOptions;
	TArray<FIntPoint> ResolutionValues;
	TArray<TSharedPtr<FString>> QualityOptions;
	TArray<int32> QualityValues;
	TArray<TSharedPtr<FString>> FpsCapOptions;
	TArray<float> FpsCapValues;

	// Widget references for synchronization
	TSharedPtr<STextComboBox> WindowModeComboBox;
	TSharedPtr<STextComboBox> ResolutionComboBox;
	TSharedPtr<STextComboBox> QualityComboBox;
	TSharedPtr<STextComboBox> FpsCapComboBox;
	TSharedPtr<SCheckBox> VSyncCheckBox;
};
