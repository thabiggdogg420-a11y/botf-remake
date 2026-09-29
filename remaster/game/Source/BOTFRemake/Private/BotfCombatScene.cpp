#include "BotfCombatScene.h"
#include "Simulation.h"
#include "TacticalCombat.h"
#include "BotfAssetRegistry.h"
#include "BotfAudioSubsystem.h"
#include "BotfCombatTimeline.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

namespace
{
// Shields read as a hit response, not a permanent bubble: invisible at rest, a flare on impact whose brightness follows
// the remaining shield fraction (a failing shield answers weakly).
float ShieldDisplayStrength(float Fraction, float Flare)
{
    return Flare > 0.01f ? Flare * (0.3f + 0.7f * FMath::Clamp(Fraction, 0.f, 1.f)) : 0.f;
}

// Fits the engine sphere (radius 50) around the hull as an ellipsoid; no axis thinner than 40% of the longest.
void FitShieldShell(UStaticMeshComponent* Shell, const UStaticMeshComponent* Ship, const FVector& ShipScale, const FQuat& Rotation)
{
    if (!Shell || !Ship || !Ship->GetStaticMesh()) return;
    const FVector Extent = Ship->GetStaticMesh()->GetBounds().BoxExtent * ShipScale * 1.3f;
    const float Floor = Extent.GetMax() * 0.4f;
    Shell->SetRelativeRotation(Rotation);
    Shell->SetRelativeScale3D(FVector(FMath::Max(Extent.X, Floor), FMath::Max(Extent.Y, Floor), FMath::Max(Extent.Z, Floor)) / 50.f);
}
}

ABotfCombatScene::ABotfCombatScene()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("BattleCapture"));
    Capture->SetupAttachment(RootComponent);
    Capture->SetRelativeLocation(BaseCameraLocation);
    Capture->SetRelativeRotation(BaseCameraRotation);
    Capture->FOVAngle = 55;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
    Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
    Capture->PostProcessSettings.AutoExposureMinBrightness = 1.f;
    Capture->PostProcessSettings.AutoExposureMaxBrightness = 1.f;
    // Cinematic grade: bloom so beams, torpedoes and explosions glow, a soft lens flare on the brightest flashes,
    // vignette, a little chromatic fringe and film grain, and a cool-shadow / warm-highlight split.
    FPostProcessSettings& Post = Capture->PostProcessSettings;
    Post.bOverride_BloomIntensity = true;            Post.BloomIntensity = 1.1f;
    Post.bOverride_BloomThreshold = true;            Post.BloomThreshold = 1.0f;
    Post.bOverride_LensFlareIntensity = true;        Post.LensFlareIntensity = 0.35f;
    Post.bOverride_VignetteIntensity = true;         Post.VignetteIntensity = 0.55f;
    Post.bOverride_SceneFringeIntensity = true;      Post.SceneFringeIntensity = 0.6f;
    Post.bOverride_FilmGrainIntensity = true;        Post.FilmGrainIntensity = 0.06f;
    Post.bOverride_ColorSaturation = true;           Post.ColorSaturation = FVector4(1.08f, 1.08f, 1.12f, 1.f);
    Post.bOverride_ColorContrast = true;             Post.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.f);
    Post.bOverride_ColorGainShadows = true;          Post.ColorGainShadows = FVector4(0.94f, 0.98f, 1.08f, 1.f);
    Post.bOverride_ColorGainHighlights = true;       Post.ColorGainHighlights = FVector4(1.06f, 1.0f, 0.93f, 1.f);

    auto* Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
    Light->SetupAttachment(RootComponent);
    Light->SetRelativeLocation(FVector(0, -500, 1100));
    Light->SetIntensity(60000.f);   // soft fill; the sun and rim lights (SetupEnvironment) carry the key
    Light->SetAttenuationRadius(7000.f);
    Light->SetCastShadows(false);
}

UTextureRenderTarget2D* ABotfCombatScene::GetBattleTexture() const { return BattleTexture; }

// Scale that gives a sphere body the radius Radius, whichever sphere mesh it carries.
static FVector BodyScale(const UStaticMeshComponent* Body, float Radius)
{
    const UStaticMesh* Mesh = Body ? Body->GetStaticMesh() : nullptr;
    return FVector(Radius / FMath::Max(1.f, Mesh ? Mesh->GetBounds().SphereRadius : 50.f));
}

void ABotfCombatScene::SetupEnvironment()
{
    // The generated 256-segment sphere keeps the gas giant's limb round at title-screen size; the basic shape is the
    // fallback when generated art is missing.
    auto* SphereMesh = FPackageName::DoesPackageExist(TEXT("/Game/Generated/Environment/SM_BodySphere"))
        ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Generated/Environment/SM_BodySphere.SM_BodySphere")) : nullptr;
    if (!SphereMesh) SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* SkyMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Environment/M_SkySphere.M_SkySphere"));
    auto* GiantMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Environment/MI_GasGiant.MI_GasGiant"));
    auto* MoonMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Environment/MI_RockyMoon.MI_RockyMoon"));
    const auto MakeBody = [this, SphereMesh](UMaterialInterface* Material, const FVector& Location, float Radius) -> UStaticMeshComponent*
    {
        if (!SphereMesh || !Material) return nullptr;
        auto* Body = NewObject<UStaticMeshComponent>(this);
        Body->SetupAttachment(RootComponent);
        Body->SetStaticMesh(SphereMesh);
        Body->SetMaterial(0, Material);
        Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Body->SetCastShadow(false);
        Body->SetRelativeLocation(Location);
        Body->SetRelativeScale3D(BodyScale(Body, Radius));
        Body->RegisterComponent();
        Capture->ShowOnlyComponent(Body);
        return Body;
    };
    if (SkyMaterial)
    {
        auto* SkyInstance = UMaterialInstanceDynamic::Create(SkyMaterial, this);
        SkyInstance->SetScalarParameterValue(TEXT("Brightness"), 0.9f);
        SkyDome = MakeBody(SkyInstance, BaseCameraLocation, 60000.f);
    }
    // Behind the arena as the default camera sees it (the camera looks along +Y, tilted down).
    // Placed each frame relative to the camera (AnimateCamera); the radii set their apparent size at that distance.
    Planet = MakeBody(GiantMaterial, FVector(0.f, 26000.f, 0.f), 5600.f);
    if (Planet) Planet->SetRelativeRotation(FRotator(18.f, 0.f, -12.f));
    Moon = MakeBody(MoonMaterial, FVector(0.f, 30000.f, 0.f), 700.f);

    // Key light from the upper left in front of the fleets, warm like a nearby star; a cool rim from behind.
    SunLight = NewObject<UDirectionalLightComponent>(this);
    SunLight->SetupAttachment(RootComponent);
    SunLight->SetRelativeRotation(FRotator(-38.f, 62.f, 0.f));
    SunLight->SetIntensity(4.0f);
    SunLight->SetLightColor(FLinearColor(1.0f, 0.90f, 0.78f));
    SunLight->SetCastShadows(false);
    SunLight->RegisterComponent();
    RimLight = NewObject<UDirectionalLightComponent>(this);
    RimLight->SetupAttachment(RootComponent);
    RimLight->SetRelativeRotation(FRotator(-10.f, -118.f, 0.f));
    RimLight->SetIntensity(2.2f);
    RimLight->SetLightColor(FLinearColor(0.45f, 0.62f, 1.0f));
    RimLight->SetCastShadows(false);
    RimLight->RegisterComponent();
}

void ABotfCombatScene::SetCameraMode(int InMode)
{
    CameraMode = FMath::Clamp(InMode, 0, static_cast<int>(Cam_Count) - 1);
}

void ABotfCombatScene::CycleCameraMode()
{
    CameraMode = (CameraMode + 1) % static_cast<int>(Cam_Count);
}

void ABotfCombatScene::SetFocusedShips(int InShipId, int InTargetId)
{
    FocusedFriendlyId = InShipId;
    FocusedTargetId = InTargetId;
}

void ABotfCombatScene::AddCameraOrbit(float DeltaYaw, float DeltaPitch)
{
    OrbitYaw = FMath::Fmod(OrbitYaw + DeltaYaw, 360.f);
    OrbitPitch = FMath::Clamp(OrbitPitch + DeltaPitch, -65.f, 65.f);
}

void ABotfCombatScene::AddCameraZoom(float DeltaZoom)
{
    ZoomOffset = FMath::Clamp(ZoomOffset + DeltaZoom, -1200.f, 1500.f);
}

void ABotfCombatScene::ResetCameraOrbit()
{
    OrbitYaw = 0.f;
    OrbitPitch = 0.f;
    ZoomOffset = 0.f;
}

FString ABotfCombatScene::GetCameraModeName() const
{
    switch (CameraMode)
    {
    case Cam_Overview: return TEXT("OVERVIEW");
    case Cam_Chase: return TEXT("DYNAMIC CHASE");
    case Cam_FocusShip: return TEXT("FOCUS SHIP");
    case Cam_FocusTarget: return TEXT("FOCUS TARGET");
    case Cam_Overhead: return TEXT("TOP-DOWN");
    case Cam_Cinematic: return TEXT("CINEMATIC");
    default: return TEXT("TACTICAL");
    }
}

FLinearColor ABotfCombatScene::GetFactionBeamColor(int FactionId)
{
    switch (FactionId)
    {
        case 1: return FLinearColor(1.0f, 0.45f, 0.05f, 1.0f); // Federation: Amber/Orange phasers
        case 2: return FLinearColor(0.1f, 1.0f, 0.2f, 1.0f);  // Klingon: Emerald disruptors
        case 3: return FLinearColor(0.05f, 0.95f, 0.35f, 1.0f);// Romulan: Vivid green plasma disruptors
        case 4: return FLinearColor(1.0f, 0.85f, 0.1f, 1.0f); // Cardassian: Amber-yellow spiral disruptors
        case 5: return FLinearColor(0.1f, 0.75f, 1.0f, 1.0f); // Ferengi: Cyan particle beam
        default: return FLinearColor(1.0f, 0.5f, 0.2f, 1.0f);
    }
}

FLinearColor ABotfCombatScene::GetFactionTorpedoColor(int FactionId)
{
    switch (FactionId)
    {
        case 1: return FLinearColor(1.0f, 0.15f, 0.05f, 1.0f); // Federation: Red photon torpedo
        case 2: return FLinearColor(1.0f, 0.35f, 0.05f, 1.0f); // Klingon: Orange-red photon torpedo
        case 3: return FLinearColor(0.2f, 1.0f, 0.4f, 1.0f);  // Romulan: High-yield plasma torpedo
        case 4: return FLinearColor(1.0f, 0.45f, 0.05f, 1.0f); // Cardassian: Heavy plasma charge
        case 5: return FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);  // Ferengi: Ion charge
        default: return FLinearColor(1.0f, 0.2f, 0.1f, 1.0f);
    }
}

void ABotfCombatScene::SetupEffectPools()
{
    auto* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!CylinderMesh) CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));

    auto* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/M_SpaceGlow.M_SpaceGlow"));
    if (!GlowMaterial) GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));

    // Pool of 8 beam meshes
    for (int Index = 0; Index < 8; ++Index)
    {
        if (CylinderMesh && GlowMaterial)
        {
            auto* Beam = NewObject<UStaticMeshComponent>(this);
            Beam->SetupAttachment(RootComponent);
            Beam->SetStaticMesh(CylinderMesh);
            Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Beam->SetCastShadow(false);
            Beam->SetVisibility(false);
            auto* MatInst = UMaterialInstanceDynamic::Create(GlowMaterial, this);
            Beam->SetMaterial(0, MatInst);
            Beam->RegisterComponent();
            Capture->ShowOnlyComponent(Beam);
            BeamMeshes.Add(Beam);
            BeamMaterials.Add(MatInst);
        }
    }

    // Pool of 8 torpedo meshes
    for (int Index = 0; Index < 8; ++Index)
    {
        if (SphereMesh && GlowMaterial)
        {
            auto* Torp = NewObject<UStaticMeshComponent>(this);
            Torp->SetupAttachment(RootComponent);
            Torp->SetStaticMesh(SphereMesh);
            Torp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Torp->SetCastShadow(false);
            Torp->SetVisibility(false);
            Torp->SetRelativeScale3D(FVector(0.35f));
            auto* MatInst = UMaterialInstanceDynamic::Create(GlowMaterial, this);
            Torp->SetMaterial(0, MatInst);
            Torp->RegisterComponent();
            Capture->ShowOnlyComponent(Torp);
            TorpedoMeshes.Add(Torp);
            TorpedoMaterials.Add(MatInst);
        }
    }
}

