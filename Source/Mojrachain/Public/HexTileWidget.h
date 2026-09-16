#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "World/WorldMapTypes.h"
#include "HexTileWidget.generated.h"

class UGameMapWidget;
class AProceduralTerrainActor;
class UImage;

UCLASS()
class MOJRACHAIN_API UHexTileWidget : public UUserWidget
{
	GENERATED_BODY()

	public:
		UPROPERTY(BlueprintReadOnly, Category = "Hex")
		FVector2D GridCoords;

		UPROPERTY(BlueprintReadOnly, Category = "Hex")
		EHexState TileState;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex")
		TSubclassOf<AProceduralTerrainActor> TerrainClass;

		UPROPERTY()
		UGameMapWidget* ParentMapWidget;

		UFUNCTION(BlueprintCallable, Category = "Hex")
		void SetupTile(FVector2D InCoords, EHexState InState, UGameMapWidget* InParentMap);

	protected:
		UPROPERTY(meta = (BindWidget))
		class UButton* HexButton;

		UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
		TObjectPtr<UImage> HexImage;

		UFUNCTION(BlueprintImplementableEvent, Category = "Hex")
		void UpdateVisuals();

		UFUNCTION()
		void OnHexClicked();

		void ApplyTransparentButtonStyle();
		void RefreshVisuals();
};
