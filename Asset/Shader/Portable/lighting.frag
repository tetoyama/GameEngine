#version 450
#extension GL_GOOGLE_include_directive : require
#define float3 vec3
#define saturate(x) clamp((x),0.0,1.0)
#define PI 3.14159265359
#include "../../../Source/Shader/Material/BRDF.hlsli"
layout(location=0) in vec2 uv;
layout(set=2,binding=0) uniform sampler2D albedoTexture;
layout(set=2,binding=1) uniform sampler2D normalTexture;
layout(set=2,binding=2) uniform sampler2D positionTexture;
layout(set=2,binding=3) uniform sampler2DShadow shadowTexture;
layout(set=2,binding=4) uniform sampler2D materialTexture;
layout(set=2,binding=5) uniform sampler2D emissiveTexture;
layout(set=2,binding=6) uniform sampler2D environmentTexture;
layout(set=3,binding=0,std140) uniform Frame { mat4 viewProjection; mat4 lightViewProjection; vec4 lightDirection; vec4 lightColor; vec4 ambientColor; vec4 outputTransform; vec4 cameraPosition; } frame;
layout(location=0) out vec4 outColor;

vec2 DirectionToSkyUV(vec3 direction) {
    return vec2(atan(direction.x,direction.z)*.1591549+.5,
                .5-asin(clamp(direction.y,-1,1))*.3183098);
}
vec3 EnvironmentReflection(vec3 worldPosition,vec3 n,vec3 v,vec3 base,float metallic,float roughness) {
    vec3 reflection=reflect(-v,n);
    vec3 tangent=normalize(cross(abs(reflection.y)<.999?vec3(0,1,0):vec3(1,0,0),reflection));
    vec3 bitangent=cross(reflection,tangent);
    vec2 seed=worldPosition.xy+worldPosition.z;
    float noise=fract(52.9829189*fract(dot(seed,vec2(.06711056,.00583715))));
    float angle=noise*2*PI, s=sin(angle), c=cos(angle);
    vec3 accumulated=vec3(0);
    for(int i=0;i<8;++i) {
        float radius=sqrt(float(i)+.5)*.35355339;
        float theta=float(i)*2.3999632;
        vec2 offset=vec2(cos(theta),sin(theta))*radius*roughness*roughness;
        vec2 rotated=vec2(offset.x*c-offset.y*s,offset.x*s+offset.y*c);
        vec3 direction=normalize(reflection+tangent*rotated.x+bitangent*rotated.y);
        accumulated+=texture(environmentTexture,DirectionToSkyUV(direction)).rgb;
    }
    vec3 f0=base*metallic;
    float dotNV=max(dot(n,v),0);
    vec3 fresnel=f0+(max(vec3(1-roughness),f0)-f0)*pow(1-dotNV,5);
    return accumulated*.125*fresnel;
}
void main() {
    vec4 p=texture(positionTexture,uv);
    if(p.a<.5) { outColor=vec4(mix(vec3(.025,.035,.065),vec3(.12,.19,.3),1-uv.y),1); return; }
    vec4 albedo=texture(albedoTexture,uv), emission=texture(emissiveTexture,uv);
    vec3 emissive=emission.rgb*emission.a;
    if(texture(normalTexture,uv).a>.5) { outColor=vec4(albedo.rgb+emissive,albedo.a); return; }
    vec4 material=texture(materialTexture,uv);
    int flags=int(material.a+.5);
    float metallic=saturate(material.r), roughness=saturate(material.g);
    vec3 n=normalize(texture(normalTexture,uv).xyz);
    vec3 viewVector=frame.cameraPosition.xyz-p.xyz;
    vec3 v=length(viewVector)>1e-7?normalize(viewVector):n;
    vec3 l=normalize(-frame.lightDirection.xyz);
    float ndl=saturate(dot(n,l)), ndv=saturate(dot(n,v));
    vec4 clip=frame.lightViewProjection*vec4(p.xyz,1);
    vec3 light=clip.xyz/clip.w;
    vec2 shadowUV=light.xy*vec2(.5,-.5)+.5;
    float visibility=1;
    if((flags&1)!=0 && frame.lightColor.w>.5 && ndl>0 &&
       all(greaterThanEqual(shadowUV,vec2(0))) && all(lessThanEqual(shadowUV,vec2(1))) && light.z>=0 && light.z<=1) {
        visibility=0;
        vec2 texel=1.0/vec2(textureSize(shadowTexture,0));
        float bias=max(.0008,.003*(1-ndl));
        for(int y=-2;y<=2;++y) for(int x=-2;x<=2;++x)
            visibility+=texture(shadowTexture,vec3(shadowUV+vec2(x,y)*texel,light.z-bias))/25.0;
    }
    // Match the existing MaterialFunc + PBRShader lighting equations.
    vec3 diffuse=vec3(0), ambient=vec3(0), specular=vec3(0);
    if(ndl>0) {
        diffuse=saturate(frame.lightColor.rgb*ndl*mix(.1,1,visibility));
        ambient=frame.ambientColor.rgb;
        vec3 halfVector=v+l;
        vec3 h=length(halfVector)>1e-7?normalize(halfVector):n;
        vec3 f0=mix(vec3(.04),vec3(1),metallic);
        vec3 f=FresnelSchlick(saturate(dot(v,h)),f0);
        float g=G_Smith(n,v,l,roughness), d=D_GTR2(saturate(dot(n,h)),roughness);
        vec3 brdf=(d*g*f)/max(4*ndl*ndv,.001);
        specular=brdf*frame.lightColor.rgb*ndl*mix(1,visibility,1-roughness);
    }
    vec3 environment=vec3(0);
    if((flags&2)!=0 && frame.cameraPosition.w>.5 && metallic>0)
        environment=EnvironmentReflection(p.xyz,n,v,albedo.rgb,metallic,roughness);
    vec3 color=diffuse*albedo.rgb+ambient*albedo.rgb*(1-metallic)+specular+environment;
    outColor=vec4(saturate(color)+emissive,albedo.a);
}
