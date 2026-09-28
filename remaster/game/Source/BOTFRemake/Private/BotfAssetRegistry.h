#pragma once
// Resolves asset keys (ship.<fac>.<class>, weapon.<Type>[.<fac>], sfx.*, vo.*, mus.*) to /Game object paths.
//
// Reads LocalAssets/resolved-manifest.json (written by Import-Assets.ps1; BOTF_RESOLVED_MANIFEST overrides the path)
// once at startup. All parsing and the fallback walk live in the engine-independent BotfAssetCatalog.h; this class
// only converts strings to FSoftObjectPath and loads objects. A missing file, key or asset never fails: ships fall
// back to the built-in /Game/Presentation meshes and then to engine shapes, sounds to silence.

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "BotfAssetCatalog.h"
#include "BotfAssetRegistry.generated.h"

class UStaticMesh;
class USoundBase;

UCLASS()
class UBotfAssetRegistry final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Re-reads the resolved manifest (e.g. after running Import-Assets.ps1 with the game open).
    void Reload();

    // Registry of the running game instance, or nullptr before one exists (editor previews, early startup).
    static UBotfAssetRegistry* Get();

    const botf::assets::Catalog& GetCatalog() const { return Catalog; }
    bool HasLocalManifest() const { return bLoaded; }
    FString GetManifestPath() const { return ManifestPath; }

    // Key -> first variant's object path, walking fallbacks. Empty path when nothing resolves.
    FSoftObjectPath ResolvePath(const FString& Key) const;

    // Ship mesh for a legacy or roster ship id, owned by the combat faction (1..5). Never null unless the engine's
    // basic shapes are missing. OutLengthMeters is the class's canon length (0 when unknown); OutKey the key used.
    UStaticMesh* LoadShipMesh(int32 ReferenceShipId, int32 OwnerFaction, double& OutLengthMeters, FString& OutKey) const;

    // First key in Keys that resolves to a loadable sound. Variant picks among several files (voice lines).
    USoundBase* LoadSound(const std::vector<std::string>& Keys, int32 Variant = 0, FString* OutKey = nullptr) const;
    int32 CountVariants(const std::string& Key) const;

private:
    botf::assets::Catalog Catalog;
    FString ManifestPath;
    bool bLoaded = false;
};
