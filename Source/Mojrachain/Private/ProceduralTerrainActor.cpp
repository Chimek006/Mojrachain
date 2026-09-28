#include "ProceduralTerrainActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UnrealType.h"

namespace
{
bool AreRiverSettingsEquivalent(const FRiverTerrainSettings& A, const FRiverTerrainSettings& B)
{
	if (A.WorldPoints.Num() != B.WorldPoints.Num())
	{
		return false;
	}

	if (A.WorldPoints.IsEmpty())
	{
		return true;
	}

	return FVector2D::DistSquared(A.WorldPoints[0], B.WorldPoints[0]) <= 1.0f
		&& FVector2D::DistSquared(A.WorldPoints.Last(), B.WorldPoints.Last()) <= 1.0f
		&& FMath::IsNearlyEqual(A.Width, B.Width, 0.1f)
		&& FMath::IsNearlyEqual(A.Depth, B.Depth, 0.1f);
}
}

AProceduralTerrainActor::AProceduralTerrainActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	SetRootComponent(ProcMesh);

	ProcMesh->bUseAsyncCooking = true;
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AProceduralTerrainActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureDefaultObjectSpawnRules();
	if (ActiveBiomes.IsEmpty())
	{
		if (Biome == ETerrainBiome::All)
		{
			ActiveBiomes = {
				ETerrainBiome::Grassland,
				ETerrainBiome::Forest,
				ETerrainBiome::Hills,
				ETerrainBiome::Desert,
				ETerrainBiome::Mountain,
				ETerrainBiome::Swamp,
				ETerrainBiome::Tundra
			};
		}
		else
		{
			ActiveBiomes.Add(Biome);
		}
	}
	// Runtime MIDs are transient and are not restored when an editor session
	// reloads a saved procedural mesh. Reapply the terrain material whenever
	// the actor is reconstructed so the mesh does not fall back to gray.
	ApplyBiomeMaterial();

	// A procedural mesh is not created just by placing the actor in a level.
	// Construction runs when the level is opened, when the actor is moved, and
	// when its editable properties change, so this is the correct place to
	// restore the visible terrain in both the editor and a spawned game world.
	// GenerateHex() still calls GenerateTerrain() explicitly after copying the
	// source settings and selecting its biome layers.
	GenerateTerrain();
}

float AProceduralTerrainActor::GetRealNoiseScale() const
{
	return NoiseScale * NoiseScaleMultiplier * GetWorldNoiseScale();
}

float AProceduralTerrainActor::GetRealDetailNoiseScale() const
{
	return DetailNoiseScale * NoiseScaleMultiplier * GetWorldNoiseScale();
}

float AProceduralTerrainActor::GetRealRidgeNoiseScale() const
{
	return RidgeNoiseScale * NoiseScaleMultiplier * GetWorldNoiseScale();
}

float AProceduralTerrainActor::GetWorldSizeScale() const
{
	return FMath::Max(Size / ReferenceTerrainSize, 0.1f);
}

float AProceduralTerrainActor::GetWorldNoiseScale() const
{
	// Noise properties are authored for the original 100 m tile. When the
	// tile is enlarged, reduce the world-space frequency so the landscape gets
	// larger geographic features instead of ten times more repetitions.
	return ReferenceTerrainSize / FMath::Max(Size, 100.0f);
}

float AProceduralTerrainActor::GetBiomeTerrainTransitionWidth(
	ETerrainBiome FirstBiome,
	ETerrainBiome SecondBiome
) const
{
	if (FirstBiome == SecondBiome)
	{
		return TextureBiomeBlendWidth;
	}

	const FBiomeTerrainSettings FirstSettings =
		UTerrainBiomeLibrary::GetTerrainBiomeSettings(FirstBiome);
	const FBiomeTerrainSettings SecondSettings =
		UTerrainBiomeLibrary::GetTerrainBiomeSettings(SecondBiome);

	// The material still blends over 1 m. Geometry gets a wider transition only
	// when the two biome height profiles actually need it, so a mountain does
	// not fall into a lowland through a one-metre near-vertical wall.
	const float WorldSizeScale = GetWorldSizeScale();
	const float HeightDifference = (
		FMath::Abs(FirstSettings.BaseHeight - SecondSettings.BaseHeight)
		+ 0.5f * FMath::Abs(FirstSettings.HeightScale - SecondSettings.HeightScale)
	) * WorldSizeScale;

	const float MinimumTransitionWidth = FMath::Max(
		TextureBiomeBlendWidth * 2.0f,
		Size * 0.02f
	);
	const float MaximumTransitionWidth = FMath::Max(
		MinimumTransitionWidth,
		Size * 0.20f
	);
	constexpr float MaximumRecommendedSlope = 0.65f;

	return FMath::Clamp(
		FMath::Max(MinimumTransitionWidth, HeightDifference / MaximumRecommendedSlope),
		MinimumTransitionWidth,
		MaximumTransitionWidth
	);
}

bool AProceduralTerrainActor::HasActiveRivers() const
{
	return RuntimeRiverSettings.Num() > 0 || RiverSettings.bEnabled;
}

FRiverTerrainSample AProceduralTerrainActor::SampleRiverCorridor(float WorldX, float WorldY) const
{
	FRiverTerrainSample BestSample;

	auto MergeSample = [&](const FRiverTerrainSettings& Settings)
		{
			if (!Settings.bEnabled)
			{
				return;
			}

			const FRiverTerrainSample Sample = UTerrainRiverLibrary::SampleRiver(
				WorldX,
				WorldY,
				Seed,
				Settings
			);

			if (Sample.CorridorMask > BestSample.CorridorMask)
			{
				BestSample = Sample;
			}
		};

	if (RuntimeRiverSettings.Num() > 0)
	{
		for (const FRiverTerrainSettings& Settings : RuntimeRiverSettings)
		{
			MergeSample(Settings);
		}
	}
	else
	{
		MergeSample(RiverSettings);
	}

	return BestSample;
}

float AProceduralTerrainActor::SmoothStep01(float Value) const
{
	const float T = FMath::Clamp(Value, 0.0f, 1.0f);
	return T * T * (3.0f - 2.0f * T);
}

float AProceduralTerrainActor::SampleFBM(float X, float Y, float OffsetX, float OffsetY, float Scale, int32 InOctaves) const
{
	float Amplitude = 1.0f;
	float Frequency = 1.0f;
	float Total = 0.0f;
	float Normalizer = 0.0f;

	for (int32 i = 0; i < InOctaves; ++i)
	{
		const float NX = (X + OffsetX) * Scale * Frequency;
		const float NY = (Y + OffsetY) * Scale * Frequency;

		const float N = FMath::PerlinNoise2D(FVector2D(NX, NY));

		Total += N * Amplitude;
		Normalizer += Amplitude;

		Amplitude *= Persistence;
		Frequency *= Lacunarity;
	}

	return Total / FMath::Max(Normalizer, 0.0001f);
}

float AProceduralTerrainActor::SampleRidgedNoise(float X, float Y, float OffsetX, float OffsetY) const
{
	const float N = SampleFBM(
		X,
		Y,
		OffsetX + 9142.0f,
		OffsetY - 2711.0f,
		GetRealRidgeNoiseScale(),
		Octaves
	);

	float R = 1.0f - FMath::Abs(N);
	R = FMath::Clamp(R, 0.0f, 1.0f);

	return R * R;
}

float AProceduralTerrainActor::GetHexEdgeMask(const FVector2D& P, float Radius) const
{
	if (EdgeFalloff <= 0.0f)
	{
		return 1.0f;
	}

	const float X = FMath::Abs(P.X);
	const float Y = FMath::Abs(P.Y);

	const float HexDistance = FMath::Max(
		X / Radius,
		(0.5f * X + 0.8660254f * Y) / Radius
	);

	const float Inner = 1.0f - EdgeFalloff;
	const float T = (HexDistance - Inner) / FMath::Max(EdgeFalloff, 0.0001f);

	return 1.0f - SmoothStep01(T);
}

