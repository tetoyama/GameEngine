#include "../common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    // 元の UV
    float2 uv = In.TexCoord;
    float MosaicBlockPixelSize = Parameter.x;
    if (MosaicBlockPixelSize < 1)
    {
        MosaicBlockPixelSize = 1;
    }
    
    // テクスチャ解像度取得
    uint texWidth, texHeight;
    g_Texture.GetDimensions(texWidth, texHeight);

    // UV → ピクセル座標
    float2 pixelPos;
    pixelPos.x = uv.x * texWidth;
    pixelPos.y = uv.y * texHeight;

    // ピクセル座標をモザイク単位で丸める
    pixelPos = floor(pixelPos / MosaicBlockPixelSize) * MosaicBlockPixelSize
             + MosaicBlockPixelSize * 0.5f;

    // ピクセル → UV に戻す
    uv.x = pixelPos.x / texWidth;
    uv.y = pixelPos.y / texHeight;
    uv = clamp(uv, 0.0, 1.0);

    // サンプリング
    outDiffuse = g_Texture.Sample(g_SamplerState, uv);
}
