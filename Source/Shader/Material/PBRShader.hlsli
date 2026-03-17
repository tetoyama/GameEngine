#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

// ------------------------------------------------------------
// 方向ベクトル → Equirectangular UV
// ------------------------------------------------------------
float2 DirToSkyUV(float3 dir)
{
    float phi = atan2(dir.x, dir.z);
    float theta = asin(clamp(dir.y, -1.0f, 1.0f));

    return float2(phi * 0.1591549f + 0.5f, 0.5f - theta * 0.3183098f);
}

// ------------------------------------------------------------
// ワールド座標ノイズ
// ------------------------------------------------------------
float GetNoiseFromWorldPos(float3 worldPos)
{
    float2 seed = worldPos.xy + worldPos.z;
    return frac(52.9829189f * frac(dot(seed, float2(0.06711056f, 0.00583715f))));
}

// ------------------------------------------------------------
// フレネル
// ------------------------------------------------------------
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max((float3) (1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

// ------------------------------------------------------------
// PBR
// ------------------------------------------------------------
float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    float3 N = normalize(materialInput.normal);
    float3 V = normalize(CameraPosition.xyz - materialInput.worldPos);

    float dotNV = saturate(dot(N, V));

    float roughness = saturate(materialInput.Roughness);
    float metallic = saturate(materialInput.Metallic);

    float3 baseColor = materialInput.baseColor.rgb;

    float3 F0 = baseColor * metallic;

    ShadowPCFParams pcf = { 2, 1 };
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    float3 envSpecular = 0;

    float3 F = FresnelSchlickRoughness(dotNV, F0, roughness);

    // ------------------------------------------------
    // Environment reflection
    // ------------------------------------------------
    if ((materialInput.materialFlags & MATERIAL_FLAG_USE_ENVIRONMENT_MAP) &&
        metallic > 0.0f)
    {
        const int SAMPLE_COUNT = 8;
        const float INV_SAMPLE_COUNT = 0.125f;
        const float INV_SQRT_SAMPLE_COUNT = 0.35355339f;

        float3 R = reflect(-V, N);

        float3 up = abs(R.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);

        float3 T = normalize(cross(up, R));
        float3 B = cross(R, T);

        float spread = roughness * roughness;

        float noise = GetNoiseFromWorldPos(materialInput.worldPos);

        float angle = noise * 2.0f * PI;

        float s, c;
        sincos(angle, s, c);

        float3 envAccum = 0;

        [unroll]
        for (int i = 0; i < SAMPLE_COUNT; i++)
        {
            float r = sqrt(i + 0.5f) * INV_SQRT_SAMPLE_COUNT;

            float theta = i * 2.3999632f;

            float2 p = float2(cos(theta), sin(theta)) * r * spread;

            float2 rotatedP = float2(
                p.x * c - p.y * s,
                p.x * s + p.y * c
            );

            float3 sampleDir = normalize(R + T * rotatedP.x + B * rotatedP.y);

            float2 uv = DirToSkyUV(sampleDir);

            envAccum += EnvironmentMap.Sample(EnvSampler, uv).rgb;
        }

        envSpecular = envAccum * INV_SAMPLE_COUNT * F;
    }

    // ------------------------------------------------
    // Energy conservation
    // ------------------------------------------------
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metallic);

    float3 diffuse =
    saturate(lighting.diffuse) * baseColor;

    float3 ambient =
    lighting.ambient * baseColor * (1.0f - metallic);

    float3 litColor =
    diffuse +
    ambient +
    lighting.specular +
    envSpecular;

    float3 emissive =
        materialInput.Emissive.rgb *
        materialInput.Emissive.a;

    return float4(saturate(litColor) + emissive,
                  materialInput.baseColor.a);
}