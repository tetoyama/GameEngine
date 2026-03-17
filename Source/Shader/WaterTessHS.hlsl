struct VS_OUT
{
    float3 LocalPos : TEXCOORD0;
};

struct HS_CONSTANT_DATA_OUT
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
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

float CalcDistanceTessFactor(float3 worldPos)
{
    float dist = distance(CameraPos, worldPos);
    float t = saturate((dist - TessellationMinDistance) / max(0.001f, TessellationMaxDistance - TessellationMinDistance));
    return max(1.0f, lerp(TessellationMax, TessellationMin, t));
}

HS_CONSTANT_DATA_OUT PatchConstantFunc(InputPatch<VS_OUT, 4> patch, uint patchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUT Out;

    float3 p0 = mul(float4(patch[0].LocalPos, 1.0f), WT_World).xyz;
    float3 p1 = mul(float4(patch[1].LocalPos, 1.0f), WT_World).xyz;
    float3 p2 = mul(float4(patch[2].LocalPos, 1.0f), WT_World).xyz;
    float3 p3 = mul(float4(patch[3].LocalPos, 1.0f), WT_World).xyz;

    Out.EdgeTess[0] = CalcDistanceTessFactor((p0 + p1) * 0.5f);
    Out.EdgeTess[1] = CalcDistanceTessFactor((p1 + p3) * 0.5f);
    Out.EdgeTess[2] = CalcDistanceTessFactor((p2 + p3) * 0.5f);
    Out.EdgeTess[3] = CalcDistanceTessFactor((p0 + p2) * 0.5f);

    float inside = CalcDistanceTessFactor((p0 + p1 + p2 + p3) * 0.25f);
    Out.InsideTess[0] = inside;
    Out.InsideTess[1] = inside;
    return Out;
}

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PatchConstantFunc")]
VS_OUT main(InputPatch<VS_OUT, 4> patch, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    return patch[i];
}