void ABotfCombatScene::ConfigureTitleScene(int EmpireId)
{
    const int Faction = FMath::Clamp(EmpireId, 1, 5);
    // Lead ship first; the escorts fly off its quarters. Classes without a generated hull keep the faction mesh.
    static const TCHAR* Heroes[5][3] = {
        {TEXT("ship.fed.sovereign"), TEXT("ship.fed.galaxy"), TEXT("ship.fed.defiant")},
        {TEXT("ship.kli.neghvar"), TEXT("ship.kli.vorcha"), TEXT("ship.kli.bop")},
        {TEXT("ship.rom.dderidex"), TEXT("ship.rom.valdore"), TEXT("ship.rom.bop")},
        {TEXT("ship.car.keldon"), TEXT("ship.car.galor"), TEXT("ship.car.hideki")},
        {TEXT("ship.fer.dkora"), TEXT("ship.fer.nagus"), TEXT("ship.fer.pod")}};
    // Ship frame (bow +X before the heading turn): the escorts trail high and low, deeper than the hero, so all three
    // stay in frame against the gas giant.
    static const FVector Formation[3] = {FVector(0.f, 0.f, 0.f), FVector(-360.f, 165.f, -110.f), FVector(-360.f, 300.f, 90.f)};
    ConfigureBattle(Faction, Faction, 3, 0, 1999);
    bTitleScene = true;
    const UBotfAssetRegistry* Registry = UBotfAssetRegistry::Get();
    double HeroLength = 0.0;
    for (int Index = 0; Index < Ships.Num() && Index < 3; ++Index)
    {
        auto* Ship = Ships[Index].Get();
        if (!Ship) continue;
        FString Key = Heroes[Faction - 1][Index];
        if (TitleShipOverride.IsValidIndex(Index)) Key = TitleShipOverride[Index];
        // A local model (hero ship) first, else the class's procedural hull.
        UStaticMesh* Mesh = nullptr;
        double LengthMeters = 0.0;
        if (Registry)
        {
            const auto Resolution = Registry->GetCatalog().resolve(TCHAR_TO_UTF8(*Key));
            LengthMeters = Resolution.targetLengthMeters;
            if (!Resolution.placeholder && Resolution.resolvedKey == Resolution.requestedKey)
                Mesh = Cast<UStaticMesh>(FSoftObjectPath(UTF8_TO_TCHAR(Resolution.primaryPath().c_str())).TryLoad());
        }
        if (!Mesh)
        {
            const FString Path = UTF8_TO_TCHAR(botf::assets::generatedShipPath(TCHAR_TO_UTF8(*Key)).c_str());
            if (!Path.IsEmpty() && FPackageName::DoesPackageExist(FSoftObjectPath(Path).GetLongPackageName()))
                Mesh = Cast<UStaticMesh>(FSoftObjectPath(Path).TryLoad());
        }
        if (Mesh) Ship->SetStaticMesh(Mesh);
        // The hero reads largest; escorts keep their canon size relative to it (a Defiant beside a Sovereign is a
        // quarter of its length), never below a readable minimum.
        if (Index == 0) HeroLength = LengthMeters;
        float Size = 170.f;
        if (Index > 0)
            Size = (HeroLength > 0.0 && LengthMeters > 0.0) ? FMath::Clamp(170.f * static_cast<float>(LengthMeters / HeroLength), 45.f, 150.f) : 110.f;
        const FVector Scale(Size / FMath::Max(1.f, Ship->GetStaticMesh()->GetBounds().SphereRadius));
        Ship->SetRelativeScale3D(Scale);
        // The formation turns its bows toward the camera side (-Y) for a three-quarter hero view.
        const FRotator Heading(0.f, -60.f, 0.f);
        const FVector At = Heading.RotateVector(Formation[Index]);
        Ship->SetRelativeRotation(FRotator(0.f, Heading.Yaw, Index == 0 ? 0.f : (Index == 1 ? -8.f : 8.f)));
        Ship->SetRelativeLocation(At);
        if (Kinematics.IsValidIndex(Index))
        {
            Kinematics[Index].BaseScale = Scale;
            Kinematics[Index].CurrentPosition = At;
            Kinematics[Index].bAlive = true;
            Kinematics[Index].bRetreated = false;
        }
        if (ShieldShells.IsValidIndex(Index) && ShieldShells[Index]) ShieldShells[Index]->SetVisibility(false);
    }
    // Title lighting: the sun from the side so the gas giant shows a terminator and hulls get a hard rim; the planet
    // drawn larger. ConfigureBattle restores both.
    if (SunLight) SunLight->SetRelativeRotation(FRotator(-18.f, 178.f, 0.f));
    if (Planet) Planet->SetRelativeScale3D(BodyScale(Planet, 8200.f));
    Capture->PostProcessSettings.LensFlareIntensity = 0.f; // flare ghosts read as smudges on a still backdrop
    CameraMode = Cam_Cinematic;
    ResetCameraOrbit();
    Elapsed = 0.f;
}

void ABotfCombatScene::ConfigureBattle(int AttackerFaction, int DefenderFaction, int AttackerCount, int DefenderCount, uint32 Seed, uint64 StationSlots)
{
    if (bTitleScene)
    {
        if (SunLight) SunLight->SetRelativeRotation(FRotator(-38.f, 62.f, 0.f));
        if (Planet) Planet->SetRelativeScale3D(BodyScale(Planet, 5600.f));
        Capture->PostProcessSettings.LensFlareIntensity = 0.35f;
    }
    bTitleScene = false;
    if (!BattleTexture)
    {
        BattleTexture = NewObject<UTextureRenderTarget2D>(this);
        BattleTexture->ClearColor = FLinearColor(0.002f, 0.004f, 0.012f);
        BattleTexture->InitAutoFormat(1920, 1080);
        Capture->TextureTarget = BattleTexture;
        SetupEnvironment();
    }
    if (!SkyDome && Stars.IsEmpty())
    {
        auto* StarMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        auto* StarGlow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/M_SpaceGlow.M_SpaceGlow"));
        auto* StarLit = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
        const FRotator ViewRotation = Capture->GetRelativeRotation();
        const FVector ViewCenter = Capture->GetRelativeLocation() + ViewRotation.Vector() * 3000.f;
        const FVector ViewRight = ViewRotation.RotateVector(FVector::RightVector);
        const FVector ViewUp = ViewRotation.RotateVector(FVector::UpVector);
        FRandomStream StarRandom(1701);
        if (StarMesh && (StarGlow || StarLit)) for (int Index = 0; Index < 135; ++Index)
        {
            auto* Star = NewObject<UStaticMeshComponent>(this);
            Star->SetupAttachment(RootComponent);
            Star->SetStaticMesh(StarMesh);
            Star->SetMaterial(0, (Index % 4 == 0 && StarLit) ? StarLit : (StarGlow ? StarGlow : StarLit));
            Star->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Star->SetCastShadow(false);
            const float ScreenX = StarRandom.FRandRange(-1400.f, 1400.f);
            const float ScreenY = StarRandom.FRandRange(-760.f, 760.f);
            Star->SetRelativeLocation(ViewCenter + ViewRight * ScreenX + ViewUp * ScreenY);
            Star->SetRelativeScale3D(FVector(Index % 17 == 0 ? 0.12f : StarRandom.FRandRange(0.035f, 0.08f)));
            Star->RegisterComponent();
            Stars.Add(Star);
            Capture->ShowOnlyComponent(Star);
        }
    }

    for (const auto& Ship : Ships) if (Ship)
    {
        Capture->RemoveShowOnlyComponent(Ship);
        Ship->DestroyComponent();
    }
    for (const auto& Shell : ShieldShells) if (Shell)
    {
        Capture->RemoveShowOnlyComponent(Shell);
        Shell->DestroyComponent();
    }
    for (const auto& Beam : BeamMeshes) if (Beam)
    {
        Capture->RemoveShowOnlyComponent(Beam);
        Beam->DestroyComponent();
    }
    for (const auto& Torp : TorpedoMeshes) if (Torp)
    {
        Capture->RemoveShowOnlyComponent(Torp);
        Torp->DestroyComponent();
    }

    ClearTimelineFx();
    for (const auto* Pool : {&FxBeams, &FxOrbs, &FxBubbles, &FxFireballs, &FxDebris})
        for (const auto& Fx : *Pool) if (Fx)
        {
            Capture->RemoveShowOnlyComponent(Fx);
            Fx->DestroyComponent();
        }
    for (const auto& Light : FxLights) if (Light) Light->DestroyComponent();
    FxBeams.Reset(); FxBeamMats.Reset(); FxOrbs.Reset(); FxOrbMats.Reset(); FxBubbles.Reset(); FxBubbleMats.Reset();
    FxFireballs.Reset(); FxFireballMats.Reset(); FxDebris.Reset(); FxLights.Reset();
    bTimelinePlaying = false;
    TrackSlot.Reset(); SlotSockets.Reset(); SlotSocketNames.clear();

    Ships.Reset(); Positions.Reset(); ShipOwners.Reset();
    ShieldShells.Reset(); ShieldMaterials.Reset();
    BeamMeshes.Reset(); BeamMaterials.Reset();
    TorpedoMeshes.Reset(); TorpedoMaterials.Reset();
    Kinematics.Reset(); Salvos.Reset();

    SetupEffectPools();
    SetupTimelinePools();

    auto* ShieldMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* ShieldMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/M_ShieldShell.M_ShieldShell"));
    static const TCHAR* Names[] = {TEXT("Federation"), TEXT("Klingon"), TEXT("Romulan"), TEXT("Cardassian"), TEXT("Ferengi")};
    // Stations (schema 13) are immobile units recorded like ships: they get a flat placeholder disc, never a ship mesh.
    const auto IsStationSlot = [StationSlots](int Side, int Index)
    {
        return Index < MaxShipsPerSide && (StationSlots & (uint64(1) << (Side * MaxShipsPerSide + Index))) != 0;
    };
    UStaticMesh* StationMesh = nullptr;
    if (StationSlots)
    {
        StationMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        if (!StationMesh) StationMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    }
    FRandomStream Random(Seed);
    for (int Side = 0; Side < 2; ++Side)
    {
        const int Faction = FMath::Clamp(Side == 0 ? AttackerFaction : DefenderFaction, 1, 5);
        const int Count = FMath::Clamp(Side == 0 ? AttackerCount : DefenderCount, 0, MaxShipsPerSide);
        bool bNeedsShipMesh = false;
        for (int Index = 0; Index < Count; ++Index) bNeedsShipMesh = bNeedsShipMesh || !IsStationSlot(Side, Index);
        UStaticMesh* Mesh = nullptr;
        if (bNeedsShipMesh)
        {
            const FString Name(Names[Faction - 1]);
            const FString Path = FString::Printf(TEXT("/Game/Presentation/%s/StaticMeshes/%s.%s"), *Name, *Name, *Name);
            Mesh = Faction == 1 ? LoadObject<UStaticMesh>(nullptr,
                TEXT("/Game/Presentation/Community/franger/StaticMeshes/franger.franger")) : nullptr;
            if (!Mesh) Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
            if (!Mesh)
            {
                UE_LOG(LogTemp, Warning, TEXT("BOTF battle visualization missing mesh: %s"), *Path);
                Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
            }
        }
        if (!Mesh) Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        if (!Mesh) continue;

        // Two columns up to 8 ships (the original layout); larger sides add columns and tighten the rows.
        const int Columns = Count <= 8 ? 2 : (Count + 3) / 4;
        const int Rows = (Count + Columns - 1) / Columns;
        const float RowSpacing = Rows <= 4 ? 230.f : 920.f / (Rows - 1);
        for (int Index = 0; Index < Count; ++Index)
        {
            const bool bStation = IsStationSlot(Side, Index) && StationMesh != nullptr;
            UStaticMesh* SlotMesh = bStation ? StationMesh : Mesh;
            auto* Ship = NewObject<UStaticMeshComponent>(this);
            Ship->SetupAttachment(RootComponent);
            Ship->SetStaticMesh(SlotMesh);
            Ship->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Ship->RegisterComponent();
            const float Radius = FMath::Max(1.f, SlotMesh->GetBounds().SphereRadius);
            const FVector BaseScale = bStation ? FVector(170.f / Radius, 170.f / Radius, 40.f / Radius) : FVector(80.f / Radius);
            Ship->SetRelativeScale3D(BaseScale);
            const int Row = Index / Columns;
            const int Column = Index % Columns;
            const FVector Position((Side == 0 ? -1.f : 1.f) * (Count == 1 ? 445.f : 335.f + Column * 235.f),
                (Row - (Rows - 1) * 0.5f) * 60.f, (Row - (Rows - 1) * 0.5f) * RowSpacing + Random.FRandRange(-10.f, 10.f));
            const FRotator Rotation(0, Side == 0 ? 0.f : 180.f, 0);

            Ship->SetRelativeLocation(Position);
            Ship->SetRelativeRotation(Rotation);
            Ships.Add(Ship); Positions.Add(Position); ShipOwners.Add(Faction);
            CacheSlotSockets(Ships.Num() - 1);
            Capture->ShowOnlyComponent(Ship);

            FShipKinematicState Kin;
            Kin.OwnerId = Faction;
            Kin.StartPosition = Position;
            Kin.TargetPosition = Position;
            Kin.CurrentPosition = Position;
            Kin.StartRotation = Rotation;
            Kin.TargetRotation = Rotation;
            Kin.CurrentRotation = Rotation;
            Kin.BaseScale = BaseScale;
            Kinematics.Add(Kin);

            UStaticMeshComponent* Shell = nullptr;
            UMaterialInstanceDynamic* ShieldInstance = nullptr;
            if (ShieldMesh && ShieldMaterial)
            {
                Shell = NewObject<UStaticMeshComponent>(this);
                Shell->SetupAttachment(RootComponent);
                Shell->SetStaticMesh(ShieldMesh);
                Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Shell->SetCastShadow(false);
                Shell->SetRelativeLocation(Position);
                Shell->SetRelativeScale3D(FVector(1.85f));
                ShieldInstance = UMaterialInstanceDynamic::Create(ShieldMaterial, this);
                Shell->SetMaterial(0, ShieldInstance);
                Shell->SetVisibility(false);
                Shell->RegisterComponent();
                Capture->ShowOnlyComponent(Shell);
            }
            ShieldShells.Add(Shell);
            ShieldMaterials.Add(ShieldInstance);
        }
    }
    Elapsed = 0.f;
    RoundAnimElapsed = 0.f;
    bIsRoundAnimating = false;
    if (!Capture->bCaptureEveryFrame) Capture->CaptureScene();
}

