#include "../Source/Shader/commonDefine.h"
#include "../Source/Shader/common.hlsl"
#include "../Source/Shader/Material/MaterialDefine.hlsli"
#include "../Source/Shader/Material/FowardFunc.hlsli"
#include "../Source/Shader/Material/MaterialFunc.hlsli"
#include "../Source/Shader/Material/PBRShader.hlsli"

float4 main(PS_IN input) : SV_Target
{
    MaterialInput materialInput = GetMaterialInput(input);
    return ShadeMaterial_PBR(materialInput);
}
