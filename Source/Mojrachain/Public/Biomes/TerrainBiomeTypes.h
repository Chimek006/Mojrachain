#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerrainMountainTypes.h"
#include "TerrainBiomeTypes.generated.h"

UENUM(BlueprintType)
enum class ETerrainBiome : uint8
{
	Grassland UMETA(DisplayName = "Grassland"),
	Forest UMETA(DisplayName = "Forest"),
	Hills UMETA(DisplayName = "Hills"),
	Desert UMETA(DisplayName = "Desert"),
	Mountain UMETA(DisplayName = "Mountain"),
	Swamp UMETA(DisplayName = "Swamp"),
	Tundra UMETA(DisplayName = "Tundra")
};

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FBiomeTerrainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Height")
	float BaseHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Height")
	float HeightScale = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise")
	float NoiseScale = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise")
	int32 Octaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise")
	float Persistence = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise")
	float Lacunarity = 1.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Shape")
	float HeightPower = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Detail")
	float DetailStrength = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Detail")
	float DetailNoiseScale = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Ridges")
	float RidgeStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Ridges")
	float RidgeNoiseScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain")
	FMountainTerrainSettings MountainSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Smoothing")
	int32 SmoothingIterations = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Smoothing")
	float SmoothingStrength = 0.48f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Hex Edge")
	float EdgeFalloff = 0.0f;
};

UCLASS()
class MOJRACHAIN_API UTerrainBiomeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Terrain|Biome")
	static FBiomeTerrainSettings GetTerrainBiomeSettings(ETerrainBiome Biome);
};
