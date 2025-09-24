#include "../common.hlsl"

Texture2D g_OriginalTexture : register(t0);
Texture2D g_BloomTexture : register(t1);
SamplerState g_SamplerState : register(s0);

float4 main(PS_IN In) : SV_Target
{
    float BloomIntensity = Parameter.x + 1.0f;
    
    float4 original = g_OriginalTexture.Sample(g_SamplerState, In.TexCoord);
    float4 bloom = g_BloomTexture.Sample(g_SamplerState, In.TexCoord);

    return original + bloom * BloomIntensity;
}
