#include "Engine/Resources/resourceService.h"
#include "Engine/Resources/Loader/TextureImageFile.h"
#include "Service/Graphics/RHI/Null/NullRHIBackend.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
struct Record { bool flag; };
}
int main(int argc,char** argv) {
    try {
        Require(argc==2,"Texture asset directory required");
        DebugLogService log; log.Initialize();
        ResourceService resources; resources.InitializeNative(&log);
        Require(!resources.Load<Record>("unregistered"),"Unregistered type loaded");
        Require(!resources.RegisterLoader<Record>({}),"Empty callback registered");
        std::atomic<int> loads=0;
        auto decode=[&](const std::string& path,std::shared_ptr<void> args) {
            if(path=="failure") throw std::runtime_error("Expected load failure");
            ++loads;
            return std::make_shared<Record>(Record{std::get<0>(*std::static_pointer_cast<std::tuple<bool>>(args))});
        };
        Require(resources.RegisterLoader<Record>(decode),"Resource loader registration failed");
        Require(!resources.RegisterLoader<Record>(decode),"Duplicate registration replaced existing cache");
        auto original=resources.Load<Record>("shared",true);
        Require(original && original->flag,"Loader argument missing");
        Require(resources.Load<Record>("shared",true)==original && loads==1,"Cache did not reuse resource identity");
        auto other=resources.Load<Record>("shared",false);
        Require(other && !other->flag && other!=original && loads==2,"Arguments did not separate cached resources");
        resources.Unload<Record>("shared");
        Require(resources.Load<Record>("shared",true)==original,"Unload invalidated a retained resource");
        std::weak_ptr<Record> weak=original; original.reset();
        resources.ClearUnused<Record>(); Require(weak.expired(),"Unused CPU resource stayed cached");
        Require(!resources.Load<Record>("failure",true),"Failed load was accepted");
        Require(resources.Load<Record>("shared",false)==other,"Failed load corrupted existing cache");
        // Exercise the same argument/key path through the existing CPU async API.
        ResourceLoader<Record> asynchronous; asynchronous.SetLoadFunction(decode);
        auto future=asynchronous.LoadAsync("async",true); auto async=future.get();
        Require(async && async->flag && asynchronous.Load("async",true)==async,"Async loader did not unpack arguments or reuse its cache");

        RHI::NullRHIDevice device({});
        Require(resources.RegisterLoader<TextureData>([&](const std::string& path,std::shared_ptr<void>) {
            return LoadTextureFromFile(std::filesystem::path(std::u8string(path.begin(),path.end())),device);
        }),"Texture loader registration failed");
        const auto path=(std::filesystem::path(argv[1])/"mesh.png").u8string();
        const std::string key(path.begin(),path.end());
        auto texture=resources.Load<TextureData>(key);
        Require(texture && resources.Load<TextureData>(key)==texture,"Texture cache identity changed");
        const auto view=texture->GetRHIView(); Require(bool(view),"Texture resource lacks GPU view");
        const auto handle=device.GetTextureViewDesc(view)->texture;
        resources.ClearAllUnused(); Require(device.GetTextureDesc(handle)!=nullptr,"Retained texture was destroyed");
        std::weak_ptr<TextureData> weakTexture=texture; texture.reset();
        resources.ClearAllUnused(); Require(weakTexture.expired() && !device.GetTextureDesc(handle),"Unused texture did not release its GPU resource");
        const auto sink=log.GetSink<MemoryLogSink>();
        Require(sink && !sink->GetSnapshot().empty(),"Common diagnostics did not receive resource events");
        resources.Shutdown(); resources.Shutdown();
        Require(other && !other->flag,"Shutdown destroyed externally retained data");
        Require(!resources.Load<Record>("shared",false),"Shutdown retained loader registry");
        resources.InitializeNative(&log); Require(resources.RegisterLoader<Record>(decode),"Resource registry could not reinitialize");
        Require(resources.Load<Record>("new",true)!=nullptr,"Reload after reinitialization failed");
        resources.Shutdown(); log.Shutdown();
        std::cout<<"Resource service: cache identity, arguments, CPU async, failed loads, retained/unreferenced GPU ownership, logs and reinitialization passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