void ABotfCombatScene::ApplyShipMesh(int VisualIndex, int ReferenceShipId, int OwnerFaction, bool bLegacyDestroyer)
{
    if (!Ships.IsValidIndex(VisualIndex) || !Ships[VisualIndex]) return;
    if (Kinematics.IsValidIndex(VisualIndex)) Kinematics[VisualIndex].LengthMeters = 0.0; // slots are reused per battle
    UStaticMesh* Mesh = nullptr;
    double LengthMeters = 0.0;
    FString Key;
    const UBotfAssetRegistry* Registry = UBotfAssetRegistry::Get();
    if (Registry && ReferenceShipId >= 0)
    {
        // Without a local model the chain ends at a generic faction placeholder (study.<fac> or built-in); keep the
        // scene's own placeholder then (it may be a better community mesh, e.g. the Federation franger).
        const auto Resolution = Registry->GetCatalog().resolveShip(ReferenceShipId, OwnerFaction);
        if (!Resolution.placeholder && Resolution.resolvedKey.rfind("study.", 0) != 0)
            Mesh = Registry->LoadShipMesh(ReferenceShipId, OwnerFaction, LengthMeters, Key);
        else
        {
            // No local model: the class's procedural hull when Build-GeneratedArt.ps1 has made it (/Game/Generated).
            const FString Generated = UTF8_TO_TCHAR(botf::assets::generatedShipPath(Resolution.requestedKey).c_str());
            if (!Generated.IsEmpty() && FPackageName::DoesPackageExist(FSoftObjectPath(Generated).GetLongPackageName()))
                Mesh = Cast<UStaticMesh>(FSoftObjectPath(Generated).TryLoad());
            if (Mesh)
            {
                Key = UTF8_TO_TCHAR(Resolution.requestedKey.c_str());
                LengthMeters = Resolution.targetLengthMeters > 0.0 ? Resolution.targetLengthMeters
                    : Mesh->GetBounds().BoxExtent.GetMax() * 2.0 / 100.0; // Blender metres import as centimetres
            }
        }
    }
    // Interim art kept from the prototype: Federation destroyers without a local model use the community Ulysses mesh.
    if (!Mesh && bLegacyDestroyer && OwnerFaction == 1)
        Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/Community/fUlysses/StaticMeshes/fUlysses.fUlysses"));
    if (!Mesh) return;
    auto* Ship = Ships[VisualIndex].Get();
    Ship->SetStaticMesh(Mesh);
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BaseScale(static_cast<float>(botf::assets::uniformScaleFor(LengthMeters, Bounds.SphereRadius, Bounds.BoxExtent.X * 2.0)));
    Ship->SetRelativeScale3D(BaseScale);
    if (Kinematics.IsValidIndex(VisualIndex))
    {
        Kinematics[VisualIndex].BaseScale = BaseScale;
        Kinematics[VisualIndex].LengthMeters = LengthMeters;
    }
    CacheSlotSockets(VisualIndex);
    if (!Key.IsEmpty()) UE_LOG(LogTemp, Verbose, TEXT("BOTF battle: ship %d uses %s"), ReferenceShipId, *Key);
}

void ABotfCombatScene::ApplyStationMesh(int VisualIndex, int OwnerFaction)
{
    // The faction's procedural starbase (or outpost) when Build-GeneratedArt.ps1 has made one; else the disc stays.
    if (!Ships.IsValidIndex(VisualIndex) || !Ships[VisualIndex]) return;
    const std::string Prefix = botf::assets::ownerFactionPrefix(OwnerFaction);
    UStaticMesh* Mesh = nullptr;
    for (const char* Kind : {".starbase", ".outpost"})
    {
        const FString Path = UTF8_TO_TCHAR(botf::assets::generatedShipPath("ship." + Prefix + Kind).c_str());
        if (!Path.IsEmpty() && FPackageName::DoesPackageExist(FSoftObjectPath(Path).GetLongPackageName()))
            Mesh = Cast<UStaticMesh>(FSoftObjectPath(Path).TryLoad());
        if (Mesh) break;
    }
    if (!Mesh) return;
    auto* Station = Ships[VisualIndex].Get();
    Station->SetStaticMesh(Mesh);
    const FVector BaseScale(170.f / FMath::Max(1.f, Mesh->GetBounds().SphereRadius)); // the disc's footprint
    Station->SetRelativeScale3D(BaseScale);
    if (Kinematics.IsValidIndex(VisualIndex)) Kinematics[VisualIndex].BaseScale = BaseScale;
    CacheSlotSockets(VisualIndex);
}

void ABotfCombatScene::SetPresentationActive(bool bActive)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!TitleShotPath.IsEmpty()) bActive = true; // a pending -BotfTitleShot keeps rendering until its frame is saved
#endif
    SetActorTickEnabled(bActive);
    Capture->bCaptureEveryFrame = bActive;
}

void ABotfCombatScene::ConfigureEncounter(const botf::tactical::Encounter& Encounter)
{
    const auto Report = botf::tactical::toCombatReport(Encounter);
    bSuppressReportTimeline = true; // The live encounter below starts the rules 3 timeline with its own poses.
    ConfigureBattleReport(Report, -1);
    bSuppressReportTimeline = false;

    TArray<int> OrderedIds;
    for (std::size_t Index = 0; Index < std::min<std::size_t>(MaxShipsPerSide, Report.firstFleetIds.size()); ++Index)
        OrderedIds.Add(Report.firstFleetIds[Index]);
    for (std::size_t Index = 0; Index < std::min<std::size_t>(MaxShipsPerSide, Report.secondFleetIds.size()); ++Index)
        OrderedIds.Add(Report.secondFleetIds[Index]);

    const bool bHasRound = !Encounter.rounds.empty();
    const auto* LastRound = bHasRound ? &Encounter.rounds.back() : nullptr;

    for (int Index = 0; Index < Ships.Num() && Index < OrderedIds.Num(); ++Index)
    {
        const int FleetId = OrderedIds[Index];
        const auto* Ship = Encounter.getShip(FleetId);
        if (!Ship) continue;

        if (Kinematics.IsValidIndex(Index))
        {
            Kinematics[Index].FleetId = FleetId;
            Kinematics[Index].OwnerId = Ship->ownerId;
            Kinematics[Index].Hull = Ship->hull;
            Kinematics[Index].InitialHull = FMath::Max(1, Ship->initialHull);
            Kinematics[Index].Shields = Ship->shields;
            Kinematics[Index].InitialShields = FMath::Max(0, Ship->initialShields);
            Kinematics[Index].bAlive = Ship->isAlive && Ship->hull > 0;
            Kinematics[Index].bRetreated = Ship->retreated;
            Kinematics[Index].bCloaked = Ship->isCloaked;
            Kinematics[Index].bDecloaking = false;
            Kinematics[Index].bDestroyedThisRound = false;

            const float CenterX = (Encounter.arenaWidth - 1) * 0.5f;
            const float CenterY = (Encounter.arenaHeight - 1) * 0.5f;
            // Rules 2 has no flight tracks; ships still hold their 3D home height (BotfCombatTimeline.h).
            const float ElevationOffset = static_cast<float>(-botf::combatfx::homeHeight(FleetId, Kinematics[Index].LengthMeters,
                Ship->ownerId == Report.firstOwner) * CellUnits);

            if (LastRound)
            {
                const auto* Status = LastRound->getStatus(FleetId);
                if (Status)
                {
                    Kinematics[Index].StartPosition = FVector((Status->positionBefore.x - CenterX) * 85.f,
                        ElevationOffset, (Status->positionBefore.y - CenterY) * 85.f);
                    Kinematics[Index].TargetPosition = FVector((Status->positionAfter.x - CenterX) * 85.f,
                        ElevationOffset, (Status->positionAfter.y - CenterY) * 85.f);
                    Kinematics[Index].CurrentPosition = Kinematics[Index].StartPosition;
                    Kinematics[Index].bDecloaking = Status->decloakedThisRound;
                    Kinematics[Index].bDestroyedThisRound = Status->destroyedThisRound;

                    // Compute flight heading and bank
                    const FVector FlightDir = Kinematics[Index].TargetPosition - Kinematics[Index].StartPosition;
                    const float BaseYaw = (Ship->ownerId == Report.firstOwner) ? 0.f : 180.f;
                    float FlightYaw = BaseYaw;
                    float BankRoll = 0.f;
                    if (FlightDir.SizeSquared2D() > 10.f)
                    {
                        const float MovingAngle = FMath::RadiansToDegrees(FMath::Atan2(FlightDir.Z, FlightDir.X));
                        FlightYaw = MovingAngle;
                        BankRoll = FMath::Clamp(FlightDir.Z * -0.3f, -25.f, 25.f);
                    }
                    Kinematics[Index].StartRotation = FRotator(0.f, BaseYaw, 0.f);
                    Kinematics[Index].TargetRotation = FRotator(0.f, FlightYaw, BankRoll);
                    Kinematics[Index].CurrentRotation = Kinematics[Index].StartRotation;
                    Kinematics[Index].BankRoll = BankRoll;
                }
            }
            else
            {
                const FVector Pos((Ship->position.x - CenterX) * 85.f, ElevationOffset, (Ship->position.y - CenterY) * 85.f);
                Kinematics[Index].StartPosition = Pos;
                Kinematics[Index].TargetPosition = Pos;
                Kinematics[Index].CurrentPosition = Pos;
                const float BaseYaw = (Ship->ownerId == Report.firstOwner) ? 0.f : 180.f;
                Kinematics[Index].StartRotation = FRotator(0.f, BaseYaw, 0.f);
                Kinematics[Index].TargetRotation = FRotator(0.f, BaseYaw, 0.f);
                Kinematics[Index].CurrentRotation = FRotator(0.f, BaseYaw, 0.f);
            }
        }
    }

    // Tactical rules 3: the round's event stream drives the scene (flight tracks, weapons, impacts, audio).
    if (LastRound && !LastRound->events.empty())
    {
        PlaneCenterX = (Encounter.arenaWidth - 1) * 0.5f;
        PlaneCenterY = (Encounter.arenaHeight - 1) * 0.5f;
        std::vector<botf::combatfx::ShipInfo> Infos;
        for (int Index = 0; Index < Ships.Num() && Index < OrderedIds.Num(); ++Index)
        {
            const auto* Ship = Encounter.getShip(OrderedIds[Index]);
            if (!Ship || !Kinematics.IsValidIndex(Index)) continue;
            const auto* Status = LastRound->getStatus(Ship->fleetId);
            bool bRetreatedNow = false;
            for (int Id : LastRound->retreatedFleetIds) bRetreatedNow = bRetreatedNow || Id == Ship->fleetId;
            auto& K = Kinematics[Index];
            K.bAliveAtRoundStart = (K.bAlive && !K.bRetreated) || (Status && Status->destroyedThisRound) || bRetreatedNow;
            K.ShieldsAtRoundStart = Status ? Status->shieldBefore : Ship->shields;
            K.bCloakedAtRoundStart = Ship->isCloaked || (Status && Status->decloakedThisRound);
            if (!K.bAliveAtRoundStart) continue;
            botf::combatfx::ShipInfo Info;
            Info.fleetId = Ship->fleetId;
            Info.ownerId = Ship->ownerId;
            Info.shields = K.ShieldsAtRoundStart;
            Info.maxShields = FMath::Max(1, K.InitialShields);
            Info.startsCloaked = K.bCloakedAtRoundStart;
            Info.x = Status ? Status->positionBefore.x : Ship->position.x;
            Info.y = Status ? Status->positionBefore.y : Ship->position.y;
            Info.facing = Ship->facing;
            Info.lengthMeters = K.LengthMeters;
            Infos.push_back(Info);
        }
        const FString Signature = FString::Printf(TEXT("E%d:%d:%d:%d"), static_cast<int>(Encounter.rounds.size()),
            static_cast<int>(LastRound->events.size()), OrderedIds.Num() > 0 ? OrderedIds[0] : 0, LastRound->roundIndex);
        Salvos.Reset();
        bIsRoundAnimating = false;
        StartTimeline(LastRound->events, Infos, Signature);
        Capture->CaptureScene();
        return;
    }

    // Populate weapon salvos for 3D trajectory visualization
    Salvos.Reset();
    if (LastRound)
    {
        for (const auto& S : LastRound->salvos)
        {
            int AttackerIdx = -1, DefenderIdx = -1;
            for (int I = 0; I < OrderedIds.Num(); ++I)
            {
                if (OrderedIds[I] == S.attackerFleetId) AttackerIdx = I;
                if (OrderedIds[I] == S.defenderFleetId) DefenderIdx = I;
            }
            if (AttackerIdx >= 0 && DefenderIdx >= 0 && AttackerIdx < Ships.Num() && DefenderIdx < Ships.Num())
            {
                FSalvoVisualItem Item;
                Item.AttackerVisualIndex = AttackerIdx;
                Item.DefenderVisualIndex = DefenderIdx;
                Item.BeamDamage = S.beamDamage;
                Item.TorpedoDamage = S.torpedoDamage;
                Item.AttackerOwner = Kinematics.IsValidIndex(AttackerIdx) ? Kinematics[AttackerIdx].OwnerId : 1;
                Item.bIsRam = S.ram;   // rules 2: ram impact and recoil records
                Salvos.Add(Item);
            }
        }
        bIsRoundAnimating = true;
        RoundAnimElapsed = 0.f;
    }
    else
    {
        bIsRoundAnimating = false;
    }

    Capture->CaptureScene();
}

