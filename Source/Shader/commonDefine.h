#ifdef __cplusplus
// C++ Build
#pragma once

#else
// HLSL Build
#define TexSlot(i) (t##i)

#endif

#define LOW_RESOLUTION

#define TextureSlot_Albedo		(0)
#define TextureSlot_Normal		(1)
#define TextureSlot_Roughness	(2)
#define TextureSlot_Metallic	(3)
#define TextureSlot_AO			(4)
#define TextureSlot_HeightMap	(5)
#define TextureSlot_EmissiveMap	(6)
#define TextureSlot_ShadowMap	(7)

#define TextureSlot_Max			(8)

#define LIGHT_TYPE_NONE			(0)
#define LIGHT_TYPE_DIRECTIONAL	(1)
#define LIGHT_TYPE_POINT		(2)
#define LIGHT_TYPE_SPOT			(3)


#ifdef LOW_RESOLUTION
#define SHADOWMAP_SIZE			(4096)
#define LIGHT_MAX_COUNT			(16)
#else
#define SHADOWMAP_SIZE			(16384)
#define LIGHT_MAX_COUNT			(64)
#endif // LOWRESOLUTION





