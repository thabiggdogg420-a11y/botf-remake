#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <string>
#include <vector>
#include "BotfCombatScene.generated.h"

class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UDirectionalLightComponent;
class UNiagaraSystem;
class UNiagaraComponent;
namespace botf { struct CombatReport; struct CombatEvent; }
namespace botf::tactical { struct Encounter; }
namespace botf::combatfx { struct RoundTimeline; struct ShipInfo; struct VisualJob; class CinematicDirector; }
namespace botf::camera { struct Body; struct View; struct RigSmoother; }

// One ship as the battle camera sees it, for the tactical markers drawn over the 3D view.
struct FBotfShipMarker
{
    int32 FleetId = 0;
    int32 OwnerId = 0;
    FVector2D Screen = FVector2D::ZeroVector; // 0..1 across and down the rendered image
    float Radius = 0.f;                        // projected hull radius, as a fraction of the image height
    float Hull = 1.f;                          // fractions of the starting values
    float Shields = 0.f;
    bool bSelected = false;                    // the player's active ship
    bool bTarget = false;                      // the player's target
};

struct FShipKinematicState
{
    int FleetId = 0;
    int OwnerId = 0;
    FVector StartPosition = FVector::ZeroVector;
    FVector TargetPosition = FVector::ZeroVector;
    FVector CurrentPosition = FVector::ZeroVector;
    FRotator StartRotation = FRotator::ZeroRotator;
    FRotator TargetRotation = FRotator::ZeroRotator;
    FRotator CurrentRotation = FRotator::ZeroRotator;
    float BankRoll = 0.f;
    int InitialHull = 100;
    int Hull = 100;
    int InitialShields = 50;
    int Shields = 50;
    bool bAlive = true;
    bool bRetreated = false;
    bool bCloaked = false;
    bool bDecloaking = false;
    bool bDestroyedThisRound = false;
    float FlareStrength = 0.f;
    float DestructionProgress = 0.f;
    FVector BaseScale = FVector::OneVector; // Scale-normalised mesh scale; the destruction shrink multiplies it.
    double LengthMeters = 0.0;              // Canon length of the shown class (0 = unknown): flight style and wings.
    FVector SpacingOffset = FVector::ZeroVector; // Smoothed presentation shift that keeps true-scale hulls apart.
    // Rules 3 timeline playback: state at the start of the shown round (ships already gone get no track).
    bool bAliveAtRoundStart = true;
    int ShieldsAtRoundStart = 0;
    bool bCloakedAtRoundStart = false;
};

struct FSalvoVisualItem
{
    int AttackerVisualIndex = -1;
    int DefenderVisualIndex = -1;
    int BeamDamage = 0;
    int TorpedoDamage = 0;
    int AttackerOwner = 0;
    bool bIsRam = false;
};

UCLASS()
class BOTFREMAKE_API ABotfCombatScene : public AActor
{
    GENERATED_BODY()
public:
    ABotfCombatScene();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UTextureRenderTarget2D* GetBattleTexture() const;
    // Ships drawn per side. Raised from 8: the station mask is 64 bits wide (2 x 32) and every ship costs a mesh, a
    // shield shell and its share of the effect pools in the scene capture; 24 keeps a full rules 3 arena readable.
    static constexpr int32 MaxShipsPerSide = 24;
    // StationSlots: bit (Side * MaxShipsPerSide + Index) marks a schema-13 station (an immobile unit, not a ship). Station slots
    // get a placeholder disc and never a ship mesh; a side made only of stations loads no ship mesh at all.
    void ConfigureBattle(int AttackerFaction, int DefenderFaction, int AttackerCount, int DefenderCount, uint32 Seed, uint64 StationSlots = 0);
    void ConfigureBattleReport(const botf::CombatReport& Report, int RoundIndex = -1);
    void ConfigureEncounter(const botf::tactical::Encounter& Encounter);
    void SetPresentationActive(bool bActive);
    // Title-screen backdrop: the empire's hero ships (generated hulls) in formation over the gas giant, filmed by a
    // slow drifting camera. Render it with SetPresentationActive(true) and show GetBattleTexture() full-bleed behind
    // the menu; any ConfigureBattle* call ends it.
    void ConfigureTitleScene(int EmpireId);
    bool IsTitleScene() const { return bTitleScene; }
    // Listening player's empire: combat voice barks are only for its ships.
    void SetPlayerEmpire(int32 EmpireId) { PlayerEmpire = EmpireId; }
    // Seconds one rules 3 round (12 ticks) takes on screen.
    void SetTimelineRoundSeconds(float Seconds) { TimelineRoundSeconds = FMath::Clamp(Seconds, 2.f, 30.f); }
    bool IsTimelinePlaying() const { return bTimelinePlaying; }

