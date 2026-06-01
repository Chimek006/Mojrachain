#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexTileWidget.h" 
#include "GameMapWidget.generated.h"

UCLASS()
class MOJRACHAIN_API UGameMapWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	class UWidgetSwitcher* ContentSwitcher;

	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Map;

	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Inventory;

	UPROPERTY(meta = (BindWidget))
	class UButton* Btn_Settings;

	UPROPERTY(meta = (BindWidget))
	class UCanvasPanel* MapCanvas;

	UPROPERTY(EditAnywhere, Category = "Map Settings")
	TSubclassOf<UUserWidget> HexTileClass;

	UPROPERTY()
	TMap<FVector2D, EHexState> MapState;

	virtual void NativeConstruct() override;

	UFUNCTION() void ShowMap();
	UFUNCTION() void ShowInventory();
	UFUNCTION() void ShowSettings();

public:
	UFUNCTION(BlueprintCallable, Category = "Map Logic")
	void BuildHexMap();

	UFUNCTION(BlueprintCallable, Category = "Map Logic")
	void ExpandMap(FVector2D CenterCoords);

private:
	class UHexTileWidget* SpawnHexUI(FVector2D Coords, EHexState State);
};