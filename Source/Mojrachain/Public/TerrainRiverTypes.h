#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerrainRiverTypes.generated.h"

class AProceduralTerrainActor;

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FGeneratedTerrainRiverPath
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|River")
	TArray<FVector2D> WorldPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|River")
	float HalfWidth = 0.0f;
};

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FRiverForbiddenArea
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|River")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|River")
	float Radius = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|River")
	float Clearance = 0.0f;
};

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FRiverTerrainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|River")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|River", meta = (
		ClampMin = "100.0",
		UIMin = "300.0",
		UIMax = "2200.0",
		DisplayName = "River Width"
		))
	float Width = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|River", meta = (
		ClampMin = "0.0",
		UIMin = "120.0",
		UIMax = "1600.0",
		DisplayName = "River Depth"
		))
	float Depth = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|River", meta = (
		ClampMin = "0.05",
		UIMin = "0.2",
		UIMax = "0.8",
		DisplayName = "Flat Bed Ratio"
		))
	float FlatBedRatio = 0.46f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|River", meta = (
		ClampMin = "0.0",
		UIMin = "0.0",
		UIMax = "0.4",
		DisplayName = "Bed Noise Strength"
		))
	float BedNoiseStrength = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|River", meta = (
		ClampMin = "0.05",
		UIMin = "0.2",
		UIMax = "1.5",
		DisplayName = "Bed Noise Scale"
		))
	float BedNoiseScale = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|River")
	TArray<FVector2D> WorldPoints;
};

struct FRiverTerrainSample
{
	float Mask = 0.0f;
	float CorridorMask = 0.0f;
	float BankMask = 0.0f;
	float CenterMask = 0.0f;
	float BedNoise = 0.0f;
	float Width = 0.0f;
	float Depth = 0.0f;
};

UCLASS()
class MOJRACHAIN_API UTerrainRiverLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool BuildRiverForTerrain(
		const AProceduralTerrainActor* Terrain,
		float WidthMin,
		float WidthMax,
		float DepthMin,
		float DepthMax,
		float ClearanceFromOtherRivers,
		const TArray<FGeneratedTerrainRiverPath>& ExistingRiverPaths,
		const TArray<FRiverForbiddenArea>& ForbiddenAreas,
		FRiverTerrainSettings& OutSettings,
		FGeneratedTerrainRiverPath& OutPath
	);

	static FRiverTerrainSample SampleRiver(
		float WorldX,
		float WorldY,
		int32 Seed,
		const FRiverTerrainSettings& Settings
	);

private:
	static float SmoothStep01(float Value);
};
