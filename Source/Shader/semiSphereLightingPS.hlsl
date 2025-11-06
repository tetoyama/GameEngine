
#include "common.hlsl"

Texture2D g_Texture : register(t0); //テクスチャ０番
SamplerState g_SamplerState : register(s0); //サンプラー０番

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
	//ピクセルの法線を正規化
    float4 normal = normalize(In.Normal);
    float light = -dot(normal.xyz, Lights[0].Direction.xyz); //光源計算をする
    light = saturate(light);
	
    float blendFactor = normal.y * 0.5f + 0.5f;
    float4 color = lerp(Lights[0].GroundColor, Lights[0].SkyColor, blendFactor);
    
    
	//テクスチャのピクセル色を取得
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord) * Material.Diffuse;
    outDiffuse.rgb *= In.Diffuse.rgb * light; //明るさを乗算
    outDiffuse.a *= In.Diffuse.a; //αに明るさは関係ないので別計算
    outDiffuse.rgb += color.rgb * color.a;
	
	//カメラからピクセルへ向かうベクトル
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev); //正規化する

	//ハーフベクトルの作成
    float3 halfv = eyev + Lights[0].Direction.xyz; //視線とライトベクトルを加算
    halfv = normalize(halfv); //正規化する

    float specular = -dot(halfv, normal.xyz); //ハーフベクトルと法線の内積を計算

    specular = saturate(specular); //サチュレートする
    specular = pow(specular, 30);

    outDiffuse.rgb += specular; //スペキュラ値をデフューズとして足しこむ
    outDiffuse.rgb = outDiffuse.rgb * Material.TextureEnable + (!Material.TextureEnable * Material.Diffuse.rgb);

}
