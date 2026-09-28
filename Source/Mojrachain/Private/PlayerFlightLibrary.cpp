#include "PlayerFlightLibrary.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

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

bool UPlayerFlightLibrary::IsFlying(const ACharacter* Character)
{
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	return Movement && Movement->MovementMode == MOVE_Flying;
}
