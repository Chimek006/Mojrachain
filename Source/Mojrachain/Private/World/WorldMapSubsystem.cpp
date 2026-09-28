#include "World/WorldMapSubsystem.h"

#include "EngineUtils.h"
#include "ProceduralTerrainActor.h"
#include "World/WorldBoundaryActor.h"

namespace
{
struct FAffectedTerrainUpdate
{
	AProceduralTerrainActor* Terrain = nullptr;
	float DistanceSq = 0.0f;
};
}

void UWorldMapSubsystem::Configure(const FWorldMapGenerationSettings& InSettings)
{
	Settings = InSettings;
}

void UWorldMapSubsystem::ResetWorld()
{
	if (WorldBoundaryActor)
	{
		WorldBoundaryActor->ClearBoundary();
	}

	Hexes.Reset();
	Rivers.Reset();
	GeneratedNonDesertTilesSinceRiver = 0;
	NextRiverDistance = 0;
	bForceNextRiver = true;
	WorldRandomStream.Initialize(Settings.WorldSeed);
}

void UWorldMapSubsystem::InitializeWorld(AProceduralTerrainActor* ExistingCenterTerrain)
{
	if (ExistingCenterTerrain && ExistingCenterTerrain->Biome == ETerrainBiome::All)
	{
		Settings.BiomeGenerationMode = EWorldBiomeGenerationMode::AllMixed;
	}

	ResetWorld();

	if (ExistingCenterTerrain)
	{
		ExistingCenterTerrain->SetWorldHexCoordinates(FIntPoint::ZeroValue);
	}

	if (ExistingCenterTerrain
		&& ExistingCenterTerrain->Biome == ETerrainBiome::All
		&& Settings.BiomeGenerationMode == EWorldBiomeGenerationMode::AllMixed)
	{
		ExistingCenterTerrain->Seed = Settings.WorldSeed;
		ExistingCenterTerrain->SetBiomeLayers(SelectBiomesForHex(FIntPoint::ZeroValue));
		ExistingCenterTerrain->GenerateTerrain();
	}

	FWorldHexRecord CenterRecord;
	CenterRecord.Coordinates = FIntPoint::ZeroValue;
	CenterRecord.State = EHexState::Generated;
	CenterRecord.TerrainActor = ExistingCenterTerrain;
	CenterRecord.Biome = ExistingCenterTerrain
		? ExistingCenterTerrain->Biome
		: ETerrainBiome::Grassland;
	CenterRecord.Biomes = ExistingCenterTerrain
		? ExistingCenterTerrain->GetActiveBiomes()
		: TArray<ETerrainBiome>{ ETerrainBiome::Grassland };
	if (CenterRecord.Biomes.IsEmpty())
	{
		CenterRecord.Biomes.Add(CenterRecord.Biome);
	}
	CenterRecord.Biome = CenterRecord.Biomes[0];
	Hexes.Add(FIntPoint::ZeroValue, CenterRecord);
	UpdateWorldBoundary();
}

TArray<FIntPoint> UWorldMapSubsystem::GetNeighborDirections()
{
	return {
		FIntPoint(1, 0),
		FIntPoint(1, -1),
		FIntPoint(0, -1),
		FIntPoint(-1, 0),
		FIntPoint(-1, 1),
		FIntPoint(0, 1)
	};
}

