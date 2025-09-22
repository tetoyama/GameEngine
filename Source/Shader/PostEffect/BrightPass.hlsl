#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

// 固定の閾値
static const float Threshold = 0.8f;

float4 main(PS_IN In) : SV_Target
{
    float4 color = g_Texture.Sample(g_SamplerState, In.TexCoord);

    float luminance = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));

    if (luminance > Threshold)
        return color;
    else
        return float4(0, 0, 0, 0);
}
