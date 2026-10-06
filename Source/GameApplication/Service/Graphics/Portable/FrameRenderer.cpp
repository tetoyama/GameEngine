#include "FrameRenderer.h"
#include "Service/Graphics/RHI/RHIRenderGraph.h"
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <tuple>
namespace Rendering {
using namespace RHI;
namespace {
void Require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
template<typename T> std::span<const std::byte> Bytes(std::span<const T> v) { return std::as_bytes(v); }
std::vector<std::byte> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file) throw std::runtime_error("Missing shader: "+path.string());
    auto size=file.tellg(); if(size<=0 || size>16*1024*1024) throw std::runtime_error("Invalid shader size");
    std::vector<std::byte> result(static_cast<size_t>(size)); file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(result.data()),size)) throw std::runtime_error("Failed shader read"); return result;
}
struct Target { TextureHandle texture; TextureViewHandle render,sample; };
struct Batch { ModelGeometryRuntimeMesh mesh; uint32_t first,count; bool shadow; TextureViewHandle albedoTexture; };
}
struct FrameRenderer::Impl {
    IRHIDevice& device;
    IRHIDevice::LifetimeToken deviceLifetime;
    std::filesystem::path shaderDirectory;
    std::vector<ShaderHandle> shaders;
    std::vector<PipelineStateHandle> pipelines;
    uint32_t width=0,height=0,instanceCapacity=0;
    BufferHandle frameBuffer,instanceBuffer;
    SamplerHandle sampler,shadowSampler,materialSampler;
    Target white;
    Target albedo,normal,position,material,emissive,depth,shadow,hdr,output;
    PipelineStateHandle geometryPipeline,shadowPipeline,lightingPipeline,tonePipeline,presentPipeline;
    Format presentFormat=Format::Unknown;
    ShaderHandle fullscreen,presentShader;
    std::unique_ptr<IRHIFence> frameFence;
    uint64_t submitted=0;
    bool targetsInitialized=false;
    Impl(IRHIDevice& d,std::filesystem::path path):device(d),deviceLifetime(d.GetLifetimeToken()),shaderDirectory(std::move(path)) {}
    ~Impl() {
        if(deviceLifetime.expired()) return;
        device.WaitIdle(); DestroyTargets();
        for(auto p:pipelines) device.DestroyPipelineState(p);
        for(auto s:shaders) device.DestroyShader(s);
        if(sampler) device.DestroySampler(sampler);
        if(shadowSampler) device.DestroySampler(shadowSampler);
        if(materialSampler) device.DestroySampler(materialSampler);
        DestroyTarget(white);
        if(frameBuffer) device.DestroyBuffer(frameBuffer);
        if(instanceBuffer) device.DestroyBuffer(instanceBuffer);
        if(frameFence) device.DestroyFence(frameFence->GetHandle());
    }
    void WaitFrame() { Require(!deviceLifetime.expired(),"RHI device expired"); if(submitted) Require(frameFence->Wait(submitted,5'000'000'000ull),"GPU frame completion timed out"); }
    ShaderHandle Shader(const char* name,ShaderStage stage,uint32_t textures=0,uint32_t uniforms=0) {
        ShaderDesc desc; desc.stage=stage; desc.sampledTextures=textures; desc.uniformBuffers=uniforms; desc.debugName=name;
        const char* extension=nullptr;
        switch(device.GetBackendType()) {
        case BackendType::Direct3D12: extension=".dxil"; desc.codeFormat=ShaderDesc::CodeFormat::DXIL; desc.entryPoint="main"; break;
        case BackendType::Vulkan: extension=".spv"; desc.codeFormat=ShaderDesc::CodeFormat::SPIRV; desc.entryPoint="main"; break;
        case BackendType::Metal: extension=".msl"; desc.codeFormat=ShaderDesc::CodeFormat::MetalSource; desc.entryPoint="main0"; break;
        default: throw std::runtime_error("Portable shader bundle does not support selected backend");
        }
        auto bytes=ReadFile(shaderDirectory/(std::string(name)+extension));
        if(desc.codeFormat==ShaderDesc::CodeFormat::MetalSource) bytes.push_back(std::byte{0});
        auto handle=device.CreateShader(desc,bytes); Require(bool(handle),"Shader creation failed"); shaders.push_back(handle); return handle;
    }
    PipelineStateHandle Pipeline(PipelineStateDesc desc) {
        auto handle=device.CreatePipelineState(desc); Require(bool(handle),"Pipeline creation failed"); pipelines.push_back(handle); return handle;
    }
    PipelineStateDesc Fullscreen(ShaderHandle pixel,Format format) {
        PipelineStateDesc desc; desc.vertexShader=fullscreen; desc.pixelShader=pixel;
        desc.depthStencil.depthEnable=false; desc.depthStencil.depthWriteEnable=false;
        desc.rasterizer.cullMode=CullMode::None; desc.renderTargets.colorAttachmentCount=1; desc.renderTargets.colorFormats[0]=format; return desc;
    }
    void Initialize() {
        Require(device.GetCapabilities().maximumColorAttachments>=5,"Renderer requires five color attachments");
        frameFence=device.CreateFence(); Require(bool(frameFence),"Frame fence creation failed");
        BufferDesc constant; constant.byteSize=sizeof(FrameUniforms); constant.bindFlags=BufferBindFlags::Constant;
        frameBuffer=device.CreateBuffer(constant); Require(bool(frameBuffer),"Frame uniforms creation failed");
        SamplerDesc sd; sd.minFilter=sd.magFilter=FilterMode::Nearest; sd.addressU=sd.addressV=sd.addressW=SamplerAddressMode::ClampToEdge;
        sampler=device.CreateSampler(sd); Require(bool(sampler),"Sampler creation failed");
        sd.comparisonEnable=true; shadowSampler=device.CreateSampler(sd); Require(bool(shadowSampler),"Shadow sampler creation failed");
        sd.comparisonEnable=false; sd.minFilter=sd.magFilter=FilterMode::Linear;
        sd.addressU=sd.addressV=sd.addressW=SamplerAddressMode::Repeat;
        materialSampler=device.CreateSampler(sd); Require(bool(materialSampler),"Material sampler creation failed");
        TextureDesc whiteDesc; whiteDesc.width=whiteDesc.height=1; whiteDesc.bindFlags=TextureBindFlags::ShaderResource;
        whiteDesc.initialState=ResourceState::ShaderResource;
        const std::array<std::byte,4> whitePixel{std::byte{255},std::byte{255},std::byte{255},std::byte{255}};
        white.texture=device.CreateTexture(whiteDesc,whitePixel,4); Require(bool(white.texture),"Default albedo creation failed");
        TextureViewDesc whiteView; whiteView.texture=white.texture;
        white.sample=device.CreateTextureView(whiteView); Require(bool(white.sample),"Default albedo view creation failed");
        auto vertex=Shader("geometry.vert",ShaderStage::Vertex,0,1),pixel=Shader("geometry.frag",ShaderStage::Pixel,1);
        PipelineStateDesc geo; geo.vertexShader=vertex; geo.pixelShader=pixel; geo.rasterizer.cullMode=CullMode::None;
        geo.renderTargets.colorAttachmentCount=5;
        for(uint32_t index=0;index<5;++index) geo.renderTargets.colorFormats[index]=Format::RGBA16_Float;
        geo.renderTargets.depthStencilFormat=Format::D32_Float;
        geo.inputLayout={{"TEXCOORD",0,Format::RGB32_Float,0,offsetof(VERTEX_3D,Position),false,0,0},{"TEXCOORD",1,Format::RGB32_Float,0,offsetof(VERTEX_3D,Normal),false,0,1}};
        for(uint32_t i=0;i<5;++i) geo.inputLayout.push_back({"TEXCOORD",i+2,Format::RGBA32_Float,1,i*16,true,1,i+2});
        geo.inputLayout.push_back({"TEXCOORD",7,Format::RG32_Float,0,offsetof(VERTEX_3D,TexCoord),false,0,7});
        for(uint32_t i=0;i<4;++i) geo.inputLayout.push_back({"TEXCOORD",8+i,Format::RGBA32_Float,1,80+i*16,true,1,8+i});
        geo.vertexBuffers={{0,sizeof(VERTEX_3D),false},{1,sizeof(Instance),true}};
        geometryPipeline=Pipeline(geo);
        auto shadowDesc=geo; shadowDesc.vertexShader=Shader("shadow.vert",ShaderStage::Vertex,0,1);
        shadowDesc.pixelShader=Shader("shadow.frag",ShaderStage::Pixel); shadowDesc.renderTargets.colorAttachmentCount=0;
        // Explicit locations preserve the shader ABI even with unused attributes.
        std::erase_if(shadowDesc.inputLayout,[](const auto& item){return item.location!=0 && (item.location<2 || item.location>5);});
        shadowPipeline=Pipeline(shadowDesc);
        fullscreen=Shader("fullscreen.vert",ShaderStage::Vertex);
        lightingPipeline=Pipeline(Fullscreen(Shader("lighting.frag",ShaderStage::Pixel,7,1),Format::RGBA16_Float));
        tonePipeline=Pipeline(Fullscreen(Shader("tonemap.frag",ShaderStage::Pixel,1,1),Format::RGBA8_UNorm));
        presentShader=Shader("present.frag",ShaderStage::Pixel,1);
    }
    Target CreateTarget(uint32_t w,uint32_t h,Format format,bool depthTarget=false) {
        TextureDesc td; td.width=w; td.height=h; td.format=format;
        td.bindFlags=TextureBindFlags::ShaderResource | (depthTarget?TextureBindFlags::DepthStencil:TextureBindFlags::RenderTarget);
        Target target; target.texture=device.CreateTexture(td); Require(bool(target.texture),"Render texture creation failed");
        TextureViewDesc vd; vd.texture=target.texture; vd.type=depthTarget?TextureViewType::DepthStencil:TextureViewType::RenderTarget;
        target.render=device.CreateTextureView(vd); vd.type=TextureViewType::ShaderResource; target.sample=device.CreateTextureView(vd);
        if(!target.render || !target.sample) { DestroyTarget(target); throw std::runtime_error("Render texture view creation failed"); } return target;
    }
    void DestroyTarget(Target& target) {
        if(target.render) device.DestroyTextureView(target.render);
        if(target.sample) device.DestroyTextureView(target.sample);
        if(target.texture) device.DestroyTexture(target.texture); target={};
    }
    void DestroyTargets() { for(auto* t:{&albedo,&normal,&position,&material,&emissive,&depth,&shadow,&hdr,&output}) DestroyTarget(*t); width=height=0; targetsInitialized=false; }
    void Resize(uint32_t w,uint32_t h) {
        Require(!deviceLifetime.expired(),"RHI device expired");
        Require(w && h && w<=device.GetCapabilities().maximumTextureDimension2D && h<=device.GetCapabilities().maximumTextureDimension2D,"Invalid render dimensions");
        if(w==width && h==height) return; WaitFrame(); device.WaitIdle(); DestroyTargets();
        try {
            albedo=CreateTarget(w,h,Format::RGBA16_Float); normal=CreateTarget(w,h,Format::RGBA16_Float);
            position=CreateTarget(w,h,Format::RGBA16_Float); depth=CreateTarget(w,h,Format::D32_Float,true);
            material=CreateTarget(w,h,Format::RGBA16_Float); emissive=CreateTarget(w,h,Format::RGBA16_Float);
            shadow=CreateTarget(1024,1024,Format::D32_Float,true); hdr=CreateTarget(w,h,Format::RGBA16_Float);
            output=CreateTarget(w,h,Format::RGBA8_UNorm); width=w; height=h;
        } catch(...) { DestroyTargets(); throw; }
    }
    void BindTexture(IRHICommandList& list,uint32_t slot,TextureViewHandle view) {
        const auto selectedSampler=view==shadow.sample?shadowSampler:(slot==6?materialSampler:sampler);
        Require(list.SetTextureView(ShaderStage::Pixel,slot,view) && list.SetSampler(ShaderStage::Pixel,slot,selectedSampler),"Texture binding failed");
    }
    void Geometry(IRHICommandList& list,const std::vector<Batch>& batches,bool shadows) {
        list.SetViewport({0,0,float(shadows?1024:width),float(shadows?1024:height),0,1});
        Require(list.SetConstantBuffer(ShaderStage::Vertex,0,frameBuffer),"Geometry uniforms binding failed");
        for(auto batch:batches) {
            if(shadows && !batch.shadow) continue;
            const auto& mesh=batch.mesh;
            Require(list.SetPipelineState(shadows?shadowPipeline:geometryPipeline),"Geometry pipeline binding failed");
            if(!shadows) {
                const auto texture=batch.albedoTexture?batch.albedoTexture:white.sample;
                Require(list.SetTextureView(ShaderStage::Pixel,0,texture) && list.SetSampler(ShaderStage::Pixel,0,materialSampler),"Material texture binding failed");
            }
            Require(list.SetVertexBuffer(0,mesh.vertexBuffer,mesh.vertexStride),"Mesh vertex binding failed");
            Require(list.SetVertexBuffer(1,instanceBuffer,sizeof(Instance),batch.first*sizeof(Instance)),"Instance binding failed");
            Require(list.SetIndexBuffer(mesh.indexBuffer,mesh.indexFormat),"Mesh index binding failed");
            Require(list.DrawIndexedInstanced(mesh.indexCount,batch.count),"Instanced drawing failed");
        }
    }
    void ScreenPass(IRHICommandList& list,Target& target,PipelineStateHandle pipeline,std::initializer_list<TextureViewHandle> inputs,bool uniforms=false) {
        RenderPassDesc pass; pass.colorAttachments.push_back({target.render,LoadOperation::Discard});
        Require(list.BeginRenderPass(pass),"Fullscreen render pass failed");
        Require(list.SetPipelineState(pipeline),"Fullscreen pipeline binding failed");
        list.SetViewport({0,0,float(width),float(height),0,1});
        uint32_t slot=0; for(auto view:inputs) BindTexture(list,slot++,view);
        if(uniforms) Require(list.SetConstantBuffer(ShaderStage::Pixel,0,frameBuffer),"Lighting uniforms binding failed");
        list.Draw(3); list.EndRenderPass();
    }
    void Render(const RenderScene& scene,bool present,bool vsync) {
        Require(!deviceLifetime.expired(),"RHI device expired");
        Require(width && height,"Resize the renderer before rendering");
        Require(scene.frame.cameraPosition[3]<.5f || bool(scene.environmentTexture),"Enabled environment map is missing");
        // Uploads are recorded on the same ordered GPU queue as the draws.
        // The backend owns update/cycling synchronization; a second fixed
        // frame-buffer ring here would duplicate that responsibility.
        for(float v:scene.frame.viewProjection) Require(std::isfinite(v),"Nonfinite camera matrix");
        std::vector<DrawItem> draws=scene.draws;
        Require(draws.size()<=1'000'000,"Too many draw instances");
        for(const auto& d:draws) {
            const auto* vertices=device.GetBufferDesc(d.mesh.vertexBuffer);
            const auto* indices=device.GetBufferDesc(d.mesh.indexBuffer);
            Require(d.mesh.IsReady() && d.mesh.vertexStride==sizeof(VERTEX_3D) && vertices && indices &&
                uint64_t(d.mesh.vertexCount)*d.mesh.vertexStride<=vertices->byteSize &&
                uint64_t(d.mesh.indexCount)*(d.mesh.indexFormat==IndexFormat::UInt16?2:4)<=indices->byteSize,"Invalid geometry runtime binding");
            for(float v:d.instance.world) Require(std::isfinite(v),"Nonfinite instance matrix");
        }
        auto key=[](const DrawItem& d){ return std::tuple(d.mesh.vertexBuffer.index,d.mesh.vertexBuffer.generation,d.mesh.indexBuffer.index,d.mesh.indexBuffer.generation,d.mesh.vertexStride,d.mesh.indexFormat,d.mesh.indexCount,d.castsShadow,d.albedoTexture.index,d.albedoTexture.generation); };
        std::stable_sort(draws.begin(),draws.end(),[&](const auto& a,const auto& b){ return key(a)<key(b); });
        std::vector<Instance> instances; std::vector<Batch> batches; instances.reserve(draws.size());
        for(const auto& d:draws) {
            if(batches.empty() || key(DrawItem{batches.back().mesh,{},batches.back().shadow,batches.back().albedoTexture})!=key(d))
                batches.push_back({d.mesh,static_cast<uint32_t>(instances.size()),0,d.castsShadow,d.albedoTexture});
            ++batches.back().count; instances.push_back(d.instance);
        }
        if(instances.size()>instanceCapacity) {
            // Resource replacement is exceptional; ordinary updates below
            // stay on the GPU timeline without a per-frame host fence wait.
            WaitFrame();
            auto capacity=std::max<uint32_t>(64,static_cast<uint32_t>(instances.size()));
            BufferDesc bd; bd.byteSize=capacity*sizeof(Instance); bd.stride=sizeof(Instance); bd.bindFlags=BufferBindFlags::Vertex;
            auto buffer=device.CreateBuffer(bd); Require(bool(buffer),"Instance buffer allocation failed");
            if(instanceBuffer) device.DestroyBuffer(instanceBuffer); instanceBuffer=buffer; instanceCapacity=capacity;
        }
        RenderGraph graph;
        auto initial=targetsInitialized?ResourceState::ShaderResource:ResourceState::Common;
        auto ga=graph.ImportTexture(albedo.texture,initial,"Albedo"),gn=graph.ImportTexture(normal.texture,initial,"Normal"),gp=graph.ImportTexture(position.texture,initial,"Position");
        auto gm=graph.ImportTexture(material.texture,initial,"Material"),ge=graph.ImportTexture(emissive.texture,initial,"Emissive");
        auto gs=graph.ImportTexture(shadow.texture,initial,"Shadow"),gd=graph.ImportTexture(depth.texture,targetsInitialized?ResourceState::DepthWrite:ResourceState::Common,"Depth");
        auto gh=graph.ImportTexture(hdr.texture,initial,"HDR"),go=graph.ImportTexture(output.texture,initial,"Output");
        std::vector<RenderGraphResource> materialTextures;
        std::vector<TextureHandle> importedTextures;
        for(const auto& batch:batches) {
            auto* view=device.GetTextureViewDesc(batch.albedoTexture?batch.albedoTexture:white.sample);
            Require(view!=nullptr,"Invalid material texture view");
            if(std::find(importedTextures.begin(),importedTextures.end(),view->texture)==importedTextures.end()) {
                importedTextures.push_back(view->texture);
                materialTextures.push_back(graph.ImportTexture(view->texture,ResourceState::ShaderResource,"Material albedo"));
            }
        }
        const auto environment=scene.environmentTexture?scene.environmentTexture:white.sample;
        const auto* environmentView=device.GetTextureViewDesc(environment);
        Require(environmentView!=nullptr,"Invalid environment texture view");
        auto genv=graph.ImportTexture(environmentView->texture,ResourceState::ShaderResource,"Environment map");
        graph.AddPass("Frame upload",[](auto&){},[&](auto& list){
            Require(list.UpdateBuffer(frameBuffer,std::as_bytes(std::span(&scene.frame,1))),"Frame uniform upload failed");
            if(!instances.empty()) Require(list.UpdateBuffer(instanceBuffer,Bytes(std::span<const Instance>(instances))),"Instance upload failed");
        });
        graph.AddPass("Directional shadow",[&](auto& b){b.Write(gs,ResourceState::DepthWrite);},[&](auto& list){
            RenderPassDesc pass; pass.hasDepthAttachment=true; pass.depthAttachment.view=shadow.render;
            pass.depthAttachment.depthLoadOperation=LoadOperation::Clear; pass.depthAttachment.stencilLoadOperation=LoadOperation::Discard;
            Require(list.BeginRenderPass(pass),"Shadow pass failed"); Geometry(list,batches,true); list.EndRenderPass();
        });
        graph.AddPass("GBuffer",[&](auto& b){b.Write(ga,ResourceState::RenderTarget);b.Write(gn,ResourceState::RenderTarget);b.Write(gp,ResourceState::RenderTarget);b.Write(gm,ResourceState::RenderTarget);b.Write(ge,ResourceState::RenderTarget);b.Write(gd,ResourceState::DepthWrite);for(auto texture:materialTextures) b.Read(texture);},[&](auto& list){
            RenderPassDesc pass;
            for(auto* target:{&albedo,&normal,&position,&material,&emissive}) pass.colorAttachments.push_back({target->render,LoadOperation::Clear,StoreOperation::Store,{0,0,0,0}});
            pass.hasDepthAttachment=true; pass.depthAttachment.view=depth.render; pass.depthAttachment.depthLoadOperation=LoadOperation::Clear; pass.depthAttachment.stencilLoadOperation=LoadOperation::Discard;
            Require(list.BeginRenderPass(pass),"GBuffer pass failed"); Geometry(list,batches,false); list.EndRenderPass();
        });
        graph.AddPass("Deferred lighting",[&](auto& b){b.Read(ga);b.Read(gn);b.Read(gp);b.Read(gs);b.Read(gm);b.Read(ge);b.Read(genv);b.Write(gh,ResourceState::RenderTarget);},[&](auto& list){
            ScreenPass(list,hdr,lightingPipeline,{albedo.sample,normal.sample,position.sample,shadow.sample,material.sample,emissive.sample,environment},true);
        });
        graph.AddPass("Output transform",[&](auto& b){b.Read(gh);b.Write(go,ResourceState::RenderTarget);},[&](auto& list){ScreenPass(list,output,tonePipeline,{hdr.sample},true);});
        graph.AddPass("Publish output",[&](auto& b){b.Read(go);},[](auto&){});
        auto commands=device.CreateCommandList({}); Require(bool(commands),"Frame command allocation failed");
        Require(graph.Execute(*commands),"RenderGraph execution failed");
        IRHICommandList* submittedList=commands.get(); QueueSubmitDesc submit; submit.commandLists={&submittedList,1};
        submit.signalFence=frameFence->GetHandle(); submit.signalValue=++submitted;
        auto* queue=device.GetQueue(CommandQueueType::Graphics); Require(queue && queue->Submit(submit),"Frame submission failed"); targetsInitialized=true;
        if(present && device.GetSwapChain()) Present(vsync);
    }
    void Present(bool vsync) {
        auto* swap=device.GetSwapChain(); Require(swap->Present(vsync),"Presentation policy failed");
        auto format=swap->GetDesc().format;
        if(!presentPipeline || format!=presentFormat) {
            device.WaitIdle();
            if(presentPipeline) { device.DestroyPipelineState(presentPipeline); std::erase(pipelines,presentPipeline); }
            presentPipeline=Pipeline(Fullscreen(presentShader,format)); presentFormat=format;
        }
        auto commands=device.CreateCommandList({}); Require(bool(commands),"Presentation command allocation failed"); commands->Begin();
        if(!swap->AcquireNextImage(*commands)) { commands->End(); return; }
        auto image=swap->GetCurrentImage(); TextureViewDesc vd; vd.texture=image; vd.type=TextureViewType::RenderTarget;
        auto view=device.CreateTextureView(vd); Require(bool(view),"Swapchain view creation failed");
        try {
            const auto* texture=device.GetTextureDesc(image); Require(texture!=nullptr,"Invalid acquired image");
            RenderPassDesc pass; pass.colorAttachments.push_back({view,LoadOperation::Discard});
            Require(commands->BeginRenderPass(pass),"Presentation pass failed"); Require(commands->SetPipelineState(presentPipeline),"Presentation pipeline binding failed");
            commands->SetViewport({0,0,float(texture->width),float(texture->height),0,1}); BindTexture(*commands,0,output.sample);
            commands->Draw(3); commands->EndRenderPass(); commands->End();
            IRHICommandList* submittedList=commands.get(); QueueSubmitDesc submit; submit.commandLists={&submittedList,1};
            submit.signalFence=frameFence->GetHandle(); submit.signalValue=++submitted;
            Require(device.GetQueue(CommandQueueType::Graphics)->Submit(submit),"Presentation submission failed");
            device.DestroyTextureView(view);
        } catch(...) { device.DestroyTextureView(view); throw; }
    }
};
FrameRenderer::FrameRenderer(IRHIDevice& d,std::filesystem::path p):m_impl(std::make_unique<Impl>(d,std::move(p))) { m_impl->Initialize(); }
FrameRenderer::~FrameRenderer()=default;
void FrameRenderer::Resize(uint32_t w,uint32_t h) { m_impl->Resize(w,h); }
void FrameRenderer::Render(const RenderScene& s,bool p,bool v) { m_impl->Render(s,p,v); }
bool FrameRenderer::Capture(TextureReadback& result,uint64_t timeout) { m_impl->WaitFrame(); return m_impl->device.ReadTexture(m_impl->output.texture,result,timeout); }
}