    // Modern 3D Combat Experience hooks:
    enum ECameraMode : int
    {
        Cam_Overview = 0,
        Cam_Chase = 1,
        Cam_FocusShip = 2,
        Cam_FocusTarget = 3,
        Cam_Overhead = 4,
        Cam_Cinematic = 5, // Follows the action of a rules 3 round (BotfCombatTimeline.h CinematicDirector)
        Cam_Count = 6
    };

    void SetCameraMode(int InMode);
    int GetCameraMode() const { return CameraMode; }
    void CycleCameraMode();
    void SetFocusedShips(int InShipId, int InTargetId);
    void AddCameraOrbit(float DeltaYaw, float DeltaPitch);
    void AddCameraZoom(float WheelNotches);                       // + closer, - farther, about the screen centre
    void ResetCameraOrbit();                                      // orbit, zoom and pan back to the mode's own framing
    FString GetCameraModeName() const;
    // Interactive camera (BotfCameraRig.h). Screen points are 0..1 across and down the rendered image.
    void AddCameraZoomAt(float WheelNotches, FVector2D Screen01); // overview: zooms toward the point under the cursor
    void AddCameraPan(FVector2D DragScreen01);                    // drag distance as a fraction of the image; frees the camera
    int32 PickShipAt(FVector2D Screen01) const;                   // fleet id of the ship under the cursor, 0 = none
    void FollowFleet(int32 FleetId);                              // fly to and follow a ship (own: SHIP mode, enemy: TARGET)
    void FlyToScreenPoint(FVector2D Screen01);                    // fly the free camera to that spot on the battle plane
    void GetShipMarkers(TArray<FBotfShipMarker>& Out) const;

private:
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> BattleTexture;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Stars;
    // Environment (original generated art, /Game/Generated/Environment; Build-GeneratedArt.ps1): sky dome centred on the capture, a gas giant and a
    // moon far behind the arena, a warm key sun and a cool rim light. Missing assets fall back to the sphere starfield.
    UPROPERTY() TObjectPtr<UStaticMeshComponent> SkyDome;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Planet;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Moon;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> SunLight;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> RimLight;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> FillLight; // soft, from just above the camera: no hull face goes black
    void SetupEnvironment();
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Ships;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ShieldShells;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ShieldMaterials;

    // Visual effect component pools
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BeamMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> BeamMaterials;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TorpedoMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> TorpedoMaterials;

    // Rules 3 effect pools (Niagara-free fallbacks) and optional Niagara systems by asset key (vfx.*).
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FxBeams;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> FxBeamMats;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FxOrbs;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> FxOrbMats;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FxBubbles;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> FxBubbleMats;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FxFireballs; // M_FxFireball (Generated/VFX), eroded by Age
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> FxFireballMats;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FxDebris;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> FxLights;
    UPROPERTY() TMap<FString, TObjectPtr<UNiagaraSystem>> NiagaraCache;
    UPROPERTY() TMap<int32, TObjectPtr<UNiagaraComponent>> JobNiagara;
    TSet<FString> NiagaraMissing;
    TSet<int32> NiagaraTried;

    TSharedPtr<botf::combatfx::RoundTimeline> Timeline;
    TSharedPtr<botf::combatfx::CinematicDirector> Director;
    TArray<int32> TrackSlot;           // Timeline ship index -> visual slot (-1 = not drawn)
    TArray<TArray<FName>> SlotSockets; // Per visual slot: the mesh's socket names
    std::vector<std::vector<std::string>> SlotSocketNames;
    float TimelineTime = 0.f;
    float TimelineRoundSeconds = 6.f;
    bool bTimelinePlaying = false;
    bool bSuppressReportTimeline = false; // ConfigureEncounter starts the timeline itself
    int32 NextSound = 0;
    int32 NextBark = 0;
    int32 NextJobStart = 0;
    FString TimelineSignature;
    float PlaneCenterX = 0.f, PlaneCenterY = 0.f;
    int32 PlayerEmpire = 0;
    // Scene units per tactical cell. 170 gives true-scale hulls room: a 685 m Sovereign spans about 1.4 cells.
    static constexpr float CellUnits = 170.f;

