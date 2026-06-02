#include "TerrainMountainTypes.h"
#include "Math/RandomStream.h"

float UTerrainMountainLibrary::SmoothStep01(float Value)
{
	const float T = FMath::Clamp(Value, 0.0f, 1.0f);
	return T * T * (3.0f - 2.0f * T);
}

float UTerrainMountainLibrary::GetTerrainHorizontalOffset(int32 Resolution, float TerrainSize)
{
	const int32 SafeResolution = FMath::Max(Resolution, 2);
	const float TerrainStep = TerrainSize / SafeResolution;

	return FMath::FloorToFloat(0.75f * SafeResolution) * TerrainStep;
}

float UTerrainMountainLibrary::GetTerrainDiagonalYOffset(int32 Resolution, float TerrainSize)
{
	const int32 SafeResolution = FMath::Max(Resolution, 2);
	const float TerrainStep = TerrainSize / SafeResolution;

	return FMath::FloorToFloat((FMath::Sqrt(3.0f) * 0.25f) * SafeResolution) * TerrainStep;
}

float UTerrainMountainLibrary::GetTerrainVerticalOffset(int32 Resolution, float TerrainSize)
{
	return GetTerrainDiagonalYOffset(Resolution, TerrainSize) * 2.0f;
}

FIntPoint UTerrainMountainLibrary::GetNearestTerrainTileCoords(const FVector& ActorLocation, int32 Resolution, float TerrainSize)
{
	const float HorizontalOffset = GetTerrainHorizontalOffset(Resolution, TerrainSize);
	const float DiagonalYOffset = GetTerrainDiagonalYOffset(Resolution, TerrainSize);
	const float VerticalOffset = GetTerrainVerticalOffset(Resolution, TerrainSize);

	if (FMath::IsNearlyZero(HorizontalOffset) || FMath::IsNearlyZero(VerticalOffset))
	{
		return FIntPoint::ZeroValue;
	}

	const float Q = ActorLocation.X / HorizontalOffset;
	const float R = (ActorLocation.Y - DiagonalYOffset * Q) / VerticalOffset;

	return FIntPoint(FMath::RoundToInt(Q), FMath::RoundToInt(R));
}

FVector2D UTerrainMountainLibrary::GetTerrainTileCenter(const FIntPoint& TileCoords, int32 Resolution, float TerrainSize)
{
	const float HorizontalOffset = GetTerrainHorizontalOffset(Resolution, TerrainSize);
	const float DiagonalYOffset = GetTerrainDiagonalYOffset(Resolution, TerrainSize);
	const float VerticalOffset = GetTerrainVerticalOffset(Resolution, TerrainSize);

	return FVector2D(
		HorizontalOffset * TileCoords.X,
		VerticalOffset * TileCoords.Y + DiagonalYOffset * TileCoords.X
	);
}

