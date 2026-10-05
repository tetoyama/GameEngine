#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Service/Graphics/RHI/SDL/SDLGPUBackend.h"
#include "Service/Graphics/RHI/RHIService.h"
#include "Service/Graphics/Portable/FrameRenderer.h"
#include "Service/Graphics/Portable/RenderMath.h"
#include "Service/Graphics/Portable/RenderPacketAdapter.h"
#include "Engine/Scene/System/Job/JobSystem.h"
#include "Engine/Scene/Registry/entityRegistry.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <cmath>
using namespace Rendering;
namespace {
struct Options {
#ifdef __APPLE__
    RHI::BackendType backend=RHI::BackendType::Metal;
#elif defined(_WIN32)
    RHI::BackendType backend=RHI::BackendType::Direct3D12;
#else
    RHI::BackendType backend=RHI::BackendType::Vulkan;
#endif
    int frames=0; uint32_t width=960,height=640; bool hidden=false,validate=false,offscreen=false;
    std::filesystem::path shaders,capture;
};
Options Parse(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        auto value=[&](){ if(++i>=argc) throw std::invalid_argument("Missing value for "+arg); return std::string(argv[i]); };
        if(arg=="--backend") {
            auto b=value(); if(b=="d3d12") o.backend=RHI::BackendType::Direct3D12; else if(b=="vulkan") o.backend=RHI::BackendType::Vulkan;
            else if(b=="metal") o.backend=RHI::BackendType::Metal; else throw std::invalid_argument("Unknown backend");
        } else if(arg=="--frames") o.frames=std::stoi(value());
        else if(arg=="--width") o.width=std::stoul(value()); else if(arg=="--height") o.height=std::stoul(value());
        else if(arg=="--shaders") o.shaders=value(); else if(arg=="--capture") o.capture=value();
        else if(arg=="--hidden") o.hidden=true; else if(arg=="--validate") o.validate=true;
        else if(arg=="--offscreen") o.offscreen=true;
        else throw std::invalid_argument("Unknown option: "+arg);
    }
    if(!o.width || !o.height || o.width>8192 || o.height>8192 || o.frames<0 || ((o.hidden||o.offscreen)&&!o.frames&&!o.validate)) throw std::invalid_argument("Invalid dimensions or frame limit");
    if(o.validate && !o.frames) o.frames=12;
    if(o.shaders.empty()) {
        const char* base=SDL_GetBasePath(); o.shaders=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(base?base:"")))/"shaders";
        if(!std::filesystem::exists(o.shaders)) o.shaders="Asset/Shader/Portable/Compiled";
    }
    return o;
}
struct Object { Entity entity; Vec3 position,scale; std::array<float,4> color; bool shadow=true; };
std::vector<Object> MakeValidationScene(EntityRegistry& registry) {
    std::vector<Object> objects;
    auto add=[&](Vec3 p,Vec3 s,std::array<float,4> c,bool shadow=true){auto entity=registry.Create(); if(!entity) throw std::runtime_error("Entity allocation failed"); objects.push_back({entity,p,s,c,shadow});};
    {
        add({0,-.5f,0},{14,.5f,14},{.27f,.31f,.36f,1},false);
        // A block-built machine, its floor and detached blocks exercise real
        // instance grouping, nonuniform transforms, depth and shadow reception.
        for(int z=-2;z<=2;++z) for(int x=-3;x<=3;++x) {
            add({float(x)*.72f,.65f,float(z)*.72f},{.34f,.34f,.34f},{.11f,.48f,.75f,1});
            if(std::abs(z)==2 && (x==-2 || x==2)) add({float(x)*.72f,.15f,float(z)*.72f},{.44f,.44f,.44f},{.07f,.08f,.11f,1});
        }
        add({0,1.4f,0},{.72f,.45f,.72f},{.8f,.27f,.06f,1});
        add({0,1.6f,1.4f},{.15f,.15f,.9f},{.75f,.77f,.8f,1});
        add({-3,1,3},{.55f,1,.55f},{.7f,.13f,.2f,1});
        add({3,.35f,-3},{.55f,.35f,.55f},{.13f,.65f,.27f,1});
    }
    return objects;
}
ModelGeometryRuntimeMesh Cube(FrameRenderer& renderer) {
    std::vector<Vertex> vertices; std::vector<uint32_t> indices;
    const Vec3 normals[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for(auto n:normals) {
        auto u=std::abs(n[1])>.5f?Vec3{1,0,0}:Normalize(Cross({0,1,0},n)); auto v=Cross(n,u); uint32_t base=static_cast<uint32_t>(vertices.size());
        for(auto xy:std::array<std::array<float,2>,4>{{{-1,-1},{1,-1},{1,1},{-1,1}}}) {
            Vec3 p{}; for(int a=0;a<3;++a) p[a]=n[a]+xy[0]*u[a]+xy[1]*v[a]; vertices.push_back({p,n,{(xy[0]+1)*.5f,(xy[1]+1)*.5f}});
        }
        for(auto i:{0u,1u,2u,0u,2u,3u}) indices.push_back(base+i);
    }
    return renderer.UploadMesh(vertices,indices);
}
void Save(const RHI::TextureReadback& image,const std::filesystem::path& path) {
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary); if(!file) throw std::runtime_error("Cannot create capture");
    file<<"P6\n"<<image.width<<' '<<image.height<<"\n255\n";
    for(uint32_t y=0;y<image.height;++y) for(uint32_t x=0;x<image.width;++x) {
        const auto* pixel=image.pixels.data()+size_t(y)*image.rowPitch+x*4;
        if(image.format==RHI::Format::BGRA8_UNorm) { const char rgb[]={char(pixel[2]),char(pixel[1]),char(pixel[0])}; file.write(rgb,3); }
        else file.write(reinterpret_cast<const char*>(pixel),3);
    }
    if(!file) throw std::runtime_error("Capture write failed");
}
bool Different(const RHI::TextureReadback& a,const RHI::TextureReadback& b) { return a.pixels!=b.pixels; }
void CheckImage(const RHI::TextureReadback& image) {
    if(image.pixels.size()!=size_t(image.rowPitch)*image.height) throw std::runtime_error("Truncated GPU readback");
    size_t colorful=0,dark=0; for(size_t i=0;i<image.pixels.size();i+=4) {
        auto r=std::to_integer<int>(image.pixels[i]),g=std::to_integer<int>(image.pixels[i+1]),b=std::to_integer<int>(image.pixels[i+2]);
        if(std::max({r,g,b})-std::min({r,g,b})>25) ++colorful;
        if(r+g+b<250) ++dark;
        if(std::to_integer<int>(image.pixels[i+3])!=255) throw std::runtime_error("Invalid output alpha");
    }
    if(colorful<image.width*image.height/20 || dark<image.width*image.height/100) throw std::runtime_error("GPU output lacks expected scene/color range");
}
}
int main(int argc,char** argv) {
    SDL_SetMainReady();
    try {
        if(!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        struct Quit { ~Quit(){SDL_Quit();} } quit;
        auto options=Parse(argc,argv);
        std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(nullptr,SDL_DestroyWindow);
        if(!options.offscreen) { window.reset(SDL_CreateWindow("GameEngine | portable renderer",int(options.width),int(options.height),SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(options.hidden?SDL_WINDOW_HIDDEN:0))); if(!window) throw std::runtime_error(SDL_GetError()); }
        RHI::RenderHardwareInterfaceService graphics;
        if(!RHI::RegisterSDLGPUBackends(graphics.GetRegistry()) || !graphics.SelectBackend(options.backend)) throw std::runtime_error(std::string("Unsupported GPU backend: ")+SDL_GetError());
        RHI::DeviceCreateDesc dd; dd.nativeWindow={window.get(),nullptr,RHI::NativeWindowHandle::Kind::SDL}; dd.swapChain.width=options.width; dd.swapChain.height=options.height;
        dd.enableDebugLayer=options.validate;
        if(!graphics.AdoptDevice(graphics.GetBackend()->CreateDevice(dd))) throw std::runtime_error(std::string("GPU creation failed: ")+SDL_GetError());
        auto* device=graphics.GetDevice();
        std::cout<<"backend="<<graphics.GetBackend()->GetName()<<'\n';
        FrameRenderer renderer(*device,options.shaders); renderer.Resize(options.width,options.height); auto cube=Cube(renderer);
        EntityRegistry entities; auto objects=MakeValidationScene(entities); JobSystem jobs; jobs.Start(2);
        RenderScene scene;
        std::vector<RenderPacket> packets(objects.size());
        std::vector<std::shared_ptr<MaterialDescriptor>> materials;
        for(const auto& object:objects) { auto m=std::make_shared<MaterialDescriptor>(); m->parameters.baseColor=object.color; materials.push_back(std::move(m)); }
        auto direction=Normalize(Vec3{.55f,-1,.35f});
        scene.frame.lightDirection={direction[0],direction[1],direction[2],0};
        scene.frame.lightViewProjection=Multiply(Orthographic(22,22,.1f,45),LookAt({-10,18,-8},{0,0,0}));
        auto update=[&](float angle){
            auto view=LookAt({10*std::sin(angle),7,-10*std::cos(angle)},{0,.6f,0});
            scene.frame.viewProjection=Multiply(Perspective(.92f,float(renderer.Width())/renderer.Height(),.1f,100),view);
            jobs.ParallelFor(0,objects.size(),16,[&](size_t i){
                const auto& object=objects[i]; if(!entities.IsAlive(object.entity)) return;
                auto& packet=packets[i]; packet.entity=object.entity; packet.kind=RenderPacketKind::Model; packet.layer=RenderLayer::Opaque3D;
                packet.passMask=object.shadow?RenderPacketPassMask::GBuffer|RenderPacketPassMask::Shadow:RenderPacketPassMask::GBuffer;
                auto world=Transform(object.position,object.scale); std::copy(world.begin(),world.end(),packet.transform.worldMatrix.values);
                packet.modelMaterial.ownedDescriptor=materials[i];
            });
            auto converted=ConvertRenderPackets(packets,scene.frame,scene.generation+1,[&](const auto&){return std::vector<ModelGeometryRuntimeMesh>{cube};});
            if(converted.unsupportedPackets || converted.unresolvedMeshes) throw std::runtime_error("Scene extraction failed");
            scene=std::move(converted.scene);
        };
        bool running=true; int frame=0; float angle=.65f; auto start=std::chrono::steady_clock::now();
        while(running && (!options.frames || frame<options.frames)) {
            SDL_Event event; while(SDL_PollEvent(&event)) { if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_KEY_DOWN && event.key.key==SDLK_ESCAPE)) running=false; }
            int w=int(options.width),h=int(options.height); if(window) SDL_GetWindowSizeInPixels(window.get(),&w,&h);
            if(w>0 && h>0 && (uint32_t(w)!=renderer.Width() || uint32_t(h)!=renderer.Height())) renderer.Resize(uint32_t(w),uint32_t(h));
            if(!options.validate) { auto keys=SDL_GetKeyboardState(nullptr); if(keys[SDL_SCANCODE_LEFT]) angle-=.02f; if(keys[SDL_SCANCODE_RIGHT]) angle+=.02f; }
            update(angle); renderer.Render(scene,!options.offscreen,!options.validate); ++frame;
        }
        RHI::TextureReadback image; if(!renderer.Capture(image)) throw std::runtime_error("GPU capture failed");
        if(options.validate) {
            CheckImage(image);
            // Frozen input is deterministic across frame resource reuse.
            renderer.Render(scene,false); RHI::TextureReadback repeat; if(!renderer.Capture(repeat) || Different(image,repeat)) throw std::runtime_error("Frame reuse changes frozen scene");
            auto original=scene.draws; scene.draws.clear(); renderer.Render(scene,false); RHI::TextureReadback empty; if(!renderer.Capture(empty) || !Different(image,empty)) throw std::runtime_error("Geometry does not affect output");
            scene.draws=original; for(auto& draw:scene.draws) draw.castsShadow=false; renderer.Render(scene,false); RHI::TextureReadback noShadow; if(!renderer.Capture(noShadow) || !Different(image,noShadow)) throw std::runtime_error("Shadow participation does not affect output");
            scene.draws=original; scene.draws.back().instance.color={1,0,1,1}; renderer.Render(scene,false); RHI::TextureReadback changed; if(!renderer.Capture(changed) || !Different(image,changed)) throw std::runtime_error("Material changes do not reach GPU");
            RHI::TextureDesc td; td.width=td.height=2; td.initialState=RHI::ResourceState::ShaderResource;
            const std::array<uint8_t,16> pixels{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
            const auto texture=device->CreateTexture(td,std::as_bytes(std::span(pixels)),8);
            if(!texture) throw std::runtime_error("Validation texture upload failed");
            RHI::TextureViewDesc vd; vd.texture=texture; const auto view=device->CreateTextureView(vd);
            if(!view) throw std::runtime_error("Validation texture view failed");
            scene.draws=original; for(auto& draw:scene.draws) draw.albedoTexture=view;
            renderer.Render(scene,false); RHI::TextureReadback textured;
            if(!renderer.Capture(textured) || !Different(image,textured)) throw std::runtime_error("Albedo texture does not affect output");
            for(auto& draw:scene.draws) draw.instance.uvTransform={0,0,.25f,.25f};
            renderer.Render(scene,false); RHI::TextureReadback sliced;
            if(!renderer.Capture(sliced) || !Different(textured,sliced)) throw std::runtime_error("UV transform does not affect output");
            for(auto& draw:scene.draws) draw.instance.shading[0]=1;
            renderer.Render(scene,false); RHI::TextureReadback unlit;
            if(!renderer.Capture(unlit) || !Different(sliced,unlit)) throw std::runtime_error("Unlit material does not affect output");
            scene.draws=original; device->DestroyTextureView(view); device->DestroyTexture(texture);
            renderer.Resize(321,213); update(angle); renderer.Render(scene,false); RHI::TextureReadback resized;
            if(!renderer.Capture(resized) || resized.width!=321 || resized.height!=213) throw std::runtime_error("Resize/readback dimension mismatch");
            renderer.Resize(options.width,options.height); update(angle); renderer.Render(scene,!options.offscreen,false);
            if(!renderer.Capture(image)) throw std::runtime_error("Post-resize capture failed"); CheckImage(image);
            std::cout<<"validation=passed frame-reuse, geometry, shadows, materials, textures, UV, unlit, resize, readback\n";
        }
        if(!options.capture.empty()) Save(image,options.capture);
        const auto& stats=renderer.Statistics(); auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"frames="<<frame<<" instances="<<stats.instances<<" batches="<<stats.batches<<" passes="<<stats.passes<<" seconds="<<elapsed<<'\n';
        jobs.Stop(); return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<" (SDL: "<<SDL_GetError()<<")\n"; return 1; }
}
