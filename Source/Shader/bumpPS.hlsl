
#include "common.hlsl"
//法線マップ用のテクスチャ変数を追加
Texture2D g_Texture : register(t0); //通常のテクスチャ用
Texture2D g_TextureNormal : register(t1); //法線マップ用
SamplerState g_SamplerState : register(s0); //これは１つでOK

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float4 normal = normalize(In.Normal);

    //光源からピクセルへのベクトル
    float4 lv = In.WorldPosition - Light.Position;
    float ld = length(lv);
    //ベクトルの正規化
    lv = normalize(lv);
    
    //減衰の計算
    float ofs = 1.0f - (1.0f / Light.PointLightParam.x) * ld;
    ofs = max(0, ofs);
    
    float4 normalMap = g_TextureNormal.Sample(g_SamplerState, In.TexCoord);
    normalMap = normalMap * 2.0f - 1.0f; //法線マップの値を-1から1の範囲に変換
    normalMap = 0.5f * (normalMap + In.Normal);
    float4 bumpNormal;
    bumpNormal.x = -normalMap.r; // X軸の法線を反転
    bumpNormal.y = normalMap.g; // Y軸の法線
    bumpNormal.z = normalMap.b; // Y軸の法線
    bumpNormal.w = 0.0f; // Y軸の法線
    
    bumpNormal = normalize(bumpNormal);
    float light = -dot(bumpNormal.xyz, lv.xyz);
    
    
    light = saturate(light);
    light *= ofs;
    
    float blendFactor = normal.y * 0.5f + 0.5f;
    float4 color = lerp(Light.GroundColor, Light.SkyColor, blendFactor);
    
    
	//テクスチャのピクセル色を取得
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord) * Material.Diffuse;
    outDiffuse.rgb *= In.Diffuse.rgb * light + Light.Ambient.rgb;
    outDiffuse.a *= In.Diffuse.a;
    outDiffuse.rgb += color.rgb * color.a;

    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
    
    float3 halfv = eyev + lv.xyz;
    halfv = normalize(halfv);
    
    float specular = -dot(halfv, bumpNormal.xyz);
    specular = saturate(specular);
    specular = pow(specular, 30);
    
    outDiffuse.rgb += (specular * ofs);
}


//    outDiffuse.rgb *= (In.Diffuse.rgb * light + Light.Ambient.rgb); //明るさを乗算
