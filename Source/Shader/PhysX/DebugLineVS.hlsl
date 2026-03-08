#include "../common.hlsl"

// PhysX デバッグライン用 頂点シェーダー
// 位置を WVP 変換し、頂点カラー (Diffuse) をそのままパススルーする。
// 法線・接線・UV は使用しないため、定数値で初期化する。
void main(in VS_IN In, out PS_IN Out)
{
    // デバッグ線分も通常描画と同じく World -> View -> Projection の順で変換する
    matrix wvp = mul(mul(World, View), Projection);
    Out.Position = mul(In.Position, wvp);

    // デバッグラインはライティングを使用しないため、法線・接線・従法線は不使用
    Out.Normal        = float4(0.0f, 1.0f, 0.0f, 0.0f);
    Out.Tangent       = float4(1.0f, 0.0f, 0.0f, 0.0f);
    Out.Bitangent     = float4(0.0f, 0.0f, 1.0f, 0.0f);

    Out.Diffuse       = In.Diffuse;
    Out.TexCoord      = float2(0.0f, 0.0f);
    Out.WorldPosition = mul(In.Position, World);
}