void ABotfCombatScene::ConfigureBattleReport(const botf::CombatReport& Report, int RoundIndex)
{
    // Station participants (fleet ids >= botf::StationCombatantBase) take the same visual slots as below.
    uint64 StationSlots = 0;
    for (int Side = 0; Side < 2; ++Side)
    {
        const int FactionId = Side == 0 ? Report.firstOwner : Report.secondOwner;
        int SideCount = 0;
        for (const auto& Participant : Report.participants)
        {
            if (Participant.ownerId != FactionId || SideCount >= MaxShipsPerSide) continue;
            if (botf::isStationCombatant(Participant.fleetId)) StationSlots |= uint64(1) << (Side * MaxShipsPerSide + SideCount);
            ++SideCount;
        }
    }
    ConfigureBattle(Report.firstOwner, Report.secondOwner,
        static_cast<int>(Report.firstFleetIds.size()), static_cast<int>(Report.secondFleetIds.size()),
        static_cast<uint32>(Report.turn * 257 + Report.x * 17 + Report.y), StationSlots);

    std::vector<botf::CombatParticipant> LegacyParticipants;
    if (Report.participants.empty())
    {
        auto AddLegacy = [&](int FleetId, int OwnerId)
        {
            int InitialHull = 12;
            for (const auto& Damage : Report.damage)
                if (Damage.fleetId == FleetId) { InitialHull = Damage.hullBefore; break; }
            LegacyParticipants.push_back({FleetId, OwnerId, botf::Building::Scout, -1, InitialHull, 0});
        };
        for (int FleetId : Report.firstFleetIds) AddLegacy(FleetId, Report.firstOwner);
        for (int FleetId : Report.secondFleetIds) AddLegacy(FleetId, Report.secondOwner);
    }
    const auto& Participants = Report.participants.empty() ? LegacyParticipants : Report.participants;
    const int FirstSlots = FMath::Clamp(static_cast<int>(Report.firstFleetIds.size()), 0, MaxShipsPerSide);
    const int ShownRound = RoundIndex < 0 ? static_cast<int>(Report.rounds.size()) - 1 :
        FMath::Min(RoundIndex, static_cast<int>(Report.rounds.size()) - 1);
    const bool bTimeline = !bSuppressReportTimeline && ShownRound >= 0 && !Report.rounds[ShownRound].events.empty();
    std::vector<botf::combatfx::ShipInfo> Infos;

    for (int Side = 0; Side < 2; ++Side)
    {
        const int FactionId = Side == 0 ? Report.firstOwner : Report.secondOwner;
        int SideCount = 0;
        for (const auto& Participant : Participants)
        {
            if (Participant.ownerId != FactionId || SideCount >= MaxShipsPerSide) continue;
            const int VisualIndex = (Side == 0 ? 0 : FirstSlots) + SideCount++;
            if (!Ships.IsValidIndex(VisualIndex) || !ShieldShells.IsValidIndex(VisualIndex) ||
                !ShieldMaterials.IsValidIndex(VisualIndex) || !ShipOwners.IsValidIndex(VisualIndex) ||
                ShipOwners[VisualIndex] != FactionId) continue;

            auto* Ship = Ships[VisualIndex].Get();
            // Per-class mesh by asset key (UBotfAssetRegistry: local HD model -> fallback chain -> built-in faction
            // mesh). Stations keep their disc; participants without a class id keep the faction placeholder.
            if (!botf::isStationCombatant(Participant.fleetId))
                ApplyShipMesh(VisualIndex, Participant.referenceShipId, FactionId, Participant.shipClass == botf::Building::Destroyer);
            else
                ApplyStationMesh(VisualIndex, FactionId);

            int Hull = Participant.initialHull;
            int Shields = Participant.initialShields;
            bool Retreated = false;
            // State before the shown round (rules 3 playback starts there).
            int HullBefore = Hull, ShieldsBefore = Shields;
            bool RetreatedBefore = false;
            const int LastRound = RoundIndex < 0 ? static_cast<int>(Report.rounds.size()) - 1 :
                FMath::Min(RoundIndex, static_cast<int>(Report.rounds.size()) - 1);

            for (int Round = 0; Round <= LastRound; ++Round)
            {
                if (Round == LastRound) { HullBefore = Hull; ShieldsBefore = Shields; RetreatedBefore = Retreated; }
                for (const auto& Damage : Report.rounds[Round].damage)
                    if (Damage.fleetId == Participant.fleetId)
                    {
                        Hull = Damage.hullAfter;
                        Shields = Damage.shieldAfter;
                    }
                for (int Id : Report.rounds[Round].retreatedFleetIds)
                    if (Id == Participant.fleetId) Retreated = true;
                for (int Id : Report.rounds[Round].lostFleetIds)
                    if (Id == Participant.fleetId) Hull = 0;
            }
            if (Report.rounds.empty())
            {
                for (const auto& Damage : Report.damage)
                    if (Damage.fleetId == Participant.fleetId)
                    {
                        Hull = Damage.hullAfter;
                        Shields = Damage.shieldAfter;
                    }
                for (int Id : Report.lostFleetIds) if (Id == Participant.fleetId) Hull = 0;
                for (int Id : Report.retreatedFleetIds) if (Id == Participant.fleetId) Retreated = true;
            }

            if (Kinematics.IsValidIndex(VisualIndex))
            {
                Kinematics[VisualIndex].FleetId = Participant.fleetId;
                Kinematics[VisualIndex].OwnerId = FactionId;
                Kinematics[VisualIndex].Hull = Hull;
                Kinematics[VisualIndex].InitialHull = FMath::Max(1, Participant.initialHull);
                Kinematics[VisualIndex].Shields = Shields;
                Kinematics[VisualIndex].InitialShields = FMath::Max(0, Participant.initialShields);
                Kinematics[VisualIndex].bAlive = Hull > 0;
                Kinematics[VisualIndex].bRetreated = Retreated;
                Kinematics[VisualIndex].bAliveAtRoundStart = HullBefore > 0 && !RetreatedBefore;
                Kinematics[VisualIndex].ShieldsAtRoundStart = ShieldsBefore;
                if (bTimeline && HullBefore > 0 && !RetreatedBefore)
                {
                    // A record has no arena or positions outside the events; the formation slot is the fallback pose.
                    botf::combatfx::ShipInfo Info;
                    Info.fleetId = Participant.fleetId;
                    Info.ownerId = FactionId;
                    Info.shields = ShieldsBefore;
                    Info.maxShields = FMath::Max(1, Participant.initialShields);
                    Info.facing = Side == 0 ? 0 : 180;
                    Info.x = Kinematics[VisualIndex].StartPosition.X / CellUnits;
                    Info.y = Kinematics[VisualIndex].StartPosition.Z / CellUnits;
                    Info.lengthMeters = Kinematics[VisualIndex].LengthMeters;
                    for (const auto& Event : Report.rounds[ShownRound].events)
                        if (Event.kind == botf::CombatEventKind::Decloak && Event.fleetId == Participant.fleetId) Info.startsCloaked = true;
                    Kinematics[VisualIndex].bCloakedAtRoundStart = Info.startsCloaked;
                    Infos.push_back(Info);
                }
            }

            Ship->SetVisibility(Hull > 0 && !Retreated);
            if (ShieldShells[VisualIndex] && ShieldMaterials[VisualIndex])
            {
                // At rest the shield is invisible; impacts flare it (AnimateShieldsAndDamage / TickTimeline).
                ShieldShells[VisualIndex]->SetVisibility(false);
                ShieldMaterials[VisualIndex]->SetScalarParameterValue(TEXT("Strength"), 0.f);
            }
        }
    }
    if (bTimeline)
    {
        // Centre the plane on the poses the round mentions; the fallback formation poses (scene units / 85 around 0)
        // are shifted into the same frame.
        const auto& Events = Report.rounds[ShownRound].events;
        int MinX = 0, MaxX = 0, MinY = 0, MaxY = 0;
        bool bAny = false;
        for (const auto& Event : Events)
            if (botf::combatfx::eventHasPose(Event.kind) && Event.fleetId != 0)
            {
                MinX = bAny ? FMath::Min(MinX, Event.x) : Event.x; MaxX = bAny ? FMath::Max(MaxX, Event.x) : Event.x;
                MinY = bAny ? FMath::Min(MinY, Event.y) : Event.y; MaxY = bAny ? FMath::Max(MaxY, Event.y) : Event.y;
                bAny = true;
            }
        PlaneCenterX = bAny ? (MinX + MaxX) * 0.5f : 0.f;
        PlaneCenterY = bAny ? (MinY + MaxY) * 0.5f : 0.f;
        for (auto& Info : Infos) { Info.x += PlaneCenterX; Info.y += PlaneCenterY; }
        StartTimeline(Events, Infos, FString::Printf(TEXT("R%d:%d:%d:%d:%d"), Report.turn, Report.x, Report.y, ShownRound,
            static_cast<int>(Events.size())));
    }
    if (!Capture->bCaptureEveryFrame) Capture->CaptureScene();
}

void ABotfCombatScene::AnimateRoundKinematics(float DeltaSeconds, float NormTime)
{
    // Smoothstep flight trajectory interpolation
    const float SmoothT = NormTime * NormTime * (3.0f - 2.0f * NormTime);

    for (int Index = 0; Index < Kinematics.Num() && Index < Ships.Num(); ++Index)
    {
        auto& K = Kinematics[Index];
        auto* ShipMesh = Ships[Index].Get();
        if (!ShipMesh) continue;

        // Kinematic trajectory interpolation
        K.CurrentPosition = FMath::Lerp(K.StartPosition, K.TargetPosition, SmoothT);

        // Subtle flight oscillation
        const FVector HoverOffset(0.f, FMath::Sin(Elapsed * 1.5f + Index) * 6.f, FMath::Cos(Elapsed * 1.2f + Index) * 4.f);
        K.CurrentPosition += HoverOffset;

        // Rotation & banking interpolation
        K.CurrentRotation = FMath::Lerp(K.StartRotation, K.TargetRotation, SmoothT);
        ShipMesh->SetRelativeLocation(K.CurrentPosition);
        ShipMesh->SetRelativeRotation(K.CurrentRotation);

        if (ShieldShells.IsValidIndex(Index) && ShieldShells[Index])
        {
            ShieldShells[Index]->SetRelativeLocation(K.CurrentPosition);
        }

        // Cloak transition visibility
        if (K.bCloaked && !K.bDecloaking)
        {
            ShipMesh->SetVisibility(false);
            if (ShieldShells.IsValidIndex(Index) && ShieldShells[Index])
                ShieldShells[Index]->SetVisibility(false);
        }
        else if (K.bDecloaking)
        {
            // Shimmering fade in during decloak window
            const bool bDecloakVisible = (NormTime >= 0.25f);
            ShipMesh->SetVisibility(bDecloakVisible && K.bAlive);
        }
        else
        {
            ShipMesh->SetVisibility(K.bAlive && !K.bRetreated);
        }
    }
}

void ABotfCombatScene::AnimateWeaponSalvos(float NormTime)
{
    // Hide all effects by default
    for (const auto& Beam : BeamMeshes) if (Beam) Beam->SetVisibility(false);
    for (const auto& Torp : TorpedoMeshes) if (Torp) Torp->SetVisibility(false);

    int BeamIdx = 0, TorpIdx = 0;

    for (const auto& Salvo : Salvos)
    {
        if (!Kinematics.IsValidIndex(Salvo.AttackerVisualIndex) ||
            !Kinematics.IsValidIndex(Salvo.DefenderVisualIndex)) continue;

        const FVector AttackerPos = Kinematics[Salvo.AttackerVisualIndex].CurrentPosition;
        const FVector DefenderPos = Kinematics[Salvo.DefenderVisualIndex].CurrentPosition;
        const FVector SalvoDelta = DefenderPos - AttackerPos;
        const float Distance = SalvoDelta.Size();
        if (Distance < 10.f) continue;

        // Beam Weapon Execution: firing window between t=0.25 and t=0.68
        if (Salvo.BeamDamage > 0 && NormTime >= 0.25f && NormTime <= 0.68f && BeamIdx < BeamMeshes.Num())
        {
            auto* Beam = BeamMeshes[BeamIdx].Get();
            auto* Mat = BeamMaterials.IsValidIndex(BeamIdx) ? BeamMaterials[BeamIdx].Get() : nullptr;
            if (Beam)
            {
                const FVector Mid = (AttackerPos + DefenderPos) * 0.5f;
                const FRotator BeamRot = FRotationMatrix::MakeFromZ(SalvoDelta).Rotator();
                Beam->SetRelativeLocation(Mid);
                Beam->SetRelativeRotation(BeamRot);
                // Standard Unreal Cylinder has radius 50, height 100 along Z
                Beam->SetRelativeScale3D(FVector(0.06f, 0.06f, Distance / 100.f));
                if (Mat)
                {
                    Mat->SetVectorParameterValue(TEXT("Color"), GetFactionBeamColor(Salvo.AttackerOwner));
                }
                Beam->SetVisibility(true);
                BeamIdx++;
            }
        }

        // Torpedo Weapon Execution: projectile flight between t=0.30 and t=0.75
        if (Salvo.TorpedoDamage > 0 && NormTime >= 0.30f && NormTime <= 0.75f && TorpIdx < TorpedoMeshes.Num())
        {
            auto* Torp = TorpedoMeshes[TorpIdx].Get();
            auto* Mat = TorpedoMaterials.IsValidIndex(TorpIdx) ? TorpedoMaterials[TorpIdx].Get() : nullptr;
            if (Torp)
            {
                const float TorpProgress = (NormTime - 0.30f) / 0.45f;
                const FVector TorpPos = FMath::Lerp(AttackerPos, DefenderPos, TorpProgress);
                Torp->SetRelativeLocation(TorpPos);
                if (Mat)
                {
                    Mat->SetVectorParameterValue(TEXT("Color"), GetFactionTorpedoColor(Salvo.AttackerOwner));
                }
                Torp->SetVisibility(true);
                TorpIdx++;
            }
        }
    }
}

