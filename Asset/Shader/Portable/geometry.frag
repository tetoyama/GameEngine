#version 450
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec4 albedo;
layout(location=3) in vec2 uv;
layout(location=4) in float unlit;
layout(set=2,binding=0) uniform sampler2D baseColorTexture;
layout(location=0) out vec4 outAlbedo;
layout(location=1) out vec4 outNormal;
layout(location=2) out vec4 outPosition;
void main() {
    outAlbedo = albedo * texture(baseColorTexture,uv);
    outNormal = vec4(normalize(worldNormal),unlit);
    outPosition = vec4(worldPosition,1);
}