float AProceduralTerrainActor::SampleHeight(float WorldX, float WorldY, float LocalX, float LocalY, float OffsetX, float OffsetY) const
{
	const float EdgeMask = GetHexEdgeMask(FVector2D(LocalX, LocalY), Size * 0.5f);
	float Base01 = 0.0f;
	float Height = 0.0f;
	const float WorldSizeScale = GetWorldSizeScale();
	const float EffectiveHeightScale = (Biome == ETerrainBiome::All
		? FMath::Max(HeightScale, 750.0f)
		: HeightScale) * WorldSizeScale;

	if (Biome == ETerrainBiome::All)
	{
		TArray<float> GlobalWeights;
		CalculateGlobalBiomeWeights(WorldX, WorldY, GlobalWeights, true);

		// All biome profiles share one low-frequency macro shape. Previously
		// every biome sampled a different base FBM here, so a hills/desert
		// boundary could become "flat -> noisy -> flat" even when both profiles
		// were meant to meet gently. One shared macro field also removes six
		// expensive FBM evaluations per vertex.
		const float SharedBaseNoise = SampleFBM(
			WorldX,
			WorldY,
			OffsetX,
			OffsetY,
			FMath::Max(0.18f * NoiseScaleMultiplier * GetWorldNoiseScale(), 0.00001f),
			3
		);
		const float SharedBase01 = FMath::Pow(
			FMath::Clamp((SharedBaseNoise + 1.0f) * 0.5f, 0.0f, 1.0f),
			1.15f
		);

		float LargestWeight = 0.0f;
		float SecondLargestWeight = 0.0f;
		for (const float Weight : GlobalWeights)
		{
			if (Weight > LargestWeight)
			{
				SecondLargestWeight = LargestWeight;
				LargestWeight = Weight;
			}
			else if (Weight > SecondLargestWeight)
			{
				SecondLargestWeight = Weight;
			}
		}
		const float TransitionBlend = FMath::Clamp(
			4.0f * LargestWeight * SecondLargestWeight,
			0.0f,
			1.0f
		);
		const float TransitionDamping = TransitionBlend * 0.72f;

		for (int32 BiomeIndex = 0; BiomeIndex < GlobalWeights.Num(); ++BiomeIndex)
		{
			const float BiomeWeight = GlobalWeights[BiomeIndex];
			if (BiomeWeight <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const ETerrainBiome ProfileBiome = static_cast<ETerrainBiome>(BiomeIndex);
			const FBiomeTerrainSettings Profile =
				UTerrainBiomeLibrary::GetTerrainBiomeSettings(ProfileBiome);

			const float ProfileBase01 = FMath::Pow(SharedBase01, Profile.HeightPower);

			float ProfileDetail = SampleFBM(
				WorldX,
				WorldY,
				OffsetX - 3812.0f,
				OffsetY + 7281.0f,
				FMath::Max(Profile.DetailNoiseScale * NoiseScaleMultiplier * GetWorldNoiseScale(), 0.00001f),
				2
			) * Profile.DetailStrength;
			ProfileDetail *= 1.0f - 0.55f * TransitionDamping;

			const float ProfileRidgeNoise = SampleFBM(
				WorldX,
				WorldY,
				OffsetX + 9142.0f,
				OffsetY - 2711.0f,
				FMath::Max(Profile.RidgeNoiseScale * NoiseScaleMultiplier * GetWorldNoiseScale(), 0.00001f),
				Profile.Octaves
			);
			float ProfileRidge = FMath::Square(
				FMath::Clamp(1.0f - FMath::Abs(ProfileRidgeNoise), 0.0f, 1.0f)
			) * Profile.RidgeStrength;
			ProfileRidge *= 1.0f - 0.72f * TransitionDamping;

			const FVector ProfileActorLocation(
				WorldX - LocalX,
				WorldY - LocalY,
				GetActorLocation().Z
			);
			float ProfileMountain = UTerrainMountainLibrary::SampleMountainContribution(
				WorldX,
				WorldY,
				ProfileActorLocation,
				Seed,
				Resolution,
				Size,
				ProfileRidge,
				Profile.MountainSettings
			);
			ProfileMountain *= 1.0f - 0.80f * TransitionDamping;

			const float ProfileFinal01 = FMath::Clamp(
				ProfileBase01 + ProfileDetail + ProfileRidge + ProfileMountain,
				-0.35f,
				2.0f
			);
			const float ProfileHeight = Profile.BaseHeight * WorldSizeScale
				+ ProfileFinal01 * Profile.HeightScale * WorldSizeScale * EdgeMask;

			Base01 += ProfileBase01 * BiomeWeight;
			Height += ProfileHeight * BiomeWeight;
		}

		if (GlobalWeights.IsEmpty())
		{
			Height = BaseHeight * WorldSizeScale + 0.5f * EffectiveHeightScale * EdgeMask;
		}
	}
	else
	{
		const float BaseNoise = SampleFBM(
			WorldX,
			WorldY,
			OffsetX,
			OffsetY,
			GetRealNoiseScale(),
			Octaves
		);

		Base01 = FMath::Pow(FMath::Clamp((BaseNoise + 1.0f) * 0.5f, 0.0f, 1.0f), HeightPower);
		const float Detail = SampleFBM(
			WorldX,
			WorldY,
			OffsetX - 3812.0f,
			OffsetY + 7281.0f,
			GetRealDetailNoiseScale(),
			2
		) * DetailStrength;
		const float Ridge = SampleRidgedNoise(WorldX, WorldY, OffsetX, OffsetY) * RidgeStrength;
		const FVector ActorLocation(WorldX - LocalX, WorldY - LocalY, GetActorLocation().Z);
		const float Mountain = UTerrainMountainLibrary::SampleMountainContribution(
			WorldX,
			WorldY,
			ActorLocation,
			Seed,
			Resolution,
			Size,
			Ridge,
			MountainSettings
		);

		const float Final01 = FMath::Clamp(Base01 + Detail + Ridge + Mountain, -0.35f, 2.0f);
		Height = BaseHeight * WorldSizeScale + Final01 * EffectiveHeightScale * EdgeMask;
	}

	auto ApplyRiverToHeight = [&](const FRiverTerrainSettings& Settings)
		{
			if (!Settings.bEnabled)
			{
				return;
			}

			const FRiverTerrainSample River = UTerrainRiverLibrary::SampleRiver(
				WorldX,
				WorldY,
				Seed,
				Settings
			);

			if (River.CorridorMask <= 0.0f)
			{
				return;
			}

			const float RiverBase01 = FMath::Clamp(Base01 * 0.32f + 0.30f + River.BedNoise * 0.18f, -0.2f, 1.0f);
			const float RiverBaseHeight = BaseHeight * WorldSizeScale
				+ RiverBase01 * EffectiveHeightScale * EdgeMask;
			const float CorridorStrength = FMath::Clamp(
				River.CorridorMask * 0.36f + River.BankMask * 0.34f,
				0.0f,
				0.82f
			);

			Height = FMath::Lerp(Height, RiverBaseHeight, CorridorStrength);

			if (River.Mask > 0.0f)
			{
				const float RiverBedHeight = RiverBaseHeight - River.Depth * WorldSizeScale;
				Height = FMath::Lerp(Height, RiverBedHeight, River.Mask);
			}
		};

	if (RuntimeRiverSettings.Num() > 0)
	{
		for (const FRiverTerrainSettings& Settings : RuntimeRiverSettings)
		{
			ApplyRiverToHeight(Settings);
		}
	}
	else
	{
		ApplyRiverToHeight(RiverSettings);
	}

	return Height;
}

float AProceduralTerrainActor::GetTerrainHeightAtWorldLocation(float WorldX, float WorldY) const
{
	FRandomStream Stream(Seed);
	const float OffsetX = Stream.FRandRange(-10000.0f, 10000.0f);
	const float OffsetY = Stream.FRandRange(-10000.0f, 10000.0f);
	const FVector ActorLocation = GetActorLocation();

	return ActorLocation.Z + SampleHeight(
		WorldX,
		WorldY,
		WorldX - ActorLocation.X,
		WorldY - ActorLocation.Y,
		OffsetX,
		OffsetY
	);
}

bool AProceduralTerrainActor::IsPointInsideHex(const FVector2D& P, float Radius) const
{
	float q = (2.0f / 3.0f * P.X) / Radius;
	float r = (-1.0f / 3.0f * P.X + FMath::Sqrt(3.0f) / 3.0f * P.Y) / Radius;
	float x = q;
	float z = r;
	float y = -x - z;

	float rx = FMath::RoundToFloat(x);
	float ry = FMath::RoundToFloat(y);
	float rz = FMath::RoundToFloat(z);

	float x_diff = FMath::Abs(rx - x);
	float y_diff = FMath::Abs(ry - y);
	float z_diff = FMath::Abs(rz - z);

	if (x_diff > y_diff && x_diff > z_diff) rx = -ry - rz;
	else if (y_diff > z_diff) ry = -rx - rz;
	else rz = -rx - ry;

	return (rx == 0 && ry == 0 && rz == 0);
}

TArray<ETerrainBiome> AProceduralTerrainActor::GetNormalizedActiveBiomes() const
{
	TArray<ETerrainBiome> Result;
	for (const ETerrainBiome ActiveBiome : ActiveBiomes)
	{
		if (!Result.Contains(ActiveBiome))
		{
			Result.Add(ActiveBiome);
		}
	}

	if (Result.IsEmpty())
	{
		Result.Add(Biome);
	}

	return Result;
}

void AProceduralTerrainActor::SetBiomeLayers(const TArray<ETerrainBiome>& InBiomes)
{
	ActiveBiomes.Reset();

	for (const ETerrainBiome InBiome : InBiomes)
	{
		if (!ActiveBiomes.Contains(InBiome))
		{
			ActiveBiomes.Add(InBiome);
		}
	}

	if (ActiveBiomes.IsEmpty())
	{
		ActiveBiomes.Add(Biome);
	}

	if (Biome != ETerrainBiome::All)
	{
		Biome = ActiveBiomes[0];
	}
	ApplyBiomeMaterial();
}

void AProceduralTerrainActor::SetWorldHexCoordinates(const FIntPoint& InCoordinates)
{
	WorldHexCoordinates = InCoordinates;
}

void AProceduralTerrainActor::CalculateGlobalBiomeWeights(
	float WorldX,
	float WorldY,
	TArray<float>& OutWeights,
	bool bForTerrainHeight
) const
{
	constexpr int32 BiomeCount = 7;
	OutWeights.Init(0.0f, BiomeCount);

	const float Radius = FMath::Max(Size * 0.5f, 1.0f);
	const float HorizontalOffset = Radius * 1.5f;
	const float DiagonalYOffset = Radius * FMath::Sqrt(3.0f) * 0.5f;
	const float VerticalOffset = Radius * FMath::Sqrt(3.0f);
	const FVector ActorLocation = GetActorLocation();
	const FVector2D MapOrigin(
		ActorLocation.X - HorizontalOffset * static_cast<float>(WorldHexCoordinates.X),
		ActorLocation.Y - (
			VerticalOffset * static_cast<float>(WorldHexCoordinates.Y)
			+ DiagonalYOffset * static_cast<float>(WorldHexCoordinates.X)
		)
	);

	const bool bBiomeCacheChanged =
		CachedGlobalBiomeSeed != Seed
		|| !FMath::IsNearlyEqual(CachedGlobalBiomeSize, Size)
		|| CachedGlobalBiomeCoordinates != WorldHexCoordinates
		|| !FMath::IsNearlyEqual(CachedGlobalBiomeActorXY.X, ActorLocation.X)
		|| !FMath::IsNearlyEqual(CachedGlobalBiomeActorXY.Y, ActorLocation.Y);
	if (bBiomeCacheChanged)
	{
		GlobalBiomeSiteCache.Reset();
		CachedGlobalBiomeSeed = Seed;
		CachedGlobalBiomeSize = Size;
		CachedGlobalBiomeCoordinates = WorldHexCoordinates;
		CachedGlobalBiomeActorXY = FVector2D(ActorLocation.X, ActorLocation.Y);
	}

	const float WarpScale = 0.00035f;
	const float WarpStrength = Radius * 0.16f;
	const float WarpX = FMath::PerlinNoise2D(FVector2D(
		(WorldX + Seed * 13.0f) * WarpScale,
		(WorldY - Seed * 7.0f) * WarpScale
	)) * WarpStrength;
	const float WarpY = FMath::PerlinNoise2D(FVector2D(
		(WorldX - Seed * 5.0f + 271.0f) * WarpScale,
		(WorldY + Seed * 11.0f - 193.0f) * WarpScale
	)) * WarpStrength;
	const FVector2D SamplePoint(WorldX + WarpX, WorldY + WarpY);

	// Resolve the query cell from the world position rather than from the
	// current actor. This makes the procedural biome field identical on both
	// sides of a shared hex edge, so a mountain cannot be cut at the boundary
	// merely because a neighbouring actor used a different local site window.
	const FVector2D RelativeSamplePoint = SamplePoint - MapOrigin;
	const float FractionalQ = (2.0f / 3.0f * RelativeSamplePoint.X) / Radius;
	const float FractionalR = (
		-1.0f / 3.0f * RelativeSamplePoint.X
		+ FMath::Sqrt(3.0f) / 3.0f * RelativeSamplePoint.Y
	) / Radius;
	float CubeX = FractionalQ;
	float CubeZ = FractionalR;
	float CubeY = -CubeX - CubeZ;
	float RoundedX = FMath::RoundToFloat(CubeX);
	float RoundedY = FMath::RoundToFloat(CubeY);
	float RoundedZ = FMath::RoundToFloat(CubeZ);
	const float XDifference = FMath::Abs(RoundedX - CubeX);
	const float YDifference = FMath::Abs(RoundedY - CubeY);
	const float ZDifference = FMath::Abs(RoundedZ - CubeZ);
	if (XDifference > YDifference && XDifference > ZDifference)
	{
		RoundedX = -RoundedY - RoundedZ;
	}
	else if (YDifference > ZDifference)
	{
		RoundedY = -RoundedX - RoundedZ;
	}
	else
	{
		RoundedZ = -RoundedX - RoundedY;
	}
	const FIntPoint QueryCoordinates(
		FMath::RoundToInt(RoundedX),
		FMath::RoundToInt(RoundedZ)
	);

	const TArray<FGlobalBiomeSite>* CachedSites = GlobalBiomeSiteCache.Find(QueryCoordinates);
	if (!CachedSites)
	{
		TArray<FGlobalBiomeSite> GeneratedSites;
		GeneratedSites.Reserve(64);

		for (int32 OffsetX = -2; OffsetX <= 2; ++OffsetX)
		{
			for (int32 OffsetY = -2; OffsetY <= 2; ++OffsetY)
			{
				if (FMath::Abs(OffsetX + OffsetY) > 2)
				{
					continue;
				}

				const FIntPoint Coordinates = QueryCoordinates + FIntPoint(OffsetX, OffsetY);
				const int32 HexSeed = static_cast<int32>(
					HashCombine(GetTypeHash(Seed), GetTypeHash(Coordinates))
				);
				FRandomStream HexStream(HexSeed);

				const int32 Roll = HexStream.RandRange(0, 99);
				const int32 BiomeCountForHex = Roll < 50
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
				SelectedBiomes.Reserve(BiomeCountForHex);
				for (int32 Index = 0; Index < BiomeCountForHex; ++Index)
				{
					const int32 SelectedIndex = HexStream.RandRange(Index, AvailableBiomes.Num() - 1);
					AvailableBiomes.Swap(Index, SelectedIndex);
					SelectedBiomes.Add(AvailableBiomes[Index]);
				}

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

				const FVector2D HexCenter(
					MapOrigin.X
					+ HorizontalOffset * static_cast<float>(Coordinates.X),
					MapOrigin.Y
					+ VerticalOffset * static_cast<float>(Coordinates.Y)
					+ DiagonalYOffset * static_cast<float>(Coordinates.X)
				);
				FRandomStream SiteStream(HexSeed ^ 0x6E624EB7);

				for (const ETerrainBiome SelectedBiome : SelectedBiomes)
				{
					const float Angle = SiteStream.FRandRange(0.0f, 2.0f * PI);
					const float SiteRadius = SiteStream.FRandRange(Radius * 0.12f, Radius * 0.48f);
					GeneratedSites.Add({
						HexCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * SiteRadius,
						SelectedBiome
					});
				}
			}
		}

		GlobalBiomeSiteCache.Add(QueryCoordinates, MoveTemp(GeneratedSites));
		CachedSites = GlobalBiomeSiteCache.Find(QueryCoordinates);
	}

	const TArray<FGlobalBiomeSite>& Sites = *CachedSites;

	int32 NearestIndex = INDEX_NONE;
	int32 SecondNearestIndex = INDEX_NONE;
	float NearestDistanceSq = TNumericLimits<float>::Max();
	float SecondNearestDistanceSq = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Sites.Num(); ++Index)
	{
		const float DistanceSq = FVector2D::DistSquared(SamplePoint, Sites[Index].Position);
		if (DistanceSq < NearestDistanceSq)
		{
			SecondNearestDistanceSq = NearestDistanceSq;
			SecondNearestIndex = NearestIndex;
			NearestDistanceSq = DistanceSq;
			NearestIndex = Index;
		}
		else if (DistanceSq < SecondNearestDistanceSq)
		{
			SecondNearestDistanceSq = DistanceSq;
			SecondNearestIndex = Index;
		}
	}

	if (NearestIndex == INDEX_NONE)
	{
		OutWeights[static_cast<int32>(ETerrainBiome::Grassland)] = 1.0f;
		return;
	}

	const float NearestDistance = FMath::Sqrt(NearestDistanceSq);
	const float SecondNearestDistance = FMath::Sqrt(SecondNearestDistanceSq);
	const float Separation = FMath::Max(0.0f, SecondNearestDistance - NearestDistance);
	const float BlendWidth = bForTerrainHeight
		? GetBiomeTerrainTransitionWidth(
			Sites[NearestIndex].Biome,
			SecondNearestIndex != INDEX_NONE
				? Sites[SecondNearestIndex].Biome
				: Sites[NearestIndex].Biome
		)
		: TextureBiomeBlendWidth;
	const float NearestWeight = 0.5f
		+ 0.5f * SmoothStep01(Separation / FMath::Max(BlendWidth, 1.0f));

	OutWeights[static_cast<int32>(Sites[NearestIndex].Biome)] += NearestWeight;
	if (SecondNearestIndex != INDEX_NONE)
	{
		OutWeights[static_cast<int32>(Sites[SecondNearestIndex].Biome)] += 1.0f - NearestWeight;
	}

	float WeightSum = 0.0f;
	for (const float Weight : OutWeights)
	{
		WeightSum += Weight;
	}
	if (WeightSum > KINDA_SMALL_NUMBER)
	{
		for (float& Weight : OutWeights)
		{
			Weight /= WeightSum;
		}
	}
}

