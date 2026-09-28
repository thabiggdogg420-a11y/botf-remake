#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BotfGameMode.generated.h"

class ABotfCombatScene;

UCLASS()
class ABotfGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ABotfGameMode();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TSharedPtr<class SWidget> Interface;
    UPROPERTY()
    TObjectPtr<ABotfCombatScene> CombatScene;
};
