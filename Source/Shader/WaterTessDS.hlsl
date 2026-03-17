struct VS_OUT
{
    float3 LocalPos : TEXCOORD0;
};

struct HS_CONSTANT_DATA_OUT
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

struct DS_OUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : TEXCOORD1;
};

cbuffer WaterTessellationCB : register(b3)
{
    float4x4 WT_World;
    float4x4 WT_View;
    float4x4 WT_Projection;
    float3 CameraPos;
    float Time;
    float TessellationMin;
    float TessellationMax;
    float TessellationMinDistance;
    float TessellationMaxDistance;
    float WaveHeight;
    float WaveScale;
    float2 FlowDir1;
    float2 FlowDir2;
    float FlowSpeed1;
    float FlowSpeed2;
    float NormalDelta;
    float FresnelPower;
};

Texture2D HeightTexture : register(t5); // TextureSlot_HeightMap(=5) のグレースケール高さマップ
SamplerState LinearSampler : register(s0);
static const float kLayer2Scale = 1.37f;
static const float2 kLayer2Offset = float2(17.31f, -23.79f);

float SampleWaveHeight(float2 worldXZ)
{
    float2 uvBase = worldXZ * WaveScale;
    float2 uv1 = uvBase + FlowDir1 * Time * FlowSpeed1;
    float2 uv2 = uvBase * kLayer2Scale + kLayer2Offset + FlowDir2 * Time * FlowSpeed2;
    float h1 = HeightTexture.SampleLevel(LinearSampler, uv1, 0).r;
    float h2 = HeightTexture.SampleLevel(LinearSampler, uv2, 0).r;
    return ((h1 + h2) - 1.0f) * WaveHeight;
}

[domain("quad")]
DS_OUT main(HS_CONSTANT_DATA_OUT input, float2 uv : SV_DomainLocation, const OutputPatch<VS_OUT, 4> patch)
{
    DS_OUT Out;

    float3 p00 = patch[0].LocalPos;
    float3 p10 = patch[1].LocalPos;
    float3 p01 = patch[2].LocalPos;
    float3 p11 = patch[3].LocalPos;

    float3 localPos = lerp(lerp(p00, p10, uv.x), lerp(p01, p11, uv.x), uv.y);
    float3 worldPos = mul(float4(localPos, 1.0f), WT_World).xyz;

    float h = SampleWaveHeight(worldPos.xz);
    worldPos.y += h;

    float hx = SampleWaveHeight(worldPos.xz + float2(NormalDelta, 0.0f));
    float hz = SampleWaveHeight(worldPos.xz + float2(0.0f, NormalDelta));
    float3 dx = float3(NormalDelta, hx - h, 0.0f);
    float3 dz = float3(0.0f, hz - h, NormalDelta);
    float3 normal = normalize(cross(dz, dx));

    float4 viewPos = mul(float4(worldPos, 1.0f), WT_View);
    Out.Position = mul(viewPos, WT_Projection);
    Out.WorldPosition = worldPos;
    Out.Normal = normal;
    return Out;
}
