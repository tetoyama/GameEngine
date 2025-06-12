
#include "common.hlsl"

Texture2D g_Texture : register(t0); //テクスチャ０番
SamplerState g_SamplerState : register(s0); //サンプラー０番

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    //光源からピクセルへのベクトル
    float4 lv = In.WorldPosition - Light.Position;
    //物体と光源の距離
    float4 normal = normalize(In.Normal);
    float light = -dot(normal.xyz, lv.xyz);

    float4 ld = length(lv);
    //ベクトルの正規化
    lv = normalize(lv);
    
    //減衰の計算
    float ofs = 1.0f - (1.0f / Light.PointLightParam.x) * ld;
    ofs = max(0, ofs);
    
    
    light = saturate(light);
    light *= ofs;
    

    
    
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
    
    float3 halfv = eyev + lv.xyz;
    halfv = normalize(halfv);
    
    float specular = -dot(halfv, normal.xyz);
    specular = saturate(specular);
    specular = pow(specular, 30);
    
    outDiffuse.rgb += (specular * ofs);

    
    float lit = 1.0f - max(0, dot(lv.xyz, eyev));
    float lim = 1.0f - max(0, dot(normal.xyz, -eyev));
    
    float blendFactor = normal.y * 0.5f + 0.5f;
    float4 color = lerp(Light.GroundColor, Light.SkyColor, blendFactor);

    
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    outDiffuse.rgb *= In.Diffuse.rgb * light + Light.Ambient.rgb + pow(lim, 3) * color.rgb;
    outDiffuse.a *= In.Diffuse.a;

    outDiffuse.rgb += color.rgb * color.a;

    outDiffuse.rgb += pow(lit * lim, 5) * color.rgb;
    outDiffuse.rgb += (specular * ofs);

}
