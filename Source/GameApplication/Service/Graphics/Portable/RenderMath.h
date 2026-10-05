#pragma once
#include "RenderScene.h"
#include <cmath>
#include <stdexcept>
namespace Rendering {
using Vec3 = std::array<float,3>;
inline Vec3 Sub(Vec3 a,Vec3 b) { return {a[0]-b[0],a[1]-b[1],a[2]-b[2]}; }
inline float Dot(Vec3 a,Vec3 b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline Vec3 Cross(Vec3 a,Vec3 b) { return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}; }
inline Vec3 Normalize(Vec3 v) { float n=std::sqrt(Dot(v,v)); if(n<1e-8f) throw std::invalid_argument("Degenerate vector"); return {v[0]/n,v[1]/n,v[2]/n}; }
inline Matrix Identity() { return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; }
inline Matrix Multiply(const Matrix& a,const Matrix& b) {
    Matrix r{}; for(int c=0;c<4;++c) for(int row=0;row<4;++row) for(int k=0;k<4;++k) r[c*4+row]+=a[k*4+row]*b[c*4+k]; return r;
}
inline Matrix Transform(Vec3 position,Vec3 scale,float yaw=0) {
    float c=std::cos(yaw),s=std::sin(yaw);
    return {c*scale[0],0,-s*scale[0],0, 0,scale[1],0,0, s*scale[2],0,c*scale[2],0,position[0],position[1],position[2],1};
}
inline Matrix LookAt(Vec3 eye,Vec3 target,Vec3 up={0,1,0}) {
    auto z=Normalize(Sub(target,eye)), x=Normalize(Cross(up,z)), y=Cross(z,x);
    return {x[0],y[0],z[0],0,x[1],y[1],z[1],0,x[2],y[2],z[2],0,-Dot(x,eye),-Dot(y,eye),-Dot(z,eye),1};
}
inline Matrix Perspective(float fov,float aspect,float nearPlane,float farPlane) {
    if(aspect<=0 || nearPlane<=0 || farPlane<=nearPlane || fov<=0 || fov>=3.14159f) throw std::invalid_argument("Invalid perspective");
    float y=1/std::tan(fov/2),z=farPlane/(farPlane-nearPlane);
    return {y/aspect,0,0,0,0,y,0,0,0,0,z,1,0,0,-nearPlane*z,0};
}
inline Matrix Orthographic(float width,float height,float nearPlane,float farPlane) {
    if(width<=0 || height<=0 || farPlane<=nearPlane) throw std::invalid_argument("Invalid orthographic projection");
    return {2/width,0,0,0,0,2/height,0,0,0,0,1/(farPlane-nearPlane),0,0,0,-nearPlane/(farPlane-nearPlane),1};
}
}
