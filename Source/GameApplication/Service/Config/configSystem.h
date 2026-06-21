// =======================================================================
//
// configSystem.h
//
// =======================================================================
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "YAMLConverters.h"
#include "buildSetting.h"
#include "appConfig.h"
#include "Service/Graphics/RHI/RHIBackend.h"

struct EngineGraphicsConfig {
	RHI::BackendType backend = RHI::BackendType::Direct3D11;
	uint32_t adapterIndex = 0;
	bool preferHighPerformanceAdapter = true;
	bool allowTearing = true;
	uint32_t swapChainBufferCount = 2;
};

struct EngineConfig {
	EngineGraphicsConfig graphics;
};

inline std::string_view ToEngineConfigBackendName(
	RHI::BackendType backend
) noexcept {
	switch(backend){
		case RHI::BackendType::Null:
			return "Null";
		case RHI::BackendType::Direct3D11:
			return "Direct3D11";
		case RHI::BackendType::Direct3D12:
			return "Direct3D12";
		case RHI::BackendType::Vulkan:
			return "Vulkan";
	}
	return "Direct3D11";
}

inline std::optional<RHI::BackendType> ParseEngineConfigBackend(
	std::string_view name
) noexcept {
	if(name == "Null" || name == "null"){
		return RHI::BackendType::Null;
	}
	if(name == "Direct3D11" || name == "D3D11" ||
		name == "direct3d11" || name == "d3d11"){
		return RHI::BackendType::Direct3D11;
	}
	if(name == "Direct3D12" || name == "D3D12" ||
		name == "direct3d12" || name == "d3d12"){
		return RHI::BackendType::Direct3D12;
	}
	if(name == "Vulkan" || name == "vulkan"){
		return RHI::BackendType::Vulkan;
	}
	return std::nullopt;
}

class ConfigService: public IService {
public:
	ConfigService() = default;
	~ConfigService() = default;

	bool Initialize(){
		if(LoadEditorConfig(EDITOR_CONFIG_PATH)){
			OutputDebugStringW(L"Engine config loaded successfully.\n");
		}
		else{
			OutputDebugStringW(L"Failed to load engine config. Using default settings.\n");
			return false;
		}

		if(LoadApplicationConfig(APPLICATION_CONFIG_PATH)){
			OutputDebugStringW(L"Application config loaded successfully.\n");
		}
		else{
			OutputDebugStringW(L"Failed to load application config. Using default settings.\n");
			return false;
		}

		// GraphicsContext初期化より前に、起動時に要求されたBackendを公開する。
		RHI::SetRequestedBackend(engineConfig.graphics.backend);
		OutputDebugStringA((
			"Requested RHI Backend: " +
			std::string(ToEngineConfigBackendName(engineConfig.graphics.backend)) +
			"\n"
		).c_str());

		return true;
	}

	void Shutdown() override {
		SaveEditorConfig(EDITOR_CONFIG_PATH);
		SaveApplicationConfig(APPLICATION_CONFIG_PATH);
	}

	APPCONFIG appConfig;
	EngineConfig engineConfig;
	YAML::Node editorConfig;

	bool LoadEditorConfig(const std::wstring& file){
		std::ifstream fin(file);
		if(!fin){
			OutputDebugStringW((L"Config file not found: " + file).c_str());
			return false;
		}

		try{
			editorConfig = YAML::Load(fin);
		}
		catch(const YAML::Exception& exception){
			OutputDebugStringA((
				std::string("Failed to parse EngineConfig.yaml: ") +
				exception.what() + "\n"
			).c_str());
			return false;
		}

		LoadTypedEngineConfig();
		return true;
	}

	void SaveEditorConfig(const std::wstring& file){
		WriteTypedEngineConfig();

		std::ofstream fout(file);
		if(!fout){
			OutputDebugStringW((L"Failed to save config file:" + file).c_str());
			return;
		}
		fout << editorConfig;
	}

