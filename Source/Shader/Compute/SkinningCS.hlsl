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
    float4 Diffuse;
};

struct OutputVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float4 Diffuse;
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

    float4 srcPos = float4(v.Position, 1.0f);
    float3 srcNor = v.Normal;

    // ----------------------------------------
    // BoneWeight 正規化（完全安全）
    // ----------------------------------------
    float weightSum =
        v.BoneWeight.x +
        v.BoneWeight.y +
        v.BoneWeight.z +
        v.BoneWeight.w;

    float4 weights = v.BoneWeight;
    if (weightSum > 0.0f)
    {
        weights /= weightSum;
    }
    else
    {
        // スキニングされない頂点対策
        OutputVertex outv;
        outv.Position = v.Position;
        outv.Normal = normalize(v.Normal);
        outv.Tangent = float3(1.0f, 0.0f, 0.0f);
        outv.Diffuse = v.Diffuse;
        outv.TexCoord = v.TexCoord;
        g_Output[id] = outv;
        return;
    }

    float4 skinnedPos = float4(0, 0, 0, 0);
    float3 skinnedNor = float3(0, 0, 0);
    bool validSkin = false;

    // ----------------------------------------
    // スキニング本体
    // ----------------------------------------
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float w = weights[i];
        if (w <= 0.0f)
            continue;

        uint idx = v.BoneIndex[i];
        if (idx >= BONE_MAX_COUNT)
            continue;

        float4x4 m = g_BoneMatrix[idx];

        // Position
        skinnedPos += mul(srcPos, m) * w;

        // Normal（逆転置行列：安全側）
        float3x3 nrmMat = (float3x3) m;
        skinnedNor += mul(srcNor, nrmMat) * w;

        validSkin = true;
    }

    // ----------------------------------------
    // フォールバック（異常防止）
    // ----------------------------------------
    if (!validSkin)
    {
        skinnedPos = srcPos;
        skinnedNor = srcNor;
    }

    skinnedNor = normalize(skinnedNor);

    // ----------------------------------------
    // 出力
    // ----------------------------------------
    OutputVertex outv;
    outv.Position = skinnedPos.xyz;
    outv.Normal = skinnedNor;
    outv.Tangent = float3(1.0f, 0.0f, 0.0f);
    outv.Diffuse = v.Diffuse;
    outv.TexCoord = v.TexCoord;

    g_Output[id] = outv;
}
