#include "World/WorldBoundaryActor.h"

#include "Components/BoxComponent.h"

namespace
{
	TArray<FIntPoint> GetNeighborDirections()
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

	FVector2D HexToWorldOffset(const FIntPoint& Coordinates, float TerrainSize)
	{
		const float HexRadius = TerrainSize * 0.5f;
		return FVector2D(
			HexRadius * 1.5f * static_cast<float>(Coordinates.X),
			HexRadius * FMath::Sqrt(3.0f) * (
				static_cast<float>(Coordinates.Y) + static_cast<float>(Coordinates.X) * 0.5f
			)
		);
	}
}

AWorldBoundaryActor::AWorldBoundaryActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	BoundaryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoundaryRoot"));
	SetRootComponent(BoundaryRoot);
	SetActorHiddenInGame(true);
}

void AWorldBoundaryActor::RemoveSegments()
{
	for (UBoxComponent* Segment : BarrierSegments)
	{
		if (Segment)
		{
			Segment->DestroyComponent();
		}
	}

	BarrierSegments.Reset();
}

void AWorldBoundaryActor::ClearBoundary()
{
	RemoveSegments();
}

void AWorldBoundaryActor::RebuildBoundary(
	const TSet<FIntPoint>& GeneratedHexes,
	float TerrainSize,
	const FVector& WorldOrigin,
	float InsetFromEdge
)
{
	RemoveSegments();

	if (GeneratedHexes.IsEmpty() || TerrainSize <= 0.0f)
	{
		return;
	}

	SetActorLocation(WorldOrigin);
	SetActorRotation(FRotator::ZeroRotator);

	const float HexRadius = TerrainSize * 0.5f;
	const float SafeInset = FMath::Clamp(InsetFromEdge, 0.0f, HexRadius * 0.25f);
	const float EdgeLength = HexRadius;
	const float HexApothem = HexRadius * 0.8660254038f;
	const float SegmentThickness = FMath::Max(25.0f, TerrainSize * 0.00025f);
	const float SegmentOverlap = FMath::Max(50.0f, TerrainSize * 0.001f);
	const float SegmentHalfHeight = FMath::Max(250000.0f, TerrainSize * 2.5f);

	for (const FIntPoint& Coordinates : GeneratedHexes)
	{
		const FVector2D CenterOffset = HexToWorldOffset(Coordinates, TerrainSize);

		for (const FIntPoint& Direction : GetNeighborDirections())
		{
			if (GeneratedHexes.Contains(Coordinates + Direction))
			{
				continue;
			}

			const FVector2D NeighborOffset = HexToWorldOffset(Direction, TerrainSize);
			const FVector2D Normal = NeighborOffset.GetSafeNormal();
			if (Normal.IsNearlyZero())
			{
				continue;
			}

			const FVector2D Tangent(-Normal.Y, Normal.X);
			// The side of a flat-top hex is at the apothem, not at the corner
			// radius. Using HexRadius here moves the wall about 13.4% too far
			// outside the playable hex and leaves a passable gap.
			const FVector2D EdgeCenter = CenterOffset + Normal * (HexApothem - SafeInset);

			UBoxComponent* Segment = NewObject<UBoxComponent>(this);
			if (!Segment)
			{
				continue;
			}

			AddInstanceComponent(Segment);
			Segment->SetupAttachment(BoundaryRoot);
			Segment->SetRelativeLocation(FVector(EdgeCenter.X, EdgeCenter.Y, 0.0f));
			Segment->SetRelativeRotation(FRotator(
				0.0f,
				FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X)),
				0.0f
			));
			Segment->SetBoxExtent(FVector(
				EdgeLength * 0.5f + SegmentOverlap,
				SegmentThickness,
				SegmentHalfHeight
			));
			Segment->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Segment->SetCollisionProfileName(TEXT("BlockAll"));
			Segment->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			Segment->SetGenerateOverlapEvents(false);
			Segment->SetCanEverAffectNavigation(false);
			Segment->SetVisibility(false, true);
			Segment->SetHiddenInGame(true);
			Segment->RegisterComponent();
			BarrierSegments.Add(Segment);
		}
	}
}