void AProceduralTerrainActor::CalculateBiomeWeights(
	float LocalX,
	float LocalY,
	TArray<float>& OutWeights,
	bool bForTerrainHeight
) const
{
	const TArray<ETerrainBiome> BiomeLayers = GetNormalizedActiveBiomes();
	OutWeights.Init(0.0f, BiomeLayers.Num());

	if (BiomeLayers.Num() == 0)
	{
		return;
	}

	if (BiomeLayers.Num() == 1)
	{
		OutWeights[0] = 1.0f;
		return;
	}

	const float HalfSize = FMath::Max(Size * 0.5f, 1.0f);
	FRandomStream SiteStream(Seed ^ (BiomeLayers.Num() * 92821) ^ 0x6E624EB7);

	// Warp the sample position with a low-frequency field. This turns the
	// biome borders into broad organic shapes instead of radial sectors.
	const float WarpScale = 0.00035f;
	const float WarpStrength = HalfSize * 0.18f;
	const float WarpX = FMath::PerlinNoise2D(FVector2D(
		(LocalX + Seed * 13.0f) * WarpScale,
		(LocalY - Seed * 7.0f) * WarpScale
	)) * WarpStrength;
	const float WarpY = FMath::PerlinNoise2D(FVector2D(
		(LocalX - Seed * 5.0f + 271.0f) * WarpScale,
		(LocalY + Seed * 11.0f - 193.0f) * WarpScale
	)) * WarpStrength;
	const FVector2D WarpedPoint(LocalX + WarpX, LocalY + WarpY);

	// Use a warped Voronoi field instead of broad Gaussian fields. Only the
	// nearest two biome sites are blended. Material masks use a tight 1 m
	// transition; terrain height can request an adaptive width separately.
	TArray<FVector2D> Sites;
	Sites.SetNum(BiomeLayers.Num());

	for (int32 Index = 0; Index < BiomeLayers.Num(); ++Index)
	{
		const float Angle = SiteStream.FRandRange(0.0f, 2.0f * PI);
		const float SiteRadius = SiteStream.FRandRange(HalfSize * 0.12f, HalfSize * 0.48f);
		Sites[Index] = FVector2D(
			FMath::Cos(Angle) * SiteRadius,
			FMath::Sin(Angle) * SiteRadius
		);
	}

	int32 NearestIndex = INDEX_NONE;
	int32 SecondNearestIndex = INDEX_NONE;
	float NearestDistanceSq = TNumericLimits<float>::Max();
	float SecondNearestDistanceSq = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Sites.Num(); ++Index)
	{
		const float DistanceSq = FVector2D::DistSquared(WarpedPoint, Sites[Index]);
		if (DistanceSq < NearestDistanceSq)
		{
			SecondNearestDistanceSq = NearestDistanceSq;
			SecondNearestIndex = NearestIndex;
			NearestDistanceSq = DistanceSq;
			NearestIndex = Index;
		}
		else if (DistanceSq < SecondNearestDistanceSq)
		{
			SecondNearestDistanceSq = DistanceSq;
			SecondNearestIndex = Index;
		}
	}

	if (NearestIndex == INDEX_NONE)
	{
		OutWeights[0] = 1.0f;
		return;
	}

	OutWeights[NearestIndex] = 1.0f;
	if (SecondNearestIndex != INDEX_NONE)
	{
		const float NearestDistance = FMath::Sqrt(NearestDistanceSq);
		const float SecondNearestDistance = FMath::Sqrt(SecondNearestDistanceSq);
		const float Separation = FMath::Max(0.0f, SecondNearestDistance - NearestDistance);
		const float BlendWidth = bForTerrainHeight
			? GetBiomeTerrainTransitionWidth(
				BiomeLayers[NearestIndex],
				BiomeLayers[SecondNearestIndex]
			)
			: TextureBiomeBlendWidth;
		const float NearestWeight = 0.5f
			+ 0.5f * SmoothStep01(Separation / FMath::Max(BlendWidth, 1.0f));
		OutWeights[NearestIndex] = NearestWeight;
		OutWeights[SecondNearestIndex] = 1.0f - NearestWeight;
	}
}

