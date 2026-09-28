#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotfCombatScene.generated.h"

class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
namespace botf { struct CombatReport; }
namespace botf::tactical { struct Encounter; }

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
    virtual void Tick(float DeltaSeconds) override;
    UTextureRenderTarget2D* GetBattleTexture() const;
    // StationSlots: bit (Side * 8 + Index) marks a schema-13 station (an immobile unit, not a ship). Station slots
    // get a placeholder disc and never a ship mesh; a side made only of stations loads no ship mesh at all.
    void ConfigureBattle(int AttackerFaction, int DefenderFaction, int AttackerCount, int DefenderCount, uint32 Seed, uint32 StationSlots = 0);
    void ConfigureBattleReport(const botf::CombatReport& Report, int RoundIndex = -1);
    void ConfigureEncounter(const botf::tactical::Encounter& Encounter);
    void SetPresentationActive(bool bActive);

    // Modern 3D Combat Experience hooks:
    enum ECameraMode : int
    {
        Cam_Overview = 0,
        Cam_Chase = 1,
        Cam_FocusShip = 2,
        Cam_FocusTarget = 3,
        Cam_Overhead = 4,
        Cam_Count = 5
    };

    void SetCameraMode(int InMode);
    int GetCameraMode() const { return CameraMode; }
    void CycleCameraMode();
    void SetFocusedShips(int InShipId, int InTargetId);
    void AddCameraOrbit(float DeltaYaw, float DeltaPitch);
    void AddCameraZoom(float DeltaZoom);
    void ResetCameraOrbit();
    FString GetCameraModeName() const;

private:
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> BattleTexture;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Stars;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Ships;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ShieldShells;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ShieldMaterials;

    // Visual effect component pools
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BeamMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> BeamMaterials;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TorpedoMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> TorpedoMaterials;

    TArray<FVector> Positions;
    TArray<int> ShipOwners;
    TArray<FShipKinematicState> Kinematics;
    TArray<FSalvoVisualItem> Salvos;

    float Elapsed = 0.f;
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
    float ZoomOffset = 0.f;

    void SetupEffectPools();
    void AnimateRoundKinematics(float DeltaSeconds, float NormTime);
    void AnimateWeaponSalvos(float NormTime);
    void AnimateShieldsAndDamage(float DeltaSeconds, float NormTime);
    void AnimateCamera(float DeltaSeconds);
    static FLinearColor GetFactionBeamColor(int FactionId);
    static FLinearColor GetFactionTorpedoColor(int FactionId);
};
