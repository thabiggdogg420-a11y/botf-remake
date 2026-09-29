#include "BotfAssetRegistry.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString ToFString(const std::string& Value) { return FString(UTF8_TO_TCHAR(Value.c_str())); }
std::string ToStd(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value)); }
}

void UBotfAssetRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Reload();
}

void UBotfAssetRegistry::Reload()
{
    Catalog = botf::assets::Catalog();
    bLoaded = false;
    const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("BOTF_RESOLVED_MANIFEST"));
    ManifestPath = !Override.IsEmpty() ? Override
        : FPaths::Combine(FPaths::ProjectDir(), TEXT("LocalAssets"), TEXT("resolved-manifest.json"));
    FString Text;
    if (!FPaths::FileExists(ManifestPath) || !FFileHelper::LoadFileToString(Text, *ManifestPath))
    {
        UE_LOG(LogTemp, Display, TEXT("BOTF assets: no local manifest at %s; using built-in placeholders"), *ManifestPath);
        return;
    }
    std::string Error;
    bLoaded = Catalog.loadFromJson(ToStd(Text), &Error);
    // Braces are required: UE 5.8's UE_LOG expands to a block, so an unbraced if/else does not compile.
    if (bLoaded)
    {
        UE_LOG(LogTemp, Display, TEXT("BOTF assets: %d keys from %s"), static_cast<int32>(Catalog.size()), *ManifestPath);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BOTF assets: cannot read %s (%s); using built-in placeholders"), *ManifestPath, *ToFString(Error));
    }
}

UBotfAssetRegistry* UBotfAssetRegistry::Get()
{
    UGameInstance* Instance = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameInstance() : nullptr;
    return Instance ? Instance->GetSubsystem<UBotfAssetRegistry>() : nullptr;
}

FSoftObjectPath UBotfAssetRegistry::ResolvePath(const FString& Key) const
{
    const auto Result = Catalog.resolve(ToStd(Key));
    return Result.placeholder ? FSoftObjectPath() : FSoftObjectPath(ToFString(Result.primaryPath()));
}

UStaticMesh* UBotfAssetRegistry::LoadShipMesh(int32 ReferenceShipId, int32 OwnerFaction, double& OutLengthMeters, FString& OutKey) const
{
    const auto Result = Catalog.resolveShip(ReferenceShipId, OwnerFaction);
    OutLengthMeters = Result.targetLengthMeters;
    OutKey = ToFString(Result.resolvedKey.empty() ? Result.requestedKey : Result.resolvedKey);
    UStaticMesh* Mesh = nullptr;
    if (!Result.primaryPath().empty())
        Mesh = Cast<UStaticMesh>(FSoftObjectPath(ToFString(Result.primaryPath())).TryLoad());
    if (!Mesh && !Result.placeholder)
    {
        // The manifest says resolved but the asset is gone (not imported, or deleted): use the faction's built-in mesh.
        UE_LOG(LogTemp, Warning, TEXT("BOTF assets: %s resolved to %s but it did not load"), *OutKey, *ToFString(Result.primaryPath()));
        const std::string BuiltIn = botf::assets::builtInShipPlaceholder(Result.factionPrefix.empty()
            ? botf::assets::ownerFactionPrefix(OwnerFaction) : Result.factionPrefix);
        if (!BuiltIn.empty()) Mesh = Cast<UStaticMesh>(FSoftObjectPath(ToFString(BuiltIn)).TryLoad());
    }
    if (!Mesh) Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    return Mesh;
}

int32 UBotfAssetRegistry::CountVariants(const std::string& Key) const
{
    return static_cast<int32>(Catalog.resolve(Key).objectPaths.size());
}

USoundBase* UBotfAssetRegistry::LoadSound(const std::vector<std::string>& Keys, int32 Variant, FString* OutKey) const
{
    for (const auto& Key : Keys)
    {
        const auto Result = Catalog.resolve(Key);
        if (Result.placeholder || Result.objectPaths.empty()) continue;
        const std::size_t Index = static_cast<std::size_t>(FMath::Max(0, Variant)) % Result.objectPaths.size();
        if (USoundBase* Sound = Cast<USoundBase>(FSoftObjectPath(ToFString(Result.objectPaths[Index])).TryLoad()))
        {
            if (OutKey) *OutKey = ToFString(Result.resolvedKey);
            return Sound;
        }
    }
    return nullptr;
}
