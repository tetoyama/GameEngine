#ifndef MATERIAL_DEFINE_HLSLI
#define MATERIAL_DEFINE_HLSLI

struct MaterialInput
{
    float2 uv;

    float4 baseColor;
    float3 normal;
    float3 worldPos;
    float3 viewDir;

    float Metallic;
    float Roughness;
    float AO;
    float Shininess;

    int materialID;
    int objectID;
    int sceneID;
};

struct LightingResult
{
    float3 diffuse; // 影込み Diffuse 強度（無色）
    float3 specular; // Specular 強度（無色 or ライト色込み）
    float3 ambient;
};

#endif
