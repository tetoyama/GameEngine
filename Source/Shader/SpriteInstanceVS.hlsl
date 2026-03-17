#include "common.hlsl"

struct VS_IN_INSTANCE
{
    float4 Position : POSITION;
    float3 Normal   : NORMAL;
    float3 Tangent  : TANGENT;
    float4 Diffuse  : COLOR;
    float2 TexCoord : TEXCOORD0;

    float4 Row0 : TEXCOORD1;
    float4 Row1 : TEXCOORD2;
    float4 Row2 : TEXCOORD3;
    float4 Row3 : TEXCOORD4;
};

void main(in VS_IN_INSTANCE In, out PS_IN Out)
{
    float4x4 InstanceWorld = float4x4(
        In.Row0,
        In.Row1,
        In.Row2,
        In.Row3
    );

    matrix wvp;
    wvp = mul(InstanceWorld, View);
    wvp = mul(wvp, Projection);
    Out.Position = mul(In.Position, wvp);

    float4 normal = float4(In.Normal.xyz, 0.0);
    float4 worldNormal = mul(normal, InstanceWorld);
    worldNormal = normalize(worldNormal);
    Out.Normal = worldNormal;

    float4 tangent = float4(In.Tangent.xyz, 0.0f);
    float4 worldTangent = mul(tangent, InstanceWorld);
    worldTangent = normalize(worldTangent);
    Out.Tangent = worldTangent;

    float3 worldNormal3 = normalize(worldNormal.xyz);
    float3 worldTangent3 = normalize(worldTangent.xyz);
    float3 worldBitangent3 = cross(worldNormal3, worldTangent3) * In.Tangent.w;
    Out.Bitangent = float4(worldBitangent3, 0.0f);

    Out.Diffuse = In.Diffuse;

    float2 animatedUV = TransformUV(In.TexCoord, UVStart, UVEnd);
    Out.TexCoord = animatedUV;

    Out.WorldPosition = mul(In.Position, InstanceWorld);
}
