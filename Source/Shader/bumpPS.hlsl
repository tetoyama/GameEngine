
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
        
    //法線マップ取得
    float4 normalMap = g_TextureNormal.Sample(g_SamplerState, In.TexCoord); //法線マップのサンプリング
    //色データをベクトルに展開
    normalMap = (normalMap * 2.0f) - 1.0f; //テクスチャによる？

    //法線として格納しなおす
    float4 bumpNormal; //サンプルのテクスチャ次第
    //normal.x = normalMap.x; //X成分 test.png
    bumpNormal.x = -normalMap.r; //X成分 normal.bmp
    bumpNormal.y = normalMap.b; //Y成分
    bumpNormal.z = normalMap.g; //Z成分
    bumpNormal.w = 0.0f; //W成分は0にする（平行移動しないため）

    //光源計算をする
    float light = -dot(bumpNormal.xyz, lv.xyz);
    light = saturate(light);
    //明るさを減衰する
    light *= ofs; //距離を減衰率で割る
    
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
    
    outDiffuse.rgb = outDiffuse.rgb * Material.TextureEnable + (!Material.TextureEnable * Material.Diffuse.rgb);
    outDiffuse.rgb += (specular * ofs);


}
