#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PlayerFlightLibrary.generated.h"

class ACharacter;

UCLASS()
class MOJRACHAIN_API UPlayerFlightLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Player|Flight")
	static void ToggleFlight(ACharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Player|Flight")
	static void ApplyFlightVerticalInput(ACharacter* Character, float AxisValue);

	UFUNCTION(BlueprintPure, Category = "Player|Flight")
	static bool IsFlying(const ACharacter* Character);
};
