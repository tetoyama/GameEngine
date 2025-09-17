#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

static const float BlurOffset = 1.0 / 512.0; // テクスチャのサイズに応じて調整

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    float4 sum = float4(0, 0, 0, 0);

    // 3x3 平均
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            sum += g_Texture.Sample(g_SamplerState, uv + float2(x, y) * BlurOffset);
        }
    }

    outDiffuse = sum / 9.0;
}
