#version 450
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec4 albedo;
layout(location=3) in vec2 uv;
layout(location=4) in float unlit;
layout(location=5) in vec4 material;
layout(location=6) in vec4 emissive;
layout(set=2,binding=0) uniform sampler2D baseColorTexture;
layout(location=0) out vec4 outAlbedo;
layout(location=1) out vec4 outNormal;
layout(location=2) out vec4 outPosition;
layout(location=3) out vec4 outMaterial;
layout(location=4) out vec4 outEmissive;
void main() {
    outAlbedo = albedo * texture(baseColorTexture,uv);
    if(outAlbedo.a<.01) discard; // Existing GBuffer alpha clip contract.
    outNormal = vec4(normalize(worldNormal),unlit);
    outPosition = vec4(worldPosition,1);
    outMaterial = material;
    outEmissive = emissive;
}
