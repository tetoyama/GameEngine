#include "Service/Graphics/Portable/RenderPacketAdapter.h"
#include "Service/Graphics/Portable/RenderMath.h"
#include <iostream>
int main() {
    using namespace Rendering;
    try {
        RenderPacket p; p.kind=RenderPacketKind::Model; p.layer=RenderLayer::Opaque3D;
        p.passMask=RenderPacketPassMask::GBuffer|RenderPacketPassMask::Shadow;
        auto world=Transform({3,4,5},{2,3,4},.5f); std::copy(world.begin(),world.end(),p.transform.worldMatrix.values);
        auto material=std::make_shared<MaterialDescriptor>(); material->parameters.baseColor={.2f,.4f,.6f,1}; p.modelMaterial.ownedDescriptor=material;
        FrameUniforms frame{Identity(),Identity(),{0,-1,0,0}};
        auto converted=ConvertRenderPackets(std::span(&p,1),frame,42,[](const auto&){return std::vector<ModelGeometryRuntimeMesh>{{{7,1},{8,1},24,3,3},{{9,1},{10,1},24,3,3}};});
        if(converted.scene.draws.size()!=2 || converted.scene.draws[0].instance.world!=world || converted.scene.draws[1].instance.color!=material->parameters.baseColor || !converted.scene.draws[0].castsShadow) return 1;
        // Snapshot ownership must survive destruction/mutation of frame packets.
        p.transform.worldMatrix.values[12]=99; material->parameters.baseColor={1,0,0,1}; p.modelMaterial={}; material.reset();
        if(converted.scene.draws[0].instance.world[12]!=3 || converted.scene.draws[0].instance.color[1]!=.4f) return 2;
        p.kind=RenderPacketKind::Particle;
        auto skipped=ConvertRenderPackets(std::span(&p,1),frame,43,[](const auto&){return std::vector<ModelGeometryRuntimeMesh>{{{7,1},{8,1},24,3,3}};});
        if(skipped.unsupportedPackets!=1 || !skipped.scene.draws.empty()) return 3;
        std::cout<<"Render packet ownership and matrix ABI passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 4; }
}
