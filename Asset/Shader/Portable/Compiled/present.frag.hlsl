Texture2D<float4> sourceTexture : register(t0, space2);
SamplerState _sourceTexture_sampler : register(s0, space2);

static float4 outColor;
static float2 uv;

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
    outColor = sourceTexture.Sample(_sourceTexture_sampler, uv);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outColor = outColor;
    return stage_output;
}
