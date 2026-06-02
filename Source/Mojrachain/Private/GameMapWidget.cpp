#include "GameMapWidget.h"
#include "HexTileWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetSwitcher.h"
#include "ProceduralTerrainActor.h"
#include "Kismet/GameplayStatics.h"

namespace
{
struct FTrackedTerrainRiver
{
	FGeneratedTerrainRiverPath Path;
	FRiverTerrainSettings Settings;
};

struct FAffectedTerrainUpdate
{
	AProceduralTerrainActor* Terrain = nullptr;
	float DistanceSq = 0.0f;
};

TMap<uint32, TArray<FTrackedTerrainRiver>> GTrackedTerrainRiversByMap;

uint32 GetMapTrackingKey(const UGameMapWidget* MapWidget)
{
	return MapWidget ? MapWidget->GetUniqueID() : 0;
}

AProceduralTerrainActor* FindTerrainSettingsSource(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> TerrainActors;
	UGameplayStatics::GetAllActorsOfClass(World, AProceduralTerrainActor::StaticClass(), TerrainActors);

	for (AActor* Actor : TerrainActors)
	{
		AProceduralTerrainActor* Terrain = Cast<AProceduralTerrainActor>(Actor);
		if (Terrain && Terrain->ActorHasTag(TEXT("TerrainSettingsSource")))
		{
			return Terrain;
		}
	}

	return nullptr;
}

bool IsPointInsideExpandedHex(const FVector2D& Point, float Radius)
{
	const float X = FMath::Abs(Point.X);
	const float Y = FMath::Abs(Point.Y);

	const float HexDistance = FMath::Max(
		X / Radius,
		(0.5f * X + 0.8660254f * Y) / Radius
	);

	return HexDistance <= 1.0f;
}

float DistanceSquaredToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
{
	const FVector2D Segment = B - A;
	const float SegmentLengthSq = Segment.SizeSquared();

	if (SegmentLengthSq <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::DistSquared(Point, A);
	}

	const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, Segment) / SegmentLengthSq, 0.0f, 1.0f);
	const FVector2D Closest = A + Segment * T;
	return FVector2D::DistSquared(Point, Closest);
}

bool DoesRiverAffectTerrain(const AProceduralTerrainActor* Terrain, const FGeneratedTerrainRiverPath& RiverPath)
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
			const FVector2D Point = FMath::Lerp(A, B, T);

			if (IsPointInsideExpandedHex(Point - TerrainCenter, ExpandedHexRadius))
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

bool ApplyExistingRiverToTerrain(UGameMapWidget* MapWidget, AProceduralTerrainActor* Terrain)
{
	if (!MapWidget
		|| !Terrain
		|| Terrain->Biome == ETerrainBiome::Desert
		|| Terrain->ActorHasTag(TEXT("TerrainSettingsSource")))
	{
		return false;
	}

	const TArray<FTrackedTerrainRiver>* TrackedRivers = GTrackedTerrainRiversByMap.Find(GetMapTrackingKey(MapWidget));
	if (!TrackedRivers)
	{
		return false;
	}

	bool bAppliedAnyRiver = false;

	for (const FTrackedTerrainRiver& River : *TrackedRivers)
	{
		if (DoesRiverAffectTerrain(Terrain, River.Path))
		{
			Terrain->AddRiver(River.Settings);
			bAppliedAnyRiver = true;
		}
	}

	return bAppliedAnyRiver;
}

void ApplyRiverToAlreadyGeneratedTerrains(
	UGameMapWidget* MapWidget,
	AProceduralTerrainActor* SourceTerrain,
	const FTrackedTerrainRiver& River
)
{
	if (!MapWidget || !SourceTerrain)
	{
		return;
	}

	UWorld* World = SourceTerrain->GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> TerrainActors;
	UGameplayStatics::GetAllActorsOfClass(World, AProceduralTerrainActor::StaticClass(), TerrainActors);

	TArray<FAffectedTerrainUpdate> AffectedTerrains;
	const FVector SourceLocation = SourceTerrain->GetActorLocation();

	for (AActor* Actor : TerrainActors)
	{
		AProceduralTerrainActor* Terrain = Cast<AProceduralTerrainActor>(Actor);
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
}

void UGameMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Btn_Map) Btn_Map->OnClicked.AddDynamic(this, &UGameMapWidget::ShowMap);
	if (Btn_Inventory) Btn_Inventory->OnClicked.AddDynamic(this, &UGameMapWidget::ShowInventory);
	if (Btn_Settings) Btn_Settings->OnClicked.AddDynamic(this, &UGameMapWidget::ShowSettings);

	BuildHexMap();
}