ETerrainBiome AProceduralTerrainActor::GetBiomeAtLocalLocation(float LocalX, float LocalY) const
{
	if (Biome == ETerrainBiome::All)
	{
		TArray<float> GlobalWeights;
		const FVector ActorLocation = GetActorLocation();
		CalculateGlobalBiomeWeights(
			ActorLocation.X + LocalX,
			ActorLocation.Y + LocalY,
			GlobalWeights
		);

		int32 BestGlobalIndex = 0;
		for (int32 Index = 1; Index < GlobalWeights.Num(); ++Index)
		{
			if (GlobalWeights[Index] > GlobalWeights[BestGlobalIndex])
			{
				BestGlobalIndex = Index;
			}
		}
		return static_cast<ETerrainBiome>(BestGlobalIndex);
	}

	const TArray<ETerrainBiome> BiomeLayers = GetNormalizedActiveBiomes();
	if (BiomeLayers.Num() <= 1)
	{
		return BiomeLayers[0];
	}

	TArray<float> Weights;
	CalculateBiomeWeights(LocalX, LocalY, Weights);

	int32 BestIndex = 0;
	for (int32 Index = 1; Index < Weights.Num(); ++Index)
	{
		if (Weights[Index] > Weights[BestIndex])
		{
			BestIndex = Index;
		}
	}

	return BiomeLayers[BestIndex];
}

void AProceduralTerrainActor::CalculateFixedBiomeWeights(
	float LocalX,
	float LocalY,
	TArray<float>& OutWeights
) const
{
	constexpr int32 BiomeCount = 7;
	OutWeights.Init(0.0f, BiomeCount);

	if (Biome == ETerrainBiome::All)
	{
		const FVector ActorLocation = GetActorLocation();
		CalculateGlobalBiomeWeights(
			ActorLocation.X + LocalX,
			ActorLocation.Y + LocalY,
			OutWeights
		);
		return;
	}

	const TArray<ETerrainBiome> BiomeLayers = GetNormalizedActiveBiomes();
	TArray<float> LayerWeights;
	CalculateBiomeWeights(LocalX, LocalY, LayerWeights);

	for (int32 LayerIndex = 0; LayerIndex < BiomeLayers.Num(); ++LayerIndex)
	{
		const int32 FixedIndex = static_cast<int32>(BiomeLayers[LayerIndex]);
		if (FixedIndex >= 0
			&& FixedIndex < BiomeCount
			&& LayerWeights.IsValidIndex(LayerIndex))
		{
			OutWeights[FixedIndex] = LayerWeights[LayerIndex];
		}
	}
}

FLinearColor AProceduralTerrainActor::GetBiomeTintAtLocalLocation(float LocalX, float LocalY) const
{
	if (Biome == ETerrainBiome::All)
	{
		TArray<float> GlobalWeights;
		const FVector ActorLocation = GetActorLocation();
		CalculateGlobalBiomeWeights(
			ActorLocation.X + LocalX,
			ActorLocation.Y + LocalY,
			GlobalWeights
		);

		FLinearColor WeightedColor = FLinearColor::Black;
		for (int32 Index = 0; Index < GlobalWeights.Num(); ++Index)
		{
			WeightedColor += UTerrainBiomeLibrary::GetBiomeSurfaceSettings(
				static_cast<ETerrainBiome>(Index)
			).BaseColor * GlobalWeights[Index];
		}
		return WeightedColor;
	}

	const TArray<ETerrainBiome> BiomeLayers = GetNormalizedActiveBiomes();
	if (BiomeLayers.Num() <= 1)
	{
		return UTerrainBiomeLibrary::GetBiomeSurfaceSettings(BiomeLayers[0]).BaseColor;
	}

	TArray<float> Weights;
	CalculateBiomeWeights(LocalX, LocalY, Weights);

	FLinearColor WeightedColor = FLinearColor::Black;
	for (int32 Index = 0; Index < BiomeLayers.Num(); ++Index)
	{
		const FBiomeSurfaceSettings SurfaceSettings =
			UTerrainBiomeLibrary::GetBiomeSurfaceSettings(BiomeLayers[Index]);
		WeightedColor += SurfaceSettings.BaseColor * Weights[Index];
	}

	return WeightedColor;
}

ETerrainBiome AProceduralTerrainActor::GetBiomeAtWorldLocation(float WorldX, float WorldY) const
{
	const FVector ActorLocation = GetActorLocation();
	return GetBiomeAtLocalLocation(WorldX - ActorLocation.X, WorldY - ActorLocation.Y);
}

void AProceduralTerrainActor::EnsureDefaultObjectSpawnRules()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return;
	}

	if (ObjectSpawnRules.IsEmpty())
	{
		ObjectSpawnRules.Add(FProceduralTerrainObjectRule());
	}

	// When the FBX was imported with Unreal's default naming, the generated
	// Static Mesh is normally exposed as /Game/Materials/grass.grass. Keep a
	// few common variants here so the first rule can be populated automatically;
	// the mesh can always be selected manually in the Details panel.
	if (ObjectSpawnRules.Num() > 0 && !ObjectSpawnRules[0].Mesh)
	{
		static const TCHAR* MeshCandidates[] =
		{
			TEXT("/Game/Materials/grass-blade.grass-blade"),
			TEXT("/Game/Materials/grass_blade.grass_blade"),
			TEXT("/Game/Materials/grass.grass"),
			TEXT("/Game/Materials/SM_grass_blade.SM_grass_blade"),
			TEXT("/Game/Materials/Grass.Grass"),
			TEXT("/Game/Materials/SM_grass.SM_grass")
		};

		for (const TCHAR* Candidate : MeshCandidates)
		{
			if (UStaticMesh* ImportedMesh = LoadObject<UStaticMesh>(nullptr, Candidate))
			{
				ObjectSpawnRules[0].Mesh = ImportedMesh;
				break;
			}
		}
	}
}

void AProceduralTerrainActor::ClearGeneratedTerrainObjects()
{
	for (UInstancedStaticMeshComponent* InstanceComponent : RuntimeObjectInstanceComponents)
	{
		if (IsValid(InstanceComponent))
		{
			InstanceComponent->DestroyComponent();
		}
	}

	RuntimeObjectInstanceComponents.Reset();
}

