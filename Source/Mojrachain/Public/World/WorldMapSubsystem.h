#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/WorldMapTypes.h"
#include "WorldMapSubsystem.generated.h"

class AProceduralTerrainActor;

UCLASS()
class MOJRACHAIN_API UWorldMapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Configure(const FWorldMapGenerationSettings& InSettings);
	void ResetWorld();
	void InitializeWorld(AProceduralTerrainActor* ExistingCenterTerrain = nullptr);

	TArray<FIntPoint> ExpandHex(const FIntPoint& CenterCoordinates);

	bool GenerateHex(
		const FIntPoint& Coordinates,
		TSubclassOf<AProceduralTerrainActor> TerrainClass,
		AProceduralTerrainActor*& OutTerrain
	);

	void RegisterExistingTerrain(const FIntPoint& Coordinates, AProceduralTerrainActor* Terrain);

	bool HasHex(const FIntPoint& Coordinates) const;
	EHexState GetHexState(const FIntPoint& Coordinates) const;
	const TMap<FIntPoint, FWorldHexRecord>& GetHexes() const { return Hexes; }
	const TArray<FWorldRiverRecord>& GetRivers() const { return Rivers; }
	int32 GetWorldSeed() const { return Settings.WorldSeed; }

	AProceduralTerrainActor* FindTerrainSettingsSource() const;

	virtual void Deinitialize() override;

private:
	FWorldMapGenerationSettings Settings;

	UPROPERTY(Transient)
	TMap<FIntPoint, FWorldHexRecord> Hexes;

	UPROPERTY(Transient)
	TArray<FWorldRiverRecord> Rivers;

	int32 GeneratedNonDesertTilesSinceRiver = 0;
	int32 NextRiverDistance = 0;
	bool bForceNextRiver = true;
	FRandomStream WorldRandomStream;

	static TArray<FIntPoint> GetNeighborDirections();
	TArray<ETerrainBiome> SelectBiomesForHex(const FIntPoint& Coordinates) const;
	static FVector2D HexToWorldOffset(const FIntPoint& Coordinates, float TerrainSize, int32 TerrainResolution);
	FVector GetHexWorldLocation(const FIntPoint& Coordinates, TSubclassOf<AProceduralTerrainActor> TerrainClass, const AProceduralTerrainActor* SourceTerrain) const;

	bool ShouldSpawnRiverForTerrain(const AProceduralTerrainActor* Terrain);
	void RollNextRiverDistance();
	bool ConfigureRiverForTerrain(AProceduralTerrainActor* Terrain);
	bool ApplyExistingRiversToTerrain(AProceduralTerrainActor* Terrain);
	void ApplyRiverToAlreadyGeneratedTerrains(AProceduralTerrainActor* SourceTerrain, const FWorldRiverRecord& River);

	bool DoesRiverAffectTerrain(const AProceduralTerrainActor* Terrain, const FGeneratedTerrainRiverPath& RiverPath) const;
	static bool IsPointInsideExpandedHex(const FVector2D& Point, float Radius);
	static float DistanceSquaredToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B);

	void StoreGeneratedTerrain(const FIntPoint& Coordinates, AProceduralTerrainActor* Terrain);
};
