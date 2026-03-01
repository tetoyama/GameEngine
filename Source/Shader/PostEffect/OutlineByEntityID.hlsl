#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ライティング済みシーンテクスチャ (t0)
// g_SamplerState は PostEffectFunc.hlsli で定義済み
Texture2D g_Texture : register(t0);

// Parameter.x : アウトライン強度 (0.0〜1.0, 推奨値 1.0)
// Parameter.y : エッジ幅ピクセル数 (推奨値 1.0)
//
// GBuffer GParam (t13) の ShaderID と ObjectID を使ったエッジ検出。
// ShaderID (= MaterialID, z チャンネル) が一致し、
// ObjectID (= EntityID,   x チャンネル) が異なる隣接ピクセル境界にアウトラインを描画する。
// これにより同じマテリアルを使うオブジェクト同士の境界のみを縁取ることができる。

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;

    // テクスチャ解像度を取得
    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);

    int2 maxCoord = int2(texWidth - 1, texHeight - 1);

    int step = max((int)round(Parameter.y), 1);

    // ライティング済みシーン色
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    // 中心ピクセルの整数座標と GParam
    int2 centerCoord = int2(uv * float2(texWidth, texHeight) - 0.5f);
    uint4 centerParam = GetObjParam(centerCoord);

    // 背景ピクセル (ObjectID == 0) はアウトライン対象外
    if (centerParam.x == 0u)
    {
        outDiffuse = sceneColor;
        return;
    }

    // 4 方向の隣接ピクセルを確認
    // ShaderID (z) が同じで ObjectID (x) が異なる隣接ピクセルがあればエッジ
    const int2 offsets[4] = {
        int2( step,  0),
        int2(-step,  0),
        int2( 0,  step),
        int2( 0, -step)
    };

    bool isEdge = false;
    for (int i = 0; i < 4; ++i)
    {
        int2 neighborCoord = clamp(centerCoord + offsets[i], int2(0, 0), maxCoord);
        uint4 neighborParam = GetObjParam(neighborCoord);

        // ShaderID 一致 かつ ObjectID 不一致 → エッジ
        if (neighborParam.z == centerParam.z && neighborParam.x != centerParam.x)
        {
            isEdge = true;
            break;
        }
    }

    float edge = isEdge ? saturate(Parameter.x) : 0.0f;

    // エッジ部分を黒くする（縁取り）
    outDiffuse = lerp(sceneColor, float4(0, 0, 0, sceneColor.a), edge);
}