TArray<ETerrainBiome> UWorldMapSubsystem::SelectBiomesForHex(const FIntPoint& Coordinates) const
{
	if (Settings.BiomeGenerationMode == EWorldBiomeGenerationMode::Manual)
	{
		return {};
	}

	const int32 HexSeed = static_cast<int32>(
		HashCombine(GetTypeHash(Settings.WorldSeed), GetTypeHash(Coordinates))
	);
	FRandomStream HexStream(HexSeed);

	const int32 Roll = HexStream.RandRange(0, 99);
	const int32 BiomeCount = Roll < 50
		? 1
		: (Roll < 80 ? 2 : (Roll < 95 ? 3 : 4));

	TArray<ETerrainBiome> AvailableBiomes = {
		ETerrainBiome::Grassland,
		ETerrainBiome::Forest,
		ETerrainBiome::Hills,
		ETerrainBiome::Desert,
		ETerrainBiome::Mountain,
		ETerrainBiome::Swamp,
		ETerrainBiome::Tundra
	};

	TArray<ETerrainBiome> SelectedBiomes;
	SelectedBiomes.Reserve(BiomeCount);
	for (int32 Index = 0; Index < BiomeCount; ++Index)
	{
		const int32 SelectedIndex = HexStream.RandRange(Index, AvailableBiomes.Num() - 1);
		AvailableBiomes.Swap(Index, SelectedIndex);
		SelectedBiomes.Add(AvailableBiomes[Index]);
	}

	// Desert and tundra are deliberately kept apart.  A mixed hex can still
	// contain either climate, but never both at the same time; the available
	// buffer biomes create a more believable transition between them.
	if (SelectedBiomes.Contains(ETerrainBiome::Desert)
		&& SelectedBiomes.Contains(ETerrainBiome::Tundra))
	{
		const TArray<ETerrainBiome> BufferBiomes = {
			ETerrainBiome::Grassland,
			ETerrainBiome::Forest,
			ETerrainBiome::Hills,
			ETerrainBiome::Swamp
		};

		for (const ETerrainBiome BufferBiome : BufferBiomes)
		{
			if (SelectedBiomes.Contains(BufferBiome))
			{
				continue;
			}

			const int32 TundraIndex = SelectedBiomes.Find(ETerrainBiome::Tundra);
			if (TundraIndex != INDEX_NONE)
			{
				SelectedBiomes[TundraIndex] = BufferBiome;
			}
			break;
		}
	}

	return SelectedBiomes;
}

TArray<FIntPoint> UWorldMapSubsystem::ExpandHex(const FIntPoint& CenterCoordinates)
{
	TArray<FIntPoint> AddedCoordinates;

	for (const FIntPoint& Direction : GetNeighborDirections())
	{
		const FIntPoint NeighborCoordinates = CenterCoordinates + Direction;
		if (Hexes.Contains(NeighborCoordinates))
		{
			continue;
		}

		FWorldHexRecord Record;
		Record.Coordinates = NeighborCoordinates;
		Record.State = EHexState::Available;
		Hexes.Add(NeighborCoordinates, Record);
		AddedCoordinates.Add(NeighborCoordinates);
	}

	return AddedCoordinates;
}

bool UWorldMapSubsystem::HasHex(const FIntPoint& Coordinates) const
{
	return Hexes.Contains(Coordinates);
}

EHexState UWorldMapSubsystem::GetHexState(const FIntPoint& Coordinates) const
{
	if (const FWorldHexRecord* Record = Hexes.Find(Coordinates))
	{
		return Record->State;
	}

	return EHexState::Available;
}

void UWorldMapSubsystem::RegisterExistingTerrain(const FIntPoint& Coordinates, AProceduralTerrainActor* Terrain)
{
	if (!Terrain)
	{
		return;
	}

	Terrain->SetWorldHexCoordinates(Coordinates);

	FWorldHexRecord& Record = Hexes.FindOrAdd(Coordinates);
	Record.Coordinates = Coordinates;
	Record.State = EHexState::Generated;
	Record.Biome = Terrain->Biome;
	Record.Biomes = Terrain->GetActiveBiomes();
	if (Record.Biomes.IsEmpty())
	{
		Record.Biomes.Add(Record.Biome);
	}
	Record.Biome = Record.Biomes[0];
	Record.TerrainActor = Terrain;
	UpdateWorldBoundary();
}

