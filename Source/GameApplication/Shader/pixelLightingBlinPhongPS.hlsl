
#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    In.Diffuse.rgb = float3(1.0, 1.0, 1.0);
    //光源からピクセルへのベクトル
    float4 lv = In.WorldPosition - Light.Position;
    //物体と光源の距離
    float ld = length(lv);
    //ベクトルの正規化
    lv = normalize(lv);
    
    //減衰の計算
    float ofs = 1.0f - (1.0f / Light.PointLightParam.x) * ld; //減衰の計算
    ofs = max(0, ofs);
    
    //ピクセルの法線を正規化
    float3 normal = normalize(In.Normal.xyz);
    //光源計算
    float light = -dot(normal.xyz, lv.xyz);
    light = saturate(light);
    light *= ofs;
    
    //テクスチャのピクセル色を取得
    if (Material.TextureEnable)
    {
        outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    }
    else
    {
        outDiffuse = In.Diffuse;
    }
    outDiffuse.rgb *= In.Diffuse.rgb * light + Light.Ambient.rgb; //明るさを乗算
    outDiffuse.a *= In.Diffuse.a;
    
    //カメラからピクセルへ向かうベクトル
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
    
    //ハーフベクトルを計算
    float3 halfv = eyev + lv.xyz;
    halfv = normalize(halfv);
    
    //スペキュラの計算
    float specular = -dot(halfv, normal.xyz);//ハーフベクトルと法線の内積
    specular = saturate(specular);
    specular = pow(specular, 30);
    
    outDiffuse.rgb += (specular * ofs);//スペキュラも減衰させてから加算して出力
}
