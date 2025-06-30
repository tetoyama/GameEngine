
#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord) * Material.Diffuse;
    if (outDiffuse.a <= 0.05f)//αテスト
    {
        discard;
    }

}