float UTerrainMountainLibrary::SampleTileMountainContribution(
	float WorldX,
	float WorldY,
	const FIntPoint& TileCoords,
	const FVector2D& GridOrigin,
	int32 Seed,
	int32 Resolution,
	float TerrainSize,
	float Ridge,
	const FMountainTerrainSettings& Settings
)
{
	const int32 MinCount = FMath::Max(0, FMath::Min(Settings.CountMin, Settings.CountMax));
	const int32 MaxCount = FMath::Max(MinCount, Settings.CountMax);

	if (MaxCount <= 0)
	{
		return 0.0f;
	}

	const int32 TileSeed = Seed
		^ (TileCoords.X * 73856093)
		^ (TileCoords.Y * 19349663)
		^ 0x45d9f3b;

	FRandomStream MountainStream(TileSeed);
	const int32 MountainCount = MountainStream.RandRange(MinCount, MaxCount);
	const float HalfSize = TerrainSize * 0.5f;
	const float MinRadiusMultiplier = FMath::Max(0.05f, FMath::Min(Settings.RadiusMinMultiplier, Settings.RadiusMaxMultiplier));
	const float MaxRadiusMultiplier = FMath::Max(MinRadiusMultiplier, Settings.RadiusMaxMultiplier);
	const float PlacementRange = FMath::Max(0.0f, Settings.PlacementRange);
	const float HeightVariation = FMath::Max(0.0f, Settings.HeightVariation);
	const FVector2D TileCenter = GridOrigin + GetTerrainTileCenter(TileCoords, Resolution, TerrainSize);
	const FVector2D SamplePoint(WorldX, WorldY);
	float CombinedMask = 0.0f;

	for (int32 Index = 0; Index < MountainCount; ++Index)
	{
		FVector2D RandomOffset = FVector2D::ZeroVector;

		if (!(Index == 0 && MountainStream.FRand() < 0.28f))
		{
			const float Angle = MountainStream.FRandRange(0.0f, 2.0f * UE_PI);
			const float Distance = FMath::Sqrt(MountainStream.FRand()) * PlacementRange;
			RandomOffset = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
		}

		const FVector2D Center = TileCenter + (Settings.Center + RandomOffset) * HalfSize;
		const FVector2D JaggedNoiseOffset(
			MountainStream.FRandRange(-1000.0f, 1000.0f),
			MountainStream.FRandRange(-1000.0f, 1000.0f)
		);
		const float Radius = FMath::Max(1.0f, Settings.Radius * HalfSize * MountainStream.FRandRange(MinRadiusMultiplier, MaxRadiusMultiplier));
		const float FeatureStrength = FMath::Max(0.05f, 1.0f + MountainStream.FRandRange(-HeightVariation, HeightVariation));
		const float FeatureSharpness = FMath::Max(0.1f, Settings.Sharpness * MountainStream.FRandRange(0.85f, 1.15f));
		const float FeatureJaggedNoiseScale = FMath::Max(0.01f, Settings.JaggedNoiseScale * MountainStream.FRandRange(0.75f, 1.3f));
		const float FeatureJaggedStrength = FMath::Max(0.0f, Settings.JaggedStrength * MountainStream.FRandRange(0.75f, 1.25f));

		const float Distance = FVector2D::Distance(SamplePoint, Center);
		float Mask = 1.0f - SmoothStep01(Distance / Radius);
		Mask = FMath::Pow(FMath::Clamp(Mask, 0.0f, 1.0f), FeatureSharpness);

		if (FeatureJaggedStrength > 0.0f && Mask > 0.0f)
		{
			const FVector2D MountainSpace = (SamplePoint - Center) / Radius;
			const FVector2D NoisePosition = MountainSpace * FeatureJaggedNoiseScale + JaggedNoiseOffset;

			const float CoarseNoise = 1.0f - FMath::Abs(FMath::PerlinNoise2D(NoisePosition));
			const float FineNoise = 1.0f - FMath::Abs(FMath::PerlinNoise2D(NoisePosition * 2.35f + FVector2D(19.17f, -41.83f)));
			const float JaggedNoise = (CoarseNoise * 0.7f + FineNoise * 0.3f) - 0.45f;

			Mask = FMath::Clamp(Mask + JaggedNoise * FeatureJaggedStrength * SmoothStep01(Mask), 0.0f, 1.25f);
		}

		CombinedMask = FMath::Max(CombinedMask, Mask * FeatureStrength);
	}

	return CombinedMask;
}

float UTerrainMountainLibrary::SampleMountainContribution(
	float WorldX,
	float WorldY,
	const FVector& ActorLocation,
	int32 Seed,
	int32 Resolution,
	float TerrainSize,
	float Ridge,
	const FMountainTerrainSettings& Settings
)
{
	if (!Settings.bEnabled || Settings.Strength <= 0.0f)
	{
		return 0.0f;
	}

	const int32 MinCount = FMath::Max(0, FMath::Min(Settings.CountMin, Settings.CountMax));
	const int32 MaxCount = FMath::Max(MinCount, Settings.CountMax);
	const float MountainNoise = 0.85f + Ridge * 0.35f;

	if (MaxCount <= 0)
	{
		const FVector2D Center = FVector2D(ActorLocation.X, ActorLocation.Y) + Settings.Center * (TerrainSize * 0.5f);
		const float Radius = FMath::Max(1.0f, Settings.Radius * TerrainSize * 0.5f);
		const float Distance = FVector2D::Distance(FVector2D(WorldX, WorldY), Center);
		float Mask = 1.0f - SmoothStep01(Distance / Radius);
		Mask = FMath::Pow(FMath::Clamp(Mask, 0.0f, 1.0f), FMath::Max(0.1f, Settings.Sharpness));

		return Mask * Settings.Strength * MountainNoise;
	}

	const FIntPoint CenterTileCoords = GetNearestTerrainTileCoords(ActorLocation, Resolution, TerrainSize);
	const FVector2D GridOrigin = FVector2D(ActorLocation.X, ActorLocation.Y) - GetTerrainTileCenter(CenterTileCoords, Resolution, TerrainSize);
	const FIntPoint NeighborDirections[] = {
		FIntPoint(0, 0),
		FIntPoint(1, 0),
		FIntPoint(1, -1),
		FIntPoint(0, -1),
		FIntPoint(-1, 0),
		FIntPoint(-1, 1),
		FIntPoint(0, 1)
	};

	float CombinedMask = 0.0f;
	for (const FIntPoint& Direction : NeighborDirections)
	{
		const FIntPoint TileCoords(CenterTileCoords.X + Direction.X, CenterTileCoords.Y + Direction.Y);
		CombinedMask = FMath::Max(
			CombinedMask,
			SampleTileMountainContribution(WorldX, WorldY, TileCoords, GridOrigin, Seed, Resolution, TerrainSize, Ridge, Settings)
		);
	}

	return FMath::Clamp(CombinedMask, 0.0f, 1.5f) * Settings.Strength * MountainNoise;
}
