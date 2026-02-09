//#include "PostEffectFunc.hlsli"

//POST_PS_OUT main(PS_IN In)
//{
//    POST_PS_OUT Out;

//    float4 albedo = GetAlbedo(In.TexCoord);
//    float3 normal = GetWorldNormal(In.TexCoord);
//    float4 emissive = GetEmissive(In.TexCoord);

//    // 例: アルベド + エミッシブ
//    Out.Color = albedo;
//    Out.Normal = float4(normal, 1.0);
//    Out.Emissive = emissive;

//    return Out;
//}

#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
}