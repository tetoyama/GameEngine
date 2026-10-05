#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 world0;
layout(location=3) in vec4 world1;
layout(location=4) in vec4 world2;
layout(location=5) in vec4 world3;
layout(location=6) in vec4 color;
layout(set=1,binding=0,std140) uniform Frame { mat4 viewProjection; mat4 lightViewProjection; vec4 lightDirection; } frame;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec4 albedo;
void main() {
    mat4 world = mat4(world0,world1,world2,world3);
    vec4 p = world * vec4(position,1);
    worldPosition = p.xyz;
    worldNormal = transpose(inverse(mat3(world))) * normal;
    albedo = color;
    gl_Position = frame.viewProjection * p;
}
