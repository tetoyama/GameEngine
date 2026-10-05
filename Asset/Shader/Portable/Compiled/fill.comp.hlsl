static const uint3 gl_WorkGroupSize = uint3(8u, 8u, 1u);

cbuffer Fill : register(b0, space2)
{
    float4 fill_color : packoffset(c0);
};

RWTexture2D<unorm float4> destination : register(u0, space1);

static uint3 gl_GlobalInvocationID;
struct SPIRV_Cross_Input
{
    uint3 gl_GlobalInvocationID : SV_DispatchThreadID;
};

uint2 spvImageSize(RWTexture2D<unorm float4> Tex, out uint Param)
{
    uint2 ret;
    Tex.GetDimensions(ret.x, ret.y);
    Param = 0u;
    return ret;
}

void comp_main()
{
    int2 p = int2(gl_GlobalInvocationID.xy);
    uint _24_dummy_parameter;
    int2 _24 = int2(spvImageSize(destination, _24_dummy_parameter));
    if (all(bool2(p.x < _24.x, p.y < _24.y)))
    {
        destination[p] = fill_color;
    }
}

[numthreads(8, 8, 1)]
void main(SPIRV_Cross_Input stage_input)
{
    gl_GlobalInvocationID = stage_input.gl_GlobalInvocationID;
    comp_main();
}
