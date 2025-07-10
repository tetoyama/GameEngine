#include "common.hlsl"

// 法線マップ用のテクスチャ変数
Texture2D g_Texture : register(t0); // 通常のテクスチャ
Texture2D g_TextureNormal : register(t1); // 法線マップ
SamplerState g_SamplerState : register(s0); // サンプラ

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    // ------------------------
    // TBN 行列の構築
    float3 T = normalize(In.Tangent.xyz);
    float3 B = normalize(In.Bitangent.xyz);
    float3 N = normalize(In.Normal.xyz);
    float3x3 TBN = float3x3(T, B, N);

    // ------------------------
    // ノーマルマップ取得 → [-1,1] に変換
    float3 normalMap = g_TextureNormal.Sample(g_SamplerState, In.TexCoord).rgb;
    //normalMap.z *= -1.0f;
    normalMap.z *= -1.0f;
    
    normalMap = normalMap * 2.0f - 1.0f;

    // ------------------------
    // TBNでワールド空間に変換
    float3 bumpNormal = normalize(mul(normalMap, TBN));

    // ------------------------
    // 光ベクトル（ライトからピクセルへのベクトル）
    float3 lv = normalize(In.WorldPosition.xyz - Light.Position.xyz);
    float ld = length(In.WorldPosition.xyz - Light.Position.xyz);

    // 減衰計算
    float ofs = 1.0f - (1.0f / Light.PointLightParam.x) * ld;
    ofs = max(0, ofs);

    // ------------------------
    // 拡散光計算（バンプ法線使用）
    float light = -dot(bumpNormal, lv);
    light = saturate(light) * ofs;

    // ------------------------
    // グラデーション環境色（N.yベース）
    float blendFactor = N.y * 0.5f + 0.5f;
    float4 envColor = lerp(Light.GroundColor, Light.SkyColor, blendFactor);

    // ------------------------
    // テクスチャカラー取得 + 拡散・環境光適用
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord) * Material.Diffuse;
    outDiffuse.rgb *= In.Diffuse.rgb * light + Light.Ambient.rgb;
    outDiffuse.a *= In.Diffuse.a;
    outDiffuse.rgb += envColor.rgb * envColor.a;

    // ------------------------
    // スペキュラ（Blinn-Phong）
    float3 eyev = normalize(In.WorldPosition.xyz - CameraPosition.xyz);
    float3 halfv = normalize(eyev + lv);
    float specular = dot(halfv, bumpNormal);
    specular = saturate(specular);
    specular = pow(specular, 30); // Specular Power
    outDiffuse.rgb = outDiffuse.rgb * Material.TextureEnable + (!Material.TextureEnable * Material.Diffuse.rgb);
    outDiffuse.rgb += specular * ofs;
}