void ABotfCombatScene::AnimateShieldsAndDamage(float DeltaSeconds, float NormTime)
{
    for (int Index = 0; Index < Kinematics.Num() && Index < ShieldShells.Num(); ++Index)
    {
        auto& K = Kinematics[Index];
        auto* Shell = ShieldShells[Index].Get();
        auto* Mat = ShieldMaterials.IsValidIndex(Index) ? ShieldMaterials[Index].Get() : nullptr;
        if (!Shell || !Mat) continue;

        // Base shield strength fraction
        const float ShieldFraction = FMath::Clamp(static_cast<float>(K.Shields) / FMath::Max(1, K.InitialShields), 0.f, 1.f);

        // Shield impact flare: on hit window t=0.65 to 0.90
        if (NormTime >= 0.65f && NormTime <= 0.90f)
        {
            K.FlareStrength = FMath::FInterpTo(K.FlareStrength, 1.0f, DeltaSeconds, 15.f);
        }
        else
        {
            K.FlareStrength = FMath::FInterpTo(K.FlareStrength, 0.0f, DeltaSeconds, 5.f);
        }

        const float EffectiveStrength = ShieldDisplayStrength(ShieldFraction, K.FlareStrength);
        Shell->SetVisibility(K.bAlive && !K.bRetreated && K.Shields > 0 && EffectiveStrength > 0.01f);
        if (Ships.IsValidIndex(Index)) FitShieldShell(Shell, Ships[Index], K.BaseScale, K.CurrentRotation.Quaternion());
        Mat->SetScalarParameterValue(TEXT("Strength"), EffectiveStrength);

        // Ship Destruction Sequence
        if (K.bDestroyedThisRound && Ships.IsValidIndex(Index) && Ships[Index])
        {
            if (NormTime >= 0.70f)
            {
                K.DestructionProgress = (NormTime - 0.70f) / 0.30f;
                // Violent tumble and scale contraction
                Ships[Index]->AddRelativeRotation(FRotator(DeltaSeconds * 220.f, DeltaSeconds * 160.f, DeltaSeconds * 280.f));
                const float ShrinkScale = FMath::Max(0.01f, 1.0f - K.DestructionProgress);
                Ships[Index]->SetRelativeScale3D(K.BaseScale * ShrinkScale);

                if (NormTime >= 0.98f)
                {
                    Ships[Index]->SetVisibility(false);
                    Shell->SetVisibility(false);
                }
            }
        }
    }
}

void ABotfCombatScene::AnimateCamera(float DeltaSeconds)
{
    if (!Capture) return;

    FVector DesiredLocation = BaseCameraLocation;
    FRotator DesiredRotation = BaseCameraRotation;
    FVector FocusPoint = FVector::ZeroVector;

    if (bTitleScene)
    {
        // A slow low pass alongside the formation, the gas giant behind it: the camera looks along +Y (toward the
        // planet, see SetupEnvironment) from below and slightly ahead of the lead ship, drifting aft over ~70 s.
        // Looking roughly along the planet's direction (SetupEnvironment) but a little higher, so the gas giant fills
        // the lower background behind the ships; a slow lateral drift and breathing distance keep it alive.
        const float T = Elapsed * 0.08f;
        const FVector View = FVector(0.58f, 0.81f, -0.02f).GetSafeNormal(); // planet sits lower right of centre
        const FVector Side = FVector::CrossProduct(View, FVector::UpVector).GetSafeNormal();
        FocusPoint = FVector(-60.f, 60.f, -10.f) - Side * 80.f + Side * FMath::Sin(T) * 40.f; // ships left of centre
        DesiredLocation = FocusPoint - View * (700.f + FMath::Sin(T * 0.6f) * 70.f) + Side * FMath::Sin(T) * 180.f + FVector(0.f, 0.f, 90.f);
        DesiredRotation = (FocusPoint - DesiredLocation).Rotation();
        DesiredRotation.Roll = FMath::Sin(T * 0.8f) * 3.f;
        Capture->SetRelativeLocation(FMath::VInterpTo(Capture->GetRelativeLocation(), DesiredLocation, DeltaSeconds, 1.2f));
        Capture->SetRelativeRotation(FMath::RInterpTo(Capture->GetRelativeRotation(), DesiredRotation, DeltaSeconds, 1.2f));
        const FVector Eye = Capture->GetRelativeLocation();
        if (SkyDome) SkyDome->SetRelativeLocation(Eye);
        // Side points screen-left: the gas giant hangs lower right, partly out of frame; the moon upper left.
        if (Planet) Planet->SetRelativeLocation(Eye + (View - Side * 0.62f - FVector::UpVector * 0.40f).GetSafeNormal() * 26000.f);
        if (Moon) Moon->SetRelativeLocation(Eye + (View + Side * 0.55f + FVector::UpVector * 0.28f).GetSafeNormal() * 30000.f);
        return;
    }

    switch (CameraMode)
    {
    case Cam_Chase:
        if (Salvos.Num() > 0)
        {
            const auto& PrimarySalvo = Salvos[0];
            if (Kinematics.IsValidIndex(PrimarySalvo.AttackerVisualIndex) &&
                Kinematics.IsValidIndex(PrimarySalvo.DefenderVisualIndex))
            {
                FocusPoint = (Kinematics[PrimarySalvo.AttackerVisualIndex].CurrentPosition +
                              Kinematics[PrimarySalvo.DefenderVisualIndex].CurrentPosition) * 0.5f;
                DesiredLocation = FocusPoint + FVector(0.f, -1150.f, 550.f);
                DesiredRotation = (FocusPoint - DesiredLocation).Rotation();
            }
        }
        else
        {
            DesiredLocation = BaseCameraLocation;
            DesiredRotation = BaseCameraRotation;
        }
        break;

    case Cam_FocusShip:
        {
            bool bFound = false;
            for (const auto& K : Kinematics)
            {
                if (K.FleetId == FocusedFriendlyId && K.bAlive && !K.bRetreated)
                {
                    FocusPoint = K.CurrentPosition;
                    DesiredLocation = FocusPoint + FVector(-350.f, -480.f, 240.f);
                    DesiredRotation = (FocusPoint - DesiredLocation).Rotation();
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                DesiredLocation = BaseCameraLocation;
                DesiredRotation = BaseCameraRotation;
            }
        }
        break;

    case Cam_FocusTarget:
        {
            bool bFound = false;
            for (const auto& K : Kinematics)
            {
                if (K.FleetId == FocusedTargetId && K.bAlive && !K.bRetreated)
                {
                    FocusPoint = K.CurrentPosition;
                    DesiredLocation = FocusPoint + FVector(350.f, -480.f, 240.f);
                    DesiredRotation = (FocusPoint - DesiredLocation).Rotation();
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                DesiredLocation = BaseCameraLocation;
                DesiredRotation = BaseCameraRotation;
            }
        }
        break;

    case Cam_Cinematic:
        {
            bool bShot = false;
            if (bTimelinePlaying && Timeline && Director)
            {
                Director->update(*Timeline, TimelineTime, FocusedFriendlyId);
                const int32 Subject = SlotForFleet(Director->shot.subjectFleetId);
                const int32 Other = SlotForFleet(Director->shot.otherFleetId);
                if (Kinematics.IsValidIndex(Subject))
                {
                    const FVector SubjectPos = Kinematics[Subject].CurrentPosition;
                    const FVector OtherPos = Kinematics.IsValidIndex(Other) ? Kinematics[Other].CurrentPosition : SubjectPos + FVector(400.f, 0.f, 0.f);
                    FVector Line = OtherPos - SubjectPos;
                    if (Line.SizeSquared() < 1.f) Line = FVector(1.f, 0.f, 0.f);
                    const FVector Dir = Line.GetSafeNormal();
                    // Over the shoulder: behind the subject and out of the plane toward the viewer, looking at a point
                    // two thirds of the way to the other ship.
                    FocusPoint = SubjectPos + Line * 0.66f;
                    const float Back = FMath::Clamp(static_cast<float>(Line.Size()) * 0.6f + 380.f, 450.f, 1400.f);
                    DesiredLocation = SubjectPos - Dir * Back + FVector(0.f, -520.f, 180.f);
                    DesiredRotation = (FocusPoint - DesiredLocation).Rotation();
                    bShot = true;
                }
            }
            if (!bShot)
            {
                // Between rounds: a slow establishing drift around the arena.
                const float Angle = FMath::DegreesToRadians(Elapsed * 6.f);
                FocusPoint = FVector::ZeroVector;
                DesiredLocation = FVector(FMath::Sin(Angle) * 900.f, -1700.f, 900.f + FMath::Cos(Angle) * 250.f);
                DesiredRotation = (FocusPoint - DesiredLocation).Rotation();
            }
        }
        break;

    case Cam_Overhead:
        FocusPoint = FVector::ZeroVector;
        DesiredLocation = FVector(0.f, 0.f, 2300.f);
        DesiredRotation = FRotator(-89.9f, 90.f, 0.f);
        break;

    case Cam_Overview:
    default:
        FocusPoint = FVector::ZeroVector;
        DesiredLocation = BaseCameraLocation;
        DesiredRotation = BaseCameraRotation;
        break;
    }

    // Apply interactive orbit angles and zoom distance
    if (FMath::Abs(OrbitYaw) > 0.01f || FMath::Abs(OrbitPitch) > 0.01f || FMath::Abs(ZoomOffset) > 0.01f)
    {
        FRotator Rotated = DesiredRotation;
        Rotated.Yaw += OrbitYaw;
        Rotated.Pitch = FMath::Clamp(Rotated.Pitch + OrbitPitch, -89.f, 85.f);

        float Distance = FVector::Distance(DesiredLocation, FocusPoint);
        Distance = FMath::Clamp(Distance + ZoomOffset, 200.f, 4500.f);

        DesiredLocation = FocusPoint - Rotated.Vector() * Distance;
        DesiredRotation = Rotated;
    }

    const float InterpSpeed = CameraMode == Cam_Chase ? 2.5f : CameraMode == Cam_Cinematic ? 1.8f : 3.5f;
    Capture->SetRelativeLocation(FMath::VInterpTo(Capture->GetRelativeLocation(), DesiredLocation, DeltaSeconds, InterpSpeed));
    Capture->SetRelativeRotation(FMath::RInterpTo(Capture->GetRelativeRotation(), DesiredRotation, DeltaSeconds, InterpSpeed));
    // The sky and the distant bodies are effectively at infinity: they travel with the camera, so no camera mode can fly
    // up to them, and they never show parallax or the dome's edge.
    const FVector Eye = Capture->GetRelativeLocation();
    if (SkyDome) SkyDome->SetRelativeLocation(Eye);
    if (Planet) Planet->SetRelativeLocation(Eye + FVector(0.30f, 0.92f, -0.25f).GetSafeNormal() * 26000.f);
    if (Moon) Moon->SetRelativeLocation(Eye + FVector(-0.42f, 0.88f, 0.10f).GetSafeNormal() * 30000.f);
}

void ABotfCombatScene::BeginPlay()
{
    Super::BeginPlay();
#if WITH_DEV_AUTOMATION_TESTS
    if (FParse::Value(FCommandLine::Get(), TEXT("BotfTitleShot="), TitleShotPath))
    {
        int32 Empire = 1;
        FParse::Value(FCommandLine::Get(), TEXT("BotfTitleEmpire="), Empire);
        // -BotfTitleShips=ship.kli.vorcha,ship.kli.neghvar,ship.kli.bop puts other classes in the formation (art review).
        FString Keys;
        if (FParse::Value(FCommandLine::Get(), TEXT("BotfTitleShips="), Keys, false)) Keys.ParseIntoArray(TitleShipOverride, TEXT(","));
        ConfigureTitleScene(Empire);
        SetPresentationActive(true);
    }
#endif
}

void ABotfCombatScene::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Elapsed += DeltaSeconds;
#if WITH_DEV_AUTOMATION_TESTS
    if (!TitleShotPath.IsEmpty() && bTitleScene && Elapsed > 6.f && BattleTexture)
    {
        if (FArchive* Out = IFileManager::Get().CreateFileWriter(*TitleShotPath))
        {
            FImageUtils::ExportRenderTarget2DAsPNG(BattleTexture, *Out);
            delete Out;
            UE_LOG(LogTemp, Display, TEXT("BOTF_TITLE_SHOT %s"), *TitleShotPath);
        }
        TitleShotPath.Reset();
        FPlatformMisc::RequestExit(false);
    }
#endif

    if (bTimelinePlaying)
    {
        TickTimeline(DeltaSeconds);
    }
    else if (bIsRoundAnimating)
    {
        RoundAnimElapsed += DeltaSeconds;
        const float NormTime = FMath::Clamp(RoundAnimElapsed / RoundDuration, 0.f, 1.f);

        AnimateRoundKinematics(DeltaSeconds, NormTime);
        AnimateWeaponSalvos(NormTime);
        AnimateShieldsAndDamage(DeltaSeconds, NormTime);

        if (RoundAnimElapsed >= RoundDuration)
        {
            bIsRoundAnimating = false;
            // Hide remaining beams/torpedoes
            for (const auto& Beam : BeamMeshes) if (Beam) Beam->SetVisibility(false);
            for (const auto& Torp : TorpedoMeshes) if (Torp) Torp->SetVisibility(false);
        }
    }
    else
    {
        // Idle ambient kinematics
        for (int Index = 0; Index < Ships.Num() && Index < Kinematics.Num(); ++Index)
        {
            if (Ships[Index] && Kinematics[Index].bAlive && !Kinematics[Index].bRetreated)
            {
                const FVector IdlePos = Kinematics[Index].CurrentPosition +
                    FVector(0.f, FMath::Sin(Elapsed * 0.6f + Index) * 8.f, FMath::Cos(Elapsed * 0.5f + Index) * 6.f);
                Ships[Index]->SetRelativeLocation(IdlePos);
                if (ShieldShells.IsValidIndex(Index) && ShieldShells[Index])
                {
                    ShieldShells[Index]->SetRelativeLocation(IdlePos);
                }
            }
        }
    }

    AnimateCamera(DeltaSeconds);
}

// ---------------------------------------------------------------------------------------------------------------
// Tactical rules 3 playback: BotfCombatTimeline.h builds the schedule; this code only draws and plays it.
// Every effect has a Niagara-free fallback (pooled cylinders, emissive spheres, the shield-shell material, point
// lights and cube debris). A Niagara system installed under the job's vfx.* asset key replaces the fallback.
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    constexpr int32 FxBeamPool = 48;
    constexpr int32 FxOrbPool = 64;
    constexpr int32 FxBubblePool = 16;
    constexpr int32 FxFireballPool = 24;
    constexpr int32 FxDebrisPool = 40;
    constexpr int32 FxLightPool = 8;
    constexpr int32 DebrisPerExplosion = 8;
    constexpr int32 SparksPerHullHit = 3;

    FLinearColor ToLinear(const botf::combatfx::Rgb& Color, float Alpha = 1.f)
    {
        return FLinearColor(Color.r, Color.g, Color.b, Alpha);
    }
    FString Utf8ToFString(const std::string& Text) { return FString(UTF8_TO_TCHAR(Text.c_str())); }
    // Rises and falls over a job (0 at both ends).
    float Envelope(float U) { return FMath::Sin(UE_PI * FMath::Clamp(U, 0.f, 1.f)); }
}