void AProceduralTerrainActor::GenerateTerrainObjects(
	const TArray<float>& Heights,
	const TArray<FVector>& Normals,
	int32 VertCount,
	float Step,
	float MinHeight,
	float MaxHeight
)
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)
		|| IsTemplate())
	{
		return;
	}

	ClearGeneratedTerrainObjects();
	EnsureDefaultObjectSpawnRules();

	if (VertCount < 2 || Step <= KINDA_SMALL_NUMBER || Heights.Num() < VertCount * VertCount)
	{
		return;
	}

	const float HeightRange = FMath::Max(MaxHeight - MinHeight, KINDA_SMALL_NUMBER);
	const float HalfSize = Size * 0.5f;
	const float HexRadius = HalfSize + 2.0f;

	auto GridHeightAt = [&Heights, VertCount](int32 X, int32 Y) -> float
	{
		X = FMath::Clamp(X, 0, VertCount - 1);
		Y = FMath::Clamp(Y, 0, VertCount - 1);
		return Heights[Y * VertCount + X];
	};

	auto GridNormalAt = [&Normals, VertCount](int32 X, int32 Y) -> FVector
	{
		X = FMath::Clamp(X, 0, VertCount - 1);
		Y = FMath::Clamp(Y, 0, VertCount - 1);
		return Normals.IsValidIndex(Y * VertCount + X)
			? Normals[Y * VertCount + X]
			: FVector::UpVector;
	};

	for (int32 RuleIndex = 0; RuleIndex < ObjectSpawnRules.Num(); ++RuleIndex)
	{
		const FProceduralTerrainObjectRule& Rule = ObjectSpawnRules[RuleIndex];
		if (!Rule.Mesh || Rule.AttemptsPerHex <= 0)
		{
			continue;
		}

		bool bHasSpawnableBiome = false;
		for (const FBiomeObjectSpawnChance& BiomeChance : Rule.BiomeChances)
		{
			if (BiomeChance.ChancePercent > 0.0f)
			{
				bHasSpawnableBiome = true;
				break;
			}
		}

		if (!bHasSpawnableBiome)
		{
			continue;
		}

		const FString ComponentName = FString::Printf(
			TEXT("TerrainObjectHISM_%d"),
			RuleIndex
		);
		UInstancedStaticMeshComponent* InstanceComponent =
			NewObject<UInstancedStaticMeshComponent>(this, FName(*ComponentName));
		if (!InstanceComponent)
		{
			continue;
		}

		InstanceComponent->SetStaticMesh(Rule.Mesh);
		InstanceComponent->SetMobility(EComponentMobility::Movable);
		InstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InstanceComponent->SetGenerateOverlapEvents(false);
		InstanceComponent->SetupAttachment(ProcMesh);
		InstanceComponent->RegisterComponent();
		RuntimeObjectInstanceComponents.Add(InstanceComponent);

		const FVector ActorLocation = GetActorLocation();
		const int32 LocationSeed =
			FMath::RoundToInt(ActorLocation.X * 0.01f)
			+ FMath::RoundToInt(ActorLocation.Y * 0.013f)
			+ FMath::RoundToInt(ActorLocation.Z * 0.017f);
		FRandomStream ObjectStream(Seed + RuleIndex * 7919 + LocationSeed);

		for (int32 Attempt = 0; Attempt < Rule.AttemptsPerHex; ++Attempt)
		{
			const float LocalX = ObjectStream.FRandRange(-HalfSize, HalfSize);
			const float LocalY = ObjectStream.FRandRange(-HalfSize, HalfSize);
			if (!IsPointInsideHex(FVector2D(LocalX, LocalY), HexRadius))
			{
				continue;
			}

			const float GridX = FMath::Clamp((LocalX + HalfSize) / Step, 0.0f, static_cast<float>(VertCount - 1));
			const float GridY = FMath::Clamp((LocalY + HalfSize) / Step, 0.0f, static_cast<float>(VertCount - 1));
			const int32 X0 = FMath::FloorToInt(GridX);
			const int32 Y0 = FMath::FloorToInt(GridY);
			const int32 X1 = FMath::Min(X0 + 1, VertCount - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, VertCount - 1);
			const float TX = GridX - static_cast<float>(X0);
			const float TY = GridY - static_cast<float>(Y0);

			const float BottomHeight = FMath::Lerp(
				GridHeightAt(X0, Y0),
				GridHeightAt(X1, Y0),
				TX
			);
			const float TopHeight = FMath::Lerp(
				GridHeightAt(X0, Y1),
				GridHeightAt(X1, Y1),
				TX
			);
			const float Height = FMath::Lerp(BottomHeight, TopHeight, TY);

			const FVector BottomNormal = FMath::Lerp(
				GridNormalAt(X0, Y0),
				GridNormalAt(X1, Y0),
				TX
			);
			const FVector TopNormal = FMath::Lerp(
				GridNormalAt(X0, Y1),
				GridNormalAt(X1, Y1),
				TX
			);
			const FVector SurfaceNormal = FMath::Lerp(BottomNormal, TopNormal, TY).GetSafeNormal();
			const float Altitude01 = FMath::Clamp((Height - MinHeight) / HeightRange, 0.0f, 1.0f);
			const ETerrainBiome PointBiome = GetBiomeAtLocalLocation(LocalX, LocalY);

			float SpawnChance = 0.0f;
			for (const FBiomeObjectSpawnChance& BiomeChance : Rule.BiomeChances)
			{
				if (BiomeChance.Biome == PointBiome)
				{
					SpawnChance = FMath::Clamp(BiomeChance.ChancePercent, 0.0f, 100.0f);
					break;
				}
			}

			if (SpawnChance <= 0.0f
				|| ObjectStream.FRandRange(0.0f, 100.0f) > SpawnChance)
			{
				continue;
			}

			const FBiomeSurfaceSettings SurfaceSettings =
				UTerrainBiomeLibrary::GetBiomeSurfaceSettings(PointBiome);
			const float GrassUpperLimit = FMath::Min(SurfaceSettings.RockStart, SurfaceSettings.SnowStart);

			// Grass belongs to the lowland part of each biome. This prevents it
			// from covering exposed rock and the snow cap on mountain terrain.
			if (Altitude01 > GrassUpperLimit)
			{
				continue;
			}

			const float RandomYaw = ObjectStream.FRandRange(0.0f, 360.0f);
			FRotator Rotation = FRotator(0.0f, RandomYaw, 0.0f);
			if (Rule.bAlignToTerrain)
			{
				Rotation = FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator();
				Rotation.Yaw += RandomYaw;
			}

			const float SafeMinScale = FMath::Max(Rule.MinScale, 0.01f);
			const float SafeMaxScale = FMath::Max(Rule.MaxScale, SafeMinScale);
			const float Scale = FMath::Lerp(
				SafeMinScale,
				SafeMaxScale,
				ObjectStream.FRand()
			);

			InstanceComponent->AddInstance(FTransform(
				Rotation,
				FVector(LocalX, LocalY, Height),
				FVector(Scale)
			));
		}
	}
}

void AProceduralTerrainActor::CopySettingsFrom(const AProceduralTerrainActor* OtherTerrain)
{
	if (!OtherTerrain)
	{
		return;
	}

	Seed = OtherTerrain->Seed;
	Resolution = OtherTerrain->Resolution;
	Size = OtherTerrain->Size;
	Biome = OtherTerrain->Biome;
	ActiveBiomes = OtherTerrain->ActiveBiomes;

	BaseHeight = OtherTerrain->BaseHeight;
	HeightScale = OtherTerrain->HeightScale;

	NoiseScale = OtherTerrain->NoiseScale;
	Octaves = OtherTerrain->Octaves;
	Persistence = OtherTerrain->Persistence;
	Lacunarity = OtherTerrain->Lacunarity;

	HeightPower = OtherTerrain->HeightPower;

	DetailStrength = OtherTerrain->DetailStrength;
	DetailNoiseScale = OtherTerrain->DetailNoiseScale;

	RidgeStrength = OtherTerrain->RidgeStrength;
	RidgeNoiseScale = OtherTerrain->RidgeNoiseScale;

	MountainSettings = OtherTerrain->MountainSettings;
	RiverSettings = OtherTerrain->RiverSettings;
	RuntimeRiverSettings.Reset();

	SmoothingIterations = OtherTerrain->SmoothingIterations;
	SmoothingStrength = OtherTerrain->SmoothingStrength;

	EdgeFalloff = OtherTerrain->EdgeFalloff;
	ObjectSpawnRules = OtherTerrain->ObjectSpawnRules;
}

void AProceduralTerrainActor::ApplyBiomePreset()
{
	if (Biome == ETerrainBiome::All)
	{
		ActiveBiomes = {
			ETerrainBiome::Grassland,
			ETerrainBiome::Forest,
			ETerrainBiome::Hills,
			ETerrainBiome::Desert,
			ETerrainBiome::Mountain,
			ETerrainBiome::Swamp,
			ETerrainBiome::Tundra
		};

		// A mixed world uses one shared height field. These defaults prevent it
		// from inheriting a nearly flat Grassland preset from the source actor;
		// biome textures are still selected independently by the material.
		BaseHeight = 0.0f;
		HeightScale = 900.0f;
		NoiseScale = 0.16f;
		Octaves = 4;
		Persistence = 0.52f;
		Lacunarity = 2.0f;
		HeightPower = 1.25f;
		DetailStrength = 0.08f;
		DetailNoiseScale = 1.8f;
		RidgeStrength = 0.16f;
		RidgeNoiseScale = 0.9f;
		MountainSettings = FMountainTerrainSettings();
		SmoothingIterations = 2;
		SmoothingStrength = 0.32f;
		EdgeFalloff = 0.0f;
		ApplyBiomeMaterial();
		return;
	}

	ActiveBiomes = { Biome };
	const FBiomeTerrainSettings Settings = UTerrainBiomeLibrary::GetTerrainBiomeSettings(Biome);

	BaseHeight = Settings.BaseHeight;
	HeightScale = Settings.HeightScale;

	NoiseScale = Settings.NoiseScale;
	Octaves = Settings.Octaves;
	Persistence = Settings.Persistence;
	Lacunarity = Settings.Lacunarity;

	HeightPower = Settings.HeightPower;

	DetailStrength = Settings.DetailStrength;
	DetailNoiseScale = Settings.DetailNoiseScale;

	RidgeStrength = Settings.RidgeStrength;
	RidgeNoiseScale = Settings.RidgeNoiseScale;

	MountainSettings = Settings.MountainSettings;
	RiverSettings = FRiverTerrainSettings();

	SmoothingIterations = Settings.SmoothingIterations;
	SmoothingStrength = Settings.SmoothingStrength;

	EdgeFalloff = Settings.EdgeFalloff;
	ApplyBiomeMaterial();
}

