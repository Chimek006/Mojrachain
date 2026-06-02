#include "ProceduralTerrainActor.h"
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

float AProceduralTerrainActor::GetRealNoiseScale() const
{
	return NoiseScale * NoiseScaleMultiplier;
}

float AProceduralTerrainActor::GetRealDetailNoiseScale() const
{
	return DetailNoiseScale * NoiseScaleMultiplier;
}

float AProceduralTerrainActor::GetRealRidgeNoiseScale() const
{
	return RidgeNoiseScale * NoiseScaleMultiplier;
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
	const float BaseNoise = SampleFBM(
		WorldX,
		WorldY,
		OffsetX,
		OffsetY,
		GetRealNoiseScale(),
		Octaves
	);

	float Base01 = (BaseNoise + 1.0f) * 0.5f;
	Base01 = FMath::Clamp(Base01, 0.0f, 1.0f);
	Base01 = FMath::Pow(Base01, HeightPower);

	const float DetailNoise = SampleFBM(
		WorldX,
		WorldY,
		OffsetX - 3812.0f,
		OffsetY + 7281.0f,
		GetRealDetailNoiseScale(),
		2
	);

	const float Detail = DetailNoise * DetailStrength;

	const float Ridge = SampleRidgedNoise(
		WorldX,
		WorldY,
		OffsetX,
		OffsetY
	) * RidgeStrength;

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

	float Final01 = Base01 + Detail + Ridge + Mountain;
	Final01 = FMath::Clamp(Final01, -0.35f, 2.0f);

	const float EdgeMask = GetHexEdgeMask(FVector2D(LocalX, LocalY), Size * 0.5f);
	float Height = BaseHeight + Final01 * HeightScale * EdgeMask;

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
			const float RiverBaseHeight = BaseHeight + RiverBase01 * HeightScale * EdgeMask;
			const float CorridorStrength = FMath::Clamp(
				River.CorridorMask * 0.36f + River.BankMask * 0.34f,
				0.0f,
				0.82f
			);

			Height = FMath::Lerp(Height, RiverBaseHeight, CorridorStrength);

			if (River.Mask > 0.0f)
			{
				const float RiverBedHeight = RiverBaseHeight - River.Depth;
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
}

void AProceduralTerrainActor::ApplyBiomePreset()
{
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
}

#if WITH_EDITOR
void AProceduralTerrainActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralTerrainActor, Biome))
	{
		ApplyBiomePreset();
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
	if (!ProcMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateTerrain failed: ProcMesh is null."));
		return;
	}

	ProcMesh->ClearAllMeshSections();

	Resolution = FMath::Clamp(Resolution, 2, 250);
	Size = FMath::Clamp(Size, 100.0f, 50000.0f);

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
	const float HexRadius = (Size * 0.5f) + 2.0f;

	FRandomStream Stream(Seed);
	const float OffsetX = Stream.FRandRange(-10000.0f, 10000.0f);
	const float OffsetY = Stream.FRandRange(-10000.0f, 10000.0f);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<float> Heights;

	const FVector ActorLocation = GetActorLocation();

	Vertices.SetNum(VertCount * VertCount);
	UV0.SetNum(VertCount * VertCount);
	Heights.SetNum(VertCount * VertCount);
	Colors.SetNum(VertCount * VertCount);
	Normals.SetNum(VertCount * VertCount);

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

			UV0[Idx] = FVector2D(
				static_cast<float>(X) / static_cast<float>(SafeResolution),
				static_cast<float>(Y) / static_cast<float>(SafeResolution)
			);

			Colors[Idx] = FLinearColor::White;
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

	for (int32 Y = 0; Y < VertCount; ++Y)
	{
		for (int32 X = 0; X < VertCount; ++X)
		{
			const int32 Idx = Y * VertCount + X;

			const float LocalX = X * Step - HalfSize;
			const float LocalY = Y * Step - HalfSize;

			Vertices[Idx] = FVector(LocalX, LocalY, Heights[Idx]);
		}
	}

	for (int32 Y = 0; Y < VertCount; ++Y)
	{
		for (int32 X = 0; X < VertCount; ++X)
		{
			const float HL = HeightAt(Heights, X - 1, Y);
			const float HR = HeightAt(Heights, X + 1, Y);
			const float HD = HeightAt(Heights, X, Y - 1);
			const float HU = HeightAt(Heights, X, Y + 1);

			const FVector N = FVector(HL - HR, HD - HU, 2.0f * Step).GetSafeNormal();
			Normals[Y * VertCount + X] = N;
		}
	}

	for (int32 Y = 0; Y < SafeResolution; ++Y)
	{
		for (int32 X = 0; X < SafeResolution; ++X)
		{
			const float CellCenterX = (X + 0.5f) * Step - HalfSize;
			const float CellCenterY = (Y + 0.5f) * Step - HalfSize;

			if (!IsPointInsideHex(FVector2D(CellCenterX, CellCenterY), HexRadius))
			{
				continue;
			}

			const int32 I0 = Y * VertCount + X;
			const int32 I1 = I0 + 1;
			const int32 I2 = I0 + VertCount;
			const int32 I3 = I2 + 1;

			Triangles.Add(I0);
			Triangles.Add(I2);
			Triangles.Add(I1);

			Triangles.Add(I1);
			Triangles.Add(I2);
			Triangles.Add(I3);
		}
	}

	ProcMesh->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UV0,
		TArray<FVector2D>(),
		TArray<FVector2D>(),
		TArray<FVector2D>(),
		Colors,
		Tangents,
		true
	);
}
