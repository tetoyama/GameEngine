#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

static const float TexelSizeX = 1.0f / 1024.0f;
static const float TexelSizeY = 1.0f / 768.0f;

float4 main(PS_IN In) : SV_Target
{
    float4 sum = float4(0, 0, 0, 0);

    sum += g_Texture.Sample(g_SamplerState, In.TexCoord + float2(0, -2.0f * TexelSizeY)) * 0.1216216f;
    sum += g_Texture.Sample(g_SamplerState, In.TexCoord + float2(0, -1.0f * TexelSizeY)) * 0.2332432f;
    sum += g_Texture.Sample(g_SamplerState, In.TexCoord) * 0.2909139f;
    sum += g_Texture.Sample(g_SamplerState, In.TexCoord + float2(0, 1.0f * TexelSizeY)) * 0.2332432f;
    sum += g_Texture.Sample(g_SamplerState, In.TexCoord + float2(0, 2.0f * TexelSizeY)) * 0.1216216f;

    return sum;
}
