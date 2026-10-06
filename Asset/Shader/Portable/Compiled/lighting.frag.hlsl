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

Texture2D<float4> environmentTexture : register(t6, space2);
SamplerState _environmentTexture_sampler : register(s6, space2);
Texture2D<float4> positionTexture : register(t2, space2);
SamplerState _positionTexture_sampler : register(s2, space2);
Texture2D<float4> albedoTexture : register(t0, space2);
SamplerState _albedoTexture_sampler : register(s0, space2);
Texture2D<float4> emissiveTexture : register(t5, space2);
SamplerState _emissiveTexture_sampler : register(s5, space2);
Texture2D<float4> normalTexture : register(t1, space2);
SamplerState _normalTexture_sampler : register(s1, space2);
Texture2D<float4> materialTexture : register(t4, space2);
SamplerState _materialTexture_sampler : register(s4, space2);
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

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + ((1.0f.xxx - F0) * pow(1.0f - cosTheta, 5.0f));
}

float SchlickGGX(float NdotV, float roughness)
{
    float k = ((roughness * roughness) + 1.0f) / 8.0f;
    return NdotV / ((NdotV * (1.0f - k)) + k);
}

float G_Smith(float3 N, float3 V, float3 L, float roughness)
{
    float param = clamp(dot(N, V), 0.0f, 1.0f);
    float param_1 = roughness;
    float param_2 = clamp(dot(N, L), 0.0f, 1.0f);
    float param_3 = roughness;
    return SchlickGGX(param, param_1) * SchlickGGX(param_2, param_3);
}

float D_GTR2(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = ((NdotH * NdotH) * (a2 - 1.0f)) + 1.0f;
    float denominator = (3.1415927410125732421875f * d) * d;
    float _120;
    if (denominator > 0.0f)
    {
        _120 = a2 / denominator;
    }
    else
    {
        _120 = 0.0f;
    }
    return _120;
}

float2 DirectionToSkyUV(float3 direction)
{
    return float2((atan2(direction.x, direction.z) * 0.15915490686893463134765625f) + 0.5f, 0.5f - (asin(clamp(direction.y, -1.0f, 1.0f)) * 0.3183098137378692626953125f));
}

float3 EnvironmentReflection(float3 worldPosition, float3 n, float3 v, float3 base, float metallic, float roughness)
{
    float3 reflection = reflect(-v, n);
    bool3 _168 = (abs(reflection.y) < 0.999000012874603271484375f).xxx;
    float3 tangent = normalize(cross(float3(_168.x ? float3(0.0f, 1.0f, 0.0f).x : float3(1.0f, 0.0f, 0.0f).x, _168.y ? float3(0.0f, 1.0f, 0.0f).y : float3(1.0f, 0.0f, 0.0f).y, _168.z ? float3(0.0f, 1.0f, 0.0f).z : float3(1.0f, 0.0f, 0.0f).z), reflection));
    float3 bitangent = cross(reflection, tangent);
    float2 seed = worldPosition.xy + worldPosition.z.xx;
    float _noise = frac(52.98291778564453125f * frac(dot(seed, float2(0.067110560834407806396484375f, 0.005837149918079376220703125f))));
    float angle = (_noise * 2.0f) * 3.1415927410125732421875f;
    float s = sin(angle);
    float c = cos(angle);
    float3 accumulated = 0.0f.xxx;
    for (int i = 0; i < 8; i++)
    {
        float radius = sqrt(float(i) + 0.5f) * 0.3535533845424652099609375f;
        float theta = float(i) * 2.3999631404876708984375f;
        float2 offset = ((float2(cos(theta), sin(theta)) * radius) * roughness) * roughness;
        float2 rotated = float2((offset.x * c) - (offset.y * s), (offset.x * s) + (offset.y * c));
        float3 direction = normalize((reflection + (tangent * rotated.x)) + (bitangent * rotated.y));
        float3 param = direction;
        accumulated += environmentTexture.Sample(_environmentTexture_sampler, DirectionToSkyUV(param)).xyz;
    }
    float3 f0 = base * metallic;
    float dotNV = max(dot(n, v), 0.0f);
    float3 fresnel = f0 + ((max((1.0f - roughness).xxx, f0) - f0) * pow(1.0f - dotNV, 5.0f));
    return (accumulated * 0.125f) * fresnel;
}

