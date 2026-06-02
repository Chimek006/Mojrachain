#include "HexTileWidget.h"
#include "GameMapWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "ProceduralTerrainActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UHexTileWidget::SetupTile(FVector2D InCoords, EHexState InState, UGameMapWidget* InParentMap)
{
	GridCoords = InCoords;
	TileState = InState;
	ParentMapWidget = InParentMap;

	if (HexButton)
	{
		ApplyTransparentButtonStyle();
		HexButton->OnClicked.RemoveAll(this);
		HexButton->OnClicked.AddDynamic(this, &UHexTileWidget::OnHexClicked);
	}

	RefreshVisuals();
}

void UHexTileWidget::OnHexClicked()
{
	UWorld* World = GetWorld();

	if (TileState == EHexState::Available && World)
	{
		UE_LOG(LogTemp, Warning, TEXT("Generowanie terenu na kordach: %s"), *GridCoords.ToString());

		if (TerrainClass)
		{
			if (!TerrainClass->IsChildOf(AProceduralTerrainActor::StaticClass()))
			{
				UE_LOG(LogTemp, Error, TEXT("BŁĄD: TerrainClass musi dziedziczyć po ProceduralTerrainActor."));
				return;
			}

			TArray<AActor*> FoundTerrainActors;
			UGameplayStatics::GetAllActorsOfClass(World, AProceduralTerrainActor::StaticClass(), FoundTerrainActors);

			AProceduralTerrainActor* SourceTerrain = nullptr;

			for (AActor* Actor : FoundTerrainActors)
			{
				AProceduralTerrainActor* TerrainActor = Cast<AProceduralTerrainActor>(Actor);

				if (TerrainActor && TerrainActor->ActorHasTag(TEXT("TerrainSettingsSource")))
				{
					SourceTerrain = TerrainActor;
					break;
				}
			}

			const AProceduralTerrainActor* TerrainDefaults = TerrainClass.GetDefaultObject();
			const float TerrainSize = SourceTerrain ? SourceTerrain->Size : (TerrainDefaults ? TerrainDefaults->Size : 10000.0f);
			const int32 TerrainResolution = SourceTerrain ? SourceTerrain->Resolution : (TerrainDefaults ? TerrainDefaults->Resolution : 100);
			const int32 SafeResolution = FMath::Max(TerrainResolution, 2);
			const float TerrainStep = TerrainSize / SafeResolution;

			const float HorizontalOffset = FMath::FloorToFloat(0.75f * SafeResolution) * TerrainStep;
			const float DiagonalYOffset = FMath::FloorToFloat((FMath::Sqrt(3.0f) * 0.25f) * SafeResolution) * TerrainStep;
			const float VerticalOffset = DiagonalYOffset * 2.0f;

			const float WorldX = HorizontalOffset * GridCoords.X;
			const float WorldY = VerticalOffset * GridCoords.Y + DiagonalYOffset * GridCoords.X;

			const FVector SpawnOrigin = SourceTerrain ? SourceTerrain->GetActorLocation() : FVector::ZeroVector;
			const FVector SpawnLocation = SpawnOrigin + FVector(WorldX, WorldY, 0.0f);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AProceduralTerrainActor* NewTerrain = World->SpawnActor<AProceduralTerrainActor>(
				TerrainClass,
				SpawnLocation,
				FRotator::ZeroRotator,
				SpawnParams
			);
			AActor* SpawnedActor = NewTerrain;

			if (NewTerrain)
			{
				if (SourceTerrain)
				{
					NewTerrain->CopySettingsFrom(SourceTerrain);
				}

				if (ParentMapWidget)
				{
					ParentMapWidget->HandleGeneratedTerrain(NewTerrain);
				}

				NewTerrain->GenerateTerrain();

				TileState = EHexState::Generated;
				RefreshVisuals();

				if (ParentMapWidget)
				{
					ParentMapWidget->ExpandMap(GridCoords);
				}
			}
			else if (SpawnedActor)
			{
				UE_LOG(LogTemp, Error, TEXT("BŁĄD: Zespawnowany aktor nie jest ProceduralTerrainActor."));
				SpawnedActor->Destroy();
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("BŁĄD: Nie ustawiono TerrainClass w WBP_HexTile!"));
		}
	}
}

void UHexTileWidget::ApplyTransparentButtonStyle()
{
	if (!HexButton)
	{
		return;
	}

	FButtonStyle TransparentStyle = HexButton->GetStyle();
	const FSlateColor TransparentColor(FLinearColor::Transparent);

	TransparentStyle.Normal.TintColor = TransparentColor;
	TransparentStyle.Hovered.TintColor = TransparentColor;
	TransparentStyle.Pressed.TintColor = TransparentColor;
	TransparentStyle.Disabled.TintColor = TransparentColor;

	HexButton->SetStyle(TransparentStyle);
	HexButton->SetBackgroundColor(FLinearColor::Transparent);
}

void UHexTileWidget::RefreshVisuals()
{
	const FLinearColor TileColor = TileState == EHexState::Generated
		? FLinearColor(0.1f, 0.8f, 0.18f, 0.85f)
		: FLinearColor(0.95f, 0.9f, 0.1f, 0.78f);

	if (HexImage)
	{
		HexImage->SetColorAndOpacity(TileColor);
	}

	if (HexButton)
	{
		ApplyTransparentButtonStyle();
	}
}