FVector2D UWorldMapSubsystem::HexToWorldOffset(const FIntPoint& Coordinates, float TerrainSize, int32 /*TerrainResolution*/)
{
	// Flat-top hex geometry in Unreal units. Do not snap these values to the
	// procedural grid: the old FloorToFloat version accumulated a positional
	// error in every row/column and opened visible gaps between hexes.
	const float HexRadius = TerrainSize * 0.5f;
	const float HorizontalOffset = HexRadius * 1.5f;
	const float DiagonalYOffset = HexRadius * FMath::Sqrt(3.0f) * 0.5f;
	const float VerticalOffset = HexRadius * FMath::Sqrt(3.0f);

	return FVector2D(
		HorizontalOffset * static_cast<float>(Coordinates.X),
		VerticalOffset * static_cast<float>(Coordinates.Y) + DiagonalYOffset * static_cast<float>(Coordinates.X)
	);
}

FVector UWorldMapSubsystem::GetHexWorldLocation(
	const FIntPoint& Coordinates,
	TSubclassOf<AProceduralTerrainActor> /*TerrainClass*/,
	const AProceduralTerrainActor* SourceTerrain
) const
{
	// Do not inspect TerrainClass's CDO here.  A stale Blueprint/class value
	// after Live Coding can resolve to Default__Object; asking for its typed CDO
	// then produces the fatal "Cast of Object ... to Actor failed" before the
	// actual spawn.  The existing terrain already contains the real dimensions,
	// and the constants are a safe fallback for a world with no source terrain.
	const float TerrainSize = SourceTerrain
		? SourceTerrain->Size
		: 100000.0f;
	const int32 TerrainResolution = SourceTerrain
		? SourceTerrain->Resolution
		: 100;

	const FVector2D Offset = HexToWorldOffset(Coordinates, TerrainSize, TerrainResolution);
	const FVector Origin = SourceTerrain ? SourceTerrain->GetActorLocation() : FVector::ZeroVector;
	return Origin + FVector(Offset.X, Offset.Y, 0.0f);
}

AProceduralTerrainActor* UWorldMapSubsystem::FindTerrainSettingsSource() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AProceduralTerrainActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("TerrainSettingsSource")))
		{
			return *It;
		}
	}

	return nullptr;
}

void UWorldMapSubsystem::StoreGeneratedTerrain(const FIntPoint& Coordinates, AProceduralTerrainActor* Terrain)
{
	if (!Terrain)
	{
		return;
	}

	FWorldHexRecord& Record = Hexes.FindOrAdd(Coordinates);
	Record.Coordinates = Coordinates;
	Record.State = EHexState::Generated;
	Record.Biome = Terrain->Biome;
	Record.Biomes = Terrain->GetActiveBiomes();
	if (Record.Biomes.IsEmpty())
	{
		Record.Biomes.Add(Record.Biome);
	}
	Record.Biome = Record.Biomes[0];
	Record.TerrainActor = Terrain;
	UpdateWorldBoundary();
}

