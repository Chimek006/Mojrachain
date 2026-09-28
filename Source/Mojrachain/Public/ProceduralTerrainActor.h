#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Biomes/TerrainBiomeTypes.h"
#include "TerrainRiverTypes.h"
#include "ProceduralTerrainActor.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UInstancedStaticMeshComponent;

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FBiomeObjectSpawnChance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (DisplayName = "Biome"))
	ETerrainBiome Biome = ETerrainBiome::Grassland;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (
		ClampMin = "0.0",
		ClampMax = "100.0",
		UIMin = "0.0",
		UIMax = "100.0",
		DisplayName = "Chance (%)"
		))
	float ChancePercent = 0.0f;

	FBiomeObjectSpawnChance() = default;

	FBiomeObjectSpawnChance(ETerrainBiome InBiome, float InChancePercent)
		: Biome(InBiome)
		, ChancePercent(InChancePercent)
	{
	}
};

USTRUCT(BlueprintType)
struct MOJRACHAIN_API FProceduralTerrainObjectRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object", meta = (DisplayName = "Object Mesh"))
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object", meta = (DisplayName = "Biomes and Frequency"))
	TArray<FBiomeObjectSpawnChance> BiomeChances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object", meta = (
		ClampMin = "1",
		UIMin = "1",
		UIMax = "1000",
		DisplayName = "Attempts per Hex"
		))
	int32 AttemptsPerHex = 250;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object", meta = (
		ClampMin = "0.01",
		UIMin = "0.1",
		UIMax = "5.0",
		DisplayName = "Minimum Scale"
		))
	float MinScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object", meta = (
		ClampMin = "0.01",
		UIMin = "0.1",
		UIMax = "5.0",
		DisplayName = "Maximum Scale"
		))
	float MaxScale = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object", meta = (DisplayName = "Align to Terrain"))
	bool bAlignToTerrain = true;

	FProceduralTerrainObjectRule()
	{
		BiomeChances = {
			FBiomeObjectSpawnChance(ETerrainBiome::Grassland, 100.0f),
			FBiomeObjectSpawnChance(ETerrainBiome::Forest, 25.0f),
			FBiomeObjectSpawnChance(ETerrainBiome::Swamp, 25.0f),
			FBiomeObjectSpawnChance(ETerrainBiome::Mountain, 40.0f),
			FBiomeObjectSpawnChance(ETerrainBiome::Hills, 75.0f),
			FBiomeObjectSpawnChance(ETerrainBiome::Desert, 0.0f),
			FBiomeObjectSpawnChance(ETerrainBiome::Tundra, 0.0f)
		};
	}
};

// Runtime-only entries for the deterministic world biome field. Keeping the
// generated site layout cached avoids rebuilding the same 25-hex neighbourhood
// for every vertex sampled during one terrain generation pass.
struct FGlobalBiomeSite
{
	FVector2D Position = FVector2D::ZeroVector;
	ETerrainBiome Biome = ETerrainBiome::Grassland;
};

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
		ClampMin = "100.0",
		ClampMax = "500000.0",
		UIMin = "10000.0",
		UIMax = "200000.0"
		))
	float Size = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Basic")
	ETerrainBiome Biome = ETerrainBiome::Grassland;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|Biome")
	TArray<ETerrainBiome> ActiveBiomes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain|World")
	FIntPoint WorldHexCoordinates = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Surface")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Objects", meta = (
		DisplayName = "Objects"
		))
	TArray<FProceduralTerrainObjectRule> ObjectSpawnRules;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Terrain")
	void GenerateTerrain();

	UFUNCTION(BlueprintCallable, Category = "Terrain")
	float GetTerrainHeightAtWorldLocation(float WorldX, float WorldY) const;

	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void CopySettingsFrom(const AProceduralTerrainActor* OtherTerrain);

	UFUNCTION(BlueprintCallable, Category = "Terrain|Biome")
	void SetBiomeLayers(const TArray<ETerrainBiome>& InBiomes);

	void SetWorldHexCoordinates(const FIntPoint& InCoordinates);

	UFUNCTION(BlueprintPure, Category = "Terrain|Biome")
	ETerrainBiome GetBiomeAtWorldLocation(float WorldX, float WorldY) const;

	const TArray<ETerrainBiome>& GetActiveBiomes() const { return ActiveBiomes; }

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

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Terrain|Surface")
	void ApplyBiomeMaterial();

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	static constexpr float ReferenceTerrainSize = 10000.0f;
	static constexpr float NoiseScaleMultiplier = 0.001f;
	static constexpr float TextureBiomeBlendWidth = 100.0f;

	TArray<FRiverTerrainSettings> RuntimeRiverSettings;

	mutable TMap<FIntPoint, TArray<FGlobalBiomeSite>> GlobalBiomeSiteCache;
	mutable int32 CachedGlobalBiomeSeed = TNumericLimits<int32>::Min();
	mutable float CachedGlobalBiomeSize = -1.0f;
	mutable FIntPoint CachedGlobalBiomeCoordinates = FIntPoint(ForceInitToZero);
	mutable FVector2D CachedGlobalBiomeActorXY = FVector2D(
		TNumericLimits<float>::Max(),
		TNumericLimits<float>::Max()
	);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RuntimeTerrainMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RuntimeTerrainMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> RuntimeObjectInstanceComponents;

	float GetRealNoiseScale() const;
	float GetRealDetailNoiseScale() const;
	float GetRealRidgeNoiseScale() const;
	float GetWorldSizeScale() const;
	float GetWorldNoiseScale() const;
	float GetBiomeTerrainTransitionWidth(ETerrainBiome FirstBiome, ETerrainBiome SecondBiome) const;

	bool HasActiveRivers() const;
	FRiverTerrainSample SampleRiverCorridor(float WorldX, float WorldY) const;

	float SampleHeight(float WorldX, float WorldY, float LocalX, float LocalY, float OffsetX, float OffsetY) const;

	float SampleFBM(float X, float Y, float OffsetX, float OffsetY, float Scale, int32 InOctaves) const;

	float SampleRidgedNoise(float X, float Y, float OffsetX, float OffsetY) const;

	float GetHexEdgeMask(const FVector2D& P, float Radius) const;

	float SmoothStep01(float Value) const;

	bool IsPointInsideHex(const FVector2D& P, float Radius) const;

	void EnsureDefaultObjectSpawnRules();
	void ClearGeneratedTerrainObjects();
	void ApplyBiomeMaterialsToSections(const TArray<ETerrainBiome>& SectionBiomes);
	UMaterialInstanceDynamic* CreateBiomeMaterialInstance(UMaterialInterface* MaterialToUse, ETerrainBiome InBiome);
	void CalculateBiomeWeights(
		float LocalX,
		float LocalY,
		TArray<float>& OutWeights,
		bool bForTerrainHeight = false
	) const;
	void CalculateGlobalBiomeWeights(
		float WorldX,
		float WorldY,
		TArray<float>& OutWeights,
		bool bForTerrainHeight = false
	) const;
	void CalculateFixedBiomeWeights(float LocalX, float LocalY, TArray<float>& OutWeights) const;
	ETerrainBiome GetBiomeAtLocalLocation(float LocalX, float LocalY) const;
	FLinearColor GetBiomeTintAtLocalLocation(float LocalX, float LocalY) const;
	TArray<ETerrainBiome> GetNormalizedActiveBiomes() const;
	void GenerateTerrainObjects(
		const TArray<float>& Heights,
		const TArray<FVector>& Normals,
		int32 VertCount,
		float Step,
		float MinHeight,
		float MaxHeight
	);
};
