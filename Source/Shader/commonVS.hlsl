#include "common.hlsl"
void main(in VS_IN In, out PS_IN Out)
{
    
	//ここで頂点変換
	//頂点座標を出力
	//頂点変換処理  この処理は必ず必要
    matrix wvp; //行列変数を作成
    wvp = mul(World, View); //wvp = ワールド行列＊カメラ行列
    wvp = mul(wvp, Projection); //wvp = wvp *プロジェクション行列
    Out.Position = mul(In.Position, wvp); //変換結果を出力する

	//頂点法線をワールド行列で回転させる(頂点と同じ回転をさせる)
    float4 worldNormal, normal; //ローカル変数を作成
    normal = float4(In.Normal.xyz, 0.0); //入力法線ベクトルのwを0としてコピー（平行移動しないため)
    worldNormal = mul(normal, World); //コピーされた法線をワールド行列で回転する
    worldNormal = normalize(worldNormal); //回転後の法線を正規化する
    Out.Normal = worldNormal; //回転後の法線出力 In.Normalでなく回転後の法線を出力

    // Tangent のワールド空間への変換
    float4 tangent = float4(In.Tangent.xyz, 0.0f); // 移動成分不要なのでw=0
    float4 worldTangent = mul(tangent, World); // ワールド行列で回転
    worldTangent = normalize(worldTangent); // 正規化
    Out.Tangent = worldTangent; // 出力に格納
    
    // Bitangent を計算（右手系）
    float3 worldNormal3 = normalize(worldNormal.xyz);
    float3 worldTangent3 = normalize(worldTangent.xyz);
    float3 worldBitangent3 = cross(worldNormal3, worldTangent3) * In.Tangent.w;
    Out.Bitangent = float4(worldBitangent3, 0.0f);
    
    Out.Diffuse = In.Diffuse; //頂点の物をそのまま出力

    float2 animatedUV = TransformUV(In.TexCoord, UVStart, UVEnd);
    Out.TexCoord = animatedUV;

	//ワールド変換した頂点座標を出力
    Out.WorldPosition = mul(In.Position, World);
}
