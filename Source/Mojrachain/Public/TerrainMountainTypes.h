#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerrainMountainTypes.generated.h"

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FMountainTerrainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.0",
		ClampMax = "1.0",
		UIMin = "0.0",
		UIMax = "1.0",
		DisplayName = "Mountain Strength"
		))
	float Strength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		DisplayName = "Mountain Center"
		))
	FVector2D Center = FVector2D(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.01",
		ClampMax = "2.0",
		UIMin = "0.25",
		UIMax = "1.0",
		DisplayName = "Mountain Radius"
		))
	float Radius = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0",
		ClampMax = "8",
		UIMin = "0",
		UIMax = "4",
		DisplayName = "Mountain Count Min"
		))
	int32 CountMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0",
		ClampMax = "8",
		UIMin = "0",
		UIMax = "4",
		DisplayName = "Mountain Count Max"
		))
	int32 CountMax = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.05",
		UIMin = "0.2",
		UIMax = "1.0",
		DisplayName = "Radius Min Multiplier"
		))
	float RadiusMinMultiplier = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.05",
		UIMin = "0.4",
		UIMax = "1.5",
		DisplayName = "Radius Max Multiplier"
		))
	float RadiusMaxMultiplier = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.0",
		UIMin = "0.0",
		UIMax = "1.5",
		DisplayName = "Placement Range"
		))
	float PlacementRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.1",
		UIMin = "0.8",
		UIMax = "4.0",
		DisplayName = "Sharpness"
		))
	float Sharpness = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.0",
		UIMin = "0.0",
		UIMax = "0.75",
		DisplayName = "Height Variation"
		))
	float HeightVariation = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.0",
		UIMin = "0.0",
		UIMax = "0.75",
		DisplayName = "Jagged Strength"
		))
	float JaggedStrength = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain", meta = (
		ClampMin = "0.0",
		UIMin = "1.0",
		UIMax = "6.0",
		DisplayName = "Jagged Noise Scale"
		))
	float JaggedNoiseScale = 2.8f;
};

UCLASS()
class MOJRACHAIN_API UTerrainMountainLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static float SampleMountainContribution(
		float WorldX,
		float WorldY,
		const FVector& ActorLocation,
		int32 Seed,
		int32 Resolution,
		float TerrainSize,
		float Ridge,
		const FMountainTerrainSettings& Settings
	);

private:
	static float SmoothStep01(float Value);
	static float GetTerrainHorizontalOffset(int32 Resolution, float TerrainSize);
	static float GetTerrainDiagonalYOffset(int32 Resolution, float TerrainSize);
	static float GetTerrainVerticalOffset(int32 Resolution, float TerrainSize);
	static FIntPoint GetNearestTerrainTileCoords(const FVector& ActorLocation, int32 Resolution, float TerrainSize);
	static FVector2D GetTerrainTileCenter(const FIntPoint& TileCoords, int32 Resolution, float TerrainSize);
	static float SampleTileMountainContribution(
		float WorldX,
		float WorldY,
		const FIntPoint& TileCoords,
		const FVector2D& GridOrigin,
		int32 Seed,
		int32 Resolution,
		float TerrainSize,
		float Ridge,
		const FMountainTerrainSettings& Settings
	);
};
