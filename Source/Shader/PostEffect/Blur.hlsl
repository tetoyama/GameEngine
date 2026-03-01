#include "../common.hlsl"

// ======================================
// Adaptive Gaussian Blur Pixel Shader
// DX11 Safe / Param.x Adaptive
// ======================================

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(PS_IN In) : SV_Target
{
    // ----------------------------
    // 基本設定
    // ----------------------------
    float BaseOffset = 1.0 / 512.0;

    // Param.x が 1 以上でも安全
    float strength = max(Parameter.x, 0.0);

    float BlurOffset = BaseOffset * max(strength, 0.001);

    // ----------------------------
    // カーネルサイズ分岐
    // 3x3 ～ 15x15
    // ----------------------------
    int radius;

    if (strength < 0.25)
        radius = 1; // 3x3
    else if (strength < 0.5)
        radius = 2; // 5x5
    else if (strength < 0.75)
        radius = 3; // 7x7
    else if (strength < 1.0)
        radius = 5; // 11x11
    else
        radius = 7; // 15x15（最大）

    // ----------------------------
    // ガウス分布パラメータ
    // ----------------------------
    float sigma = max(radius * 0.5, 0.5);
    float invSigma2 = 1.0 / (2.0 * sigma * sigma);

    float2 uv = In.TexCoord;

    float4 sum = float4(0, 0, 0, 0);
    float weightSum = 0.0;

    // ----------------------------
    // 最大 15x15 固定ループ
    // ----------------------------
    [unroll]
    for (int y = -7; y <= 7; y++)
    {
        [unroll]
        for (int x = -7; x <= 7; x++)
        {
            // 有効半径外は無視
            if (abs(x) > radius || abs(y) > radius)
                continue;

            float2 offset = float2(x, y) * BlurOffset;
            float2 sampleUV = clamp(uv + offset, 0.0, 1.0);

            float dist2 = (float) (x * x + y * y);
            float weight = exp(-dist2 * invSigma2);

            float4 col = g_Texture.Sample(g_SamplerState, sampleUV);

            sum += col * weight;
            weightSum += weight;
        }
    }

    // ----------------------------
    // 正規化（破綻防止）
    // ----------------------------
    sum /= max(weightSum, 1e-5);

    return sum;
}

