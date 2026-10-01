#include "Biomes/TerrainBiomeTypes.h"

FBiomeTerrainSettings UTerrainBiomeLibrary::GetTerrainBiomeSettings(ETerrainBiome Biome)
{
	FBiomeTerrainSettings Settings;

	switch (Biome)
	{
	case ETerrainBiome::Grassland:
		Settings.HeightScale = 420.0f;
		Settings.NoiseScale = 0.2f;
		Settings.Octaves = 4;
		Settings.Persistence = 0.5f;
		Settings.Lacunarity = 1.95f;
		Settings.HeightPower = 1.4f;
		Settings.DetailStrength = 0.04f;
		Settings.DetailNoiseScale = 1.6f;
		Settings.RidgeStrength = 0.0f;
		Settings.RidgeNoiseScale = 0.8f;
		Settings.MountainSettings.bEnabled = false;
		Settings.SmoothingIterations = 3;
		Settings.SmoothingStrength = 0.48f;
		Settings.EdgeFalloff = 0.0f;
		break;

	case ETerrainBiome::Forest:
		Settings.HeightScale = 540.0f;
		Settings.NoiseScale = 0.34f;
		Settings.Octaves = 5;
		Settings.Persistence = 0.6f;
		Settings.Lacunarity = 2.2f;
		Settings.HeightPower = 1.25f;
		Settings.DetailStrength = 0.08f;
		Settings.DetailNoiseScale = 3.2f;
		Settings.RidgeStrength = 0.08f;
		Settings.RidgeNoiseScale = 1.25f;
		Settings.MountainSettings.bEnabled = false;
		Settings.SmoothingIterations = 0.35f;
		Settings.SmoothingStrength = 0.18f;
		Settings.EdgeFalloff = 0.0f;
		break;

	case ETerrainBiome::Hills:
		Settings.HeightScale = 940.0f;
		Settings.NoiseScale = 0.7f;
		Settings.Octaves = 1;
		Settings.Persistence = 0.48f;
		Settings.Lacunarity = 1.85f;
		Settings.HeightPower = 1.05f;
		Settings.DetailStrength = 0.04f;
		Settings.DetailNoiseScale = 1.25f;
		Settings.RidgeStrength = 0.02f;
		Settings.RidgeNoiseScale = 0.7f;
		Settings.MountainSettings.bEnabled = true;
		Settings.MountainSettings.Strength = 0.08f;
		Settings.MountainSettings.Radius = 0.95f;
		Settings.MountainSettings.Sharpness = 1.1f;
		Settings.MountainSettings.CountMin = 0;
		Settings.MountainSettings.CountMax = 0;
		Settings.SmoothingIterations = 5;
		Settings.SmoothingStrength = 0.64f;
		Settings.EdgeFalloff = 0.0f;
		break;

	case ETerrainBiome::Desert:
		// Keep desert low and relatively calm, but not sunken below the
		// surrounding lowlands when it is blended into the All world.
		Settings.HeightScale = 680.0f;
		Settings.NoiseScale = 0.03f;
		Settings.Octaves = 2;
		Settings.Persistence = 0.75f;
		Settings.Lacunarity = 1.5f;
		Settings.HeightPower = 1.0f;
		Settings.DetailStrength = 0.12f;
		Settings.DetailNoiseScale = 1.0f;
		Settings.RidgeStrength = 0.18f;
		Settings.RidgeNoiseScale = 0.7f;
		Settings.MountainSettings.bEnabled = false;
		Settings.SmoothingIterations = 2;
		Settings.SmoothingStrength = 0.38f;
		Settings.EdgeFalloff = 0.0f;
		break;

	case ETerrainBiome::Mountain:
		Settings.HeightScale = 1400.0f;
		Settings.NoiseScale = 0.08f;
		Settings.Octaves = 3;
		Settings.Persistence = 0.42f;
		Settings.Lacunarity = 1.8f;
		Settings.HeightPower = 2.2f;
		Settings.DetailStrength = 0.055f;
		Settings.DetailNoiseScale = 2.1f;
		Settings.RidgeStrength = 0.24f;
		Settings.RidgeNoiseScale = 1.05f;

		Settings.MountainSettings.bEnabled = true;
		Settings.MountainSettings.Strength = 0.82f;
		Settings.MountainSettings.Radius = 0.48f;
		Settings.MountainSettings.Sharpness = 1.2f;
		Settings.MountainSettings.CountMin = 1;
		Settings.MountainSettings.CountMax = 4;
		Settings.MountainSettings.RadiusMinMultiplier = 0.55f;
		Settings.MountainSettings.RadiusMaxMultiplier = 1.3f;
		Settings.MountainSettings.PlacementRange = 1.25f;
		Settings.MountainSettings.HeightVariation = 0.35f;
		Settings.MountainSettings.JaggedStrength = 0.26f;
		Settings.MountainSettings.JaggedNoiseScale = 3.1f;

		Settings.SmoothingIterations = 0.35f;
		Settings.SmoothingStrength = 0.22f;
		Settings.EdgeFalloff = 0.0f;
		break;

	case ETerrainBiome::Swamp:
		Settings.BaseHeight = -30.0f;
		Settings.HeightScale = 300.0f;
		Settings.NoiseScale = 0.36f;
		Settings.Octaves = 5;
		Settings.Persistence = 0.66f;
		Settings.Lacunarity = 2.05f;
		Settings.HeightPower = 2.1f;
		Settings.DetailStrength = 0.2f;
		Settings.DetailNoiseScale = 2.7f;
		Settings.RidgeStrength = 0.02f;
		Settings.RidgeNoiseScale = 1.0f;
		Settings.MountainSettings.bEnabled = false;
		Settings.SmoothingIterations = 2;
		Settings.SmoothingStrength = 0.28f;
		Settings.EdgeFalloff = 0.0f;
		break;

	case ETerrainBiome::Tundra:
		Settings.HeightScale = 900.0f;
		Settings.NoiseScale = 0.11f;
		Settings.Octaves = 4;
		Settings.Persistence = 0.53f;
		Settings.Lacunarity = 2.35f;
		Settings.HeightPower = 1.5f;
		Settings.DetailStrength = 0.06f;
		Settings.DetailNoiseScale = 1.9f;
		Settings.RidgeStrength = 0.34f;
		Settings.RidgeNoiseScale = 0.95f;
		Settings.MountainSettings.bEnabled = true;
		Settings.MountainSettings.Strength = 0.18f;
		Settings.MountainSettings.Radius = 0.58f;
		Settings.MountainSettings.Sharpness = 2.6f;
		Settings.MountainSettings.CountMin = 0;
		Settings.MountainSettings.CountMax = 0;
		Settings.SmoothingIterations = 1;
		Settings.SmoothingStrength = 0.18f;
		Settings.EdgeFalloff = 0.0f;
		break;

	default:
		break;
	}

	return Settings;
}

