#ifndef GAMEENGINE_PORTABLE
// =======================================================================
//
// gameApplication.cpp
//
// =======================================================================
#include <windows.h>
#include "gameApplication.h"

#include "engine.h"
#include "engineContext.h"

int GameApplication::Run(HINSTANCE hInstance, int nCmdShow){
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if(FAILED(hr)){
		OutputDebugStringA("CoInitializeEx failed\n");
		return -1;
	}

	int exitCode = 0;

	EngineContextBuilder builder;
	std::unique_ptr<EngineContext> context = builder.Build();
	if(!context){
		CoUninitialize();
		return -1;
	}

	Engine engine;
	if(engine.Initialize(context.get(), hInstance, nCmdShow)){
		engine.Run(context.get());
	}
	else{
		exitCode = -1;
		OutputDebugStringA("GameApplication::Run aborted because Engine::Initialize failed\n");
	}

	engine.Shutdown(context.get());
	context.reset();

	CoUninitialize();
	return exitCode;
}

#else
#include "gameApplication.h"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Service/Graphics/RHI/SDL/SDLGPUBackend.h"
#include "Service/Graphics/Portable/FrameRenderer.h"
#include <fstream>
#include <stdexcept>
#include <string>

int GameApplication::Run(int argc, char** argv) {
    // Native renderer preview. The existing Scene/editor startup still requires Windows.
#ifdef __APPLE__
    auto api = RHI::BackendType::Metal;
#elif defined(_WIN32)
    auto api = RHI::BackendType::Direct3D12;
#else
    auto api = RHI::BackendType::Vulkan;
#endif
    int frames = 0;
    uint32_t width = 960, height = 640;
    bool offscreen = false, hidden = false;
    std::filesystem::path capture;
    for(int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        auto value = [&]() { if(++i >= argc) throw std::invalid_argument("Missing value for " + option); return std::string(argv[i]); };
        if(option == "--backend") {
            const auto name = value();
            if(name == "d3d12") api = RHI::BackendType::Direct3D12;
            else if(name == "vulkan") api = RHI::BackendType::Vulkan;
            else if(name == "metal") api = RHI::BackendType::Metal;
            else throw std::invalid_argument("Unknown GPU backend");
        } else if(option == "--frames") frames = std::stoi(value());
        else if(option == "--width") width = std::stoul(value());
        else if(option == "--height") height = std::stoul(value());
        else if(option == "--capture") capture = value();
        else if(option == "--offscreen") offscreen = true;
        else if(option == "--hidden") hidden = true;
        else throw std::invalid_argument("Unknown option: " + option);
    }
    if(!width || !height || width > 4096 || height > 4096 || frames < 0 || ((offscreen || hidden) && !frames))
        throw std::invalid_argument("Invalid render size or frame limit");

    SDL_SetMainReady();
    if(!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    struct SDLShutdown { ~SDLShutdown() { SDL_Quit(); } } shutdown;
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(nullptr, SDL_DestroyWindow);
    if(!offscreen) {
        window.reset(SDL_CreateWindow("GameEngine | rendering preview", int(width), int(height), SDL_WINDOW_RESIZABLE | (hidden ? SDL_WINDOW_HIDDEN : 0)));
        if(!window) throw std::runtime_error(SDL_GetError());
    }
    RHI::SDLGPUBackend backend(api);
    RHI::DeviceCreateDesc description;
    description.nativeWindow = {window.get(), nullptr, RHI::NativeWindowHandle::Kind::SDL};
    description.swapChain.width = width; description.swapChain.height = height;
    auto device = backend.CreateDevice(description);
    if(!device) throw std::runtime_error(SDL_GetError());

    // Use the engine model vertex type; there is no separate portable layout.
    // The application owns these buffers; FrameRenderer only borrows their handles.
    struct MeshOwner {
        RHI::IRHIDevice& device;
        ModelGeometryRuntimeMesh mesh;
        ~MeshOwner() {
            device.WaitIdle();
            if(mesh.indexBuffer) device.DestroyBuffer(mesh.indexBuffer);
            if(mesh.vertexBuffer) device.DestroyBuffer(mesh.vertexBuffer);
        }
    } geometry{*device, {}};
    const std::array<VERTEX_3D, 3> vertices{{
        {{-.7f,-.7f,.5f}, {0,0,-1}, {1,0,0}, {1,1,1,1}, {0,0}},
        {{ .7f,-.7f,.5f}, {0,0,-1}, {1,0,0}, {1,1,1,1}, {1,0}},
        {{   0, .7f,.5f}, {0,0,-1}, {1,0,0}, {1,1,1,1}, {.5f,1}}}};
    const std::array<uint32_t, 3> indices{0,1,2};
    RHI::BufferDesc buffer; buffer.byteSize = sizeof(vertices); buffer.stride = sizeof(VERTEX_3D); buffer.bindFlags = RHI::BufferBindFlags::Vertex;
    geometry.mesh.vertexBuffer = device->CreateBuffer(buffer, std::as_bytes(std::span(vertices)));
    buffer.byteSize = sizeof(indices); buffer.stride = sizeof(uint32_t); buffer.bindFlags = RHI::BufferBindFlags::Index;
    geometry.mesh.indexBuffer = device->CreateBuffer(buffer, std::as_bytes(std::span(indices)));
    geometry.mesh.vertexStride = sizeof(VERTEX_3D); geometry.mesh.vertexCount = geometry.mesh.indexCount = 3;
    if(!geometry.mesh.IsReady()) throw std::runtime_error("Preview geometry creation failed");
    const char* base = SDL_GetBasePath();
    const auto shaders = std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(base ? base : ""))) / "shaders";
    Rendering::FrameRenderer renderer(*device, shaders);
    Rendering::RenderScene scene;
    const Rendering::Matrix identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    scene.frame.viewProjection = scene.frame.lightViewProjection = identity;
    scene.frame.lightDirection = {0,0,-1,0};
    Rendering::DrawItem draw; draw.mesh = geometry.mesh; draw.instance.world = identity;
    draw.instance.color = {.11f,.48f,.75f,1}; draw.instance.shading[0] = 1; draw.castsShadow = false;
    scene.draws.push_back(draw);
    for(int frame = 0; !frames || frame < frames; ++frame) {
        SDL_Event event;
        while(SDL_PollEvent(&event))
            if(event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) return 0;
        int w = int(width), h = int(height);
        if(window) SDL_GetWindowSizeInPixels(window.get(), &w, &h);
        if(w <= 0 || h <= 0) { SDL_WaitEventTimeout(nullptr, 16); continue; }
        renderer.Resize(uint32_t(w), uint32_t(h));
        renderer.Render(scene, !offscreen);
    }
    if(!capture.empty()) {
        RHI::TextureReadback image;
        if(!renderer.Capture(image)) throw std::runtime_error("GPU capture failed");
        if(!capture.parent_path().empty()) std::filesystem::create_directories(capture.parent_path());
        std::ofstream file(capture, std::ios::binary);
        file << "P6\n" << image.width << ' ' << image.height << "\n255\n";
        for(uint32_t y = 0; y < image.height; ++y) for(uint32_t x = 0; x < image.width; ++x) {
            const auto* pixel = image.pixels.data() + size_t(y) * image.rowPitch + x * 4;
            if(image.format == RHI::Format::BGRA8_UNorm) {
                const char rgb[]{char(pixel[2]), char(pixel[1]), char(pixel[0])}; file.write(rgb, 3);
            } else file.write(reinterpret_cast<const char*>(pixel), 3);
        }
        if(!file) throw std::runtime_error("Capture write failed");
    }
    return 0;
}
#endif