bool UWorldMapSubsystem::GenerateHex(
	const FIntPoint& Coordinates,
	TSubclassOf<AProceduralTerrainActor> TerrainClass,
	AProceduralTerrainActor*& OutTerrain
)
{
	OutTerrain = nullptr;

	UWorld* World = GetWorld();
	UClass* ResolvedTerrainClass = TerrainClass.Get();
	if (!World
		|| !ResolvedTerrainClass
		|| !ResolvedTerrainClass->IsChildOf(AProceduralTerrainActor::StaticClass()))
	{
		UE_LOG(LogTemp, Error,
			TEXT("WorldMapSubsystem: invalid terrain class '%s'; hex generation skipped."),
			*GetNameSafe(ResolvedTerrainClass));
		return false;
	}

	FWorldHexRecord* Record = Hexes.Find(Coordinates);
	if (!Record || Record->State == EHexState::Generated)
	{
		return false;
	}

	AProceduralTerrainActor* SourceTerrain = FindTerrainSettingsSource();
	const FVector SpawnLocation = GetHexWorldLocation(Coordinates, TerrainClass, SourceTerrain);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
	// Keep this non-templated. The project has previously encountered a stale
	// Blueprint class resolving to CoreUObject.Default__Object; typed spawn
	// overloads can perform an internal CastChecked before our validation.
	AActor* SpawnedActor = World->SpawnActor(
		ResolvedTerrainClass,
		&SpawnTransform,
		SpawnParams
	);
	if (!SpawnedActor)
	{
		return false;
	}

	if (!SpawnedActor->IsA(AProceduralTerrainActor::StaticClass()))
	{
		UE_LOG(LogTemp, Error,
			TEXT("WorldMapSubsystem: class '%s' spawned a non-terrain actor '%s'; hex discarded."),
			*GetNameSafe(ResolvedTerrainClass),
			*GetNameSafe(SpawnedActor));
		SpawnedActor->Destroy();
		return false;
	}

	AProceduralTerrainActor* NewTerrain = static_cast<AProceduralTerrainActor*>(SpawnedActor);

	NewTerrain->SetWorldHexCoordinates(Coordinates);
	const ETerrainBiome RequestedBiome = SourceTerrain
		? SourceTerrain->Biome
		: NewTerrain->Biome;

	if (SourceTerrain)
	{
		NewTerrain->CopySettingsFrom(SourceTerrain);
		if (SourceTerrain->Biome == ETerrainBiome::All)
		{
			NewTerrain->Seed = Settings.WorldSeed;
		}
	}

	const bool bGenerateMixedBiomes = Settings.BiomeGenerationMode == EWorldBiomeGenerationMode::AllMixed
		&& (!SourceTerrain || SourceTerrain->Biome == ETerrainBiome::All);

	if (bGenerateMixedBiomes)
	{
		TArray<ETerrainBiome> GeneratedBiomes = SelectBiomesForHex(Coordinates);

		// Avoid a direct desert/tundra border when a new hex is generated next
		// to an already generated one. The replacement is deterministic for the
		// current generation order and keeps the original biome count whenever a
		// suitable transition biome is available.
		auto ReplaceWithTransitionBiome = [&GeneratedBiomes](ETerrainBiome BiomeToReplace) -> bool
		{
			const int32 ReplaceIndex = GeneratedBiomes.Find(BiomeToReplace);
			if (ReplaceIndex == INDEX_NONE)
			{
				return false;
			}

			const TArray<ETerrainBiome> TransitionBiomes = {
				ETerrainBiome::Grassland,
				ETerrainBiome::Forest,
				ETerrainBiome::Hills,
				ETerrainBiome::Swamp,
				ETerrainBiome::Mountain
			};
			for (const ETerrainBiome TransitionBiome : TransitionBiomes)
			{
				if (!GeneratedBiomes.Contains(TransitionBiome))
				{
					GeneratedBiomes[ReplaceIndex] = TransitionBiome;
					return true;
				}
			}

			return false;
		};

		for (const FIntPoint& Direction : GetNeighborDirections())
		{
			if (const FWorldHexRecord* Neighbor = Hexes.Find(Coordinates + Direction))
			{
				const bool bNeighborHasDesert = Neighbor->Biomes.Contains(ETerrainBiome::Desert);
				const bool bNeighborHasTundra = Neighbor->Biomes.Contains(ETerrainBiome::Tundra);

				if (bNeighborHasTundra)
				{
					ReplaceWithTransitionBiome(ETerrainBiome::Desert);
				}
				if (bNeighborHasDesert)
				{
					ReplaceWithTransitionBiome(ETerrainBiome::Tundra);
				}
			}
		}

		NewTerrain->SetBiomeLayers(GeneratedBiomes);
	}
	else
	{
		NewTerrain->Biome = RequestedBiome;
		NewTerrain->ApplyBiomePreset();
	}

	if (ApplyExistingRiversToTerrain(NewTerrain))
	{
		// Existing river settings were attached before the mesh is generated.
	}
	else if (ShouldSpawnRiverForTerrain(NewTerrain))
	{
		if (!ConfigureRiverForTerrain(NewTerrain))
		{
			bForceNextRiver = true;
		}
	}
	else
	{
		NewTerrain->ClearRiver();
	}

	NewTerrain->GenerateTerrain();
	StoreGeneratedTerrain(Coordinates, NewTerrain);
	OutTerrain = NewTerrain;
	return true;
}

