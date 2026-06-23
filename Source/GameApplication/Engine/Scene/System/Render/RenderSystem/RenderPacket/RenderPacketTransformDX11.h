#pragma once

#include <cstddef>
#include <DirectXMath.h>

#include "RenderPacket.h"

inline void StoreRenderPacketMatrix(
    RenderPacketMatrix4x4& destination,
    DirectX::FXMMATRIX matrix
) noexcept {
    DirectX::XMFLOAT4X4 value{};
    DirectX::XMStoreFloat4x4(&value, matrix);
    const float source[16] = {
        value._11, value._12, value._13, value._14,
        value._21, value._22, value._23, value._24,
        value._31, value._32, value._33, value._34,
        value._41, value._42, value._43, value._44
    };
    for(std::size_t index = 0; index < 16; ++index){
        destination.values[index] = source[index];
    }
}

inline DirectX::XMMATRIX LoadRenderPacketMatrix(
    const RenderPacketMatrix4x4& source
) noexcept {
    return DirectX::XMMatrixSet(
        source.values[0], source.values[1], source.values[2], source.values[3],
        source.values[4], source.values[5], source.values[6], source.values[7],
        source.values[8], source.values[9], source.values[10], source.values[11],
        source.values[12], source.values[13], source.values[14], source.values[15]
    );
}
