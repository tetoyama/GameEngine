#version 450
layout(location=0) in vec2 uv;
layout(set=2,binding=0) uniform sampler2D sourceTexture;
layout(location=0) out vec4 outColor;
void main() { outColor=texture(sourceTexture,uv); }