void AProceduralTerrainActor::ApplyBiomeMaterial()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)
		|| !ProcMesh
		|| !IsValid(ProcMesh))
	{
		return;
	}

	ApplyBiomeMaterialsToSections(GetNormalizedActiveBiomes());
}

UMaterialInstanceDynamic* AProceduralTerrainActor::CreateBiomeMaterialInstance(
	UMaterialInterface* MaterialToUse,
	ETerrainBiome InBiome
)
{
	if (!MaterialToUse || !IsValid(MaterialToUse))
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(
		MaterialToUse,
		GetTransientPackage()
	);
	if (!MaterialInstance)
	{
		return nullptr;
	}

	const FBiomeSurfaceSettings SurfaceSettings =
		UTerrainBiomeLibrary::GetBiomeSurfaceSettings(InBiome);

	MaterialInstance->SetVectorParameterValue(TEXT("SurfaceColor"), SurfaceSettings.BaseColor);
	MaterialInstance->SetVectorParameterValue(TEXT("AccentColor"), SurfaceSettings.AccentColor);
	MaterialInstance->SetVectorParameterValue(TEXT("RockColor"), SurfaceSettings.RockColor);
	MaterialInstance->SetVectorParameterValue(TEXT("SnowColor"), SurfaceSettings.SnowColor);
	MaterialInstance->SetVectorParameterValue(
		TEXT("DesertColor"),
		FLinearColor(0.95f, 0.65f, 0.15f, 1.0f)
	);
	MaterialInstance->SetScalarParameterValue(
		TEXT("BiomeIsDesert"),
		(GetNormalizedActiveBiomes().Num() == 1 && InBiome == ETerrainBiome::Desert)
			? 1.0f
			: 0.0f
	);
	MaterialInstance->SetScalarParameterValue(TEXT("PatternScale"), SurfaceSettings.PatternScale);
	MaterialInstance->SetScalarParameterValue(TEXT("SurfaceRoughness"), SurfaceSettings.Roughness);
	MaterialInstance->SetScalarParameterValue(TEXT("RockStart"), SurfaceSettings.RockStart);
	MaterialInstance->SetScalarParameterValue(TEXT("RockEnd"), SurfaceSettings.RockEnd);
	MaterialInstance->SetScalarParameterValue(TEXT("SnowStart"), SurfaceSettings.SnowStart);
	MaterialInstance->SetScalarParameterValue(TEXT("SnowEnd"), SurfaceSettings.SnowEnd);
	MaterialInstance->SetScalarParameterValue(TEXT("SlopeRockStrength"), SurfaceSettings.SlopeRockStrength);

	return MaterialInstance;
}

void AProceduralTerrainActor::ApplyBiomeMaterialsToSections(const TArray<ETerrainBiome>& SectionBiomes)
{
	UMaterialInterface* MaterialToUse = TerrainMaterial;
	if (!MaterialToUse)
	{
		MaterialToUse = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Materials/M_Terrain_Master_Altitude.M_Terrain_Master_Altitude")
		);
	}

	if (!MaterialToUse || !IsValid(MaterialToUse))
	{
		return;
	}

	RuntimeTerrainMaterials.Reset();

	// The mixed terrain is one continuous mesh section.  It must therefore use
	// one material instance; the biome blend is carried by UV1/UV2 and blended
	// in the material.  Creating one material per biome would recreate hard
	// section borders and is what produced the old triangular wedges.
	const ETerrainBiome MaterialBiome = SectionBiomes.IsEmpty()
		? Biome
		: SectionBiomes[0];
	if (UMaterialInstanceDynamic* MaterialInstance = CreateBiomeMaterialInstance(MaterialToUse, MaterialBiome))
	{
		RuntimeTerrainMaterials.Add(MaterialInstance);
	}

	if (RuntimeTerrainMaterials.IsEmpty())
	{
		return;
	}

	RuntimeTerrainMaterial = RuntimeTerrainMaterials[0];
	for (int32 MaterialIndex = 0; MaterialIndex < RuntimeTerrainMaterials.Num(); ++MaterialIndex)
	{
		ProcMesh->SetMaterial(MaterialIndex, RuntimeTerrainMaterials[MaterialIndex]);
	}
}

#if WITH_EDITOR
void AProceduralTerrainActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;
	const FName MemberPropertyName = PropertyChangedEvent.MemberProperty
		? PropertyChangedEvent.MemberProperty->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralTerrainActor, Biome))
	{
		ApplyBiomePreset();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralTerrainActor, TerrainMaterial))
	{
		ApplyBiomeMaterial();
	}
	else if (
		PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralTerrainActor, ObjectSpawnRules)
		|| MemberPropertyName == GET_MEMBER_NAME_CHECKED(AProceduralTerrainActor, ObjectSpawnRules)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FProceduralTerrainObjectRule, Mesh)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FProceduralTerrainObjectRule, BiomeChances)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FProceduralTerrainObjectRule, AttemptsPerHex)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FProceduralTerrainObjectRule, MinScale)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FProceduralTerrainObjectRule, MaxScale)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FProceduralTerrainObjectRule, bAlignToTerrain)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FBiomeObjectSpawnChance, Biome)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FBiomeObjectSpawnChance, ChancePercent)
	)
	{
		GenerateTerrain();
	}
}
#endif

bool AProceduralTerrainActor::ConfigureRiver(
	float WidthMin,
	float WidthMax,
	float DepthMin,
	float DepthMax,
	float ClearanceFromOtherRivers,
	const TArray<FGeneratedTerrainRiverPath>& ExistingRiverPaths,
	const TArray<FRiverForbiddenArea>& ForbiddenAreas,
	FGeneratedTerrainRiverPath& OutRiverPath
)
{
	RiverSettings = FRiverTerrainSettings();
	RuntimeRiverSettings.Reset();

	const bool bConfigured = UTerrainRiverLibrary::BuildRiverForTerrain(
		this,
		WidthMin,
		WidthMax,
		DepthMin,
		DepthMax,
		ClearanceFromOtherRivers,
		ExistingRiverPaths,
		ForbiddenAreas,
		RiverSettings,
		OutRiverPath
	);

	if (bConfigured && RiverSettings.bEnabled)
	{
		RuntimeRiverSettings.Add(RiverSettings);
	}

	return bConfigured;
}

void AProceduralTerrainActor::ClearRiver()
{
	RiverSettings = FRiverTerrainSettings();
	RuntimeRiverSettings.Reset();
}

void AProceduralTerrainActor::AddRiver(const FRiverTerrainSettings& InRiverSettings)
{
	if (!InRiverSettings.bEnabled)
	{
		return;
	}

	if (RuntimeRiverSettings.IsEmpty() && RiverSettings.bEnabled)
	{
		RuntimeRiverSettings.Add(RiverSettings);
	}

	for (const FRiverTerrainSettings& ExistingRiverSettings : RuntimeRiverSettings)
	{
		if (AreRiverSettingsEquivalent(ExistingRiverSettings, InRiverSettings))
		{
			return;
		}
	}

	if (!RiverSettings.bEnabled)
	{
		RiverSettings = InRiverSettings;
	}

	RuntimeRiverSettings.Add(InRiverSettings);
}

