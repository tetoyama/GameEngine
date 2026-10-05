#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Service/Graphics/RHI/SDL/SDLGPUBackend.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <cmath>
namespace {
void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
}
int main(int argc,char** argv) {
    using namespace RHI;
    SDL_SetMainReady();
    try {
        Check(argc==3,"Expected backend and shader directory"); Check(SDL_Init(SDL_INIT_VIDEO),"SDL initialization failed");
        struct Quit { ~Quit(){SDL_Quit();} } quit;
        std::string backend=argv[1]; auto type=backend=="d3d12"?BackendType::Direct3D12:backend=="metal"?BackendType::Metal:BackendType::Vulkan;
        SDLGPUBackend factory(type); DeviceCreateDesc dd; dd.enableDebugLayer=true;
        auto device=factory.CreateDevice(dd); Check(bool(device),"GPU device creation failed");
        BufferDesc bd; bd.byteSize=16; bd.bindFlags=BufferBindFlags::Constant;
        auto old=device->CreateBuffer(bd); Check(bool(old)&&device->DestroyBuffer(old),"Buffer allocation/release failed");
        auto uniform=device->CreateBuffer(bd); Check(uniform!=old && !device->GetBufferDesc(old),"Stale resource handle accepted");
        TextureDesc td; td.width=37; td.height=19; td.bindFlags=TextureBindFlags::ShaderResource|TextureBindFlags::UnorderedAccess;
        auto texture=device->CreateTexture(td); Check(bool(texture),"Compute output allocation failed");
        TextureViewDesc vd; vd.texture=texture; vd.type=TextureViewType::UnorderedAccess;
        auto view=device->CreateTextureView(vd); Check(bool(view),"Compute output view failed");
        ShaderDesc sd; sd.stage=ShaderStage::Compute; sd.uniformBuffers=1; sd.writableStorageTextures=1; sd.threadGroupSize={8,8,1};
        sd.codeFormat=type==BackendType::Direct3D12?ShaderDesc::CodeFormat::DXIL:type==BackendType::Metal?ShaderDesc::CodeFormat::MetalSource:ShaderDesc::CodeFormat::SPIRV;
        sd.entryPoint=type==BackendType::Metal?"main0":"main";
        std::string extension=type==BackendType::Direct3D12?".dxil":type==BackendType::Metal?".msl":".spv";
        std::ifstream file(std::string(argv[2])+"/fill.comp"+extension,std::ios::binary);
        Check(bool(file),"Missing compute shader"); std::vector<char> code{std::istreambuf_iterator<char>(file),{}};
        if(type==BackendType::Metal) code.push_back(0);
        auto shader=device->CreateShader(sd,std::as_bytes(std::span(code))); Check(bool(shader),"Compute shader creation failed");
        PipelineStateDesc pd; pd.computeShader=shader; auto pipeline=device->CreatePipelineState(pd); Check(bool(pipeline),"Compute pipeline creation failed");
        Check(device->DestroyShader(shader),"Shader release failed");
        auto commands=device->CreateCommandList({}); commands->Begin();
        bool foreignAccepted=true; std::thread foreign([&]{foreignAccepted=commands->SetConstantBuffer(ShaderStage::Compute,0,uniform);}); foreign.join();
        Check(!foreignAccepted,"Command recording accepted a foreign thread");
        Check(commands->SetPipelineState(pipeline),"Compute pipeline binding failed");
        Check(!commands->SetConstantBuffer(static_cast<ShaderStage>(255),0,uniform),"Invalid shader stage accepted");
        const std::array<float,4> color{.25f,.5f,.75f,1};
        Check(commands->UpdateBuffer(uniform,std::as_bytes(std::span(color))),"Compute uniform update failed");
        Check(commands->SetConstantBuffer(ShaderStage::Compute,0,uniform)&&commands->SetTextureView(ShaderStage::Compute,0,view),"Compute resource binding failed");
        commands->Dispatch(5,3,1); commands->End();
        auto fence=device->CreateFence(); Check(bool(fence)&&!fence->Wait(1,0),"Unsubmitted fence wait accepted");
        IRHICommandList* list=commands.get(); QueueSubmitDesc submit; submit.commandLists={&list,1}; submit.signalFence=fence->GetHandle(); submit.signalValue=1;
        auto* queue=device->GetQueue(CommandQueueType::Graphics);
        Check(queue->Submit(submit)&&!queue->Submit(submit),"Command submitted more than once"); commands.reset();
        Check(fence->Wait(1,5'000'000'000ull)&&fence->GetCompletedValue()==1,"Fence completion/lifetime failed");
        TextureReadback image; Check(device->ReadTexture(texture,image,5'000'000'000ull),"Compute output readback failed");
        Check(image.width==37&&image.height==19&&image.rowPitch==148,"Readback row layout failed");
        const int expected[]={64,128,191,255};
        for(size_t i=0;i<image.pixels.size();++i) Check(std::abs(std::to_integer<int>(image.pixels[i])-expected[i%4])<=1,"Compute shader did not write expected pixels");
        auto fenceHandle=fence->GetHandle(); Check(device->DestroyFence(fenceHandle)&&!fence->Wait(1,0),"Destroyed fence handle accepted");
        device.reset(); Check(!fence->Wait(1,0),"Fence accessed a destroyed device");
        std::cout<<backend<<" GPU resource, shader, compute and fence contracts passed\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<" (SDL: "<<SDL_GetError()<<")\n"; return 1; }
}
