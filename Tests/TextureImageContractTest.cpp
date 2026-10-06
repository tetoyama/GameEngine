#include "Engine/Resources/Loader/TextureImageFile.h"
#include "Service/Graphics/RHI/Null/NullRHIBackend.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
template<class F> void MustFail(F f) { bool failed=false; try { f(); } catch(const std::exception&) { failed=true; } Require(failed,"Invalid image accepted"); }
}
int main(int argc,char** argv) {
    try {
        Require(argc==2,"Texture asset directory required");
        const auto root=std::filesystem::path(argv[1]);
        for(const char* name:{"white.tga","mesh.png","Daylight.png"}) {
            const auto image=LoadTextureImage(root/name);
            Require(image.IsValid(),"Existing texture asset not decoded");
        }
        const auto suffix=std::chrono::steady_clock::now().time_since_epoch().count();
        auto path=std::filesystem::temp_directory_path()/("gameengine-image-"+std::to_string(suffix));
        path+=std::filesystem::path(u8"-画像.tga");
        struct Remove { std::filesystem::path path; ~Remove(){std::error_code error; std::filesystem::remove(path,error);} } remove{path};
        // Top-left, uncompressed BGRA, with distinct rows and alpha values.
        const std::array<unsigned char,34> tga{0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,32,40,
            0,0,255,255, 0,255,0,128, 255,0,0,64, 255,255,255,0};
        { std::ofstream file(path,std::ios::binary); file.write(reinterpret_cast<const char*>(tga.data()),tga.size()); Require(bool(file),"Fixture write failed"); }
        const auto image=LoadTextureImage(path);
        const std::array<unsigned char,16> expected{255,0,0,255, 0,255,0,128, 0,0,255,64, 255,255,255,0};
        Require(image.width==2 && image.height==2,"Decoded image dimensions changed");
        for(size_t i=0;i<expected.size();++i) Require(std::to_integer<unsigned char>(image.pixels[i])==expected[i],"RGBA, alpha or image orientation changed");
        RHI::TextureHandle texture;
        RHI::TextureViewHandle view;
        RHI::NullRHIDevice device({});
        {
            auto owned=LoadTextureFromFile(path,device); view=owned->GetRHIView();
            Require(bool(view),"TextureData upload failed");
            const auto* binding=device.GetTextureViewDesc(view); Require(binding!=nullptr,"Texture view missing"); texture=binding->texture;
            Require(device.GetTextureDesc(texture)->width==2 && owned->Width==2,"CPU/GPU dimensions differ");
            Require(!device.DestroyTexture(texture),"Texture destroyed while view is live");
            Require(!owned->EnsureRHI(device,TextureImage{}) && owned->GetRHIView()==view,"Invalid pixels replaced valid binding");
        }
        Require(!device.GetTextureDesc(texture) && !device.GetTextureViewDesc(view),"TextureData leaked GPU resources");
        auto owned=std::make_shared<TextureData>();
        { RHI::NullRHIDevice shortLived({}); Require(bool(owned->EnsureRHI(shortLived,image)),"Lifetime fixture upload failed"); }
        Require(!owned->GetRHIView(),"Expired device still publishes a texture view"); owned.reset();
        MustFail([&]{LoadTextureImage(root/"missing-texture.png");});
        { std::ofstream file(path,std::ios::binary|std::ios::trunc); file<<"invalid"; }
        MustFail([&]{LoadTextureImage(path);});
        std::cout<<"Texture import: real assets, RGBA, alpha, orientation, Unicode path, invalid input and GPU ownership passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
