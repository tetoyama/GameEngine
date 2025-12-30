#ifdef __cplusplus
// C++ Build
#pragma once

#else
// HLSL Build
#define TexSlot(i) (t##i)

#endif

#define BONE_MAX_COUNT			(256)

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

#define GBufferSlot_Albedo		(0)
#define GBufferSlot_Normal		(1)
#define GBufferSlot_Position	(2)
#define GBufferSlot_Material	(3)
#define GBufferSlot_Param		(4)

#define GBufferSlot_Max			(5)

#define LightingSlot_GAlbedo    (0)
#define LightingSlot_GNormal    (1)
#define LightingSlot_GPosition  (2)
#define LightingSlot_GMaterial  (3)
#define LightingSlot_GParam     (4)
#define LightingSlot_ShadowMap  (5)

#define LightingSlot_Max        (6)


#define LIGHT_TYPE_NONE			(0)
#define LIGHT_TYPE_DIRECTIONAL	(1)
#define LIGHT_TYPE_POINT		(2)
#define LIGHT_TYPE_SPOT			(3)


#ifdef LOW_RESOLUTION
#define SHADOWMAP_SIZE			(4096)
#define LIGHT_MAX_COUNT			(9)
#else
#define SHADOWMAP_SIZE			(16384)
#define LIGHT_MAX_COUNT			(64)
#endif // LOWRESOLUTION





