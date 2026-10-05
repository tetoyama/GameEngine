#include "Service/Config/configSystem.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void Require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
int main() {
    const auto path = std::filesystem::temp_directory_path() / ("gameengine-config-"+std::to_string(GetCurrentProcessId())+".yaml");
    try {
        for(auto api : {RHI::BackendType::Direct3D11,RHI::BackendType::Direct3D12,RHI::BackendType::Vulkan,RHI::BackendType::Metal}) {
            for(auto target : {BuildTarget::Windows,BuildTarget::MacOS}) {
                ConfigService original;
                original.editorConfig["Systems"]["PreservedSetting"] = 37;
                original.engineConfig.graphics.backend = api;
                original.engineConfig.graphics.maximumFrameLatency = 3;
                original.engineConfig.buildTarget = target;
                original.SaveEditorConfig(path.wstring());
                ConfigService loaded;
                Require(loaded.LoadEditorConfig(path.wstring()),"Cannot reload editor configuration");
                Require(loaded.engineConfig.graphics.backend==api,"Rendering API was lost on reload");
                Require(loaded.engineConfig.buildTarget==target,"Build target was lost on reload");
                Require(loaded.engineConfig.graphics.maximumFrameLatency==3,"Frame latency was changed");
                Require(loaded.editorConfig["Systems"]["PreservedSetting"].as<int>()==37,"Other settings were overwritten");
            }
        }
        { std::ofstream output(path); output << "Build:\n  Target: macOS\n"; }
        ConfigService buildOnly;
        Require(buildOnly.LoadEditorConfig(path.wstring()) && buildOnly.engineConfig.buildTarget==BuildTarget::MacOS,"Build target depends on optional Graphics section");
        std::filesystem::remove(path);
        std::cout << "Rendering API and Windows/macOS build target persistence passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::error_code ignored; std::filesystem::remove(path,ignored);
        std::cerr << error.what() << "\n"; return 1;
    }
}
