#version 450
layout(location=0) in vec2 uv;
layout(set=2,binding=0) uniform sampler2D sourceTexture;
layout(location=0) out vec4 outColor;
void main() {
    vec3 hdr=max(texture(sourceTexture,uv).rgb,vec3(0));
    vec3 mapped=clamp((hdr*(2.51*hdr+.03))/(hdr*(2.43*hdr+.59)+.14),0,1);
    outColor=vec4(pow(mapped,vec3(1.0/2.2)),1);
}
