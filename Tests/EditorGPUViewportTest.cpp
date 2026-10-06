#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Service/Graphics/Portable/EditorGPUViewport.h"
#include "Service/Graphics/Portable/RenderPacketAdapter.h"
#include "Service/Graphics/Portable/RenderMath.h"
#include "Service/Graphics/RHI/SDL/SDLGPUBackend.h"
#include "Service/Graphics/RHI/RHIService.h"
#include <iostream>

using Microsoft::WRL::ComPtr;
static void Require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
static std::vector<std::byte> ReadImage(ID3D11ShaderResourceView* view, ID3D11Device* device, ID3D11DeviceContext* context, UINT width, UINT height) {
    Require(view != nullptr, "Missing editor viewport image");
    ComPtr<ID3D11Resource> source; view->GetResource(&source);
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width; desc.Height=height; desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)), "Cannot create staging image");
    context->CopyResource(staging.Get(),source.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Require(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)), "Cannot read editor image");
    std::vector<std::byte> pixels(width*height*4);
    for(UINT row=0;row<height;++row) std::memcpy(pixels.data()+row*width*4,static_cast<std::byte*>(mapped.pData)+row*mapped.RowPitch,width*4);
    context->Unmap(staging.Get(),0); return pixels;
}
int main(int argc,char** argv) {
    SDL_SetMainReady();
    if(!SDL_Init(SDL_INIT_VIDEO)) return 1;
    int result=0;
    try {
        Require(argc==2,"Specify d3d12 or vulkan");
        const auto backend=std::string_view(argv[1])=="d3d12"?RHI::BackendType::Direct3D12:RHI::BackendType::Vulkan;
        RHI::RenderHardwareInterfaceService service;
        RHI::RegisterSDLGPUBackends(service.GetRegistry());
        Require(service.SelectBackend(backend),"Requested scene API unavailable");
        Require(service.AdoptDevice(service.GetBackend()->CreateDevice({})),"Scene device creation failed");
        Require(service.GetDevice()->GetBackendType()==backend,"Rendering API was substituted");
        ComPtr<ID3D11Device> shell; ComPtr<ID3D11DeviceContext> shellContext;
        Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&shell,nullptr,&shellContext)),"DX11 shell creation failed");
        {
            Rendering::FrameRenderer meshOwner(*service.GetDevice(),"Asset/Shader/Portable/Compiled");
            const Rendering::Vertex vertices[]={{{-.8f,-.7f,.5f},{0,0,-1}},{{0,.8f,.5f},{0,0,-1}},{{.8f,-.7f,.5f},{0,0,-1}}};
            const uint32_t indices[]={0,1,2,2,1,0};
            const auto mesh=meshOwner.UploadMesh(vertices,indices);
            RenderPacket packet; packet.passMask=RenderPacketPassMask::GBuffer;
            const auto world=Rendering::Identity(); std::copy(world.begin(),world.end(),packet.transform.worldMatrix.values);
            // Direction points from the light toward the surface; the test
            // triangle normal faces -Z, so illuminate it from -Z.
            Rendering::FrameUniforms uniforms{world,world,{0,0,1,0}};
            auto scene=Rendering::ConvertRenderPackets(std::span(&packet,1),uniforms,1,[&](const RenderPacket&){return std::vector{mesh};}).scene;
            Require(scene.draws.size()==1,"Existing packet/binding conversion failed");
            Rendering::EditorGPUViewport viewport(*service.GetDevice(),shell.Get(),shellContext.Get());
            scene.draws[0].instance.color={1,.03f,.03f,1};
            auto red=ReadImage(viewport.Render(scene,160,120),shell.Get(),shellContext.Get(),160,120);
            scene.draws[0].instance.color={.03f,.03f,1,1};
            auto blue=ReadImage(viewport.Render(scene,160,120),shell.Get(),shellContext.Get(),160,120);
            size_t changed=0; for(size_t i=0;i<red.size();i+=4) if(red[i]!=blue[i]) ++changed;
            Require(changed>1000,"Selected API geometry/material changes did not reach the editor image");
            Require(ReadImage(viewport.Render(scene,73,51),shell.Get(),shellContext.Get(),73,51).size()==73*51*4,"Viewport resize failed");
            Require(viewport.Render(scene,0,0)==nullptr,"Zero-sized viewport was rendered");
        }
        std::cout<<argv[1]<<" scene -> DX11 editor viewport: material, resize and readback passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<"\n"; result=1; }
    SDL_Quit(); return result;
}
