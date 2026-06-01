#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexTileWidget.generated.h"

class UGameMapWidget;

UENUM(BlueprintType)
enum class EHexState : uint8
{
	Available UMETA(DisplayName = "Available to Generate"),
	Generated UMETA(DisplayName = "Already Generated")
};

UCLASS()
class MOJRACHAIN_API UHexTileWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Hex")
	FVector2D GridCoords;

	UPROPERTY(BlueprintReadOnly, Category = "Hex")
	EHexState TileState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AActor> TerrainClass;

	UPROPERTY()
	UGameMapWidget* ParentMapWidget;

	UFUNCTION(BlueprintCallable, Category = "Hex")
	void SetupTile(FVector2D InCoords, EHexState InState, UGameMapWidget* InParentMap);

protected:
	UPROPERTY(meta = (BindWidget))
	class UButton* HexButton;

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex")
	void UpdateVisuals();

	UFUNCTION()
	void OnHexClicked();
};