#include "HexTileWidget.h"
#include "GameMapWidget.h" 
#include "Components/Button.h"
#include "ProceduralTerrainActor.h"
#include "Engine/World.h"

void UHexTileWidget::SetupTile(FVector2D InCoords, EHexState InState, UGameMapWidget* InParentMap)
{
	GridCoords = InCoords;
	TileState = InState;
	ParentMapWidget = InParentMap;

	if (HexButton)
	{
		HexButton->OnClicked.RemoveAll(this);
		HexButton->OnClicked.AddDynamic(this, &UHexTileWidget::OnHexClicked);
	}

	UpdateVisuals();
}

void UHexTileWidget::OnHexClicked()
{
	UWorld* World = GetWorld();
	if (TileState == EHexState::Available && World)
	{
		UE_LOG(LogTemp, Warning, TEXT("Generowanie terenu na kordach: %s"), *GridCoords.ToString());
		const float R = 5000.0f;

		float HorizontalSpacing = 1.5f;
		float VerticalSpacing = FMath::Sqrt(2.9584f);
		float RowOffset = 0.5f;

		float WorldX = R * HorizontalSpacing * GridCoords.X;
		float WorldY = R * VerticalSpacing * (GridCoords.Y + GridCoords.X * RowOffset);

		FVector SpawnLocation(WorldX, WorldY, 0.0f);

		if (TerrainClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AProceduralTerrainActor* NewTerrain = World->SpawnActor<AProceduralTerrainActor>(
				TerrainClass,
				SpawnLocation,
				FRotator::ZeroRotator,
				SpawnParams
			);

			if (NewTerrain)
			{
				NewTerrain->GenerateTerrain();

				TileState = EHexState::Generated;
				UpdateVisuals();

				if (ParentMapWidget)
				{
					ParentMapWidget->ExpandMap(GridCoords);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("BŁĄD: Nie ustawiono TerrainClass w WBP_HexTile!"));
		}
	}
}