bool UWorldMapSubsystem::ShouldSpawnRiverForTerrain(const AProceduralTerrainActor* Terrain)
{
	if (!Terrain
		|| Terrain->Biome == ETerrainBiome::Desert
		|| Terrain->ActorHasTag(TEXT("TerrainSettingsSource")))
	{
		return false;
	}

	if (bForceNextRiver)
	{
		bForceNextRiver = false;
		GeneratedNonDesertTilesSinceRiver = 0;
		RollNextRiverDistance();
		return true;
	}

	++GeneratedNonDesertTilesSinceRiver;

	if (NextRiverDistance <= 0)
	{
		RollNextRiverDistance();
	}

	if (GeneratedNonDesertTilesSinceRiver >= NextRiverDistance)
	{
		GeneratedNonDesertTilesSinceRiver = 0;
		RollNextRiverDistance();
		return true;
	}

	return false;
}

void UWorldMapSubsystem::RollNextRiverDistance()
{
	const int32 MinDistance = FMath::Max(1, FMath::Min(Settings.RiverDistanceMin, Settings.RiverDistanceMax));
	const int32 MaxDistance = FMath::Max(MinDistance, Settings.RiverDistanceMax);
	NextRiverDistance = WorldRandomStream.RandRange(MinDistance, MaxDistance);
}

bool UWorldMapSubsystem::ApplyExistingRiversToTerrain(AProceduralTerrainActor* Terrain)
{
	if (!Terrain
		|| Terrain->Biome == ETerrainBiome::Desert
		|| Terrain->ActorHasTag(TEXT("TerrainSettingsSource")))
	{
		return false;
	}

	bool bAppliedAnyRiver = false;
	for (const FWorldRiverRecord& River : Rivers)
	{
		if (DoesRiverAffectTerrain(Terrain, River.Path))
		{
			Terrain->AddRiver(River.Settings);
			bAppliedAnyRiver = true;
		}
	}

	return bAppliedAnyRiver;
}

bool UWorldMapSubsystem::ConfigureRiverForTerrain(AProceduralTerrainActor* Terrain)
{
	if (!Terrain)
	{
		return false;
	}

	AProceduralTerrainActor* SettingsSource = FindTerrainSettingsSource();
	TArray<FRiverForbiddenArea> ForbiddenAreas;

	if (SettingsSource && SettingsSource != Terrain)
	{
		FRiverForbiddenArea ForbiddenArea;
		const FVector SourceLocation = SettingsSource->GetActorLocation();
		ForbiddenArea.Center = FVector2D(SourceLocation.X, SourceLocation.Y);
		ForbiddenArea.Radius = SettingsSource->Size * 0.5f;
		ForbiddenArea.Clearance = SettingsSource->Size * 0.08f;
		ForbiddenAreas.Add(ForbiddenArea);
	}

	TArray<FGeneratedTerrainRiverPath> ExistingRiverPaths;
	ExistingRiverPaths.Reserve(Rivers.Num());
	for (const FWorldRiverRecord& River : Rivers)
	{
		ExistingRiverPaths.Add(River.Path);
	}

	FGeneratedTerrainRiverPath GeneratedRiverPath;
	const float WorldSizeScale = FMath::Max(Terrain->Size / 10000.0f, 0.1f);
	if (!Terrain->ConfigureRiver(
		Settings.RiverWidthMin * WorldSizeScale,
		Settings.RiverWidthMax * WorldSizeScale,
		Settings.RiverDepthMin * WorldSizeScale,
		Settings.RiverDepthMax * WorldSizeScale,
		Settings.RiverClearance * WorldSizeScale,
		ExistingRiverPaths,
		ForbiddenAreas,
		GeneratedRiverPath
	))
	{
		Terrain->ClearRiver();
		return false;
	}

	if (SettingsSource && SettingsSource != Terrain && DoesRiverAffectTerrain(SettingsSource, GeneratedRiverPath))
	{
		Terrain->ClearRiver();
		return false;
	}

	FWorldRiverRecord RiverRecord;
	RiverRecord.Path = GeneratedRiverPath;
	RiverRecord.Settings = Terrain->RiverSettings;
	Rivers.Add(RiverRecord);
	ApplyRiverToAlreadyGeneratedTerrains(Terrain, RiverRecord);
	return true;
}