void ABotfCombatScene::SetupTimelinePools()
{
    auto* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    // Generated effect materials (Tools/ue_build_vfx.py) when built: additive light instead of solid shapes.
    const auto LoadGenerated = [](const TCHAR* Path) -> UMaterialInterface*
    {
        return FPackageName::DoesPackageExist(FSoftObjectPath(Path).GetLongPackageName())
            ? Cast<UMaterialInterface>(FSoftObjectPath(Path).TryLoad()) : nullptr;
    };
    auto* GlowMaterial = LoadGenerated(TEXT("/Game/Generated/VFX/M_FxGlow.M_FxGlow"));
    if (!GlowMaterial) GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/M_SpaceGlow.M_SpaceGlow"));
    auto* FireballMaterial = LoadGenerated(TEXT("/Game/Generated/VFX/M_FxFireball.M_FxFireball"));
    auto* ShieldMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/M_ShieldShell.M_ShieldShell"));
    auto* LitMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    auto* DebrisMaterial = LoadGenerated(TEXT("/Game/Generated/VFX/MI_FxDebris.MI_FxDebris"));
    if (!GlowMaterial) GlowMaterial = LitMaterial;
    if (!FireballMaterial) FireballMaterial = GlowMaterial;
    if (!ShieldMaterial) ShieldMaterial = GlowMaterial;
    if (!DebrisMaterial) DebrisMaterial = LitMaterial;

    const auto Make = [this](UStaticMesh* Mesh, UMaterialInterface* Material, bool bDynamic,
        TArray<TObjectPtr<UStaticMeshComponent>>& Pool, TArray<TObjectPtr<UMaterialInstanceDynamic>>* Mats, int32 Count)
    {
        if (!Mesh || !Material) return;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            auto* Component = NewObject<UStaticMeshComponent>(this);
            Component->SetupAttachment(RootComponent);
            Component->SetStaticMesh(Mesh);
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Component->SetCastShadow(false);
            Component->SetVisibility(false);
            UMaterialInstanceDynamic* Instance = bDynamic ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
            Component->SetMaterial(0, Instance ? static_cast<UMaterialInterface*>(Instance) : Material);
            Component->RegisterComponent();
            Capture->ShowOnlyComponent(Component);
            Pool.Add(Component);
            if (Mats) Mats->Add(Instance);
        }
    };
    Make(CylinderMesh ? CylinderMesh : CubeMesh, GlowMaterial, true, FxBeams, &FxBeamMats, FxBeamPool);
    Make(SphereMesh, GlowMaterial, true, FxOrbs, &FxOrbMats, FxOrbPool);
    Make(SphereMesh, ShieldMaterial, true, FxBubbles, &FxBubbleMats, FxBubblePool);
    Make(SphereMesh, FireballMaterial, true, FxFireballs, &FxFireballMats, FxFireballPool);
    Make(CubeMesh, DebrisMaterial, false, FxDebris, nullptr, FxDebrisPool);
    for (int32 Index = 0; Index < FxLightPool; ++Index)
    {
        auto* Light = NewObject<UPointLightComponent>(this);
        Light->SetupAttachment(RootComponent);
        Light->SetIntensity(0.f);
        Light->SetAttenuationRadius(1500.f);
        Light->SetCastShadows(false);
        Light->SetVisibility(false);
        Light->RegisterComponent();
        FxLights.Add(Light);
    }
}

void ABotfCombatScene::ClearTimelineFx()
{
    for (auto& Pair : JobNiagara)
        if (UNiagaraComponent* Component = Pair.Value)
        {
            Capture->RemoveShowOnlyComponent(Component);
            Component->DestroyComponent();
        }
    JobNiagara.Reset();
    NiagaraTried.Reset();
    for (const auto* Pool : {&FxBeams, &FxOrbs, &FxBubbles, &FxFireballs, &FxDebris})
        for (const auto& Fx : *Pool) if (Fx) Fx->SetVisibility(false);
    for (const auto& Light : FxLights) if (Light) { Light->SetIntensity(0.f); Light->SetVisibility(false); }
}

void ABotfCombatScene::CacheSlotSockets(int32 VisualIndex)
{
    if (VisualIndex < 0) return;
    while (SlotSockets.Num() <= VisualIndex) SlotSockets.AddDefaulted();
    while (static_cast<int32>(SlotSocketNames.size()) <= VisualIndex) SlotSocketNames.emplace_back();
    SlotSockets[VisualIndex].Reset();
    SlotSocketNames[VisualIndex].clear();
    if (!Ships.IsValidIndex(VisualIndex) || !Ships[VisualIndex]) return;
    SlotSockets[VisualIndex] = Ships[VisualIndex]->GetAllSocketNames();
    for (const FName& Name : SlotSockets[VisualIndex]) SlotSocketNames[VisualIndex].push_back(std::string(TCHAR_TO_UTF8(*Name.ToString())));
}

int32 ABotfCombatScene::SlotForFleet(int32 FleetId) const
{
    if (FleetId == 0) return -1;
    for (int32 Index = 0; Index < Kinematics.Num(); ++Index) if (Kinematics[Index].FleetId == FleetId) return Index;
    return -1;
}

FVector ABotfCombatScene::PlaneToScene(double X, double Y, double AltitudeCells) const
{
    // Same frame as the rules 2 path: tactical x -> scene X, tactical y -> scene Z; height above the plane points at
    // the default camera (-Y), the side the ships' tops face.
    return FVector((X - PlaneCenterX) * CellUnits, -AltitudeCells * CellUnits, (Y - PlaneCenterY) * CellUnits);
}

FQuat ABotfCombatScene::PoseRotation(double FacingDegrees, double BankDegrees, double PitchDegrees) const
{
    // Facing 0 = +x, 90 = +y in the tactical plane (scene X/Z); ship tops face the default camera (-Y). Pitch raises
    // the bow toward the ship's top; bank rolls about the (pitched) bow.
    const double Radians = FMath::DegreesToRadians(FacingDegrees);
    const double Pitch = FMath::DegreesToRadians(PitchDegrees);
    const FVector Level(FMath::Cos(Radians), 0.0, FMath::Sin(Radians));
    const FVector Top(0.0, -1.0, 0.0);
    const FVector Forward = Level * FMath::Cos(Pitch) + Top * FMath::Sin(Pitch);
    const FVector Up = Top * FMath::Cos(Pitch) - Level * FMath::Sin(Pitch);
    const FQuat Base = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
    return FQuat(Forward, FMath::DegreesToRadians(BankDegrees)) * Base;
}

float ABotfCombatScene::ShipRadius(int32 VisualIndex) const
{
    if (!Ships.IsValidIndex(VisualIndex) || !Ships[VisualIndex] || !Ships[VisualIndex]->GetStaticMesh()) return 80.f;
    const float Scale = Kinematics.IsValidIndex(VisualIndex) ? static_cast<float>(Kinematics[VisualIndex].BaseScale.GetMax()) : 1.f;
    return FMath::Max(10.f, static_cast<float>(Ships[VisualIndex]->GetStaticMesh()->GetBounds().SphereRadius) * Scale);
}

FVector ABotfCombatScene::MuzzleLocation(int32 VisualIndex, int32 Family, int32 MountIndex, int32 EmitterIndex) const
{
    if (!Ships.IsValidIndex(VisualIndex) || !Ships[VisualIndex]) return FVector::ZeroVector;
    const UStaticMeshComponent* Ship = Ships[VisualIndex];
    const FTransform Relative = Ship->GetRelativeTransform();
    // Named sockets from the imported model (WPN_Beam_NN, WPN_Torp_NN, ...), else a hardpoint from the mesh bounds.
    if (static_cast<int32>(SlotSocketNames.size()) > VisualIndex && SlotSockets.IsValidIndex(VisualIndex))
    {
        const int32 Socket = botf::combatfx::pickWeaponSocket(SlotSocketNames[VisualIndex], Family, MountIndex, EmitterIndex);
        if (SlotSockets[VisualIndex].IsValidIndex(Socket))
            return Relative.TransformPosition(Ship->GetSocketTransform(SlotSockets[VisualIndex][Socket], RTS_Component).GetLocation());
    }
    if (const UStaticMesh* Mesh = Ship->GetStaticMesh())
    {
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const auto Point = botf::combatfx::fallbackHardpoint(Family, MountIndex, EmitterIndex);
        return Relative.TransformPosition(Bounds.Origin + FVector(Bounds.BoxExtent.X * Point.x, Bounds.BoxExtent.Y * Point.y, Bounds.BoxExtent.Z * Point.z));
    }
    return Ship->GetRelativeLocation();
}

UNiagaraSystem* ABotfCombatScene::FindNiagara(const std::vector<std::string>& Keys)
{
    UBotfAssetRegistry* Registry = UBotfAssetRegistry::Get();
    for (const auto& Key : Keys)
    {
        const FString Name = Utf8ToFString(Key);
        if (const TObjectPtr<UNiagaraSystem>* Found = NiagaraCache.Find(Name)) return Found->Get();
        if (NiagaraMissing.Contains(Name) || !Registry) continue;
        const FSoftObjectPath Path = Registry->ResolvePath(Name);
        UNiagaraSystem* System = Path.IsValid() ? Cast<UNiagaraSystem>(Path.TryLoad()) : nullptr;
        if (System) { NiagaraCache.Add(Name, System); return System; }
        NiagaraMissing.Add(Name);
    }
    return nullptr;
}

bool ABotfCombatScene::UpdateJobNiagara(int32 JobIndex, const botf::combatfx::VisualJob& Job, const FVector& From, const FVector& To, float U)
{
    // User parameters a system may read: User.Color, User.BeamStart / User.BeamEnd (world), User.Progress (0..1).
    const FTransform ActorTransform = GetActorTransform();
    if (TObjectPtr<UNiagaraComponent>* Existing = JobNiagara.Find(JobIndex))
    {
        if (UNiagaraComponent* Component = *Existing)
        {
            Component->SetRelativeLocation(From);
            Component->SetVariableVec3(FName(TEXT("User.BeamStart")), ActorTransform.TransformPosition(From));
            Component->SetVariableVec3(FName(TEXT("User.BeamEnd")), ActorTransform.TransformPosition(To));
            Component->SetVariableFloat(FName(TEXT("User.Progress")), U);
        }
        return true;
    }
    if (NiagaraTried.Contains(JobIndex)) return false;
    NiagaraTried.Add(JobIndex);
    UNiagaraSystem* System = FindNiagara(botf::combatfx::vfxKeysFor(Job));
    if (!System) return false;
    UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(System, RootComponent, NAME_None, From,
        (To - From).Rotation(), EAttachLocation::KeepRelativeOffset, false);
    if (!Component) return false;
    Component->SetVariableLinearColor(FName(TEXT("User.Color")), ToLinear(Job.color));
    Component->SetVariableVec3(FName(TEXT("User.BeamStart")), ActorTransform.TransformPosition(From));
    Component->SetVariableVec3(FName(TEXT("User.BeamEnd")), ActorTransform.TransformPosition(To));
    Component->SetVariableFloat(FName(TEXT("User.Progress")), U);
    Capture->ShowOnlyComponent(Component);
    JobNiagara.Add(JobIndex, Component);
    return true;
}

