#pragma once
#include "RenderScene.h"
#include "Scene/System/Render/RenderSystem/RenderPacket/RenderPacket.h"
#include <functional>
#include <span>
#include <stdexcept>
namespace Rendering {
struct PacketConversion { RenderScene scene; size_t unsupportedPackets=0, unresolvedMeshes=0; };
// A model packet can represent multiple submeshes. The resolver emits the
// existing ModelGeometryRuntimeMesh for each selected submesh; ModelData stays outside the
// renderer. Unsupported packet kinds are reported, never silently substituted.
using PacketMeshResolver=std::function<std::vector<ModelGeometryRuntimeMesh>(const RenderPacket&)>;
inline PacketConversion ConvertRenderPackets(std::span<const RenderPacket> packets,
    const FrameUniforms& frame,uint64_t generation,const PacketMeshResolver& resolver) {
    if(!resolver) throw std::invalid_argument("Missing render packet mesh resolver");
    PacketConversion result; result.scene.frame=frame; result.scene.generation=generation;
    for(const auto& packet:packets) {
        if(packet.kind!=RenderPacketKind::Model || packet.layer!=RenderLayer::Opaque3D ||
           !HasRenderPacketPass(packet.passMask,RenderPacketPassMask::GBuffer)) { ++result.unsupportedPackets; continue; }
        auto meshes=resolver(packet); if(meshes.empty()) { ++result.unresolvedMeshes; continue; }
        Instance instance;
        // DirectX uses row-major storage with row vectors. The same 16 floats
        // are the transposed matrix for this column-vector ABI; do not transpose
        // a second time or parent-world translation and rotation will break.
        std::copy_n(packet.transform.worldMatrix.values,16,instance.world.begin());
        if(const auto* material=packet.modelMaterial.GetDescriptor()) {
            if(material->renderState.alphaMode!=MaterialAlphaMode::Opaque || !material->textures.empty()) {
                ++result.unsupportedPackets; continue;
            }
            instance.color=material->parameters.baseColor;
        }
        for(auto mesh:meshes) {
            if(!mesh.IsReady()) { ++result.unresolvedMeshes; continue; }
            result.scene.draws.push_back({mesh,instance,HasRenderPacketPass(packet.passMask,RenderPacketPassMask::Shadow)});
        }
    }
    return result;
}
}
