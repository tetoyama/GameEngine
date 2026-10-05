#version 450
layout(location=0) in vec2 uv;
layout(set=2,binding=0) uniform sampler2D albedoTexture;
layout(set=2,binding=1) uniform sampler2D normalTexture;
layout(set=2,binding=2) uniform sampler2D positionTexture;
layout(set=2,binding=3) uniform sampler2DShadow shadowTexture;
layout(set=3,binding=0,std140) uniform Frame { mat4 viewProjection; mat4 lightViewProjection; vec4 lightDirection; vec4 lightColor; vec4 ambientColor; } frame;
layout(location=0) out vec4 outColor;
void main() {
    vec4 p = texture(positionTexture,uv);
    if(p.a < 0.5) { outColor=vec4(mix(vec3(.025,.035,.065),vec3(.12,.19,.3),1-uv.y),1); return; }
    if(texture(normalTexture,uv).a > .5) { outColor=vec4(texture(albedoTexture,uv).rgb,1); return; }
    vec3 n=normalize(texture(normalTexture,uv).xyz);
    vec4 clip=frame.lightViewProjection * vec4(p.xyz,1);
    vec3 light=clip.xyz/clip.w;
    vec2 shadowUV=light.xy*vec2(.5,-.5)+.5;
    float visibility=1;
    if(frame.lightColor.w>.5 && all(greaterThanEqual(shadowUV,vec2(0))) && all(lessThanEqual(shadowUV,vec2(1))) && light.z>=0 && light.z<=1) {
        visibility=0;
        vec2 texel=1.0/vec2(textureSize(shadowTexture,0));
        float bias=max(.0008,.003*(1-dot(n,-frame.lightDirection.xyz)));
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
            visibility += texture(shadowTexture,vec3(shadowUV+vec2(x,y)*texel,light.z-bias))/9.0;
    }
    vec3 base=texture(albedoTexture,uv).rgb;
    float diffuse=max(dot(n,-frame.lightDirection.xyz),0);
    outColor=vec4(base*(frame.ambientColor.rgb+diffuse*visibility*frame.lightColor.rgb),1);
}
