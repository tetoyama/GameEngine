#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

// ------------------------------------------------------------
// Direction → SkySphere UV (Equirectangular)
// ------------------------------------------------------------
float2 DirToSkyUV(float3 dir)
{
    float phi = atan2(dir.x, dir.z);
    float theta = asin(clamp(dir.y, -1.0, 1.0));

    float u = phi / (2.0 * PI) + 0.5;
    float v = 0.5 - theta / PI;

    return float2(u, v);
}

// ------------------------------------------------------------
// Poisson Disk Sample Pattern
// ------------------------------------------------------------
static const float2 PoissonDisk[16] =
{
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),
    float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

// ------------------------------------------------------------
// PBR Material
// ------------------------------------------------------------
float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;

    LightingResult lighting =
        ComputeLightingFromMaterialInput(materialInput, pcf);

    float3 baseColor = materialInput.baseColor.rgb;

    float3 directSpecular = lighting.specular;
    float3 envSpecular = 0;

    float envMapFactor = 0.0;

    if (materialInput.materialFlags & MATERIAL_FLAG_USE_ENVIRONMENT_MAP)
    {
        float3 N = normalize(materialInput.normal);
        float3 V = normalize(CameraPosition.xyz - materialInput.worldPos);
        float3 R = reflect(-V, N);

        float roughness = saturate(materialInput.Roughness);

        // ------------------------------------------------
        // Tangent frame
        // ------------------------------------------------
        float3 up = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);

        float3 tangent = normalize(cross(up, N));
        float3 bitangent = cross(N, tangent);

        float radius = roughness;

        float3 envAccum = 0;

        if (roughness <= 0.0)
        {
            float2 envUV = DirToSkyUV(R);
            envAccum = EnvironmentMap.Sample(EnvSampler, envUV).rgb;
        }
        else
        {
            [unroll]
            for (int i = 0; i < 16; i++)
            {
                float2 offset = PoissonDisk[i] * radius;

                float3 sampleDir =
                    normalize(
                        R +
                        tangent * offset.x +
                        bitangent * offset.y
                    );

                float2 envUV = DirToSkyUV(sampleDir);

                float3 envColor =
                    EnvironmentMap.Sample(
                        EnvSampler,
                        envUV
                    ).rgb;

                envAccum += envColor;
            }

            envAccum /= 16.0;
        }

        envMapFactor = materialInput.Metallic;

        // Roughness energy compensation
        float specEnergy = lerp(1.0, 0.4, roughness);

        envSpecular = envAccum * envMapFactor * specEnergy;
    }

    // ------------------------------------------------------------
    // Diffuse lighting
    // ------------------------------------------------------------
    float3 diffuse =
        saturate(lighting.diffuse + lighting.ambient)
        * baseColor
        * (1.0 - envMapFactor);

    // ------------------------------------------------------------
    // Combine lighting
    // ------------------------------------------------------------
    float3 litColor =
        diffuse +
        directSpecular +
        envSpecular;

    // ------------------------------------------------------------
    // Emissive
    // ------------------------------------------------------------
    float3 emissive =
        materialInput.Emissive.rgb *
        materialInput.Emissive.a;

    return float4(
        saturate(litColor) + emissive,
        materialInput.baseColor.a
    );
}