#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float rand(float2 co)
{
    return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // 1. 水平ラインの固定ずれ
    float lineOffset = sin(uv.y * 50.0) * 0.02;
    uv.x += lineOffset;

    // 2. RGBチャンネルの固定オフセット
    float offsetR = 0.01;
    float offsetG = -0.005;
    float offsetB = 0.0;

    float4 color;
    color.r = g_Texture.Sample(g_SamplerState, uv + float2(offsetR, 0)).r;
    color.g = g_Texture.Sample(g_SamplerState, uv + float2(offsetG, 0)).g;
    color.b = g_Texture.Sample(g_SamplerState, uv + float2(offsetB, 0)).b;
    color.a = 1.0;

    // 3. ランダムノイズで部分的に色反転（静的）
    if (rand(In.TexCoord) > 0.95)
    {
        color.rgb = 1.0 - color.rgb;
    }

    outDiffuse = color;
}