void UWorldMapSubsystem::ApplyRiverToAlreadyGeneratedTerrains(
	AProceduralTerrainActor* SourceTerrain,
	const FWorldRiverRecord& River
)
{
	if (!SourceTerrain)
	{
		return;
	}

	TArray<FAffectedTerrainUpdate> AffectedTerrains;
	const FVector SourceLocation = SourceTerrain->GetActorLocation();

	for (const TPair<FIntPoint, FWorldHexRecord>& Pair : Hexes)
	{
		AProceduralTerrainActor* Terrain = Pair.Value.TerrainActor;
		if (!Terrain
			|| Terrain == SourceTerrain
			|| Terrain->Biome == ETerrainBiome::Desert
			|| Terrain->ActorHasTag(TEXT("TerrainSettingsSource")))
		{
			continue;
		}

		if (DoesRiverAffectTerrain(Terrain, River.Path))
		{
			Terrain->AddRiver(River.Settings);
			FAffectedTerrainUpdate Update;
			Update.Terrain = Terrain;
			Update.DistanceSq = FVector::DistSquared2D(SourceLocation, Terrain->GetActorLocation());
			AffectedTerrains.Add(Update);
		}
	}

	AffectedTerrains.Sort([](const FAffectedTerrainUpdate& A, const FAffectedTerrainUpdate& B)
	{
		return A.DistanceSq < B.DistanceSq;
	});

	constexpr int32 MaxImmediateRegenerations = 8;
	const int32 RegenerationCount = FMath::Min(MaxImmediateRegenerations, AffectedTerrains.Num());

	for (int32 Index = 0; Index < RegenerationCount; ++Index)
	{
		if (AffectedTerrains[Index].Terrain)
		{
			AffectedTerrains[Index].Terrain->GenerateTerrain();
		}
	}
}

bool UWorldMapSubsystem::IsPointInsideExpandedHex(const FVector2D& Point, float Radius)
{
	const float X = FMath::Abs(Point.X);
	const float Y = FMath::Abs(Point.Y);
	const float HexDistance = FMath::Max(
		X / Radius,
		(0.5f * X + 0.8660254f * Y) / Radius
	);
	return HexDistance <= 1.0f;
}

float UWorldMapSubsystem::DistanceSquaredToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
{
	const FVector2D Segment = B - A;
	const float SegmentLengthSq = Segment.SizeSquared();
	if (SegmentLengthSq <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::DistSquared(Point, A);
	}

	const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, Segment) / SegmentLengthSq, 0.0f, 1.0f);
	return FVector2D::DistSquared(Point, A + Segment * T);
}

