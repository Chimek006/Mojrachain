#include "ProceduralTerrainActor.h"

AProceduralTerrainActor::AProceduralTerrainActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	SetRootComponent(ProcMesh);

	ProcMesh->bUseAsyncCooking = true;
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

float AProceduralTerrainActor::SampleHeight(float X, float Y, float OffsetX, float OffsetY) const
{
	float Amplitude = 1.0f;
	float Frequency = 1.0f;
	float Total = 0.0f;
	float Normalizer = 0.0f;

	for (int32 i = 0; i < Octaves; ++i)
	{
		const float NX = (X + OffsetX) * NoiseScale * Frequency;
		const float NY = (Y + OffsetY) * NoiseScale * Frequency;

		const float N = FMath::PerlinNoise2D(FVector2D(NX, NY));
		Total += N * Amplitude;
		Normalizer += Amplitude;

		Amplitude *= Persistence;
		Frequency *= 2.0f;
	}

	return (Total / FMath::Max(Normalizer, 0.0001f)) * HeightScale;
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

void AProceduralTerrainActor::GenerateTerrain()
{
	ProcMesh->ClearAllMeshSections();

	const int32 VertCount = Resolution + 1;
	const float Step = Size / Resolution;

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

	FVector ActorLocation = GetActorLocation();

	Vertices.SetNum(VertCount * VertCount);
	UV0.SetNum(VertCount * VertCount);
	Heights.SetNum(VertCount * VertCount);
	Colors.SetNum(VertCount * VertCount);

	for (int32 Y = 0; Y < VertCount; ++Y)
	{
		for (int32 X = 0; X < VertCount; ++X)
		{
			const int32 Idx = Y * VertCount + X;

			const float LocalX = X * Step - HalfSize;
			const float LocalY = Y * Step - HalfSize;
			const float GlobalX = ActorLocation.X + LocalX;
			const float GlobalY = ActorLocation.Y + LocalY;
			const float H = SampleHeight(GlobalX, GlobalY, OffsetX, OffsetY);

			Heights[Idx] = H;

			Vertices[Idx] = FVector(LocalX, LocalY, H);

			UV0[Idx] = FVector2D(
				(float)X / (float)Resolution,
				(float)Y / (float)Resolution
			);

			Colors[Idx] = FLinearColor::White;
		}
	}

	auto HeightAt = [&](int32 X, int32 Y) -> float
		{
			X = FMath::Clamp(X, 0, VertCount - 1);
			Y = FMath::Clamp(Y, 0, VertCount - 1);
			return Heights[Y * VertCount + X];
		};

	Normals.SetNum(VertCount * VertCount);

	for (int32 Y = 0; Y < VertCount; ++Y)
	{
		for (int32 X = 0; X < VertCount; ++X)
		{
			const float HL = HeightAt(X - 1, Y);
			const float HR = HeightAt(X + 1, Y);
			const float HD = HeightAt(X, Y - 1);
			const float HU = HeightAt(X, Y + 1);

			const FVector N = FVector(HL - HR, HD - HU, 2.0f * Step).GetSafeNormal();
			Normals[Y * VertCount + X] = N;
		}
	}

	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
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