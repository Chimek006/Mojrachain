#include "HexTileWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "GameMapWidget.h"
#include "ProceduralTerrainActor.h"

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
	if (TileState != EHexState::Available || !ParentMapWidget)
	{
		return;
	}

	if (!TerrainClass)
	{
		UE_LOG(LogTemp, Error, TEXT("HexTileWidget: nie ustawiono TerrainClass w WBP_HexTile."));
		return;
	}

	if (!TerrainClass->IsChildOf(AProceduralTerrainActor::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("HexTileWidget: TerrainClass musi dziedziczyć po ProceduralTerrainActor."));
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("Generowanie terenu na kordach: %s"), *GridCoords.ToString());

	if (!ParentMapWidget->GenerateHex(GridCoords, TerrainClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("HexTileWidget: generator nie wygenerował hexa na kordach %s."), *GridCoords.ToString());
		return;
	}

	TileState = EHexState::Generated;
	RefreshVisuals();
	ParentMapWidget->ExpandMap(GridCoords);
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
