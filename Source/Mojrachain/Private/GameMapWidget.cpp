#include "GameMapWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetSwitcher.h"
#include "HexTileWidget.h"
#include "ProceduralTerrainActor.h"
#include "World/WorldMapSubsystem.h"

void UGameMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Map)
	{
		Btn_Map->OnClicked.AddDynamic(this, &UGameMapWidget::ShowMap);
	}
	if (Btn_Inventory)
	{
		Btn_Inventory->OnClicked.AddDynamic(this, &UGameMapWidget::ShowInventory);
	}
	if (Btn_Settings)
	{
		Btn_Settings->OnClicked.AddDynamic(this, &UGameMapWidget::ShowSettings);
	}

	BuildHexMap();
}

void UGameMapWidget::ShowMap()
{
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(0);
	}
}

void UGameMapWidget::ShowInventory()
{
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(1);
	}
}

void UGameMapWidget::ShowSettings()
{
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(2);
	}
}

UWorldMapSubsystem* UGameMapWidget::ResolveWorldMapSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UWorldMapSubsystem>() : nullptr;
}

FWorldMapGenerationSettings UGameMapWidget::BuildWorldGenerationSettings() const
{
	FWorldMapGenerationSettings Settings;
	Settings.WorldSeed = WorldSeed;
	Settings.BiomeGenerationMode = BiomeGenerationMode;
	Settings.BoundaryInset = BoundaryInset;
	Settings.RiverWidthMin = RiverWidthMin;
	Settings.RiverWidthMax = RiverWidthMax;
	Settings.RiverDepthMin = RiverDepthMin;
	Settings.RiverDepthMax = RiverDepthMax;
	Settings.RiverDistanceMin = RiverDistanceMin;
	Settings.RiverDistanceMax = RiverDistanceMax;
	Settings.RiverClearance = RiverClearance;
	return Settings;
}

void UGameMapWidget::BuildHexMap()
{
	if (!MapCanvas || !HexTileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMapWidget: MapCanvas albo HexTileClass nie jest ustawione."));
		return;
	}

	UWorldMapSubsystem* WorldGenerator = ResolveWorldMapSubsystem();
	if (!WorldGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMapWidget: nie znaleziono UWorldMapSubsystem dla aktualnego świata."));
		return;
	}

	WorldGenerator->Configure(BuildWorldGenerationSettings());
	WorldGenerator->InitializeWorld(WorldGenerator->FindTerrainSettingsSource());

	MapCanvas->ClearChildren();

	const FVector2D Center(0.0f, 0.0f);
	SpawnHexUI(Center, EHexState::Generated);
	ExpandMap(Center);
}

void UGameMapWidget::ExpandMap(FVector2D CenterCoords)
{
	UWorldMapSubsystem* WorldGenerator = ResolveWorldMapSubsystem();
	if (!WorldGenerator)
	{
		return;
	}

	const FIntPoint CenterCoordinates(
		FMath::RoundToInt(CenterCoords.X),
		FMath::RoundToInt(CenterCoords.Y)
	);

	const TArray<FIntPoint> AddedCoordinates = WorldGenerator->ExpandHex(CenterCoordinates);
	for (const FIntPoint& Coordinates : AddedCoordinates)
	{
		SpawnHexUI(
			FVector2D(static_cast<float>(Coordinates.X), static_cast<float>(Coordinates.Y)),
			WorldGenerator->GetHexState(Coordinates)
		);
	}
}

bool UGameMapWidget::GenerateHex(FVector2D Coordinates, TSubclassOf<AProceduralTerrainActor> TerrainClass)
{
	UWorldMapSubsystem* WorldGenerator = ResolveWorldMapSubsystem();
	if (!WorldGenerator)
	{
		return false;
	}

	const FIntPoint HexCoordinates(
		FMath::RoundToInt(Coordinates.X),
		FMath::RoundToInt(Coordinates.Y)
	);

	AProceduralTerrainActor* GeneratedTerrain = nullptr;
	return WorldGenerator->GenerateHex(HexCoordinates, TerrainClass, GeneratedTerrain);
}

void UGameMapWidget::HandleGeneratedTerrain(AProceduralTerrainActor* Terrain)
{
	// Kept for compatibility with older Widget Blueprints. New hexes are
	// registered and configured by UWorldMapSubsystem::GenerateHex.
	if (Terrain)
	{
		UE_LOG(LogTemp, Verbose, TEXT("GameMapWidget: HandleGeneratedTerrain jest przestarzałe dla %s."), *Terrain->GetName());
	}
}

UHexTileWidget* UGameMapWidget::SpawnHexUI(FVector2D Coords, EHexState State)
{
	if (!MapCanvas || !HexTileClass)
	{
		return nullptr;
	}

	UHexTileWidget* NewHex = CreateWidget<UHexTileWidget>(this, HexTileClass);
	if (!NewHex)
	{
		return nullptr;
	}

	MapCanvas->AddChild(NewHex);
	NewHex->SetupTile(Coords, State, this);

	constexpr float HexRadius = 50.0f;
	constexpr float CanvasOffsetX = 640.0f;
	constexpr float CanvasOffsetY = 360.0f;

	const float PosX = HexRadius * 1.5f * Coords.X;
	const float PosY = HexRadius * FMath::Sqrt(3.0f) * (Coords.Y + Coords.X * 0.5f);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(NewHex->Slot))
	{
		CanvasSlot->SetPosition(FVector2D(PosX + CanvasOffsetX, PosY + CanvasOffsetY));
		CanvasSlot->SetSize(FVector2D(HexRadius * 2.0f, HexRadius * 2.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	}

	return NewHex;
}