void AProceduralTerrainActor::GenerateTerrain()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)
		|| IsTemplate())
	{
		return;
	}

	if (!ProcMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateTerrain failed: ProcMesh is null."));
		return;
	}

	ApplyBiomeMaterial();

	ProcMesh->ClearAllMeshSections();

	Resolution = FMath::Clamp(Resolution, 2, 250);
	Size = FMath::Clamp(Size, 100.0f, 500000.0f);

	Octaves = FMath::Clamp(Octaves, 1, 8);
	Persistence = FMath::Clamp(Persistence, 0.0f, 1.0f);
	Lacunarity = FMath::Max(Lacunarity, 1.0f);

	HeightPower = FMath::Max(HeightPower, 0.1f);

	DetailStrength = FMath::Clamp(DetailStrength, 0.0f, 1.0f);
	RidgeStrength = FMath::Clamp(RidgeStrength, 0.0f, 1.0f);

	SmoothingIterations = FMath::Clamp(SmoothingIterations, 0, 20);
	SmoothingStrength = FMath::Clamp(SmoothingStrength, 0.0f, 1.0f);

	EdgeFalloff = FMath::Clamp(EdgeFalloff, 0.0f, 0.5f);

	const int32 SafeResolution = FMath::Max(Resolution, 2);
	const int32 VertCount = SafeResolution + 1;
	const float Step = Size / SafeResolution;

	const float HalfSize = Size * 0.5f;

	FRandomStream Stream(Seed);
	const float OffsetX = Stream.FRandRange(-10000.0f, 10000.0f);
	const float OffsetY = Stream.FRandRange(-10000.0f, 10000.0f);

	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<float> Heights;
	TArray<FVector4> BiomeWeightsLow;
	TArray<FVector4> BiomeWeightsHigh;

	const FVector ActorLocation = GetActorLocation();

	Heights.SetNum(VertCount * VertCount);
	Normals.SetNum(VertCount * VertCount);
	BiomeWeightsLow.SetNum(VertCount * VertCount);
	BiomeWeightsHigh.SetNum(VertCount * VertCount);

	for (int32 Y = 0; Y < VertCount; ++Y)
	{
		for (int32 X = 0; X < VertCount; ++X)
		{
			const int32 Idx = Y * VertCount + X;

			const float LocalX = X * Step - HalfSize;
			const float LocalY = Y * Step - HalfSize;

			const float WorldX = ActorLocation.X + LocalX;
			const float WorldY = ActorLocation.Y + LocalY;

			const float H = SampleHeight(WorldX, WorldY, LocalX, LocalY, OffsetX, OffsetY);

			Heights[Idx] = H;
		}
	}

	auto HeightAt = [&](const TArray<float>& Source, int32 X, int32 Y) -> float
		{
			X = FMath::Clamp(X, 0, VertCount - 1);
			Y = FMath::Clamp(Y, 0, VertCount - 1);
			return Source[Y * VertCount + X];
		};

	for (int32 Iteration = 0; Iteration < SmoothingIterations; ++Iteration)
	{
		TArray<float> SmoothedHeights = Heights;

		for (int32 Y = 0; Y < VertCount; ++Y)
		{
			for (int32 X = 0; X < VertCount; ++X)
			{
				const int32 Idx = Y * VertCount + X;

				const float Center = HeightAt(Heights, X, Y);
				const float Left = HeightAt(Heights, X - 1, Y);
				const float Right = HeightAt(Heights, X + 1, Y);
				const float Down = HeightAt(Heights, X, Y - 1);
				const float Up = HeightAt(Heights, X, Y + 1);

				const float Average = (Center * 2.0f + Left + Right + Down + Up) / 6.0f;

				SmoothedHeights[Idx] = FMath::Lerp(Center, Average, SmoothingStrength);
			}
		}

		Heights = SmoothedHeights;
	}

	if (HasActiveRivers())
	{
		constexpr int32 RiverSmoothingIterations = 2;

		for (int32 Iteration = 0; Iteration < RiverSmoothingIterations; ++Iteration)
		{
			TArray<float> SmoothedHeights = Heights;

			for (int32 Y = 0; Y < VertCount; ++Y)
			{
				for (int32 X = 0; X < VertCount; ++X)
				{
					const int32 Idx = Y * VertCount + X;
					const float LocalX = X * Step - HalfSize;
					const float LocalY = Y * Step - HalfSize;
					const float WorldX = ActorLocation.X + LocalX;
					const float WorldY = ActorLocation.Y + LocalY;

					const FRiverTerrainSample River = SampleRiverCorridor(
						WorldX,
						WorldY
					);

					if (River.CorridorMask <= 0.0f)
					{
						continue;
					}

					const float Center = HeightAt(Heights, X, Y);
					const float Left = HeightAt(Heights, X - 1, Y);
					const float Right = HeightAt(Heights, X + 1, Y);
					const float Down = HeightAt(Heights, X, Y - 1);
					const float Up = HeightAt(Heights, X, Y + 1);

					const float Average = (Center * 2.0f + Left + Right + Down + Up) / 6.0f;
					const float Strength = FMath::Clamp(
						River.CorridorMask * 0.42f + River.BankMask * 0.28f,
						0.0f,
						0.7f
					);
					SmoothedHeights[Idx] = FMath::Lerp(Center, Average, Strength);
				}
			}

			Heights = SmoothedHeights;
		}

		for (int32 Y = 0; Y < VertCount; ++Y)
		{
			for (int32 X = 0; X < VertCount; ++X)
			{
				const int32 Idx = Y * VertCount + X;
				const float LocalX = X * Step - HalfSize;
				const float LocalY = Y * Step - HalfSize;
				const float WorldX = ActorLocation.X + LocalX;
				const float WorldY = ActorLocation.Y + LocalY;

				auto ApplyRiverProfile = [&](const FRiverTerrainSettings& Settings)
					{
						if (!Settings.bEnabled)
						{
							return;
						}

						const FRiverTerrainSample River = UTerrainRiverLibrary::SampleRiver(
							WorldX,
							WorldY,
							Seed,
							Settings
						);

						if (River.Mask > 0.0f)
						{
							const float CenterPull = FMath::Lerp(River.Mask, 1.0f, River.CenterMask * 0.24f);
							Heights[Idx] -= River.Depth * CenterPull * 0.16f;
						}
					};

				if (RuntimeRiverSettings.Num() > 0)
				{
					for (const FRiverTerrainSettings& Settings : RuntimeRiverSettings)
					{
						ApplyRiverProfile(Settings);
					}
				}
				else
				{
					ApplyRiverProfile(RiverSettings);
				}
			}
		}
	}

	float MinHeight = TNumericLimits<float>::Max();
	float MaxHeight = TNumericLimits<float>::Lowest();
	for (const float Height : Heights)
	{
		MinHeight = FMath::Min(MinHeight, Height);
		MaxHeight = FMath::Max(MaxHeight, Height);
	}

	const float HeightRange = FMath::Max(MaxHeight - MinHeight, KINDA_SMALL_NUMBER);
	const TArray<ETerrainBiome> SectionBiomes = GetNormalizedActiveBiomes();

	for (int32 Y = 0; Y < VertCount; ++Y)
	{
		for (int32 X = 0; X < VertCount; ++X)
		{
			const float LocalX = X * Step - HalfSize;
			const float LocalY = Y * Step - HalfSize;
			const float HL = HeightAt(Heights, X - 1, Y);
			const float HR = HeightAt(Heights, X + 1, Y);
			const float HD = HeightAt(Heights, X, Y - 1);
			const float HU = HeightAt(Heights, X, Y + 1);

			const FVector N = FVector(HL - HR, HD - HU, 2.0f * Step).GetSafeNormal();
			const int32 Idx = Y * VertCount + X;
			Normals[Idx] = N;

			// Fixed biome order makes the material able to select the correct
			// texture even when a hex contains a different subset of biomes:
			// Grassland, Forest, Hills, Desert, Mountain, Swamp, Tundra.
			TArray<float> FixedBiomeWeights;
			CalculateFixedBiomeWeights(LocalX, LocalY, FixedBiomeWeights);
			BiomeWeightsLow[Idx] = FVector4(
				FixedBiomeWeights.IsValidIndex(0) ? FixedBiomeWeights[0] : 0.0f,
				FixedBiomeWeights.IsValidIndex(1) ? FixedBiomeWeights[1] : 0.0f,
				FixedBiomeWeights.IsValidIndex(2) ? FixedBiomeWeights[2] : 0.0f,
				FixedBiomeWeights.IsValidIndex(3) ? FixedBiomeWeights[3] : 0.0f
			);
			BiomeWeightsHigh[Idx] = FVector4(
				FixedBiomeWeights.IsValidIndex(4) ? FixedBiomeWeights[4] : 0.0f,
				FixedBiomeWeights.IsValidIndex(5) ? FixedBiomeWeights[5] : 0.0f,
				FixedBiomeWeights.IsValidIndex(6) ? FixedBiomeWeights[6] : 0.0f,
				0.0f
			);
		}
	}

	// Build six exact triangular sectors around the hex center. The old
	// square-grid clipping created a staircase boundary whose vertices were
	// different on neighboring actors. Exact corner/edge samples are shared by
	// the mathematical hex layout, so adjacent hexes cannot leave diagonal
	// holes between them.
	TArray<FVector> HexVertices;
	TArray<FVector> HexNormals;
	TArray<FVector2D> HexUV0;
	TArray<FVector2D> HexUV1;
	TArray<FVector2D> HexUV2;
	TArray<FVector2D> HexUV3;
	TArray<FLinearColor> HexColors;
	TArray<int32> HexTriangles;

	// Keep the geometry dense enough to represent the 1 m material transition
	// without rebuilding a full square grid for every hex. The 96-segment cap
	// keeps a 1 km hex affordable; at that size the edge spacing is about 5 m.
	const int32 BlendSegments = FMath::CeilToInt(Size / 100.0f);
	const int32 EdgeSegments = FMath::Max(
		8,
		FMath::Min(SafeResolution, FMath::Min(BlendSegments, 96))
	);
	const float MeshNormalStep = FMath::Max(Step * 0.5f, 1.0f);
	const int32 EstimatedSectorVertices = (EdgeSegments + 1) * (EdgeSegments + 2) / 2;
	HexVertices.Reserve(6 * EstimatedSectorVertices);
	HexNormals.Reserve(6 * EstimatedSectorVertices);
	HexUV0.Reserve(6 * EstimatedSectorVertices);
	HexUV1.Reserve(6 * EstimatedSectorVertices);
	HexUV2.Reserve(6 * EstimatedSectorVertices);
	HexUV3.Reserve(6 * EstimatedSectorVertices);
	HexColors.Reserve(6 * EstimatedSectorVertices);
	HexTriangles.Reserve(6 * EdgeSegments * EdgeSegments * 3);

	auto InterpolateHeightAt = [&](const FVector2D& LocalPoint) -> float
	{
		const float GridX = FMath::Clamp(
			(LocalPoint.X + HalfSize) / Step,
			0.0f,
			static_cast<float>(VertCount - 1)
		);
		const float GridY = FMath::Clamp(
			(LocalPoint.Y + HalfSize) / Step,
			0.0f,
			static_cast<float>(VertCount - 1)
		);
		const int32 X0 = FMath::FloorToInt(GridX);
		const int32 Y0 = FMath::FloorToInt(GridY);
		const int32 X1 = FMath::Min(X0 + 1, VertCount - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, VertCount - 1);
		const float TX = GridX - static_cast<float>(X0);
		const float TY = GridY - static_cast<float>(Y0);
		const float Bottom = FMath::Lerp(
			HeightAt(Heights, X0, Y0),
			HeightAt(Heights, X1, Y0),
			TX
		);
		const float Top = FMath::Lerp(
			HeightAt(Heights, X0, Y1),
			HeightAt(Heights, X1, Y1),
			TX
		);
		return FMath::Lerp(Bottom, Top, TY);
	};

    auto InterpolateBiomeWeights = [
        &BiomeWeightsLow,
        &BiomeWeightsHigh,
        this,
        VertCount,
        HalfSize,
		Step
	](const FVector2D& LocalPoint, bool bExactBoundary, FVector4& OutLow, FVector4& OutHigh)
	{
		if (bExactBoundary)
		{
			TArray<float> ExactWeights;
			CalculateFixedBiomeWeights(LocalPoint.X, LocalPoint.Y, ExactWeights);
			OutLow = FVector4(
				ExactWeights.IsValidIndex(0) ? ExactWeights[0] : 0.0f,
				ExactWeights.IsValidIndex(1) ? ExactWeights[1] : 0.0f,
				ExactWeights.IsValidIndex(2) ? ExactWeights[2] : 0.0f,
				ExactWeights.IsValidIndex(3) ? ExactWeights[3] : 0.0f
			);
			OutHigh = FVector4(
				ExactWeights.IsValidIndex(4) ? ExactWeights[4] : 0.0f,
				ExactWeights.IsValidIndex(5) ? ExactWeights[5] : 0.0f,
				ExactWeights.IsValidIndex(6) ? ExactWeights[6] : 0.0f,
				0.0f
			);
			return;
		}

		const float GridX = FMath::Clamp(
			(LocalPoint.X + HalfSize) / Step,
			0.0f,
			static_cast<float>(VertCount - 1)
		);
		const float GridY = FMath::Clamp(
			(LocalPoint.Y + HalfSize) / Step,
			0.0f,
			static_cast<float>(VertCount - 1)
		);
		const int32 X0 = FMath::FloorToInt(GridX);
		const int32 Y0 = FMath::FloorToInt(GridY);
		const int32 X1 = FMath::Min(X0 + 1, VertCount - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, VertCount - 1);
		const float TX = GridX - static_cast<float>(X0);
		const float TY = GridY - static_cast<float>(Y0);
		const FVector4 LowBottom = FMath::Lerp(
			BiomeWeightsLow[Y0 * VertCount + X0],
			BiomeWeightsLow[Y0 * VertCount + X1],
			TX
		);
		const FVector4 LowTop = FMath::Lerp(
			BiomeWeightsLow[Y1 * VertCount + X0],
			BiomeWeightsLow[Y1 * VertCount + X1],
			TX
		);
		const FVector4 HighBottom = FMath::Lerp(
			BiomeWeightsHigh[Y0 * VertCount + X0],
			BiomeWeightsHigh[Y0 * VertCount + X1],
			TX
		);
		const FVector4 HighTop = FMath::Lerp(
			BiomeWeightsHigh[Y1 * VertCount + X0],
			BiomeWeightsHigh[Y1 * VertCount + X1],
			TX
		);
		OutLow = FMath::Lerp(LowBottom, LowTop, TY);
		OutHigh = FMath::Lerp(HighBottom, HighTop, TY);
	};

	auto AddExactHexVertex = [&](const FVector2D& LocalPoint, bool bExactBoundary) -> int32
	{
		const float WorldX = ActorLocation.X + LocalPoint.X;
		const float WorldY = ActorLocation.Y + LocalPoint.Y;
		const float Height = bExactBoundary
			? SampleHeight(WorldX, WorldY, LocalPoint.X, LocalPoint.Y, OffsetX, OffsetY)
			: InterpolateHeightAt(LocalPoint);
		const float HeightLeft = InterpolateHeightAt(
			LocalPoint - FVector2D(MeshNormalStep, 0.0f)
		);
		const float HeightRight = InterpolateHeightAt(
			LocalPoint + FVector2D(MeshNormalStep, 0.0f)
		);
		const float HeightDown = InterpolateHeightAt(
			LocalPoint - FVector2D(0.0f, MeshNormalStep)
		);
		const float HeightUp = InterpolateHeightAt(
			LocalPoint + FVector2D(0.0f, MeshNormalStep)
		);

		const FVector Normal = FVector(
			HeightLeft - HeightRight,
			HeightDown - HeightUp,
			2.0f * MeshNormalStep
		).GetSafeNormal();
		const float Altitude01 = FMath::Clamp(
			(Height - MinHeight) / HeightRange,
			0.0f,
			1.0f
		);
		const float SlopeMask = FMath::Clamp(1.0f - Normal.Z, 0.0f, 1.0f);

		FVector4 FixedBiomeWeightsLow;
		FVector4 FixedBiomeWeightsHigh;
		InterpolateBiomeWeights(
			LocalPoint,
			bExactBoundary,
			FixedBiomeWeightsLow,
			FixedBiomeWeightsHigh
		);

		const int32 VertexIndex = HexVertices.Num();
		HexVertices.Add(FVector(LocalPoint.X, LocalPoint.Y, Height));
		HexNormals.Add(Normal);
		HexUV0.Add(FVector2D(
			(LocalPoint.X / Size) + 0.5f,
			(LocalPoint.Y / Size) + 0.5f
		) * GetWorldSizeScale());
		HexUV1.Add(FVector2D(
			FixedBiomeWeightsLow.Z,
			FixedBiomeWeightsLow.W
		));
		HexUV2.Add(FVector2D(
			FixedBiomeWeightsHigh.X,
			FixedBiomeWeightsHigh.Y
		));
		HexUV3.Add(FVector2D(
			FixedBiomeWeightsHigh.Z,
			0.0f
		));
		HexColors.Add(FLinearColor(
			Altitude01,
			SlopeMask,
			FixedBiomeWeightsLow.X,
			FixedBiomeWeightsLow.Y
		));
		return VertexIndex;
	};

	for (int32 Sector = 0; Sector < 6; ++Sector)
	{
		const float AngleA = FMath::DegreesToRadians(60.0f * static_cast<float>(Sector));
		const float AngleB = FMath::DegreesToRadians(60.0f * static_cast<float>(Sector + 1));
		const FVector2D CornerA(
			FMath::Cos(AngleA) * HalfSize,
			FMath::Sin(AngleA) * HalfSize
		);
		const FVector2D CornerB(
			FMath::Cos(AngleB) * HalfSize,
			FMath::Sin(AngleB) * HalfSize
		);

		TArray<int32> SectorVertices;
		SectorVertices.SetNum((EdgeSegments + 1) * (EdgeSegments + 2) / 2);

		auto SectorIndex = [EdgeSegments](int32 I, int32 J) -> int32
		{
			return I * (EdgeSegments + 1) - (I * (I - 1)) / 2 + J;
		};

		for (int32 I = 0; I <= EdgeSegments; ++I)
		{
			for (int32 J = 0; J <= EdgeSegments - I; ++J)
			{
				const float AlphaA = static_cast<float>(I) / static_cast<float>(EdgeSegments);
				const float AlphaB = static_cast<float>(J) / static_cast<float>(EdgeSegments);
				const FVector2D Point = CornerA * AlphaA + CornerB * AlphaB;
				const bool bExactBoundary =
					I == 0
					|| J == 0
					|| I + J == EdgeSegments;
				SectorVertices[SectorIndex(I, J)] = AddExactHexVertex(Point, bExactBoundary);
			}
		}

		for (int32 I = 0; I < EdgeSegments; ++I)
		{
			for (int32 J = 0; J < EdgeSegments - I; ++J)
			{
				// UProceduralMeshComponent uses clockwise winding for the visible
				// front face. The previous order made the hex visible only from
				// underneath because back-face culling removed the top surface.
				HexTriangles.Add(SectorVertices[SectorIndex(I, J)]);
				HexTriangles.Add(SectorVertices[SectorIndex(I, J + 1)]);
				HexTriangles.Add(SectorVertices[SectorIndex(I + 1, J)]);

				if (J < EdgeSegments - I - 1)
				{
					HexTriangles.Add(SectorVertices[SectorIndex(I + 1, J)]);
					HexTriangles.Add(SectorVertices[SectorIndex(I, J + 1)]);
					HexTriangles.Add(SectorVertices[SectorIndex(I + 1, J + 1)]);
				}
			}
		}
	}

	ProcMesh->CreateMeshSection_LinearColor(
		0,
		HexVertices,
		HexTriangles,
		HexNormals,
		HexUV0,
		HexUV1,
		HexUV2,
		HexUV3,
		HexColors,
		Tangents,
		true
	);

	if (HexVertices.IsEmpty() || HexTriangles.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Moirachain] GenerateTerrain produced an empty hex mesh for '%s' (vertices=%d, triangles=%d, resolution=%d, size=%.1f)."),
			*GetName(),
			HexVertices.Num(),
			HexTriangles.Num(),
			Resolution,
			Size);
	}
	else
	{
		UE_LOG(LogTemp, Display,
			TEXT("[Moirachain] Generated terrain '%s' (vertices=%d, triangles=%d, biomes=%d)."),
			*GetName(),
			HexVertices.Num(),
			HexTriangles.Num() / 3,
			SectionBiomes.Num());
	}

	ApplyBiomeMaterialsToSections(SectionBiomes);

	GenerateTerrainObjects(Heights, Normals, VertCount, Step, MinHeight, MaxHeight);
}