void UGameMapWidget::ShowMap() { ContentSwitcher->SetActiveWidgetIndex(0); }
void UGameMapWidget::ShowInventory() { ContentSwitcher->SetActiveWidgetIndex(1); }
void UGameMapWidget::ShowSettings() { ContentSwitcher->SetActiveWidgetIndex(2); }

void UGameMapWidget::BuildHexMap()
{
	if (!MapCanvas || !HexTileClass) return;

	MapCanvas->ClearChildren();
	MapState.Empty();
	GeneratedRiverPaths.Empty();
	GTrackedTerrainRiversByMap.FindOrAdd(GetMapTrackingKey(this)).Reset();
	GeneratedNonDesertTilesSinceRiver = 0;
	NextRiverDistance = 0;
	bForceNextRiver = true;

	FVector2D Center(0, 0);
	MapState.Add(Center, EHexState::Generated);
	SpawnHexUI(Center, EHexState::Generated);

	ExpandMap(Center);
}

void UGameMapWidget::ExpandMap(FVector2D CenterCoords)
{
	TArray<FVector2D> Directions = {
		FVector2D(1, 0), FVector2D(1, -1), FVector2D(0, -1),
		FVector2D(-1, 0), FVector2D(-1, 1), FVector2D(0, 1)
	};

	for (const FVector2D& Dir : Directions)
	{
		FVector2D NeighborCoords = CenterCoords + Dir;

		if (!MapState.Contains(NeighborCoords))
		{
			MapState.Add(NeighborCoords, EHexState::Available);
			SpawnHexUI(NeighborCoords, EHexState::Available);
		}
	}
}

void UGameMapWidget::HandleGeneratedTerrain(AProceduralTerrainActor* Terrain)
{
	if (!Terrain)
	{
		return;
	}

	if (ApplyExistingRiverToTerrain(this, Terrain))
	{
		return;
	}

	if (ShouldSpawnRiverForTerrain(Terrain))
	{
		if (!ConfigureRiverForTerrain(Terrain))
		{
			bForceNextRiver = true;
		}
		return;
	}

	Terrain->ClearRiver();
}

bool UGameMapWidget::ShouldSpawnRiverForTerrain(const AProceduralTerrainActor* Terrain)
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

bool UGameMapWidget::ConfigureRiverForTerrain(AProceduralTerrainActor* Terrain)
{
	if (!Terrain)
	{
		return false;
	}

	AProceduralTerrainActor* SettingsSource = FindTerrainSettingsSource(Terrain->GetWorld());
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

	FGeneratedTerrainRiverPath GeneratedRiverPath;
	if (!Terrain->ConfigureRiver(
		RiverWidthMin,
		RiverWidthMax,
		RiverDepthMin,
		RiverDepthMax,
		RiverClearance,
		GeneratedRiverPaths,
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

	GeneratedRiverPaths.Add(GeneratedRiverPath);

	FTrackedTerrainRiver TrackedRiver;
	TrackedRiver.Path = GeneratedRiverPath;
	TrackedRiver.Settings = Terrain->RiverSettings;

	GTrackedTerrainRiversByMap.FindOrAdd(GetMapTrackingKey(this)).Add(TrackedRiver);
	ApplyRiverToAlreadyGeneratedTerrains(this, Terrain, TrackedRiver);

	return true;
}

void UGameMapWidget::RollNextRiverDistance()
{
	const int32 MinDistance = FMath::Max(1, FMath::Min(RiverDistanceMin, RiverDistanceMax));
	const int32 MaxDistance = FMath::Max(MinDistance, RiverDistanceMax);
	NextRiverDistance = FMath::RandRange(MinDistance, MaxDistance);
}

UHexTileWidget* UGameMapWidget::SpawnHexUI(FVector2D Coords, EHexState State)
{
	UHexTileWidget* NewHex = CreateWidget<UHexTileWidget>(this, HexTileClass);
	if (NewHex)
	{
		MapCanvas->AddChild(NewHex);

		NewHex->SetupTile(Coords, State, this);

		float R = 50.0f;
		float OffsetX = 640.0f;
		float OffsetY = 360.0f;

		float PosX = R * 1.5f * Coords.X;
		float PosY = R * FMath::Sqrt(3.0f) * (Coords.Y + Coords.X * 0.5f);

		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(NewHex->Slot);
		if (CanvasSlot)
		{
			CanvasSlot->SetPosition(FVector2D(PosX + OffsetX, PosY + OffsetY));
			CanvasSlot->SetSize(FVector2D(R * 2, R * 2));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
	}
	return NewHex;
}
