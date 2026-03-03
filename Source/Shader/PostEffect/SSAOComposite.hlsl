#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ============================================================
// SSAO 合成パス
//
// ぼかし済み AO マスクをシーンカラーに乗算する。
// SSAO → Blur → SSAOComposite の最終段として使用する。
//
// t0 : シーンカラー (前パスの出力, 例: OutlineByShaderID の結果)
// t1 : ぼかし済み AO マスク (Blur パスの出力)
// ============================================================

// シーンカラー (t0)
Texture2D g_Texture   : register(t0);

// ぼかし済み AO マスク (t1)
Texture2D g_AOTexture : register(t1);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);
    float  ao         = g_AOTexture.Sample(g_SamplerState, uv).r;

    outDiffuse = float4(sceneColor.rgb * ao, sceneColor.a);
}
