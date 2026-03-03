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
    float4 Emissive;

    int materialID;
    int objectID;
    int sceneID;
    uint materialFlags;
    
    float2 screenSize;
    float2 screenUV;
};

struct MaterialOutput
{
    float4 color;
    float4 emissive;
    float4 normal;
};

struct ShadowPCFParams
{
    int KernelRadius; // 1 = 3x3, 2 = 5x5
    float StepTexel; // 1.0 = 1ピクセル間隔
};

struct LightingResult
{
    float3 diffuse; // 影込み Diffuse 強度（無色）
    float3 specular; // Specular 強度（無色 or ライト色込み）
    float3 ambient;
};

#endif
