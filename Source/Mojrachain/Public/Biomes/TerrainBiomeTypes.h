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
	Tundra UMETA(DisplayName = "Tundra"),
	All UMETA(DisplayName = "All / Seeded Mix")
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

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FBiomeSurfaceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	FLinearColor BaseColor = FLinearColor(0.18f, 0.42f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	FLinearColor AccentColor = FLinearColor(0.36f, 0.62f, 0.12f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	FLinearColor RockColor = FLinearColor(0.30f, 0.31f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	FLinearColor SnowColor = FLinearColor(0.92f, 0.96f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	float PatternScale = 0.0025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	float Roughness = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	float RockStart = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	float RockEnd = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	float SnowStart = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	float SnowEnd = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	float SlopeRockStrength = 0.45f;
};

UCLASS()
class MOJRACHAIN_API UTerrainBiomeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Terrain|Biome")
	static FBiomeTerrainSettings GetTerrainBiomeSettings(ETerrainBiome Biome);

	UFUNCTION(BlueprintPure, Category = "Terrain|Biome")
	static FBiomeSurfaceSettings GetBiomeSurfaceSettings(ETerrainBiome Biome);
};
