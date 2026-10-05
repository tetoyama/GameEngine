#pragma once

#include "FrameRenderer.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <stdexcept>

namespace Rendering {
// Compatibility presentation for the existing DX11 ImGui shell. Scene passes
// execute on the selected RHI device; only the final RGBA image crosses to DX11.
// Readback is deliberately synchronous here. This is a migration path, not a
// claim of zero-copy interop or production performance.
class EditorGPUViewport final {
public:
    EditorGPUViewport(RHI::IRHIDevice& device, ID3D11Device* shellDevice, ID3D11DeviceContext* shellContext)
        : renderer(device, "Asset/Shader/Portable/Compiled"), shellDevice(shellDevice), shellContext(shellContext) {
        if(!shellDevice || !shellContext) throw std::invalid_argument("Missing editor presentation device");
    }

    ID3D11ShaderResourceView* Render(const RenderScene& scene, uint32_t width, uint32_t height) {
        if (!width || !height) return nullptr;
        renderer.Resize(width, height);
        renderer.Render(scene, false);
        RHI::TextureReadback image;
        if (!renderer.Capture(image)) throw std::runtime_error("Selected RHI viewport readback failed");
        if (!texture || image.width != imageWidth || image.height != imageHeight) {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> replacement;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> replacementView;
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = image.width; desc.Height = image.height;
            desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(shellDevice->CreateTexture2D(&desc, nullptr, &replacement)) ||
                FAILED(shellDevice->CreateShaderResourceView(replacement.Get(), nullptr, &replacementView)))
                throw std::runtime_error("Editor viewport presentation texture creation failed");
            texture = std::move(replacement); view = std::move(replacementView);
            imageWidth = image.width; imageHeight = image.height;
        }
        shellContext->UpdateSubresource(texture.Get(), 0, nullptr, image.pixels.data(), image.rowPitch, 0);
        return view.Get();
    }
private:
    FrameRenderer renderer;
    Microsoft::WRL::ComPtr<ID3D11Device> shellDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> shellContext;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    uint32_t imageWidth = 0, imageHeight = 0;
};
}
