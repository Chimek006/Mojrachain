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

	/** Changes the maximum horizontal flight speed at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Player|Flight")
	static void SetFlightSpeed(ACharacter* Character, float NewSpeed);

	/** Returns the current maximum flight speed configured on the character. */
	UFUNCTION(BlueprintPure, Category = "Player|Flight")
	static float GetFlightSpeed(const ACharacter* Character);

	UFUNCTION(BlueprintPure, Category = "Player|Flight")
	static bool IsFlying(const ACharacter* Character);
};
