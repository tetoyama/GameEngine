cbuffer Frame : register(b0, space3)
{
    row_major float4x4 frame_viewProjection : packoffset(c0);
    row_major float4x4 frame_lightViewProjection : packoffset(c4);
    float4 frame_lightDirection : packoffset(c8);
    float4 frame_lightColor : packoffset(c9);
    float4 frame_ambientColor : packoffset(c10);
    float4 frame_outputTransform : packoffset(c11);
    float4 frame_cameraPosition : packoffset(c12);
};

Texture2D<float4> sourceTexture : register(t0, space2);
SamplerState _sourceTexture_sampler : register(s0, space2);

static float2 uv;
static float4 outColor;

struct SPIRV_Cross_Input
{
    float2 uv : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 outColor : SV_Target0;
};

void frag_main()
{
    float3 hdr = max(sourceTexture.Sample(_sourceTexture_sampler, uv).xyz, 0.0f.xxx);
    if (frame_outputTransform.x < 0.5f)
    {
        outColor = float4(clamp(hdr, 0.0f.xxx, 1.0f.xxx), 1.0f);
        return;
    }
    float3 mapped = clamp((hdr * ((hdr * 2.5099999904632568359375f) + 0.02999999932944774627685546875f.xxx)) / ((hdr * ((hdr * 2.4300000667572021484375f) + 0.589999973773956298828125f.xxx)) + 0.14000000059604644775390625f.xxx), 0.0f.xxx, 1.0f.xxx);
    outColor = float4(pow(mapped, 0.4545454680919647216796875f.xxx), 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outColor = outColor;
    return stage_output;
}
