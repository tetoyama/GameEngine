
#include "common.hlsl"

Texture2D g_Texture : register(t0); //テクスチャ０番
SamplerState g_SamplerState : register(s0); //サンプラー０番

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
	//ピクセルの法線を正規化
    float4 normal = normalize(In.Normal);
    float light = -dot(normal.xyz, Light.Direction.xyz); //光源計算をする
    light = saturate(light);
	
	//テクスチャのピクセル色を取得
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    outDiffuse.rgb *= In.Diffuse.rgb * light; //明るさを乗算
    outDiffuse.a *= In.Diffuse.a; //αに明るさは関係ないので別計算
<<<<<<< Updated upstream:Source/GameApplication/Shader/pointLightingBlinnPhongPS - 繧ｳ繝斐�ｼ (2).hlsl

=======
    outDiffuse.rgb += color.rgb * color.a;
>>>>>>> Stashed changes:Source/GameApplication/Shader/semiSphereLightingPS.hlsl
	
	//カメラからピクセルへ向かうベクトル
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev); //正規化する
//    eyev = normalize(-eyev); //図解通り版

	//ハーフベクトルの作成
    float3 halfv = eyev + Light.Direction.xyz; //視線とライトベクトルを加算
    halfv = normalize(halfv); //正規化する
 //   float3 halfv = eyev + (-Light.Direction.xyz); //図解通り版

    float specular = -dot(halfv, normal.xyz); //ハーフベクトルと法線の内積を計算
 //   float specular = dot(halfv, normal.xyz); //図解通り版
    specular = saturate(specular); //サチュレートする
    specular = pow(specular, 30);

    outDiffuse.rgb += specular; //スペキュラ値をデフューズとして足しこむ



}


//    outDiffuse.rgb *= (In.Diffuse.rgb * light + Light.Ambient.rgb); //明るさを乗算
