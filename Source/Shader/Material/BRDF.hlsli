#ifndef MATERIAL_BRDF_HLSLI
#define MATERIAL_BRDF_HLSLI

// Pure material math shared by the existing HLSL shaders and portable GLSL.
// The GLSL caller supplies float3 / saturate aliases and the same PI constant.
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float SchlickGGX(float NdotV, float roughness)
{
    float k = (roughness * roughness + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float3 N, float3 V, float3 L, float roughness)
{
    return SchlickGGX(saturate(dot(N, V)), roughness) *
           SchlickGGX(saturate(dot(N, L)), roughness);
}

float D_GTR2(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float denominator = PI * d * d;
    // Roughness=0 is allowed by the existing material UI. Avoid 0/0 at
    // the singular perfect-mirror direction in both rendering paths.
    return denominator > 0.0 ? a2 / denominator : 0.0;
}
#endif
