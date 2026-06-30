#ifndef SHADOW_ATLAS_SAMPLING_HLSLI
#define SHADOW_ATLAS_SAMPLING_HLSLI

// Samples one logical shadow tile. The input UV is local to the tile [0, 1].
// PCF offsets are expressed in atlas texels because the transformed sampling
// coordinates are already in atlas UV space.
float SampleShadowAtlasPCF(
    float2 tileUv,
    float depth,
    int tileIndex,
    ShadowPCFParams pcf)
{
    if (ShadowAtlasCount <= 0 || tileIndex < 0 || tileIndex >= ShadowAtlasCount)
        return 1.0;

    const uint grid = max((uint) ceil(sqrt((float) ShadowAtlasCount)), 1u);
    const float tileSize = 1.0 / grid;
    const uint tileX = tileIndex % grid;
    const uint tileY = tileIndex / grid;
    const float2 tileMin = float2(tileX, tileY) * tileSize;
    const float2 tileMax = tileMin + tileSize;
    const float2 sampleCenter = tileMin + tileUv * tileSize;

    uint atlasWidth, atlasHeight;
    ShadowMap.GetDimensions(atlasWidth, atlasHeight);
    const float2 atlasTexelSize =
        float2(1.0 / atlasWidth, 1.0 / atlasHeight);
    const float2 safeTileMin = tileMin + atlasTexelSize * 0.5;
    const float2 safeTileMax = tileMax - atlasTexelSize * 0.5;

    const int radius = max(pcf.KernelRadius, 0);
    const float stepTexel = max(pcf.StepTexel, 0.0);
    float shadow = 0.0;
    int sampleCount = 0;

    [loop]
    for (int sampleY = -radius; sampleY <= radius; ++sampleY)
    {
        [loop]
        for (int sampleX = -radius; sampleX <= radius; ++sampleX)
        {
            float2 sampleUv = sampleCenter +
                float2(sampleX, sampleY) * atlasTexelSize * stepTexel;

            // D3D11 sampler address clamp only protects the atlas boundary.
            // Clamp explicitly to the logical tile to prevent Point faces or
            // CSM cascades from reading a neighbouring tile.
            sampleUv = clamp(sampleUv, safeTileMin, safeTileMax);

            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                sampleUv,
                depth);
            ++sampleCount;
        }
    }

    return shadow / max(sampleCount, 1);
}

#endif // SHADOW_ATLAS_SAMPLING_HLSLI
