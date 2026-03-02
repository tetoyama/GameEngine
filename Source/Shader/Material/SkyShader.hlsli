#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

//=============================================================================
// ShadeMaterial_Sky
//  スカイドーム用シェーダー
//  エクイレクタングラー（正距円筒図法）パノラマテクスチャを
//  ワールド空間の方向ベクトルから正しい UV で射影する。
//  sky.fbx に貼られた Daylight.png 等のパノラマ画像が
//  メッシュ UV の歪みなく正しい向きで描画される。
//=============================================================================
float4 ShadeMaterial_Sky(MaterialInput input)
{
    // カメラから頂点への方向ベクトル（単位ベクトル）
    float3 dir = normalize(input.worldPos - CameraPosition.xyz);

    // エクイレクタングラー UV 変換
    // U: 経度 [0,1] — atan2(z, x) で水平方向の角度
    // V: 緯度 [0,1] — 0=北極(上), 0.5=赤道(中間), 1=南極(下)
    //    Daylight.png 等の上端が空になっているパノラマに対応する向き。
    //    テクスチャが上下反転している場合は asin 前の符号を変えてください。
    float2 sphereUV;
    sphereUV.x = atan2(dir.z, dir.x) * (1.0 / (2.0 * PI)) + 0.5;
    sphereUV.y = 0.5 - asin(dir.y) / PI;

    float4 skyColor = BaseColorTex.Sample(LinearSampler, sphereUV);
    return skyColor;
}
