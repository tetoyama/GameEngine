#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(PS_IN In) : SV_Target
{
    // Parameter.x : 輝度しきい値  (例: 0.8)
    // Parameter.y : ソフトニー幅   (例: 0.5 * Threshold; 0 ならしきい値の半分を自動使用)
    float Threshold = max(Parameter.x, 0.0f);
    float Knee      = (Parameter.y > 0.0f) ? Parameter.y : Threshold * 0.5f;

    float4 color = g_Texture.Sample(g_SamplerState, In.TexCoord);

    // Rec.709 輝度係数（知覚的に正確な輝度計算）
    float luminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));

    // ゼロ除算防止用の微小値
    static const float EPSILON = 0.0001f;

    // ソフトニー抽出（URP スタイル）
    // Knee == 0 のときはハード閾値にフォールバック
    float weight;
    if (Knee < EPSILON)
    {
        weight = step(Threshold, luminance);
    }
    else
    {
        float rq = clamp(luminance - Threshold + Knee, 0.0f, 2.0f * Knee);
        rq = (rq * rq) / (4.0f * Knee + EPSILON);
        weight = max(rq, luminance - Threshold) / max(luminance, EPSILON);
    }

    return float4(color.rgb * weight, 1.0f);
}

