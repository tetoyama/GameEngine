#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

// クワハラフィルタ (Kuwahara Filter)
//
// 画像を油絵風に平滑化するエッジ保存フィルタ。
// 各ピクセルの周囲を4つの矩形領域に分割し、最も分散が小さい
// 領域の平均色を出力色として採用することで、エッジを保ちながら
// ノイズや細部を平滑化する。
//
// Parameter.x : カーネル半径 (整数ピクセル数, 例: 3.0)
//   値が大きいほど油絵風の効果が強くなります。
//   Parameter.y, z, w は未使用。

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;
    int radius = max(1, (int)Parameter.x);

    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);
    float2 texelSize = float2(1.0f / texWidth, 1.0f / texHeight);

    // 各領域の累積値
    float3 mean[4]     = { float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0) };
    float3 meanSq[4]   = { float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0) };
    float  count[4]    = { 0, 0, 0, 0 };

    // 各領域の範囲 (重複ピクセルあり)
    // 領域0: 左上  (-radius, -radius) ~ (0, 0)
    // 領域1: 右上  (0, -radius)       ~ (+radius, 0)
    // 領域2: 左下  (-radius, 0)       ~ (0, +radius)
    // 領域3: 右下  (0, 0)             ~ (+radius, +radius)
    for (int j = -radius; j <= radius; ++j)
    {
        for (int i = -radius; i <= radius; ++i)
        {
            float2 sampleUV = clamp(uv + float2(i, j) * texelSize, 0.0f, 1.0f);
            float3 col = g_Texture.Sample(g_SamplerState, sampleUV).rgb;

            if (i <= 0 && j <= 0) { mean[0] += col; meanSq[0] += col * col; count[0] += 1.0f; }
            if (i >= 0 && j <= 0) { mean[1] += col; meanSq[1] += col * col; count[1] += 1.0f; }
            if (i <= 0 && j >= 0) { mean[2] += col; meanSq[2] += col * col; count[2] += 1.0f; }
            if (i >= 0 && j >= 0) { mean[3] += col; meanSq[3] += col * col; count[3] += 1.0f; }
        }
    }

    // 分散が最小の領域の平均色を選択
    float   minVar   = -1.0f;
    float3  bestMean = mean[0] / count[0];

    for (int k = 0; k < 4; ++k)
    {
        float3 m  = mean[k]   / count[k];
        float3 sq = meanSq[k] / count[k];
        // 輝度の分散 (RGB 各チャンネルの分散の総和)
        float3 variance = sq - m * m;
        float  totalVar = variance.r + variance.g + variance.b;
        if (minVar < 0.0f || totalVar < minVar)
        {
            minVar   = totalVar;
            bestMean = m;
        }
    }

    outDiffuse = float4(bestMean, g_Texture.Sample(g_SamplerState, uv).a);
}
