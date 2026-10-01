#include "PlayerFlightLibrary.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr float DefaultFlightSpeed = 2500.0f;
}

void UPlayerFlightLibrary::ToggleFlight(ACharacter* Character)
{
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	if (Movement->MovementMode == MOVE_Flying)
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}
	else
	{
		// UE's default MaxFlySpeed is only about 600 uu/s, which is too slow
		// for this project's kilometre-scale hexes. Preserve a value explicitly
		// configured in the Character Movement component, but upgrade the stock
		// default the first time flight is enabled.
		if (Movement->MaxFlySpeed <= 600.0f)
		{
			Movement->MaxFlySpeed = DefaultFlightSpeed;
		}

		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);
	}
}

void UPlayerFlightLibrary::ApplyFlightVerticalInput(ACharacter* Character, float AxisValue)
{
	if (!Character || !IsFlying(Character) || FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	Character->AddMovementInput(FVector::UpVector, FMath::Clamp(AxisValue, -1.0f, 1.0f));
}

void UPlayerFlightLibrary::SetFlightSpeed(ACharacter* Character, float NewSpeed)
{
	if (!Character)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->MaxFlySpeed = FMath::Max(NewSpeed, 0.0f);
	}
}

float UPlayerFlightLibrary::GetFlightSpeed(const ACharacter* Character)
{
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	return Movement ? Movement->MaxFlySpeed : 0.0f;
}

bool UPlayerFlightLibrary::IsFlying(const ACharacter* Character)
{
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	return Movement && Movement->MovementMode == MOVE_Flying;
}
