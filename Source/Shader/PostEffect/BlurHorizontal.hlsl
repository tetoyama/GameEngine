#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(PS_IN In) : SV_Target
{
    // テクスチャ解像度を動的に取得（ハードコードを排除）
    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);
    float texelSizeX = 1.0f / (float) texWidth;

    // Parameter.x: ブラー半径係数（デフォルト 0 → 1倍）
    float blurScale = max(Parameter.x + 1.0f, 0.0f);

    float2 uv = In.TexCoord;

    // 9タップ水平ガウシアンカーネル（σ ≈ 1.5、合計 ≈ 1.0 に正規化済み）
    float4 sum = float4(0, 0, 0, 0);
    sum += g_Texture.Sample(g_SamplerState, uv + float2(-4.0f * texelSizeX * blurScale, 0)) * 0.0312f;
    sum += g_Texture.Sample(g_SamplerState, uv + float2(-3.0f * texelSizeX * blurScale, 0)) * 0.0703f;
    sum += g_Texture.Sample(g_SamplerState, uv + float2(-2.0f * texelSizeX * blurScale, 0)) * 0.1250f;
    sum += g_Texture.Sample(g_SamplerState, uv + float2(-1.0f * texelSizeX * blurScale, 0)) * 0.1953f;
    sum += g_Texture.Sample(g_SamplerState, uv)                                              * 0.2344f;
    sum += g_Texture.Sample(g_SamplerState, uv + float2( 1.0f * texelSizeX * blurScale, 0)) * 0.1953f;
    sum += g_Texture.Sample(g_SamplerState, uv + float2( 2.0f * texelSizeX * blurScale, 0)) * 0.1250f;
    sum += g_Texture.Sample(g_SamplerState, uv + float2( 3.0f * texelSizeX * blurScale, 0)) * 0.0703f;
    sum += g_Texture.Sample(g_SamplerState, uv + float2( 4.0f * texelSizeX * blurScale, 0)) * 0.0312f;

    return sum;
}

