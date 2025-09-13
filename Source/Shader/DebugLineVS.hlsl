cbuffer cbPerFrame : register(b0)
{
    matrix gWorldViewProj; // ワールドビュー射影行列
};

struct VSInput
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // 座標をワールドビュー射影行列で変換
    output.pos = mul(float4(input.pos, 1.0f), gWorldViewProj);
    output.color = input.color;

    return output;
}
