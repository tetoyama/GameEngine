#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

// Parameter.x : 色収差の強度 (UV オフセット量, 例: 0.005)
//   値が大きいほど色ずれが強くなります。
//   Parameter.y, z, w は未使用。

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // 画面中心からの方向ベクトル ([-1, 1] 空間)
    float2 dir = uv - float2(0.5f, 0.5f);

    float strength = Parameter.x;

    // 各チャンネルを中心からの方向に沿って異なるオフセットでサンプリング
    float r = g_Texture.Sample(g_SamplerState, clamp(uv + dir * strength, 0.0f, 1.0f)).r;
    float g = g_Texture.Sample(g_SamplerState, uv).g;
    float b = g_Texture.Sample(g_SamplerState, clamp(uv - dir * strength, 0.0f, 1.0f)).b;
    float a = g_Texture.Sample(g_SamplerState, uv).a;

    outDiffuse = float4(r, g, b, a);
}
