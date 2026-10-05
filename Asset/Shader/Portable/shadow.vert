#version 450
layout(location=0) in vec3 position;
layout(location=2) in vec4 world0;
layout(location=3) in vec4 world1;
layout(location=4) in vec4 world2;
layout(location=5) in vec4 world3;
layout(set=1,binding=0,std140) uniform Frame { mat4 viewProjection; mat4 lightViewProjection; vec4 lightDirection; } frame;
void main() { gl_Position = frame.lightViewProjection * mat4(world0,world1,world2,world3) * vec4(position,1); }
