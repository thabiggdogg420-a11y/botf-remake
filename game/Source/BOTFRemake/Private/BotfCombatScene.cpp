#include "BotfCombatScene.h"
#include "Simulation.h"
#include "TacticalCombat.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

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
    Capture->PostProcessSettings.bOverride_BloomIntensity = true;
    Capture->PostProcessSettings.BloomIntensity = 0.25f;

    auto* Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
    Light->SetupAttachment(RootComponent);
    Light->SetRelativeLocation(FVector(0, -500, 1100));
    Light->SetIntensity(220000.f);
    Light->SetAttenuationRadius(7000.f);
    Light->SetCastShadows(false);
}

UTextureRenderTarget2D* ABotfCombatScene::GetBattleTexture() const { return BattleTexture; }

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

void ABotfCombatScene::ConfigureBattle(int AttackerFaction, int DefenderFaction, int AttackerCount, int DefenderCount, uint32 Seed)
{
    if (!BattleTexture)
    {
        BattleTexture = NewObject<UTextureRenderTarget2D>(this);
        BattleTexture->ClearColor = FLinearColor(0.002f, 0.004f, 0.012f);
        BattleTexture->InitAutoFormat(1920, 1080);
        Capture->TextureTarget = BattleTexture;
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

    Ships.Reset(); Positions.Reset(); ShipOwners.Reset();
    ShieldShells.Reset(); ShieldMaterials.Reset();
    BeamMeshes.Reset(); BeamMaterials.Reset();
    TorpedoMeshes.Reset(); TorpedoMaterials.Reset();
    Kinematics.Reset(); Salvos.Reset();

    SetupEffectPools();

    auto* ShieldMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* ShieldMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/M_ShieldShell.M_ShieldShell"));
    static const TCHAR* Names[] = {TEXT("Federation"), TEXT("Klingon"), TEXT("Romulan"), TEXT("Cardassian"), TEXT("Ferengi")};
    FRandomStream Random(Seed);
    for (int Side = 0; Side < 2; ++Side)
    {
        const int Faction = FMath::Clamp(Side == 0 ? AttackerFaction : DefenderFaction, 1, 5);
        const FString Name(Names[Faction - 1]);
        const FString Path = FString::Printf(TEXT("/Game/Presentation/%s/StaticMeshes/%s.%s"), *Name, *Name, *Name);
        auto* Mesh = Faction == 1 ? LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Presentation/Community/franger/StaticMeshes/franger.franger")) : nullptr;
        if (!Mesh) Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
        if (!Mesh)
        {
            UE_LOG(LogTemp, Warning, TEXT("BOTF battle visualization missing mesh: %s"), *Path);
            Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
        }
        if (!Mesh) Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        if (!Mesh) continue;

        const int Count = FMath::Clamp(Side == 0 ? AttackerCount : DefenderCount, 0, 8);
        const int Rows = (Count + 1) / 2;
        for (int Index = 0; Index < Count; ++Index)
        {
            auto* Ship = NewObject<UStaticMeshComponent>(this);
            Ship->SetupAttachment(RootComponent);
            Ship->SetStaticMesh(Mesh);
            Ship->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Ship->RegisterComponent();
            const float Radius = FMath::Max(1.f, Mesh->GetBounds().SphereRadius);
            Ship->SetRelativeScale3D(FVector(80.f / Radius));
            const int Row = Index / 2;
            const int Column = Index % 2;
            const FVector Position((Side == 0 ? -1.f : 1.f) * (Count == 1 ? 445.f : 335.f + Column * 235.f),
                (Row - (Rows - 1) * 0.5f) * 60.f, (Row - (Rows - 1) * 0.5f) * 230.f + Random.FRandRange(-10.f, 10.f));
            const FRotator Rotation(0, Side == 0 ? 0.f : 180.f, 0);

            Ship->SetRelativeLocation(Position);
            Ship->SetRelativeRotation(Rotation);
            Ships.Add(Ship); Positions.Add(Position); ShipOwners.Add(Faction);
            Capture->ShowOnlyComponent(Ship);

            FShipKinematicState Kin;
            Kin.OwnerId = Faction;
            Kin.StartPosition = Position;
            Kin.TargetPosition = Position;
            Kin.CurrentPosition = Position;
            Kin.StartRotation = Rotation;
            Kin.TargetRotation = Rotation;
            Kin.CurrentRotation = Rotation;
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

void ABotfCombatScene::SetPresentationActive(bool bActive)
{
    SetActorTickEnabled(bActive);
    Capture->bCaptureEveryFrame = bActive;
}

void ABotfCombatScene::ConfigureEncounter(const botf::tactical::Encounter& Encounter)
{
    const auto Report = botf::tactical::toCombatReport(Encounter);
    ConfigureBattleReport(Report, -1);

    TArray<int> OrderedIds;
    for (std::size_t Index = 0; Index < std::min<std::size_t>(8, Report.firstFleetIds.size()); ++Index)
        OrderedIds.Add(Report.firstFleetIds[Index]);
    for (std::size_t Index = 0; Index < std::min<std::size_t>(8, Report.secondFleetIds.size()); ++Index)
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
            const float ElevationOffset = ((Index % 3) - 1.0f) * 45.f;

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
    ConfigureBattle(Report.firstOwner, Report.secondOwner,
        static_cast<int>(Report.firstFleetIds.size()), static_cast<int>(Report.secondFleetIds.size()),
        static_cast<uint32>(Report.turn * 257 + Report.x * 17 + Report.y));

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
    const int FirstSlots = FMath::Clamp(static_cast<int>(Report.firstFleetIds.size()), 0, 8);

    for (int Side = 0; Side < 2; ++Side)
    {
        const int FactionId = Side == 0 ? Report.firstOwner : Report.secondOwner;
        int SideCount = 0;
        for (const auto& Participant : Participants)
        {
            if (Participant.ownerId != FactionId || SideCount >= 8) continue;
            const int VisualIndex = (Side == 0 ? 0 : FirstSlots) + SideCount++;
            if (!Ships.IsValidIndex(VisualIndex) || !ShieldShells.IsValidIndex(VisualIndex) ||
                !ShieldMaterials.IsValidIndex(VisualIndex) || !ShipOwners.IsValidIndex(VisualIndex) ||
                ShipOwners[VisualIndex] != FactionId) continue;

            auto* Ship = Ships[VisualIndex].Get();
            if (FactionId == 1 && Participant.shipClass == botf::Building::Destroyer)
            {
                if (auto* Mesh = LoadObject<UStaticMesh>(nullptr,
                    TEXT("/Game/Presentation/Community/fUlysses/StaticMeshes/fUlysses.fUlysses")))
                {
                    Ship->SetStaticMesh(Mesh);
                    Ship->SetRelativeScale3D(FVector(80.f / FMath::Max(1.f, Mesh->GetBounds().SphereRadius)));
                }
            }

            int Hull = Participant.initialHull;
            int Shields = Participant.initialShields;
            bool Retreated = false;
            const int LastRound = RoundIndex < 0 ? static_cast<int>(Report.rounds.size()) - 1 :
                FMath::Min(RoundIndex, static_cast<int>(Report.rounds.size()) - 1);

            for (int Round = 0; Round <= LastRound; ++Round)
            {
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
            }

            Ship->SetVisibility(Hull > 0 && !Retreated);
            if (ShieldShells[VisualIndex] && ShieldMaterials[VisualIndex])
            {
                ShieldShells[VisualIndex]->SetVisibility(Hull > 0 && !Retreated && Shields > 0);
                const float Fraction = FMath::Clamp(static_cast<float>(Shields) /
                    FMath::Max(1, Participant.initialShields), 0.f, 1.f);
                ShieldMaterials[VisualIndex]->SetScalarParameterValue(TEXT("Strength"), Fraction * 0.45f);
            }
        }
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

        const float EffectiveStrength = FMath::Max(ShieldFraction * 0.45f, K.FlareStrength * 0.85f);
        Shell->SetVisibility(K.bAlive && !K.bRetreated && K.Shields > 0);
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
                Ships[Index]->SetRelativeScale3D(FVector(ShrinkScale));

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

    const float InterpSpeed = (CameraMode == Cam_Chase) ? 2.5f : 3.5f;
    Capture->SetRelativeLocation(FMath::VInterpTo(Capture->GetRelativeLocation(), DesiredLocation, DeltaSeconds, InterpSpeed));
    Capture->SetRelativeRotation(FMath::RInterpTo(Capture->GetRelativeRotation(), DesiredRotation, DeltaSeconds, InterpSpeed));
}

void ABotfCombatScene::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Elapsed += DeltaSeconds;

    if (bIsRoundAnimating)
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