void frag_main()
{
    float4 p = positionTexture.Sample(_positionTexture_sampler, uv);
    if (p.w < 0.5f)
    {
        outColor = float4(lerp(float3(0.02500000037252902984619140625f, 0.0350000001490116119384765625f, 0.064999997615814208984375f), float3(0.119999997317790985107421875f, 0.189999997615814208984375f, 0.300000011920928955078125f), (1.0f - uv.y).xxx), 1.0f);
        return;
    }
    float4 albedo = albedoTexture.Sample(_albedoTexture_sampler, uv);
    float4 emission = emissiveTexture.Sample(_emissiveTexture_sampler, uv);
    float3 emissive = emission.xyz * emission.w;
    if (normalTexture.Sample(_normalTexture_sampler, uv).w > 0.5f)
    {
        outColor = float4(albedo.xyz + emissive, albedo.w);
        return;
    }
    float4 material = materialTexture.Sample(_materialTexture_sampler, uv);
    int flags = int(material.w + 0.5f);
    float metallic = clamp(material.x, 0.0f, 1.0f);
    float roughness = clamp(material.y, 0.0f, 1.0f);
    float3 n = normalize(normalTexture.Sample(_normalTexture_sampler, uv).xyz);
    float3 viewVector = frame_cameraPosition.xyz - p.xyz;
    float3 _435;
    if (length(viewVector) > 1.0000000116860974230803549289703e-07f)
    {
        _435 = normalize(viewVector);
    }
    else
    {
        _435 = n;
    }
    float3 v = _435;
    float3 l = normalize(-frame_lightDirection.xyz);
    float ndl = clamp(dot(n, l), 0.0f, 1.0f);
    float ndv = clamp(dot(n, v), 0.0f, 1.0f);
    float4 clip = mul(float4(p.xyz, 1.0f), frame_lightViewProjection);
    float3 light = clip.xyz / clip.w.xxx;
    float2 shadowUV = (light.xy * float2(0.5f, -0.5f)) + 0.5f.xx;
    float visibility = 1.0f;
    bool _489 = (flags & 1) != 0;
    bool _497;
    if (_489)
    {
        _497 = frame_lightColor.w > 0.5f;
    }
    else
    {
        _497 = _489;
    }
    bool _500 = _497 && (ndl > 0.0f);
    bool _508;
    if (_500)
    {
        _508 = all(bool2(shadowUV.x >= 0.0f.xx.x, shadowUV.y >= 0.0f.xx.y));
    }
    else
    {
        _508 = _500;
    }
    bool _515;
    if (_508)
    {
        _515 = all(bool2(shadowUV.x <= 1.0f.xx.x, shadowUV.y <= 1.0f.xx.y));
    }
    else
    {
        _515 = _508;
    }
    bool _521;
    if (_515)
    {
        _521 = light.z >= 0.0f;
    }
    else
    {
        _521 = _515;
    }
    bool _527;
    if (_521)
    {
        _527 = light.z <= 1.0f;
    }
    else
    {
        _527 = _521;
    }
    if (_527)
    {
        visibility = 0.0f;
        uint _538_dummy_parameter;
        float2 texel = 1.0f.xx / float2(int2(spvTextureSize(shadowTexture, uint(0), _538_dummy_parameter)));
        float bias = max(0.0007999999797903001308441162109375f, 0.0030000000260770320892333984375f * (1.0f - ndl));
        for (int y = -2; y <= 2; y++)
        {
            for (int x = -2; x <= 2; x++)
            {
                float3 _582 = float3(shadowUV + (float2(float(x), float(y)) * texel), light.z - bias);
                visibility += (shadowTexture.SampleCmp(_shadowTexture_sampler, _582.xy, _582.z) / 25.0f);
            }
        }
    }
    float3 diffuse = 0.0f.xxx;
    float3 ambient = 0.0f.xxx;
    float3 specular = 0.0f.xxx;
    if (ndl > 0.0f)
    {
        diffuse = clamp((frame_lightColor.xyz * ndl) * lerp(0.100000001490116119384765625f, 1.0f, visibility), 0.0f.xxx, 1.0f.xxx);
        ambient = frame_ambientColor.xyz;
        float3 halfVector = v + l;
        float3 _624;
        if (length(halfVector) > 1.0000000116860974230803549289703e-07f)
        {
            _624 = normalize(halfVector);
        }
        else
        {
            _624 = n;
        }
        float3 h = _624;
        float3 f0 = lerp(0.039999999105930328369140625f.xxx, 1.0f.xxx, metallic.xxx);
        float param = clamp(dot(v, h), 0.0f, 1.0f);
        float3 param_1 = f0;
        float3 f = FresnelSchlick(param, param_1);
        float3 param_2 = n;
        float3 param_3 = v;
        float3 param_4 = l;
        float param_5 = roughness;
        float g = G_Smith(param_2, param_3, param_4, param_5);
        float param_6 = clamp(dot(n, h), 0.0f, 1.0f);
        float param_7 = roughness;
        float d = D_GTR2(param_6, param_7);
        float3 brdf = (f * (d * g)) / max((4.0f * ndl) * ndv, 0.001000000047497451305389404296875f).xxx;
        specular = ((brdf * frame_lightColor.xyz) * ndl) * lerp(1.0f, visibility, 1.0f - roughness);
    }
    float3 environment = 0.0f.xxx;
    bool _697 = (flags & 2) != 0;
    bool _703;
    if (_697)
    {
        _703 = frame_cameraPosition.w > 0.5f;
    }
    else
    {
        _703 = _697;
    }
    if (_703 && (metallic > 0.0f))
    {
        float3 param_8 = p.xyz;
        float3 param_9 = n;
        float3 param_10 = v;
        float3 param_11 = albedo.xyz;
        float param_12 = metallic;
        float param_13 = roughness;
        environment = EnvironmentReflection(param_8, param_9, param_10, param_11, param_12, param_13);
    }
    float3 color = (((diffuse * albedo.xyz) + ((ambient * albedo.xyz) * (1.0f - metallic))) + specular) + environment;
    outColor = float4(clamp(color, 0.0f.xxx, 1.0f.xxx) + emissive, albedo.w);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outColor = outColor;
    return stage_output;
}
