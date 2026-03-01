#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ライティング済みシーンテクスチャ (t0)
// g_SamplerState は PostEffectFunc.hlsli で定義済み
Texture2D g_Texture : register(t0);

// Parameter.x : アウトライン対象の ShaderID (uint キャスト, 0 = 無効)
// Parameter.y : エッジ幅ピクセル数 (推奨値 1.0)
//
// GBuffer GParam (t13) の ShaderID と ObjectID を使ったエッジ検出。
// Parameter.x で指定した ShaderID を持つピクセルを対象とし、
// 隣接ピクセルの ObjectID (= EntityID) が異なる境界にアウトラインを描画する。
// 同一 ShaderID 同士の境界では二重アウトラインを防ぐため、
// EntityID が小さい側のピクセルのみエッジとして扱う。
// 隣接ピクセルの ShaderID が異なる場合は EntityID が異なれば常にエッジとして扱う。

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

    // Parameter.x == 0 のときはアウトライン無効
    if ((uint)Parameter.x == 0u)
    {
        outDiffuse = sceneColor;
        return;
    }

    // 中心ピクセルの整数座標と GParam
    int2 centerCoord = int2(uv * float2(texWidth, texHeight) - 0.5f);
    uint4 centerParam = GetObjParam(centerCoord);

    // Parameter.x で指定された ShaderID に一致しないピクセルはアウトライン対象外
    if (centerParam.z != (uint)Parameter.x)
    {
        outDiffuse = sceneColor;
        return;
    }

    // 4 方向の隣接ピクセルを確認
    // ・隣接ピクセルの ShaderID が中心と同じ (同一シェーダ同士) かつ EntityID が異なる場合は、
    //   中心の EntityID が小さい側のみエッジとして扱う (二重アウトライン防止)
    // ・隣接ピクセルの ShaderID が中心と異なる場合は EntityID が異なればエッジ
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

        if (neighborParam.x != centerParam.x)
        {
            // 隣接ピクセルが同じ ShaderID の場合は小さい EntityID 側だけエッジとする
            if (neighborParam.z == (uint)Parameter.x && centerParam.x > neighborParam.x)
                continue;

            isEdge = true;
            break;
        }
    }

    // エッジ部分を黒くする（縁取り）
    outDiffuse = lerp(sceneColor, float4(0, 0, 0, sceneColor.a), isEdge ? 1.0f : 0.0f);
}
