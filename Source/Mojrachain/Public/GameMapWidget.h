#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexTileWidget.h" 
#include "TerrainRiverTypes.h"
#include "GameMapWidget.generated.h"

class AProceduralTerrainActor;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|River", meta = (ClampMin = "100.0", UIMin = "300.0", UIMax = "2000.0"))
	float RiverWidthMin = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|River", meta = (ClampMin = "100.0", UIMin = "600.0", UIMax = "3200.0"))
	float RiverWidthMax = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|River", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "800.0"))
	float RiverDepthMin = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|River", meta = (ClampMin = "0.0", UIMin = "100.0", UIMax = "1400.0"))
	float RiverDepthMax = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|River", meta = (ClampMin = "1", UIMin = "1", UIMax = "12"))
	int32 RiverDistanceMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|River", meta = (ClampMin = "1", UIMin = "2", UIMax = "24"))
	int32 RiverDistanceMax = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings|River", meta = (ClampMin = "0.0", UIMin = "500.0", UIMax = "6000.0"))
	float RiverClearance = 1800.0f;

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

	UFUNCTION(BlueprintCallable, Category = "Map Logic")
	void HandleGeneratedTerrain(AProceduralTerrainActor* Terrain);

private:
	class UHexTileWidget* SpawnHexUI(FVector2D Coords, EHexState State);
	bool ShouldSpawnRiverForTerrain(const AProceduralTerrainActor* Terrain);
	bool ConfigureRiverForTerrain(AProceduralTerrainActor* Terrain);
	void RollNextRiverDistance();

	int32 GeneratedNonDesertTilesSinceRiver = 0;
	int32 NextRiverDistance = 0;
	bool bForceNextRiver = true;

	UPROPERTY()
	TArray<FGeneratedTerrainRiverPath> GeneratedRiverPaths;
};