FBiomeSurfaceSettings UTerrainBiomeLibrary::GetBiomeSurfaceSettings(ETerrainBiome Biome)
{
	FBiomeSurfaceSettings Settings;

	switch (Biome)
	{
	case ETerrainBiome::Grassland:
		Settings.BaseColor = FLinearColor(0.16f, 0.38f, 0.055f, 1.0f);
		Settings.AccentColor = FLinearColor(0.40f, 0.68f, 0.12f, 1.0f);
		Settings.RockColor = FLinearColor(0.24f, 0.28f, 0.18f, 1.0f);
		Settings.SnowColor = FLinearColor(0.40f, 0.48f, 0.40f, 1.0f);
		Settings.PatternScale = 0.0028f;
		Settings.Roughness = 0.86f;
		Settings.RockStart = 0.82f;
		Settings.RockEnd = 0.98f;
		Settings.SnowStart = 1.01f;
		Settings.SnowEnd = 1.02f;
		Settings.SlopeRockStrength = 0.18f;
		break;

	case ETerrainBiome::Forest:
		Settings.BaseColor = FLinearColor(0.075f, 0.22f, 0.045f, 1.0f);
		Settings.AccentColor = FLinearColor(0.20f, 0.38f, 0.075f, 1.0f);
		Settings.RockColor = FLinearColor(0.18f, 0.20f, 0.16f, 1.0f);
		Settings.SnowColor = FLinearColor(0.36f, 0.42f, 0.38f, 1.0f);
		Settings.PatternScale = 0.0032f;
		Settings.Roughness = 0.90f;
		Settings.RockStart = 0.76f;
		Settings.RockEnd = 0.95f;
		Settings.SnowStart = 1.01f;
		Settings.SnowEnd = 1.02f;
		Settings.SlopeRockStrength = 0.24f;
		break;

	case ETerrainBiome::Hills:
		Settings.BaseColor = FLinearColor(0.22f, 0.42f, 0.09f, 1.0f);
		Settings.AccentColor = FLinearColor(0.48f, 0.58f, 0.16f, 1.0f);
		Settings.RockColor = FLinearColor(0.34f, 0.36f, 0.29f, 1.0f);
		Settings.SnowColor = FLinearColor(0.52f, 0.57f, 0.54f, 1.0f);
		Settings.PatternScale = 0.0022f;
		Settings.Roughness = 0.84f;
		Settings.RockStart = 0.48f;
		Settings.RockEnd = 0.72f;
		Settings.SnowStart = 1.01f;
		Settings.SnowEnd = 1.02f;
		Settings.SlopeRockStrength = 0.38f;
		break;

	case ETerrainBiome::Desert:
		Settings.BaseColor = FLinearColor(0.70f, 0.34f, 0.075f, 1.0f);
		Settings.AccentColor = FLinearColor(1.0f, 0.68f, 0.20f, 1.0f);
		Settings.RockColor = FLinearColor(0.48f, 0.31f, 0.16f, 1.0f);
		Settings.SnowColor = FLinearColor(0.68f, 0.55f, 0.36f, 1.0f);
		Settings.PatternScale = 0.0045f;
		Settings.Roughness = 0.94f;
		Settings.RockStart = 0.60f;
		Settings.RockEnd = 0.86f;
		Settings.SnowStart = 1.01f;
		Settings.SnowEnd = 1.02f;
		Settings.SlopeRockStrength = 0.30f;
		break;

	case ETerrainBiome::Mountain:
		Settings.BaseColor = FLinearColor(0.16f, 0.34f, 0.075f, 1.0f);
		Settings.AccentColor = FLinearColor(0.38f, 0.58f, 0.14f, 1.0f);
		Settings.RockColor = FLinearColor(0.25f, 0.27f, 0.28f, 1.0f);
		Settings.SnowColor = FLinearColor(0.92f, 0.96f, 1.0f, 1.0f);
		Settings.PatternScale = 0.0038f;
		Settings.Roughness = 0.92f;
		Settings.RockStart = 0.34f;
		Settings.RockEnd = 0.61f;
		Settings.SnowStart = 0.70f;
		Settings.SnowEnd = 0.88f;
		Settings.SlopeRockStrength = 0.72f;
		break;

	case ETerrainBiome::Swamp:
		Settings.BaseColor = FLinearColor(0.055f, 0.16f, 0.14f, 1.0f);
		Settings.AccentColor = FLinearColor(0.27f, 0.22f, 0.09f, 1.0f);
		Settings.RockColor = FLinearColor(0.18f, 0.20f, 0.15f, 1.0f);
		Settings.SnowColor = FLinearColor(0.34f, 0.40f, 0.35f, 1.0f);
		Settings.PatternScale = 0.0036f;
		Settings.Roughness = 0.68f;
		Settings.RockStart = 0.78f;
		Settings.RockEnd = 0.98f;
		Settings.SnowStart = 1.01f;
		Settings.SnowEnd = 1.02f;
		Settings.SlopeRockStrength = 0.18f;
		break;

	case ETerrainBiome::Tundra:
		Settings.BaseColor = FLinearColor(0.66f, 0.77f, 0.80f, 1.0f);
		Settings.AccentColor = FLinearColor(0.96f, 0.99f, 1.0f, 1.0f);
		Settings.RockColor = FLinearColor(0.39f, 0.45f, 0.48f, 1.0f);
		Settings.SnowColor = FLinearColor(0.96f, 0.99f, 1.0f, 1.0f);
		Settings.PatternScale = 0.0030f;
		Settings.Roughness = 0.96f;
		Settings.RockStart = 0.30f;
		Settings.RockEnd = 0.58f;
		Settings.SnowStart = 0.44f;
		Settings.SnowEnd = 0.66f;
		Settings.SlopeRockStrength = 0.48f;
		break;

	default:
		break;
	}

	return Settings;
}
