#include "common.hlsl"
Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In)
{
    float4 outDiffuse = Material.BaseColor;
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
    {
        outDiffuse *= g_Texture.Sample(g_SamplerState, In.TexCoord);
    }
    if (outDiffuse.a <= 0.1f)
    {
        discard;
    }
}
