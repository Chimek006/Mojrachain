#include "GameMapWidget.h"
#include "HexTileWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetSwitcher.h"

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