void ABotfCombatScene::StartTimeline(const std::vector<botf::CombatEvent>& Events, const std::vector<botf::combatfx::ShipInfo>& Infos,
    const FString& Signature)
{
    botf::combatfx::TimelineConfig Config;
    Config.roundSeconds = TimelineRoundSeconds;
    Config.playerEmpire = PlayerEmpire;
    auto Built = MakeShared<botf::combatfx::RoundTimeline>(botf::combatfx::buildRoundTimeline(Events, Infos, Config));
    // The same round shown again (the UI rebuilds the scene on many changes): resume where playback was, without
    // replaying sounds that already played.
    const bool bSameRound = Timeline.IsValid() && Signature == TimelineSignature;
    const float ResumeAt = bSameRound ? FMath::Min(TimelineTime, static_cast<float>(Built->endSeconds)) : 0.f;
    Timeline = Built;
    TimelineSignature = Signature;
    if (!Director) Director = MakeShared<botf::combatfx::CinematicDirector>();
    Director->reset();
    TimelineTime = ResumeAt;
    NextSound = 0;
    NextBark = 0;
    if (bSameRound)
    {
        while (NextSound < static_cast<int32>(Timeline->sounds.size()) && Timeline->sounds[NextSound].time <= ResumeAt) ++NextSound;
        while (NextBark < static_cast<int32>(Timeline->barks.size()) && Timeline->barks[NextBark].time <= ResumeAt) ++NextBark;
    }
    TrackSlot.Reset();
    for (const auto& Track : Timeline->ships) TrackSlot.Add(SlotForFleet(Track.fleetId));
    // Ships already gone before this round stay hidden.
    for (int32 Index = 0; Index < Ships.Num() && Index < Kinematics.Num(); ++Index)
        if (!Kinematics[Index].bAliveAtRoundStart && Ships[Index])
        {
            Ships[Index]->SetVisibility(false);
            if (ShieldShells.IsValidIndex(Index) && ShieldShells[Index]) ShieldShells[Index]->SetVisibility(false);
        }
    bTimelinePlaying = true;
    TickTimeline(0.f);
}

void ABotfCombatScene::FinishTimeline()
{
    ClearTimelineFx();
    bTimelinePlaying = false;
    for (int32 Index = 0; Index < Ships.Num() && Index < Kinematics.Num(); ++Index)
    {
        const auto& K = Kinematics[Index];
        if (!Ships[Index]) continue;
        const bool bVisible = K.bAlive && !K.bRetreated && K.Hull > 0 && !K.bCloaked;
        Ships[Index]->SetVisibility(bVisible);
        Ships[Index]->SetRelativeScale3D(K.BaseScale);
        if (ShieldShells.IsValidIndex(Index) && ShieldShells[Index])
            ShieldShells[Index]->SetVisibility(false);
    }
}