    TArray<FVector> Positions;
    TArray<int> ShipOwners;
    TArray<FShipKinematicState> Kinematics;
    TArray<FSalvoVisualItem> Salvos;

    float Elapsed = 0.f;
    bool bTitleScene = false;
    FString TitleShotPath; // dev builds: -BotfTitleShot=<png> renders the title scene, saves one frame and quits
    TArray<FString> TitleShipOverride; // dev builds: -BotfTitleShips=<key,key,key> replaces the empire's hero classes
    float RoundAnimElapsed = 0.f;
    float RoundDuration = 2.4f;
    bool bIsRoundAnimating = false;
    int CameraMode = 0; // ECameraMode: Overview, Chase, FocusShip, FocusTarget, Overhead
    FVector BaseCameraLocation = FVector(0, -1750, 1100);
    FRotator BaseCameraRotation = FRotator(-32, 90, 0);
    int FocusedFriendlyId = 0;
    int FocusedTargetId = 0;
    float OrbitYaw = 0.f;
    float OrbitPitch = 0.f;
    float ZoomFactor = 1.f;         // multiplies the mode's framed distance
    // Free camera: set by panning, cursor zoom or fly-to in OVERVIEW; RESET returns to framing the whole battle.
    bool bFreeCam = false;
    FVector FreeFocus = FVector::ZeroVector; // tactical frame (x, y across the plane, z height)
    double FreeDistance = 2000.0;
    double MaxCameraDistance = 6000.0;
    float CamTransition = 0.f;      // seconds of soft focus easing left after a mode or subject change
    TSharedPtr<botf::camera::RigSmoother> CamSmoother;
    TSharedPtr<botf::camera::View> CamView; // the view last filmed: markers and picking use it
    void CollectCameraBodies(std::vector<botf::camera::Body>& Out) const;
    void DetachToFreeCam();

    void SetupEffectPools();
    // Swaps a slot to its class mesh by asset key (UBotfAssetRegistry), scale-normalised by canon length.
    void ApplyShipMesh(int VisualIndex, int ReferenceShipId, int OwnerFaction, bool bLegacyDestroyer);
    void ApplyStationMesh(int VisualIndex, int OwnerFaction);
    void AnimateRoundKinematics(float DeltaSeconds, float NormTime);
    void AnimateWeaponSalvos(float NormTime);
    void AnimateShieldsAndDamage(float DeltaSeconds, float NormTime);
    void AnimateCamera(float DeltaSeconds);

    // Rules 3 playback (BotfCombatTimeline.h).
    void SetupTimelinePools();
    void ClearTimelineFx();
    void StartTimeline(const std::vector<botf::CombatEvent>& Events, const std::vector<botf::combatfx::ShipInfo>& Infos, const FString& Signature);
    void TickTimeline(float DeltaSeconds);
    void FinishTimeline();
    void CacheSlotSockets(int32 VisualIndex);
    int32 SlotForFleet(int32 FleetId) const;
    FVector PlaneToScene(double X, double Y, double AltitudeCells) const;
    FQuat PoseRotation(double FacingDegrees, double BankDegrees, double PitchDegrees = 0.0) const;
    FVector MuzzleLocation(int32 VisualIndex, int32 Family, int32 MountIndex, int32 EmitterIndex) const;
    float ShipRadius(int32 VisualIndex) const;
    UNiagaraSystem* FindNiagara(const std::vector<std::string>& Keys);
    bool UpdateJobNiagara(int32 JobIndex, const botf::combatfx::VisualJob& Job, const FVector& From, const FVector& To, float U);
    static FLinearColor GetFactionBeamColor(int FactionId);
    static FLinearColor GetFactionTorpedoColor(int FactionId);
};
