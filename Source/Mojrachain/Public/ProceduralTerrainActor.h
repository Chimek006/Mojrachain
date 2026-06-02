#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Biomes/TerrainBiomeTypes.h"
#include "TerrainRiverTypes.h"
#include "ProceduralTerrainActor.generated.h"

UCLASS()
class MOJRACHAIN_API AProceduralTerrainActor : public AActor
{
	GENERATED_BODY()

public:
	AProceduralTerrainActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> ProcMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Basic")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Basic", meta = (
		ClampMin = "2",
		UIMin = "50",
		UIMax = "150",
		DisplayName = "Resolution (50-150)"
		))
	int32 Resolution = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Basic", meta = (
		ClampMin = "100.0"
		))
	float Size = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Basic")
	ETerrainBiome Biome = ETerrainBiome::Grassland;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Height", meta = (
		DisplayName = "Base Height"
		))
	float BaseHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Height", meta = (
		ClampMin = "0.0",
		UIMin = "0.0",
		UIMax = "2500.0",
		DisplayName = "Height Scale (50 flat, 500 hills, 1400+ mountains)"
		))
	float HeightScale = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (
		ClampMin = "0.0",
		UIMin = "0.03",
		UIMax = "2.0",
		DisplayName = "Noise Scale (0.03 long, 0.45 natural, 2.0+ chaos)"
		))
	float NoiseScale = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (
		ClampMin = "1",
		UIMin = "1",
		UIMax = "6",
		DisplayName = "Octaves (1-2 smooth, 3-4 natural, 5+ detail)"
		))
	int32 Octaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (
		ClampMin = "0.0",
		ClampMax = "1.0",
		UIMin = "0.25",
		UIMax = "0.75",
		DisplayName = "Persistence (0.3 smooth, 0.5 natural, 0.7+ rough)"
		))
	float Persistence = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (
		ClampMin = "1.0",
		UIMin = "1.5",
		UIMax = "2.5",
		DisplayName = "Lacunarity (1.5 soft, 2.0 normal, 2.5+ sharp)"
		))
	float Lacunarity = 1.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Shape", meta = (
		ClampMin = "0.1",
		UIMin = "0.8",
		UIMax = "3.0",
		DisplayName = "Height Power (1.0 even, 1.5 natural, 2.0+ lower land)"
		))
	float HeightPower = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Detail", meta = (
		ClampMin = "0.0",
		ClampMax = "1.0",
		UIMin = "0.0",
		UIMax = "0.2",
		DisplayName = "Detail Strength (0.0 none, 0.05 soft, 0.12+ rough)"
		))
	float DetailStrength = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Detail", meta = (
		ClampMin = "0.0",
		UIMin = "0.5",
		UIMax = "4.0",
		DisplayName = "Detail Noise Scale (0.5 wide, 2.0 normal, 3.0+ fine)"
		))
	float DetailNoiseScale = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Ridges", meta = (
		ClampMin = "0.0",
		ClampMax = "1.0",
		UIMin = "0.0",
		UIMax = "0.5",
		DisplayName = "Ridge Strength (0.0 none, 0.15 light, 0.35+ sharp)"
		))
	float RidgeStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Ridges", meta = (
		ClampMin = "0.0",
		UIMin = "0.3",
		UIMax = "2.0",
		DisplayName = "Ridge Noise Scale (0.3 wide, 0.8 natural, 1.2+ dense)"
		))
	float RidgeNoiseScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Mountain")
	FMountainTerrainSettings MountainSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|River")
	FRiverTerrainSettings RiverSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Smoothing", meta = (
		ClampMin = "0",
		ClampMax = "20",
		UIMin = "0",
		UIMax = "6",
		DisplayName = "Smoothing Iterations (0 none, 2 normal, 5+ very smooth)"
		))
	int32 SmoothingIterations = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Smoothing", meta = (
		ClampMin = "0.0",
		ClampMax = "1.0",
		UIMin = "0.0",
		UIMax = "0.8",
		DisplayName = "Smoothing Strength (0.2 soft, 0.45 normal, 0.7+ strong)"
		))
	float SmoothingStrength = 0.48f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Hex Edge", meta = (
		ClampMin = "0.0",
		ClampMax = "0.5",
		UIMin = "0.0",
		UIMax = "0.25",
		DisplayName = "Edge Falloff (0.0 none, 0.12 normal, 0.22+ flat edge)"
		))
	float EdgeFalloff = 0.0f;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Terrain")
	void GenerateTerrain();

	UFUNCTION(BlueprintCallable, Category = "Terrain")
	float GetTerrainHeightAtWorldLocation(float WorldX, float WorldY) const;

	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void CopySettingsFrom(const AProceduralTerrainActor* OtherTerrain);

	bool ConfigureRiver(
		float WidthMin,
		float WidthMax,
		float DepthMin,
		float DepthMax,
		float ClearanceFromOtherRivers,
		const TArray<FGeneratedTerrainRiverPath>& ExistingRiverPaths,
		const TArray<FRiverForbiddenArea>& ForbiddenAreas,
		FGeneratedTerrainRiverPath& OutRiverPath
	);

	void ClearRiver();
	void AddRiver(const FRiverTerrainSettings& InRiverSettings);

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Terrain|Biome")
	void ApplyBiomePreset();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	static constexpr float NoiseScaleMultiplier = 0.001f;

	TArray<FRiverTerrainSettings> RuntimeRiverSettings;

	float GetRealNoiseScale() const;
	float GetRealDetailNoiseScale() const;
	float GetRealRidgeNoiseScale() const;

	bool HasActiveRivers() const;
	FRiverTerrainSample SampleRiverCorridor(float WorldX, float WorldY) const;

	float SampleHeight(float WorldX, float WorldY, float LocalX, float LocalY, float OffsetX, float OffsetY) const;

	float SampleFBM(float X, float Y, float OffsetX, float OffsetY, float Scale, int32 InOctaves) const;

	float SampleRidgedNoise(float X, float Y, float OffsetX, float OffsetY) const;

	float GetHexEdgeMask(const FVector2D& P, float Radius) const;

	float SmoothStep01(float Value) const;

	bool IsPointInsideHex(const FVector2D& P, float Radius) const;
};
