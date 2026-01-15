#include "common.hlsl"
Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(in PS_IN In) : SV_Target
{
    float4 outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord) * Material.BaseColor;
    if (outDiffuse.a <= 0.1f)
    {
        discard;
    }
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