	bool LoadApplicationConfig(const std::wstring& file){
		std::ifstream fin(file);
		if(!fin){
			OutputDebugStringW((L"Config file not found: " + file).c_str());
			return false;
		}

		YAML::Node root;
		try{
			root = YAML::Load(fin);
		}
		catch(const YAML::Exception& exception){
			OutputDebugStringA((
				std::string("Failed to parse ApplicationConfig.yaml: ") +
				exception.what() + "\n"
			).c_str());
			return false;
		}

		if(root["AppType"])
			appConfig.AppType = static_cast<APPTYPE>(root["AppType"].as<int>());
		if(root["StartScene"])
			appConfig.startSceneFilePath = root["StartScene"].as<std::string>();
		if(root["Vsync"])
			appConfig.Vsync = root["Vsync"].as<bool>();
		if(root["FullScreen"])
			appConfig.FullScreen = root["FullScreen"].as<bool>();
		if(root["Width"])
			appConfig.Width = root["Width"].as<int>();
		if(root["Height"])
			appConfig.Height = root["Height"].as<int>();
		if(root["MasterVolume"])
			appConfig.MasterVolume = root["MasterVolume"].as<float>();
		if(root["BGMVolume"])
			appConfig.BGMVolume = root["BGMVolume"].as<float>();
		if(root["SEVolume"])
			appConfig.SEVolume = root["SEVolume"].as<float>();
		if(root["TemplateDir"])
			appConfig.templateDir = root["TemplateDir"].as<std::string>();

		return true;
	}

	void SaveApplicationConfig(const std::wstring& file){
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "AppType" << YAML::Value << static_cast<int>(appConfig.AppType);
		out << YAML::Key << "StartScene" << YAML::Value << appConfig.startSceneFilePath;
		out << YAML::Key << "Vsync" << YAML::Value << appConfig.Vsync;
		out << YAML::Key << "FullScreen" << YAML::Value << appConfig.FullScreen;
		out << YAML::Key << "Width" << YAML::Value << appConfig.Width;
		out << YAML::Key << "Height" << YAML::Value << appConfig.Height;
		out << YAML::Key << "MasterVolume" << YAML::Value << appConfig.MasterVolume;
		out << YAML::Key << "BGMVolume" << YAML::Value << appConfig.BGMVolume;
		out << YAML::Key << "SEVolume" << YAML::Value << appConfig.SEVolume;
		out << YAML::Key << "TemplateDir" << YAML::Value << appConfig.templateDir;
		out << YAML::EndMap;

		std::ofstream fout(file);
		if(!fout){
			OutputDebugStringW((L"Failed to save config file:" + file).c_str());
			return;
		}
		fout << out.c_str();
	}

private:
	void LoadTypedEngineConfig(){
		engineConfig = EngineConfig{};
		const YAML::Node graphics = editorConfig["Graphics"];
		if(!graphics || !graphics.IsMap()) return;

		try{
			if(graphics["Backend"]){
				const std::string backendName = graphics["Backend"].as<std::string>();
				const auto backend = ParseEngineConfigBackend(backendName);
				if(backend){
					engineConfig.graphics.backend = *backend;
				}
				else{
					OutputDebugStringA((
						"Unknown Graphics.Backend '" + backendName +
						"'. Falling back to Direct3D11.\n"
					).c_str());
				}
			}

			if(graphics["AdapterIndex"]){
				engineConfig.graphics.adapterIndex =
					graphics["AdapterIndex"].as<uint32_t>();
			}
			if(graphics["PreferHighPerformanceAdapter"]){
				engineConfig.graphics.preferHighPerformanceAdapter =
					graphics["PreferHighPerformanceAdapter"].as<bool>();
			}
			if(graphics["AllowTearing"]){
				engineConfig.graphics.allowTearing =
					graphics["AllowTearing"].as<bool>();
			}
			if(graphics["SwapChainBufferCount"]){
				const uint32_t count =
					graphics["SwapChainBufferCount"].as<uint32_t>();
				if(count >= 2 && count <= 16){
					engineConfig.graphics.swapChainBufferCount = count;
				}
				else{
					OutputDebugStringA(
						"Graphics.SwapChainBufferCount must be between 2 and 16. Using 2.\n"
					);
				}
			}
		}
		catch(const YAML::Exception& exception){
			OutputDebugStringA((
				std::string("Invalid Graphics section in EngineConfig.yaml: ") +
				exception.what() + ". Using defaults.\n"
			).c_str());
			engineConfig = EngineConfig{};
		}
	}

	void WriteTypedEngineConfig(){
		YAML::Node graphics = editorConfig["Graphics"];
		graphics["Backend"] = std::string(
			ToEngineConfigBackendName(engineConfig.graphics.backend)
		);
		graphics["AdapterIndex"] = engineConfig.graphics.adapterIndex;
		graphics["PreferHighPerformanceAdapter"] =
			engineConfig.graphics.preferHighPerformanceAdapter;
		graphics["AllowTearing"] = engineConfig.graphics.allowTearing;
		graphics["SwapChainBufferCount"] =
			engineConfig.graphics.swapChainBufferCount;
		editorConfig["Graphics"] = graphics;
	}
};
