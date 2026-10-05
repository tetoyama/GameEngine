cbuffer Frame : register(b0, space3)
{
    row_major float4x4 frame_viewProjection : packoffset(c0);
    row_major float4x4 frame_lightViewProjection : packoffset(c4);
    float4 frame_lightDirection : packoffset(c8);
    float4 frame_lightColor : packoffset(c9);
    float4 frame_ambientColor : packoffset(c10);
};

Texture2D<float4> positionTexture : register(t2, space2);
SamplerState _positionTexture_sampler : register(s2, space2);
Texture2D<float4> normalTexture : register(t1, space2);
SamplerState _normalTexture_sampler : register(s1, space2);
Texture2D<float4> albedoTexture : register(t0, space2);
SamplerState _albedoTexture_sampler : register(s0, space2);
Texture2D<float4> shadowTexture : register(t3, space2);
SamplerComparisonState _shadowTexture_sampler : register(s3, space2);

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

uint2 spvTextureSize(Texture2D<float4> Tex, uint Level, out uint Param)
{
    uint2 ret;
    Tex.GetDimensions(Level, ret.x, ret.y, Param);
    return ret;
}

void frag_main()
{
    float4 p = positionTexture.Sample(_positionTexture_sampler, uv);
    if (p.w < 0.5f)
    {
        outColor = float4(lerp(float3(0.02500000037252902984619140625f, 0.0350000001490116119384765625f, 0.064999997615814208984375f), float3(0.119999997317790985107421875f, 0.189999997615814208984375f, 0.300000011920928955078125f), (1.0f - uv.y).xxx), 1.0f);
        return;
    }
    if (normalTexture.Sample(_normalTexture_sampler, uv).w > 0.5f)
    {
        outColor = float4(albedoTexture.Sample(_albedoTexture_sampler, uv).xyz, 1.0f);
        return;
    }
    float3 n = normalize(normalTexture.Sample(_normalTexture_sampler, uv).xyz);
    float4 clip = mul(float4(p.xyz, 1.0f), frame_lightViewProjection);
    float3 light = clip.xyz / clip.w.xxx;
    float2 shadowUV = (light.xy * float2(0.5f, -0.5f)) + 0.5f.xx;
    float visibility = 1.0f;
    bool _117 = frame_lightColor.w > 0.5f;
    bool _126;
    if (_117)
    {
        _126 = all(bool2(shadowUV.x >= 0.0f.xx.x, shadowUV.y >= 0.0f.xx.y));
    }
    else
    {
        _126 = _117;
    }
    bool _133;
    if (_126)
    {
        _133 = all(bool2(shadowUV.x <= 1.0f.xx.x, shadowUV.y <= 1.0f.xx.y));
    }
    else
    {
        _133 = _126;
    }
    bool _140;
    if (_133)
    {
        _140 = light.z >= 0.0f;
    }
    else
    {
        _140 = _133;
    }
    bool _146;
    if (_140)
    {
        _146 = light.z <= 1.0f;
    }
    else
    {
        _146 = _140;
    }
    if (_146)
    {
        visibility = 0.0f;
        uint _158_dummy_parameter;
        float2 texel = 1.0f.xx / float2(int2(spvTextureSize(shadowTexture, uint(0), _158_dummy_parameter)));
        float bias = max(0.0007999999797903001308441162109375f, 0.0030000000260770320892333984375f * (1.0f - dot(n, -frame_lightDirection.xyz)));
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                float3 _210 = float3(shadowUV + (float2(float(x), float(y)) * texel), light.z - bias);
                visibility += (shadowTexture.SampleCmp(_shadowTexture_sampler, _210.xy, _210.z) / 9.0f);
            }
        }
    }
    float3 base = albedoTexture.Sample(_albedoTexture_sampler, uv).xyz;
    float diffuse = max(dot(n, -frame_lightDirection.xyz), 0.0f);
    outColor = float4(base * (frame_ambientColor.xyz + (frame_lightColor.xyz * (diffuse * visibility))), 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outColor = outColor;
    return stage_output;
}
