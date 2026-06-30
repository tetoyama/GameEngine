#ifndef MATERIAL_FUNC_HLSLI
#define MATERIAL_FUNC_HLSLI

// ReCompilePixelShaders currently tracks this file but not transitive includes.
// Lighting Diagnostic Contract verifies these Git blob IDs. When one dependency
// changes, update its blob ID here so MaterialFunc's timestamp invalidates CSO cache.
// dependency:MaterialBRDF.hlsli=9abe9b2cac74ed2ed01906bdf9a9f8d5f7e588f0
// dependency:MaterialLightingEvaluation.hlsli=b91990aad16d0edc91794742b5109d356a974d8c
// dependency:MaterialLightingTraversal.hlsli=a462b8a0a7272ba084485ca53c3c888261843fd9
// dependency:ShadowDepthBias.hlsli=d463ab364204fbda8b6a47b4ebc3ba5093fcddec

#ifndef MATERIAL_DEFINE_HLSLI
#include "MaterialDefine.hlsli"
#endif

#ifdef FORWARD_FUNC_HLSLI
#include "FowardFunc.hlsli"
#else
#include "DeferredFunc.hlsli"
#endif

#ifndef COMMON_DEFINE_H
#include "../commonDefine.h"
#endif

#ifndef COMMON_HLSL
#include "../common.hlsl"
#endif

#include "MaterialBRDF.hlsli"
#include "MaterialLightingEvaluation.hlsli"

#endif // MATERIAL_FUNC_HLSLI
