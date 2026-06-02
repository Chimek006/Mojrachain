#include "TerrainRiverTypes.h"

#include "Algo/Reverse.h"
#include "ProceduralTerrainActor.h"
#include "TerrainMountainTypes.h"

namespace
{
struct FTerrainRiverStats
{
	float MinHeight = 0.0f;
	float MaxHeight = 0.0f;
	float AverageHeight = 0.0f;

	float GetRange() const
	{
		return FMath::Max(MaxHeight - MinHeight, 1.0f);
	}
};

struct FRiverCandidate
{
	TArray<FVector2D> WorldPoints;
	float Width = 0.0f;
	float Depth = 0.0f;
	float Score = TNumericLimits<float>::Max();
};

struct FRiverClosestPoint
{
	float DistanceSq = TNumericLimits<float>::Max();
	float DistanceAlongPath = 0.0f;
	float TotalPathLength = 0.0f;
};

bool IsPointInsideHex(const FVector2D& Point, float Radius)
{
	const float X = FMath::Abs(Point.X);
	const float Y = FMath::Abs(Point.Y);

	const float HexDistance = FMath::Max(
		X / Radius,
		(0.5f * X + 0.8660254f * Y) / Radius
	);

	return HexDistance <= 1.0f;
}

FVector2D CatmullRom(
	const FVector2D& P0,
	const FVector2D& P1,
	const FVector2D& P2,
	const FVector2D& P3,
	float T
)
{
	const float T2 = T * T;
	const float T3 = T2 * T;

	return (P1 * 2.0f
		+ (P2 - P0) * T
		+ (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * T2
		+ (P0 * -1.0f + P1 * 3.0f - P2 * 3.0f + P3) * T3) * 0.5f;
}

void BuildSmoothPath(const TArray<FVector2D>& Anchors, int32 DesiredPointCount, TArray<FVector2D>& OutPoints)
{
	OutPoints.Reset();

	if (Anchors.Num() < 2)
	{
		return;
	}

	const int32 SegmentCount = Anchors.Num() - 1;
	const int32 PointsPerSegment = FMath::Max(2, FMath::CeilToInt(static_cast<float>(DesiredPointCount) / SegmentCount));

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const FVector2D& P0 = Anchors[FMath::Max(0, SegmentIndex - 1)];
		const FVector2D& P1 = Anchors[SegmentIndex];
		const FVector2D& P2 = Anchors[SegmentIndex + 1];
		const FVector2D& P3 = Anchors[FMath::Min(Anchors.Num() - 1, SegmentIndex + 2)];

		for (int32 StepIndex = 0; StepIndex < PointsPerSegment; ++StepIndex)
		{
			if (SegmentIndex > 0 && StepIndex == 0)
			{
				continue;
			}

			const float T = static_cast<float>(StepIndex) / static_cast<float>(PointsPerSegment);
			OutPoints.Add(CatmullRom(P0, P1, P2, P3, T));
		}
	}

	OutPoints.Add(Anchors.Last());
}

float DistanceSquaredToSegmentWithAlpha(const FVector2D& Point, const FVector2D& A, const FVector2D& B, float& OutAlpha)
{
	const FVector2D Segment = B - A;
	const float SegmentLengthSq = Segment.SizeSquared();

	if (SegmentLengthSq <= KINDA_SMALL_NUMBER)
	{
		OutAlpha = 0.0f;
		return FVector2D::DistSquared(Point, A);
	}

	OutAlpha = FMath::Clamp(FVector2D::DotProduct(Point - A, Segment) / SegmentLengthSq, 0.0f, 1.0f);
	const FVector2D Closest = A + Segment * OutAlpha;
	return FVector2D::DistSquared(Point, Closest);
}

FRiverClosestPoint FindClosestRiverPoint(const FVector2D& Point, const TArray<FVector2D>& WorldPoints)
{
	FRiverClosestPoint Result;

	if (WorldPoints.Num() < 2)
	{
		return Result;
	}

	float DistanceBeforeSegment = 0.0f;
	const int32 DistanceCheckStep = WorldPoints.Num() > 160 ? 2 : 1;
	int32 BestSegmentIndex = INDEX_NONE;

	for (int32 Index = 0; Index < WorldPoints.Num() - 1; ++Index)
	{
		const FVector2D& A = WorldPoints[Index];
		const FVector2D& B = WorldPoints[Index + 1];
		const float SegmentLength = FVector2D::Distance(A, B);

		if (Index % DistanceCheckStep != 0 && Index < WorldPoints.Num() - 2)
		{
			DistanceBeforeSegment += SegmentLength;
			continue;
		}

		float SegmentAlpha = 0.0f;
		const float DistanceSq = DistanceSquaredToSegmentWithAlpha(Point, A, B, SegmentAlpha);

		if (DistanceSq < Result.DistanceSq)
		{
			Result.DistanceSq = DistanceSq;
			Result.DistanceAlongPath = DistanceBeforeSegment + SegmentLength * SegmentAlpha;
			BestSegmentIndex = Index;
		}

		DistanceBeforeSegment += SegmentLength;
	}

	Result.TotalPathLength = DistanceBeforeSegment;

	if (DistanceCheckStep > 1 && BestSegmentIndex != INDEX_NONE)
	{
		const int32 StartIndex = FMath::Max(0, BestSegmentIndex - DistanceCheckStep);
		const int32 EndIndex = FMath::Min(WorldPoints.Num() - 2, BestSegmentIndex + DistanceCheckStep);
		float RefineDistanceBeforeSegment = 0.0f;

		for (int32 Index = 0; Index < StartIndex; ++Index)
		{
			RefineDistanceBeforeSegment += FVector2D::Distance(WorldPoints[Index], WorldPoints[Index + 1]);
		}

		for (int32 Index = StartIndex; Index <= EndIndex; ++Index)
		{
			const FVector2D& A = WorldPoints[Index];
			const FVector2D& B = WorldPoints[Index + 1];
			const float SegmentLength = FVector2D::Distance(A, B);

			float SegmentAlpha = 0.0f;
			const float DistanceSq = DistanceSquaredToSegmentWithAlpha(Point, A, B, SegmentAlpha);

			if (DistanceSq < Result.DistanceSq)
			{
				Result.DistanceSq = DistanceSq;
				Result.DistanceAlongPath = RefineDistanceBeforeSegment + SegmentLength * SegmentAlpha;
			}

			RefineDistanceBeforeSegment += SegmentLength;
		}
	}

	return Result;
}

bool HasClearanceFromExistingRivers(
	const TArray<FVector2D>& Points,
	float HalfWidth,
	const TArray<FGeneratedTerrainRiverPath>& ExistingRiverPaths,
	float Clearance
)
{
	const float SafeClearance = FMath::Max(0.0f, Clearance);
	const int32 ThisStep = FMath::Max(1, Points.Num() / 72);

	for (const FGeneratedTerrainRiverPath& ExistingPath : ExistingRiverPaths)
	{
		if (ExistingPath.WorldPoints.IsEmpty())
		{
			continue;
		}

		const float MinDistance = HalfWidth + ExistingPath.HalfWidth + SafeClearance;
		const float MinDistanceSq = MinDistance * MinDistance;
		const int32 ExistingStep = FMath::Max(1, ExistingPath.WorldPoints.Num() / 72);

		for (int32 Index = 0; Index < Points.Num(); Index += ThisStep)
		{
			for (int32 ExistingIndex = 0; ExistingIndex < ExistingPath.WorldPoints.Num(); ExistingIndex += ExistingStep)
			{
				if (FVector2D::DistSquared(Points[Index], ExistingPath.WorldPoints[ExistingIndex]) < MinDistanceSq)
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool TouchesForbiddenAreas(
	const TArray<FVector2D>& Points,
	float HalfWidth,
	const TArray<FRiverForbiddenArea>& ForbiddenAreas
)
{
	if (Points.Num() < 2 || ForbiddenAreas.IsEmpty())
	{
		return false;
	}

	for (const FRiverForbiddenArea& Area : ForbiddenAreas)
	{
		const float ProtectedRadius = FMath::Max(0.0f, Area.Radius + Area.Clearance + HalfWidth);
		if (ProtectedRadius <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			if (IsPointInsideHex(Points[Index] - Area.Center, ProtectedRadius))
			{
				return true;
			}
		}

		const float SegmentSampleStep = FMath::Max(250.0f, ProtectedRadius * 0.08f);

		for (int32 Index = 0; Index < Points.Num() - 1; ++Index)
		{
			const FVector2D& A = Points[Index];
			const FVector2D& B = Points[Index + 1];
			const float SegmentLength = FVector2D::Distance(A, B);
			const int32 StepCount = FMath::Max(2, FMath::CeilToInt(SegmentLength / SegmentSampleStep));

			for (int32 StepIndex = 1; StepIndex < StepCount; ++StepIndex)
			{
				const float T = static_cast<float>(StepIndex) / static_cast<float>(StepCount);
				const FVector2D Point = FMath::Lerp(A, B, T);

				if (IsPointInsideHex(Point - Area.Center, ProtectedRadius))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool HasNoSelfOverlap(const TArray<FVector2D>& Points, float HalfWidth)
{
	const float MinDistance = FMath::Max(160.0f, HalfWidth * 1.18f);
	const float MinDistanceSq = MinDistance * MinDistance;
	const int32 Step = FMath::Max(1, Points.Num() / 72);
	const int32 IgnoreNeighborRange = FMath::Max(7, Points.Num() / 7);

	for (int32 Index = 0; Index < Points.Num(); Index += Step)
	{
		for (int32 OtherIndex = Index + IgnoreNeighborRange; OtherIndex < Points.Num(); OtherIndex += Step)
		{
			if (FVector2D::DistSquared(Points[Index], Points[OtherIndex]) < MinDistanceSq)
			{
				return false;
			}
		}
	}

	return true;
}

float GetRiverPeakLimitByBiome(ETerrainBiome Biome)
{
	switch (Biome)
	{
	case ETerrainBiome::Mountain:
		return 0.44f;
	case ETerrainBiome::Tundra:
		return 0.58f;
	case ETerrainBiome::Hills:
		return 0.64f;
	case ETerrainBiome::Forest:
	case ETerrainBiome::Swamp:
		return 0.76f;
	default:
		return 0.86f;
	}
}

float GetRiverHeightVariationLimit(const AProceduralTerrainActor* Terrain, float Depth)
{
	if (!Terrain)
	{
		return 260.0f;
	}

	const float DepthAllowance = Depth * 0.95f;
	const float TerrainAllowance = Terrain->HeightScale * 0.24f;

	switch (Terrain->Biome)
	{
	case ETerrainBiome::Mountain:
		return FMath::Clamp(FMath::Max(DepthAllowance, TerrainAllowance), 300.0f, 700.0f);
	case ETerrainBiome::Hills:
	case ETerrainBiome::Tundra:
		return FMath::Clamp(FMath::Max(DepthAllowance, TerrainAllowance), 240.0f, 540.0f);
	default:
		return FMath::Clamp(FMath::Max(DepthAllowance, TerrainAllowance), 180.0f, 460.0f);
	}
}

float GetPathCurvatureScore(const TArray<FVector2D>& Points)
{
	if (Points.Num() < 4)
	{
		return 0.0f;
	}

	float TotalAngle = 0.0f;
	int32 Count = 0;

	for (int32 Index = 1; Index < Points.Num() - 1; ++Index)
	{
		FVector2D A = Points[Index] - Points[Index - 1];
		FVector2D B = Points[Index + 1] - Points[Index];

		if (!A.Normalize() || !B.Normalize())
		{
			continue;
		}

		TotalAngle += FMath::Acos(FMath::Clamp(FVector2D::DotProduct(A, B), -1.0f, 1.0f));
		++Count;
	}

	return Count > 0 ? TotalAngle / static_cast<float>(Count) : 0.0f;
}

FTerrainRiverStats MeasureTerrainStats(const AProceduralTerrainActor* Terrain)
{
	FTerrainRiverStats Stats;

	if (!Terrain)
	{
		return Stats;
	}

	const FVector TerrainLocation = Terrain->GetActorLocation();
	const float HalfSize = Terrain->Size * 0.5f;
	const float Radius = HalfSize + 2.0f;
	constexpr int32 SampleCount = 9;

	float Sum = 0.0f;
	int32 Count = 0;
	Stats.MinHeight = TNumericLimits<float>::Max();
	Stats.MaxHeight = -TNumericLimits<float>::Max();

	for (int32 Y = 0; Y < SampleCount; ++Y)
	{
		for (int32 X = 0; X < SampleCount; ++X)
		{
			const float LocalX = FMath::Lerp(-HalfSize, HalfSize, static_cast<float>(X) / static_cast<float>(SampleCount - 1));
			const float LocalY = FMath::Lerp(-HalfSize, HalfSize, static_cast<float>(Y) / static_cast<float>(SampleCount - 1));

			if (!IsPointInsideHex(FVector2D(LocalX, LocalY), Radius))
			{
				continue;
			}

			const float Height = Terrain->GetTerrainHeightAtWorldLocation(
				TerrainLocation.X + LocalX,
				TerrainLocation.Y + LocalY
			);

			Stats.MinHeight = FMath::Min(Stats.MinHeight, Height);
			Stats.MaxHeight = FMath::Max(Stats.MaxHeight, Height);
			Sum += Height;
			++Count;
		}
	}

	if (Count <= 0)
	{
		Stats.MinHeight = TerrainLocation.Z;
		Stats.MaxHeight = TerrainLocation.Z;
		Stats.AverageHeight = TerrainLocation.Z;
		return Stats;
	}

	Stats.AverageHeight = Sum / static_cast<float>(Count);
	return Stats;
}

float GetPathMountainPenalty(const AProceduralTerrainActor* Terrain, const TArray<FVector2D>& WorldPoints)
{
	if (!Terrain || !Terrain->MountainSettings.bEnabled || WorldPoints.IsEmpty())
	{
		return 0.0f;
	}

	const FVector TerrainLocation = Terrain->GetActorLocation();
	const int32 Step = FMath::Max(1, WorldPoints.Num() / 36);
	float WorstMountain = 0.0f;
	float SumMountain = 0.0f;
	int32 Count = 0;

	for (int32 Index = 0; Index < WorldPoints.Num(); Index += Step)
	{
		const float Mountain = UTerrainMountainLibrary::SampleMountainContribution(
			WorldPoints[Index].X,
			WorldPoints[Index].Y,
			TerrainLocation,
			Terrain->Seed,
			Terrain->Resolution,
			Terrain->Size,
			0.0f,
			Terrain->MountainSettings
		);

		WorstMountain = FMath::Max(WorstMountain, Mountain);
		SumMountain += Mountain;
		++Count;
	}

	const float AverageMountain = Count > 0 ? SumMountain / static_cast<float>(Count) : 0.0f;
	return WorstMountain * 1.4f + AverageMountain * 0.8f;
}
}

float UTerrainRiverLibrary::SmoothStep01(float Value)
{
	const float T = FMath::Clamp(Value, 0.0f, 1.0f);
	return T * T * (3.0f - 2.0f * T);
}

bool UTerrainRiverLibrary::BuildRiverForTerrain(
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
)
{
	OutSettings = FRiverTerrainSettings();
	OutPath = FGeneratedTerrainRiverPath();

	if (!Terrain || Terrain->Biome == ETerrainBiome::Desert)
	{
		return false;
	}

	const float MinWidth = FMath::Max(100.0f, FMath::Min(WidthMin, WidthMax));
	const float MaxWidth = FMath::Max(MinWidth, WidthMax);
	const float MinDepth = FMath::Max(0.0f, FMath::Min(DepthMin, DepthMax));
	const float MaxDepth = FMath::Max(MinDepth, DepthMax);

	FRandomStream Stream(HashCombineFast(
		::GetTypeHash(Terrain->Seed),
		::GetTypeHash(FMath::RoundToInt(Terrain->GetActorLocation().X * 0.013f + Terrain->GetActorLocation().Y * 0.017f))
	) ^ 0x7284ab31);

	const FVector TerrainLocation = Terrain->GetActorLocation();
	const FVector2D TerrainCenter(TerrainLocation.X, TerrainLocation.Y);
	const float HalfSize = Terrain->Size * 0.5f;
	const float Radius = HalfSize + 2.0f;
	const FTerrainRiverStats TerrainStats = MeasureTerrainStats(Terrain);
	const float PeakLimit = TerrainStats.MinHeight + TerrainStats.GetRange() * GetRiverPeakLimitByBiome(Terrain->Biome);

	FRiverCandidate BestCandidate;
	bool bHasCandidate = false;
	constexpr int32 AttemptCount = 120;
	constexpr int32 PathPointCount = 280;

	for (int32 AttemptIndex = 0; AttemptIndex < AttemptCount; ++AttemptIndex)
	{
		FRiverCandidate Candidate;
		Candidate.Width = Stream.FRandRange(MinWidth, MaxWidth);
		Candidate.Depth = Stream.FRandRange(MinDepth, MaxDepth) * 1.55f;

		const float Length = Terrain->Size * Stream.FRandRange(12.0f, 34.0f);
		const float Angle = Stream.FRandRange(0.0f, 2.0f * UE_PI);
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector2D Perpendicular(-Direction.Y, Direction.X);

		const float CenterOffsetRange = Terrain->Size * 0.20f;
		const FVector2D CenterOffset(
			Stream.FRandRange(-CenterOffsetRange, CenterOffsetRange),
			Stream.FRandRange(-CenterOffsetRange, CenterOffsetRange)
		);

		const FVector2D Start = TerrainCenter + CenterOffset - Direction * (Length * 0.5f);
		const FVector2D End = TerrainCenter + CenterOffset + Direction * (Length * 0.5f);

		const int32 AnchorCount = Stream.RandRange(30, 52);
		const float PrimaryAmplitude = Terrain->Size * Stream.FRandRange(0.9f, 2.15f);
		const float SecondaryAmplitude = PrimaryAmplitude * Stream.FRandRange(0.32f, 0.62f);
		const float DetailAmplitude = Terrain->Size * Stream.FRandRange(0.16f, 0.42f);
		const float PrimaryWavelength = Terrain->Size * Stream.FRandRange(1.9f, 4.2f);
		const float SecondaryWavelength = Terrain->Size * Stream.FRandRange(0.82f, 1.75f);
		const float PrimaryCycles = Length / FMath::Max(PrimaryWavelength, 1.0f);
		const float SecondaryCycles = Length / FMath::Max(SecondaryWavelength, 1.0f);
		const float PrimaryPhase = Stream.FRandRange(0.0f, 2.0f * UE_PI);
		const float SecondaryPhase = Stream.FRandRange(0.0f, 2.0f * UE_PI);
		const float DetailPhase = Stream.FRandRange(0.0f, 2.0f * UE_PI);
		const float DirectionWander = Terrain->Size * Stream.FRandRange(0.08f, 0.22f);

		TArray<FVector2D> Anchors;
		Anchors.Reserve(AnchorCount);
		Anchors.Add(Start);

		for (int32 AnchorIndex = 1; AnchorIndex < AnchorCount - 1; ++AnchorIndex)
		{
			const float T = static_cast<float>(AnchorIndex) / static_cast<float>(AnchorCount - 1);
			const FVector2D Base = Start + (End - Start) * T;
			const float Ease = FMath::Sin(T * UE_PI);
			const float MeanderOffset = Ease * (
				FMath::Sin(T * 2.0f * UE_PI * PrimaryCycles + PrimaryPhase) * PrimaryAmplitude
				+ FMath::Sin(T * 2.0f * UE_PI * SecondaryCycles + SecondaryPhase) * SecondaryAmplitude
				+ FMath::Sin(T * 2.0f * UE_PI * SecondaryCycles * 2.35f + DetailPhase) * DetailAmplitude
			);
			const float LocalSearchRadius = Terrain->Size * Stream.FRandRange(0.18f, 0.52f);

			FVector2D BestAnchor = Base;
			float BestAnchorCost = TNumericLimits<float>::Max();

			for (int32 OptionIndex = 0; OptionIndex < 12; ++OptionIndex)
			{
				const float TargetOffset = MeanderOffset + Stream.FRandRange(-LocalSearchRadius, LocalSearchRadius);

				const FVector2D CandidateAnchor =
					Base
					+ Perpendicular * TargetOffset
					+ Direction * Stream.FRandRange(-DirectionWander, DirectionWander);

				const float Height = Terrain->GetTerrainHeightAtWorldLocation(CandidateAnchor.X, CandidateAnchor.Y);
				const float NormalizedHeight = (Height - TerrainStats.MinHeight) / TerrainStats.GetRange();
				const float LocalMountain = Terrain->MountainSettings.bEnabled
					? UTerrainMountainLibrary::SampleMountainContribution(
						CandidateAnchor.X,
						CandidateAnchor.Y,
						TerrainLocation,
						Terrain->Seed,
						Terrain->Resolution,
						Terrain->Size,
						0.0f,
						Terrain->MountainSettings
					)
					: 0.0f;

				const float CenterDistance = FVector2D::Distance(CandidateAnchor, TerrainCenter);
				const float OutsidePenalty = IsPointInsideHex(CandidateAnchor - TerrainCenter, Radius)
					? 0.0f
					: FMath::Clamp((CenterDistance - Radius) / FMath::Max(Radius, 1.0f), 0.0f, 1.0f) * 0.35f;
				const float Cost = NormalizedHeight + LocalMountain * 3.0f + OutsidePenalty + Stream.FRandRange(0.0f, 0.025f);

				if (Cost < BestAnchorCost)
				{
					BestAnchorCost = Cost;
					BestAnchor = CandidateAnchor;
				}
			}

			Anchors.Add(BestAnchor);
		}

		Anchors.Add(End);
		BuildSmoothPath(Anchors, PathPointCount, Candidate.WorldPoints);

		if (Candidate.WorldPoints.Num() < 4)
		{
			continue;
		}

		const float CandidateSafeHalfWidth = Candidate.Width * 1.55f;

		if (TouchesForbiddenAreas(Candidate.WorldPoints, CandidateSafeHalfWidth, ForbiddenAreas))
		{
			continue;
		}

		if (!HasNoSelfOverlap(Candidate.WorldPoints, CandidateSafeHalfWidth))
		{
			continue;
		}

		if (!HasClearanceFromExistingRivers(
			Candidate.WorldPoints,
			CandidateSafeHalfWidth,
			ExistingRiverPaths,
			ClearanceFromOtherRivers
		))
		{
			continue;
		}

		float MinPathHeight = TNumericLimits<float>::Max();
		float MaxPathHeight = -TNumericLimits<float>::Max();
		float SumPathHeight = 0.0f;
		int32 InsideCount = 0;

		for (const FVector2D& Point : Candidate.WorldPoints)
		{
			if (!IsPointInsideHex(Point - TerrainCenter, Radius))
			{
				continue;
			}

			const float Height = Terrain->GetTerrainHeightAtWorldLocation(Point.X, Point.Y);
			MinPathHeight = FMath::Min(MinPathHeight, Height);
			MaxPathHeight = FMath::Max(MaxPathHeight, Height);
			SumPathHeight += Height;
			++InsideCount;
		}

		if (InsideCount < 8)
		{
			continue;
		}

		if (MaxPathHeight > PeakLimit)
		{
			continue;
		}

		const float HeightRange = MaxPathHeight - MinPathHeight;
		const float VariationLimit = GetRiverHeightVariationLimit(Terrain, Candidate.Depth);
		if (HeightRange > VariationLimit)
		{
			continue;
		}

		const float MountainPenalty = GetPathMountainPenalty(Terrain, Candidate.WorldPoints);
		if (MountainPenalty > 0.32f)
		{
			continue;
		}

		const float AverageHeight = SumPathHeight / static_cast<float>(InsideCount);
		const float NormalizedAverageHeight = (AverageHeight - TerrainStats.MinHeight) / TerrainStats.GetRange();
		const float NormalizedRange = HeightRange / TerrainStats.GetRange();
		const float CurvatureBonus = FMath::Clamp(GetPathCurvatureScore(Candidate.WorldPoints) * 0.95f, 0.0f, 0.42f);

		Candidate.Score = NormalizedAverageHeight + NormalizedRange * 1.45f + MountainPenalty * 2.2f - CurvatureBonus;

		if (!bHasCandidate || Candidate.Score < BestCandidate.Score)
		{
			BestCandidate = Candidate;
			bHasCandidate = true;
		}
	}

	if (!bHasCandidate)
	{
		return false;
	}

	OutSettings.bEnabled = true;
	OutSettings.Width = BestCandidate.Width;
	OutSettings.Depth = BestCandidate.Depth;
	OutSettings.WorldPoints = BestCandidate.WorldPoints;
	OutSettings.FlatBedRatio = 0.46f;
	OutSettings.BedNoiseStrength = 0.075f;
	OutSettings.BedNoiseScale = 0.38f;

	OutPath.WorldPoints = BestCandidate.WorldPoints;
	OutPath.HalfWidth = BestCandidate.Width * 1.55f;
	return true;
}

FRiverTerrainSample UTerrainRiverLibrary::SampleRiver(
	float WorldX,
	float WorldY,
	int32 Seed,
	const FRiverTerrainSettings& Settings
)
{
	FRiverTerrainSample Sample;

	if (!Settings.bEnabled || Settings.WorldPoints.Num() < 2 || Settings.Width <= 0.0f || Settings.Depth <= 0.0f)
	{
		return Sample;
	}

	const FVector2D Point(WorldX, WorldY);
	const FRiverClosestPoint Closest = FindClosestRiverPoint(Point, Settings.WorldPoints);

	if (Closest.TotalPathLength <= KINDA_SMALL_NUMBER)
	{
		return Sample;
	}

	const float PathT = FMath::Clamp(Closest.DistanceAlongPath / Closest.TotalPathLength, 0.0f, 1.0f);
	const uint32 PathSeedHash = HashCombineFast(
		::GetTypeHash(FMath::RoundToInt(Settings.WorldPoints[0].X * 0.037f + Settings.WorldPoints[0].Y * 0.041f)),
		::GetTypeHash(FMath::RoundToInt(Settings.WorldPoints.Last().X * 0.053f + Settings.WorldPoints.Last().Y * 0.059f))
	);
	const uint32 EffectiveVariationSeed = HashCombineFast(::GetTypeHash(Seed), PathSeedHash);
	const float SeedPhase = static_cast<float>(EffectiveVariationSeed % 100000u);

	const float WidthNoise = FMath::PerlinNoise2D(FVector2D(
		PathT * 7.2f + SeedPhase * 0.013f,
		SeedPhase * 0.071f
	));
	const float DepthNoise = FMath::PerlinNoise2D(FVector2D(
		PathT * 9.2f - SeedPhase * 0.021f,
		SeedPhase * 0.037f + 18.31f
	));
	const float WidthWave = FMath::Sin(PathT * 2.0f * UE_PI * 3.1f + SeedPhase * 0.017f);
	const float WidthDetailWave = FMath::Sin(PathT * 2.0f * UE_PI * 8.7f - SeedPhase * 0.029f);
	const float DepthWave = FMath::Sin(PathT * 2.0f * UE_PI * 4.2f - SeedPhase * 0.011f);

	const float MaxEndFadeLength = FMath::Max(900.0f, Closest.TotalPathLength * 0.08f);
	const float EndFadeLength = FMath::Min(FMath::Max(Settings.Width * 5.0f, 900.0f), MaxEndFadeLength);
	const float EndDistance = FMath::Min(Closest.DistanceAlongPath, Closest.TotalPathLength - Closest.DistanceAlongPath);
	const float EndFade = SmoothStep01(EndDistance / FMath::Max(EndFadeLength, 1.0f));

	if (EndFade <= 0.001f)
	{
		return Sample;
	}

	const float WidthScale = FMath::Clamp(1.0f + WidthNoise * 0.58f + WidthWave * 0.34f + WidthDetailWave * 0.14f, 0.44f, 1.92f);
	const float DepthScale = FMath::Clamp(1.0f + DepthNoise * 0.42f + DepthWave * 0.18f - WidthNoise * 0.08f, 0.72f, 1.68f);

	const float LocalWidth = Settings.Width * WidthScale * (0.56f + EndFade * 0.44f);
	const float LocalDepth = Settings.Depth * DepthScale * EndFade;
	const float Distance = FMath::Sqrt(Closest.DistanceSq);
	const float HalfWidth = LocalWidth * 0.5f;
	const float ShoulderWidth = LocalWidth * 0.85f;
	const float OuterFeatherWidth = LocalWidth * 0.7f;
	const float ShoulderOuterRadius = HalfWidth + ShoulderWidth;
	const float CorridorHalfWidth = ShoulderOuterRadius + OuterFeatherWidth;

	if (Distance >= CorridorHalfWidth)
	{
		return Sample;
	}

	Sample.Width = LocalWidth;
	Sample.Depth = LocalDepth;
	Sample.CorridorMask = EndFade;

	if (Distance > ShoulderOuterRadius)
	{
		const float OuterT = (Distance - ShoulderOuterRadius) / FMath::Max(OuterFeatherWidth, 1.0f);
		Sample.CorridorMask *= 1.0f - SmoothStep01(OuterT);
	}

	if (Distance <= ShoulderOuterRadius)
	{
		const float ShoulderT = FMath::Clamp(Distance / FMath::Max(ShoulderOuterRadius, 1.0f), 0.0f, 1.0f);
		Sample.BankMask = EndFade * (1.0f - SmoothStep01(ShoulderT) * 0.45f);
	}

	if (Distance <= HalfWidth)
	{
		const float SideT = FMath::Clamp(Distance / FMath::Max(HalfWidth, 1.0f), 0.0f, 1.0f);
		constexpr float ChannelEdgeMask = 0.24f;
		const float ChannelCurve = FMath::Pow(1.0f - SmoothStep01(SideT), 1.05f);
		const float CenterCurve = FMath::Pow(1.0f - SmoothStep01(SideT), 1.45f);

		Sample.Mask = EndFade * FMath::Lerp(ChannelEdgeMask, 1.0f, ChannelCurve);
		Sample.CenterMask = EndFade * CenterCurve;
	}
	else if (Distance <= ShoulderOuterRadius)
	{
		constexpr float ChannelEdgeMask = 0.24f;
		const float ShoulderT = (Distance - HalfWidth) / FMath::Max(ShoulderWidth, 1.0f);
		const float ShoulderCurve = FMath::Pow(1.0f - SmoothStep01(ShoulderT), 1.35f);
		Sample.Mask = EndFade * ShoulderCurve * ChannelEdgeMask;
	}

	const float NoiseScale = FMath::Max(Settings.BedNoiseScale, 0.01f) * 0.001f;
	const FVector2D NoisePoint(
		(WorldX + Seed * 13.37f) * NoiseScale,
		(WorldY - Seed * 7.91f) * NoiseScale
	);
	Sample.BedNoise = FMath::PerlinNoise2D(NoisePoint) * Settings.BedNoiseStrength * EndFade;

	return Sample;
}