bool UWorldMapSubsystem::DoesRiverAffectTerrain(
	const AProceduralTerrainActor* Terrain,
	const FGeneratedTerrainRiverPath& RiverPath
) const
{
	if (!Terrain || RiverPath.WorldPoints.Num() < 2)
	{
		return false;
	}

	const FVector TerrainLocation = Terrain->GetActorLocation();
	const FVector2D TerrainCenter(TerrainLocation.X, TerrainLocation.Y);
	const float RiverReach = FMath::Max(RiverPath.HalfWidth, 100.0f) + Terrain->Size * 0.08f;
	const float ExpandedHexRadius = Terrain->Size * 0.5f + RiverReach;

	for (const FVector2D& Point : RiverPath.WorldPoints)
	{
		if (IsPointInsideExpandedHex(Point - TerrainCenter, ExpandedHexRadius))
		{
			return true;
		}
	}

	const float SegmentSampleStep = FMath::Max(Terrain->Size * 0.08f, 250.0f);
	for (int32 Index = 0; Index < RiverPath.WorldPoints.Num() - 1; ++Index)
	{
		const FVector2D& A = RiverPath.WorldPoints[Index];
		const FVector2D& B = RiverPath.WorldPoints[Index + 1];
		const float SegmentLength = FVector2D::Distance(A, B);
		const int32 StepCount = FMath::Max(1, FMath::CeilToInt(SegmentLength / SegmentSampleStep));

		for (int32 StepIndex = 1; StepIndex < StepCount; ++StepIndex)
		{
			const float T = static_cast<float>(StepIndex) / static_cast<float>(StepCount);
			if (IsPointInsideExpandedHex(FMath::Lerp(A, B, T) - TerrainCenter, ExpandedHexRadius))
			{
				return true;
			}
		}
	}

	const float CenterTouchRadius = Terrain->Size * 0.64f + RiverReach;
	const float CenterTouchRadiusSq = CenterTouchRadius * CenterTouchRadius;
	for (int32 Index = 0; Index < RiverPath.WorldPoints.Num() - 1; ++Index)
	{
		if (DistanceSquaredToSegment(
			TerrainCenter,
			RiverPath.WorldPoints[Index],
			RiverPath.WorldPoints[Index + 1]
		) <= CenterTouchRadiusSq)
		{
			return true;
		}
	}

	return false;
}

void UWorldMapSubsystem::Deinitialize()
{
	if (WorldBoundaryActor)
	{
		WorldBoundaryActor->Destroy();
		WorldBoundaryActor = nullptr;
	}

	ResetWorld();
	Super::Deinitialize();
}

void UWorldMapSubsystem::UpdateWorldBoundary()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AProceduralTerrainActor* SettingsSource = FindTerrainSettingsSource();
	if (!SettingsSource)
	{
		for (const TPair<FIntPoint, FWorldHexRecord>& Pair : Hexes)
		{
			if (Pair.Value.State == EHexState::Generated && Pair.Value.TerrainActor)
			{
				SettingsSource = Pair.Value.TerrainActor;
				break;
			}
		}
	}

	if (!SettingsSource)
	{
		if (WorldBoundaryActor)
		{
			WorldBoundaryActor->ClearBoundary();
		}
		return;
	}

	TSet<FIntPoint> GeneratedHexes;
	for (const TPair<FIntPoint, FWorldHexRecord>& Pair : Hexes)
	{
		if (Pair.Value.State == EHexState::Generated && Pair.Value.TerrainActor)
		{
			GeneratedHexes.Add(Pair.Key);
		}
	}

	if (!WorldBoundaryActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = TEXT("WorldBoundary");
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform BoundaryTransform(SettingsSource->GetActorRotation(), SettingsSource->GetActorLocation());
		WorldBoundaryActor = World->SpawnActor<AWorldBoundaryActor>(
			AWorldBoundaryActor::StaticClass(),
			BoundaryTransform,
			SpawnParams
		);
	}

	if (WorldBoundaryActor)
	{
		WorldBoundaryActor->RebuildBoundary(
			GeneratedHexes,
			SettingsSource->Size,
			SettingsSource->GetActorLocation(),
			Settings.BoundaryInset
		);
	}
}