void ABotfCombatScene::TickTimeline(float DeltaSeconds)
{
    using namespace botf::combatfx;
    if (!Timeline) { bTimelinePlaying = false; return; }
    const RoundTimeline& TL = *Timeline;
    TimelineTime += DeltaSeconds;
    const double Now = TimelineTime;

    // True-scale hulls can overlap on the grid: shift the visible ones apart (mostly in height), eased so the
    // formation drifts rather than jumps. A zero-length tick snaps only a ship that has no spacing yet (a fresh scene).
    {
        std::vector<SpacingBody> Bodies(TL.ships.size());
        for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(TL.ships.size()); ++TrackIndex)
        {
            const int32 Slot = TrackSlot.IsValidIndex(TrackIndex) ? TrackSlot[TrackIndex] : -1;
            SpacingBody& Body = Bodies[static_cast<std::size_t>(TrackIndex)];
            Body.active = Ships.IsValidIndex(Slot) && Ships[Slot] && Kinematics.IsValidIndex(Slot) && Kinematics[Slot].bAliveAtRoundStart &&
                TL.ships[static_cast<std::size_t>(TrackIndex)].visibilityAt(Now) > 0.05;
            if (!Body.active) continue;
            const PlanePose Pose = TL.poseOf(static_cast<std::size_t>(TrackIndex), Now);
            Body.x = Pose.x; Body.y = Pose.y; Body.alt = Pose.altitude;
            Body.radius = 0.75 * ShipRadius(Slot) / CellUnits; // bounding spheres of flat hulls may overlap a little
        }
        const std::vector<SpacingOffset> Offsets = spacingOffsets(Bodies);
        const float Ease = DeltaSeconds <= 0.f ? 1.f : 1.f - FMath::Exp(-2.5f * DeltaSeconds);
        for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(TL.ships.size()); ++TrackIndex)
        {
            const int32 Slot = TrackSlot.IsValidIndex(TrackIndex) ? TrackSlot[TrackIndex] : -1;
            if (!Kinematics.IsValidIndex(Slot) || !Bodies[static_cast<std::size_t>(TrackIndex)].active) continue;
            const SpacingOffset& O = Offsets[static_cast<std::size_t>(TrackIndex)];
            const FVector Target(O.x * CellUnits, -O.alt * CellUnits, O.y * CellUnits); // PlaneToScene's axes
            FVector& Current = Kinematics[Slot].SpacingOffset;
            Current = FMath::Lerp(Current, Target, DeltaSeconds <= 0.f && !Current.IsNearlyZero() ? 0.f : Ease);
        }
    }

    // Ships: pose, visibility, warp-out stretch, destruction tumble, shield strength.
    for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(TL.ships.size()); ++TrackIndex)
    {
        const int32 Slot = TrackSlot.IsValidIndex(TrackIndex) ? TrackSlot[TrackIndex] : -1;
        if (!Ships.IsValidIndex(Slot) || !Ships[Slot] || !Kinematics.IsValidIndex(Slot)) continue;
        const ShipTrack& Track = TL.ships[static_cast<std::size_t>(TrackIndex)];
        auto& K = Kinematics[Slot];
        const PlanePose Pose = TL.poseOf(static_cast<std::size_t>(TrackIndex), Now); // own flight or wing station
        FVector Location = PlaneToScene(Pose.x, Pose.y, Pose.altitude) + K.SpacingOffset;
        FQuat Rotation = PoseRotation(Pose.facing, Pose.bank, Pose.pitch);
        FVector Scale = K.BaseScale;
        int ShieldsNow = K.ShieldsAtRoundStart;
        float Flare = 0.f;
        for (const auto& Job : TL.jobs)
        {
            if (Job.start > Now) break;
            const float U = static_cast<float>(FMath::Clamp((Now - Job.start) / FMath::Max(1e-6, Job.duration), 0.0, 1.0));
            if (Job.kind == JobKind::ShieldImpact && Job.targetFleetId == Track.fleetId)
            {
                ShieldsNow -= Job.shieldDamage;
                Flare = FMath::Max(Flare, 1.f - U);
            }
            if (Job.fleetId != Track.fleetId) continue;
            if (Job.kind == JobKind::WarpOut)
            {
                Location += Rotation.GetForwardVector() * (U * U * U * 2400.f);
                Scale.X *= 1.f + 3.f * U * U;
            }
            else if (Job.kind == JobKind::Explosion && Track.hideAt > Job.start)
            {
                const float D = static_cast<float>(FMath::Clamp((Now - Job.start) / (Track.hideAt - Job.start), 0.0, 1.0));
                Rotation = FQuat(FRotator(D * 140.f, D * 95.f, D * 210.f)) * Rotation;
                Scale *= FMath::Max(0.05f, 1.f - 0.8f * D);
            }
        }
        const float Visibility = K.bAliveAtRoundStart ? static_cast<float>(Track.visibilityAt(Now)) : 0.f;
        K.CurrentPosition = Location;
        K.CurrentRotation = Rotation.Rotator();
        Ships[Slot]->SetRelativeLocationAndRotation(Location, Rotation);
        Ships[Slot]->SetRelativeScale3D(Scale);
        Ships[Slot]->SetVisibility(Visibility > 0.05f);
        if (ShieldShells.IsValidIndex(Slot) && ShieldShells[Slot])
        {
            ShieldShells[Slot]->SetRelativeLocation(Location);
            FitShieldShell(ShieldShells[Slot], Ships[Slot], Scale, Rotation);
            const float Fraction = FMath::Clamp(static_cast<float>(ShieldsNow) / FMath::Max(1, K.InitialShields), 0.f, 1.f);
            const float Strength = ShieldDisplayStrength(Fraction, Flare) * Visibility;
            ShieldShells[Slot]->SetVisibility(Visibility > 0.5f && Strength > 0.01f);
            if (ShieldMaterials.IsValidIndex(Slot) && ShieldMaterials[Slot])
                ShieldMaterials[Slot]->SetScalarParameterValue(TEXT("Strength"), Strength);
        }
    }

    // Effects: every frame the pools are handed out again in job order.
    for (const auto* Pool : {&FxBeams, &FxOrbs, &FxBubbles, &FxFireballs, &FxDebris})
        for (const auto& Fx : *Pool) if (Fx) Fx->SetVisibility(false);
    for (const auto& Light : FxLights) if (Light) Light->SetVisibility(false);
    int32 BeamIndex = 0, OrbIndex = 0, BubbleIndex = 0, FireballIndex = 0, DebrisIndex = 0, LightIndex = 0;

    const auto DrawSegment = [&](const FVector& A, const FVector& B, float Width, const FLinearColor& Color)
    {
        if (!FxBeams.IsValidIndex(BeamIndex)) return;
        UStaticMeshComponent* Beam = FxBeams[BeamIndex];
        UMaterialInstanceDynamic* Mat = FxBeamMats.IsValidIndex(BeamIndex) ? FxBeamMats[BeamIndex].Get() : nullptr;
        ++BeamIndex;
        const FVector Delta = B - A;
        const float Length = static_cast<float>(Delta.Size());
        if (!Beam || Length < 1.f || Width <= 0.f) return;
        Beam->SetRelativeLocation((A + B) * 0.5f);
        Beam->SetRelativeRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
        Beam->SetRelativeScale3D(FVector(Width, Width, Length / 100.f)); // Engine cylinder: radius 50, height 100
        if (Mat) Mat->SetVectorParameterValue(TEXT("Color"), Color);
        Beam->SetVisibility(true);
    };
    const auto DrawOrb = [&](const FVector& At, const FVector& Scale, const FLinearColor& Color, const FQuat& Rotation)
    {
        if (!FxOrbs.IsValidIndex(OrbIndex)) return;
        UStaticMeshComponent* Orb = FxOrbs[OrbIndex];
        UMaterialInstanceDynamic* Mat = FxOrbMats.IsValidIndex(OrbIndex) ? FxOrbMats[OrbIndex].Get() : nullptr;
        ++OrbIndex;
        if (!Orb || Scale.GetMin() <= 0.001f) return;
        Orb->SetRelativeLocationAndRotation(At, Rotation);
        Orb->SetRelativeScale3D(Scale);
        if (Mat) Mat->SetVectorParameterValue(TEXT("Color"), Color);
        Orb->SetVisibility(true);
    };
    const auto DrawBubble = [&](const FVector& At, const FVector& Scale, float Strength, const FLinearColor& Color, const FQuat& Rotation)
    {
        if (!FxBubbles.IsValidIndex(BubbleIndex)) return;
        UStaticMeshComponent* Bubble = FxBubbles[BubbleIndex];
        UMaterialInstanceDynamic* Mat = FxBubbleMats.IsValidIndex(BubbleIndex) ? FxBubbleMats[BubbleIndex].Get() : nullptr;
        ++BubbleIndex;
        if (!Bubble || Strength <= 0.01f) return;
        Bubble->SetRelativeLocationAndRotation(At, Rotation);
        Bubble->SetRelativeScale3D(Scale);
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("Strength"), Strength);
            Mat->SetVectorParameterValue(TEXT("Color"), Color);
        }
        Bubble->SetVisibility(true);
    };
    const auto DrawFireball = [&](const FVector& At, float Scale, float Age, const FLinearColor& Color, const FQuat& Rotation)
    {
        if (!FxFireballs.IsValidIndex(FireballIndex)) return;
        UStaticMeshComponent* Ball = FxFireballs[FireballIndex];
        UMaterialInstanceDynamic* Mat = FxFireballMats.IsValidIndex(FireballIndex) ? FxFireballMats[FireballIndex].Get() : nullptr;
        ++FireballIndex;
        if (!Ball || Scale <= 0.001f || Age >= 1.f) return;
        Ball->SetRelativeLocationAndRotation(At, Rotation);
        Ball->SetRelativeScale3D(FVector(Scale));
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("Age"), Age);
            Mat->SetVectorParameterValue(TEXT("Color"), Color);
        }
        Ball->SetVisibility(true);
    };
    const auto Flash = [&](const FVector& At, float Intensity, float Radius, const FLinearColor& Color)
    {
        if (!FxLights.IsValidIndex(LightIndex)) return;
        UPointLightComponent* Light = FxLights[LightIndex++];
        if (!Light || Intensity <= 1.f) return;
        Light->SetRelativeLocation(At);
        Light->SetIntensity(Intensity);
        Light->SetAttenuationRadius(Radius);
        Light->SetLightColor(Color);
        Light->SetVisibility(true);
    };
    const auto Debris = [&](const FVector& Origin, int32 Seed, int32 Count, float Distance, float Size, float U)
    {
        FRandomStream Random(Seed);
        for (int32 Piece = 0; Piece < Count && FxDebris.IsValidIndex(DebrisIndex); ++Piece)
        {
            UStaticMeshComponent* Chunk = FxDebris[DebrisIndex++];
            const FVector Dir = Random.GetUnitVector();
            const FRotator Spin(Random.FRandRange(-400.f, 400.f), Random.FRandRange(-400.f, 400.f), Random.FRandRange(-400.f, 400.f));
            const float Speed = Random.FRandRange(0.5f, 1.f);
            if (!Chunk) continue;
            Chunk->SetRelativeLocationAndRotation(Origin + Dir * Distance * Speed * (1.f - (1.f - U) * (1.f - U)), Spin * U);
            Chunk->SetRelativeScale3D(FVector(Size * (1.f - 0.7f * U)));
            Chunk->SetVisibility(true);
        }
    };
    const auto ShipPos = [this](int32 Slot) { return Kinematics.IsValidIndex(Slot) ? Kinematics[Slot].CurrentPosition : FVector::ZeroVector; };
    const FVector PlaneNormal(0.f, -1.f, 0.f);
    const FQuat NoRotation = FQuat::Identity;

    for (int32 JobIndex = 0; JobIndex < static_cast<int32>(TL.jobs.size()); ++JobIndex)
    {
        const VisualJob& Job = TL.jobs[static_cast<std::size_t>(JobIndex)];
        if (Job.start > Now) break;
        if (Now >= Job.end())
        {
            if (TObjectPtr<UNiagaraComponent>* Existing = JobNiagara.Find(JobIndex))
            {
                if (UNiagaraComponent* Component = *Existing)
                {
                    Component->Deactivate();
                    Capture->RemoveShowOnlyComponent(Component);
                    Component->DestroyComponent();
                }
                JobNiagara.Remove(JobIndex);
            }
            continue;
        }
        const float U = static_cast<float>(FMath::Clamp((Now - Job.start) / FMath::Max(1e-6, Job.duration), 0.0, 1.0));
        const int32 FromSlot = SlotForFleet(Job.fleetId);
        const int32 ToSlot = SlotForFleet(Job.targetFleetId);
        const int32 SourceSlot = SlotForFleet(Job.sourceFleetId);
        const FLinearColor Color = ToLinear(Job.color);
        switch (Job.kind)
        {
        case JobKind::Beam:
        case JobKind::PulseBolt:
        case JobKind::Torpedo:
        {
            if (!Kinematics.IsValidIndex(FromSlot)) break;
            const FVector Muzzle = MuzzleLocation(FromSlot, Job.weaponFamily, Job.mountIndex, Job.index);
            FVector Target = Kinematics.IsValidIndex(ToSlot) ? ShipPos(ToSlot) : Muzzle + FVector(600.f, 0.f, 0.f);
            const FVector Dir = (Target - Muzzle).GetSafeNormal();
            if (Job.outcome == ShotOutcome::Miss) Target += Dir * static_cast<float>(Job.overshootCells * CellUnits);
            else if (Job.kind != JobKind::Torpedo && Kinematics.IsValidIndex(ToSlot))
                Target -= Dir * ShipRadius(ToSlot) * (Kinematics[ToSlot].ShieldsAtRoundStart > 0 ? 1.1f : 0.5f);
            if (Job.kind == JobKind::Beam)
            {
                if (UpdateJobNiagara(JobIndex, Job, Muzzle, Target, U)) break;
                const float Env = FMath::Sqrt(Envelope(U));
                DrawSegment(Muzzle, Target, 0.05f * Env, Color);
                DrawOrb(Muzzle, FVector(0.12f * Env), Color, NoRotation);
            }
            else if (Job.kind == JobKind::PulseBolt)
            {
                const FVector Head = FMath::Lerp(Muzzle, Target, U);
                if (UpdateJobNiagara(JobIndex, Job, Head, Target, U)) break;
                DrawSegment(Head - Dir * 70.f, Head, 0.08f, Color);
            }
            else
            {
                // Arcing flight (as botf::combatfx::torpedoPoint, in scene units): bows sideways and out of the plane.
                const float Flight = U * static_cast<float>(Job.endFraction);
                const FVector Lateral = FVector::CrossProduct(Target - Muzzle, PlaneNormal).GetSafeNormal();
                const float Bow = 4.f * Flight * (1.f - Flight);
                const float Arc = static_cast<float>(Job.arcCells * CellUnits);
                const FVector At = FMath::Lerp(Muzzle, Target, Flight) + Lateral * Arc * Bow + PlaneNormal * FMath::Abs(Arc) * 0.5f * Bow;
                if (UpdateJobNiagara(JobIndex, Job, At, Target, U)) break;
                const float Fade = Job.outcome == ShotOutcome::Miss && U > 0.75f ? (1.f - U) * 4.f : 1.f;
                const float Pulse = 0.2f + 0.05f * FMath::Sin(static_cast<float>(Now) * 40.f + Job.index);
                DrawOrb(At, FVector(Pulse * Fade), Color, NoRotation);
                DrawOrb(At, FVector(Pulse * 2.2f * Fade), Color * 0.35f, NoRotation);
            }
            break;
        }
        case JobKind::ShieldImpact:
        {
            if (!Kinematics.IsValidIndex(ToSlot)) break;
            const FVector Center = ShipPos(ToSlot);
            const FVector Source = Kinematics.IsValidIndex(SourceSlot) ? ShipPos(SourceSlot) : Center + FVector(1.f, 0.f, 0.f);
            const FVector Toward = (Source - Center).GetSafeNormal();
            const float Radius = ShipRadius(ToSlot) * 1.15f;
            const FVector Point = Center + Toward * Radius;
            if (UpdateJobNiagara(JobIndex, Job, Point, Center, U)) break;
            // A flattened bubble on the shell where the shot struck, plus a quick core glow.
            DrawBubble(Point - Toward * Radius * 0.15f, FVector(0.12f, 0.8f, 0.8f) * (Radius / 50.f) * (0.6f + 0.4f * U),
                (1.f - U) * 0.95f, Color, FRotationMatrix::MakeFromX(Toward).ToQuat());
            DrawOrb(Point, FVector(0.25f * (1.f - U) + 0.05f), Color, NoRotation);
            break;
        }
        case JobKind::HullImpact:
        {
            if (!Kinematics.IsValidIndex(ToSlot)) break;
            const FVector Center = ShipPos(ToSlot);
            const FVector Toward = ((Kinematics.IsValidIndex(SourceSlot) ? ShipPos(SourceSlot) : Center + FVector(1.f, 0.f, 0.f)) - Center).GetSafeNormal();
            const FVector Point = Center + Toward * ShipRadius(ToSlot) * 0.45f;
            if (UpdateJobNiagara(JobIndex, Job, Point, Center, U)) break;
            DrawOrb(Point, FVector(0.35f * Envelope(FMath::Min(1.f, U * 2.f)) + 0.04f), FLinearColor(1.f, 0.55f, 0.15f), NoRotation);
            DrawOrb(Point, FVector(0.12f * (1.f - U)), FLinearColor(0.08f, 0.05f, 0.03f), NoRotation); // scorch
            Flash(Point, 40000.f * (1.f - U), 600.f, FLinearColor(1.f, 0.6f, 0.25f));
            Debris(Point, JobIndex * 7919 + 17, SparksPerHullHit, 120.f, 0.05f, U);
            break;
        }
        case JobKind::PointDefence:
        {
            if (!Kinematics.IsValidIndex(FromSlot)) break;
            const FVector Muzzle = MuzzleLocation(FromSlot, 2, Job.mountIndex, 0);
            const FVector Target = Kinematics.IsValidIndex(ToSlot) ? ShipPos(ToSlot) : Muzzle;
            const FVector Point = FMath::Lerp(Muzzle, Target, static_cast<float>(TimelineConfig{}.interceptFraction));
            if (UpdateJobNiagara(JobIndex, Job, Point, Target, U)) break;
            DrawOrb(Point, FVector((0.2f + 0.8f * U) * (1.f - U)), FLinearColor(1.f, 0.9f, 0.6f), NoRotation);
            Flash(Point, 25000.f * (1.f - U), 500.f, FLinearColor(1.f, 0.85f, 0.6f));
            break;
        }
        case JobKind::Explosion:
        {
            if (!Kinematics.IsValidIndex(FromSlot)) break;
            const FVector Center = ShipPos(FromSlot);
            const float Radius = ShipRadius(FromSlot);
            if (UpdateJobNiagara(JobIndex, Job, Center, Center, U)) break;
            // A blinding core, two turbulent fireballs burning out at different rates, and a flat shockwave ring.
            FRandomStream Random(JobIndex * 7349 + Job.fleetId);
            const float Unit = Radius / 50.f;
            const float Grow = 0.45f + 1.9f * FMath::Sqrt(U);
            const float Core = FMath::Max(0.f, 1.f - U * 3.f);
            DrawOrb(Center, FVector(Unit * (0.5f + 0.6f * U) * Core), FLinearColor(1.f, 0.92f, 0.75f), NoRotation);
            DrawFireball(Center, Unit * Grow, U, FLinearColor::White, FRotator(Random.FRandRange(0.f, 360.f), Random.FRandRange(0.f, 360.f), 0.f).Quaternion());
            DrawFireball(Center + Random.GetUnitVector() * Radius * 0.35f, Unit * Grow * 0.7f, FMath::Min(1.f, U * 1.25f),
                FLinearColor(1.f, 0.85f, 0.7f), FRotator(Random.FRandRange(0.f, 360.f), Random.FRandRange(0.f, 360.f), 0.f).Quaternion());
            const FQuat RingTilt = FRotator(Random.FRandRange(-25.f, 25.f), 0.f, Random.FRandRange(-25.f, 25.f)).Quaternion();
            const float Ring = Unit * (0.6f + 7.f * FMath::Sqrt(U));
            DrawOrb(Center, FVector(Ring, Ring, Unit * 0.04f), FLinearColor(0.55f, 0.7f, 1.f, (1.f - U) * (1.f - U)), RingTilt);
            Flash(Center, 250000.f * (1.f - U) * (1.f - U), 2200.f, FLinearColor(1.f, 0.7f, 0.35f));
            Debris(Center, JobIndex * 104729 + Job.fleetId, DebrisPerExplosion, Radius * 4.f, Radius / 50.f * 0.18f, U);
            break;
        }
        case JobKind::CloakShimmer:
        case JobKind::Decloak:
        {
            if (!Kinematics.IsValidIndex(FromSlot)) break;
            const FVector Center = ShipPos(FromSlot);
            if (UpdateJobNiagara(JobIndex, Job, Center, Center, U)) break;
            const float Shimmer = Envelope(U) * (0.55f + 0.25f * FMath::Sin(static_cast<float>(Now) * 30.f));
            DrawBubble(Center, FVector(ShipRadius(FromSlot) * 1.1f / 50.f), Shimmer, Color, NoRotation);
            break;
        }
        case JobKind::WarpOut:
        {
            if (!Kinematics.IsValidIndex(FromSlot)) break;
            const FVector Center = ShipPos(FromSlot);
            const FQuat Rotation = Kinematics[FromSlot].CurrentRotation.Quaternion();
            if (UpdateJobNiagara(JobIndex, Job, Center, Center + Rotation.GetForwardVector() * 1000.f, U)) break;
            const float Stretch = 1.f + 12.f * U * U;
            DrawOrb(Center, FVector(0.3f * Stretch, 0.12f, 0.12f) * (1.f - 0.5f * U), Color, Rotation);
            if (U > 0.8f) Flash(Center, 120000.f * (1.f - U) * 5.f, 1500.f, Color);
            break;
        }
        case JobKind::Ram:
        case JobKind::Disengage:
        case JobKind::Truce:
            break;
        }
    }

    // Audio at the moments of the visual jobs; the audio subsystem's CueScheduler still rate-limits and culls.
    if (UBotfAudioSubsystem* Audio = UBotfAudioSubsystem::Get())
    {
        while (NextSound < static_cast<int32>(TL.sounds.size()) && TL.sounds[NextSound].time <= Now)
        {
            const SoundJob& Sound = TL.sounds[NextSound++];
            Audio->PlaySfxKeys(Sound.keys, botf::audio::Category::Sfx, Sound.priority);
        }
        while (NextBark < static_cast<int32>(TL.barks.size()) && TL.barks[NextBark].time <= Now)
        {
            const BarkJob& Bark = TL.barks[NextBark++];
            Audio->PlayCue(botf::audio::GameCue::CombatBark, 0, Utf8ToFString(Bark.detail));
        }
    }

    if (Now >= TL.endSeconds) FinishTimeline();
}
