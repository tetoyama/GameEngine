#include "../commonDefine.h"

cbuffer CB_Bones : register(b0)
{
    float4x4 g_BoneMatrix[BONE_MAX_COUNT];
};

cbuffer CB_Info : register(b1)
{
    uint g_VertexCount;
    uint padding0;
    uint padding1;
    uint padding2;
};

struct InputVertex
{
    float3 Position;
    float3 Normal;
    float2 TexCoord;
    uint4 BoneIndex;
    float4 BoneWeight;
    float4 Diffuse; // 追加
};

struct OutputVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent; // VERTEX_3D に対応する場合追加
    float4 Diffuse; // 追加
    float2 TexCoord;
};

StructuredBuffer<InputVertex> g_Input : register(t0);
RWStructuredBuffer<OutputVertex> g_Output : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint id = dtid.x;
    if (id >= g_VertexCount)
        return;

    InputVertex v = g_Input[id];

    float4 pos = float4(v.Position, 1.0f);
    float3 nor = v.Normal;

    float4 skinnedPos = 0;
    float3 skinnedNor = 0;

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float w = v.BoneWeight[i];
        if (w > 0.0f)
        {
            uint idx = v.BoneIndex[i];
            if (idx < BONE_MAX_COUNT)
            {
                float4x4 m = g_BoneMatrix[idx];
                skinnedPos += mul(pos, m) * w;
                skinnedNor += mul(nor, (float3x3) m) * w;
            }
        }
    }

    skinnedNor = normalize(skinnedNor);

    OutputVertex outv;
    outv.Position = skinnedPos.xyz;
    outv.Normal = skinnedNor;
    outv.Tangent = float3(1.0f, 0.0f, 0.0f); // 固定値
    outv.Diffuse = v.Diffuse; // 入力から受け渡す
    outv.TexCoord = v.TexCoord;

    g_Output[id] = outv;
}
