#pragma once

#include "CoreMinimal.h"
#include "Biomes/TerrainBiomeTypes.h"
#include "TerrainRiverTypes.h"
#include "WorldMapTypes.generated.h"

class AProceduralTerrainActor;

UENUM(BlueprintType)
enum class EHexState : uint8
{
	Available UMETA(DisplayName = "Available to Generate"),
	Generated UMETA(DisplayName = "Already Generated")
};

UENUM(BlueprintType)
enum class EWorldBiomeGenerationMode : uint8
{
	Manual UMETA(DisplayName = "Manual Biome Selection"),
	AllMixed UMETA(DisplayName = "All / Seeded Biome Mix")
};

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FWorldMapGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|Seed")
	int32 WorldSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|Biome")
	EWorldBiomeGenerationMode BiomeGenerationMode = EWorldBiomeGenerationMode::AllMixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|River", meta = (ClampMin = "100.0", UIMin = "300.0", UIMax = "2000.0"))
	float RiverWidthMin = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|River", meta = (ClampMin = "100.0", UIMin = "600.0", UIMax = "3200.0"))
	float RiverWidthMax = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|River", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "800.0"))
	float RiverDepthMin = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|River", meta = (ClampMin = "0.0", UIMin = "100.0", UIMax = "1400.0"))
	float RiverDepthMax = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|River", meta = (ClampMin = "1", UIMin = "1", UIMax = "12"))
	int32 RiverDistanceMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|River", meta = (ClampMin = "1", UIMin = "2", UIMax = "24"))
	int32 RiverDistanceMax = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|River", meta = (ClampMin = "0.0", UIMin = "500.0", UIMax = "6000.0"))
	float RiverClearance = 1800.0f;
};

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FWorldHexRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Hex")
	FIntPoint Coordinates = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Hex")
	EHexState State = EHexState::Available;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Hex")
	ETerrainBiome Biome = ETerrainBiome::Grassland;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|Hex")
	TArray<ETerrainBiome> Biomes;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "World|Hex")
	TObjectPtr<AProceduralTerrainActor> TerrainActor = nullptr;
};

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FWorldRiverRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|River")
	FGeneratedTerrainRiverPath Path;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World|River")
	FRiverTerrainSettings Settings;
};
