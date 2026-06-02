#include "ProceduralRiverActor.h"

AProceduralRiverActor::AProceduralRiverActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}
