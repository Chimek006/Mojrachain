#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralTerrainActor.generated.h"

UCLASS()
class MOJRACHAIN_API AProceduralTerrainActor : public AActor
{
	GENERATED_BODY()

public:
	AProceduralTerrainActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> ProcMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "2"))
	int32 Resolution = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "100.0"))
	float Size = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "1.0"))
	float HeightScale = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0.0001"))
	float NoiseScale = 0.008f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "1"))
	int32 Octaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Persistence = 0.5f;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Terrain")
	void GenerateTerrain();

private:
	float SampleHeight(float X, float Y, float OffsetX, float OffsetY) const;

	bool IsPointInsideHex(const FVector2D& P, float Radius) const;
};