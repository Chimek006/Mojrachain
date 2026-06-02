#include "ProceduralMountainActor.h"

AProceduralMountainActor::AProceduralMountainActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}
