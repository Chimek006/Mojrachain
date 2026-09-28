#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldBoundaryActor.generated.h"

class UBoxComponent;
class USceneComponent;

/** Invisible collision segments surrounding the currently generated hexes. */
UCLASS(NotPlaceable)
class MOJRACHAIN_API AWorldBoundaryActor : public AActor
{
	GENERATED_BODY()

public:
	AWorldBoundaryActor();

	void RebuildBoundary(
		const TSet<FIntPoint>& GeneratedHexes,
		float TerrainSize,
		const FVector& WorldOrigin,
		float InsetFromEdge
	);

	void ClearBoundary();

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> BoundaryRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> BarrierSegments;

	void RemoveSegments();
};
