#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float BaseOffset = 1.0 / 512.0;
    float BlurOffset = BaseOffset * Parameter.x;

    float2 uv = In.TexCoord;
    float4 sum = float4(0, 0, 0, 0);

    float kernel[5][5] =
    {
        { 1.0 / 256.0, 4.0 / 256.0, 6.0 / 256.0, 4.0 / 256.0, 1.0 / 256.0 },
        { 4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0 },
        { 6.0 / 256.0, 24.0 / 256.0, 36.0 / 256.0, 24.0 / 256.0, 6.0 / 256.0 },
        { 4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0 },
        { 1.0 / 256.0, 4.0 / 256.0, 6.0 / 256.0, 4.0 / 256.0, 1.0 / 256.0 }
    };

    [unroll]
    for (int y = -2; y <= 2; y++)
    {
        [unroll]
        for (int x = -2; x <= 2; x++)
        {
            float2 sampleUV = uv + float2(x, y) * BlurOffset;

            // UVを0～1に制限してループ防止
            sampleUV = clamp(sampleUV, 0.0, 1.0);

            sum += g_Texture.Sample(g_SamplerState, sampleUV) * kernel[y + 2][x + 2];
        }
    }

    outDiffuse = sum;
}
