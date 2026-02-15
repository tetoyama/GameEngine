// ConfigService.h
#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include "YAMLConverters.h"
#include "buildSetting.h"
#include "appConfig.h"

class ConfigService: public IService {
public:
	ConfigService(){
	}
	~ConfigService(){
	}

	bool Initialize(){
		return LoadConfig(CONFIG_PATH);;
	}

	void Shutdown() override{
		SaveConfig(CONFIG_PATH);
	}

	APPCONFIG appConfig;

	bool LoadConfig(const std::wstring& file){

		std::ifstream fin(file);
		if(!fin){
			OutputDebugStringW((L"Config file not found: " + file).c_str());
			return false;
		}

		YAML::Node root = YAML::Load(fin);

		if(root["AppType"])
			appConfig.AppType = static_cast<APPTYPE>(root["AppType"].as<int>());

		if(root["StartScene"])
			appConfig.startSceneFilePath = root["StartScene"].as<std::string>();

		if(root["Vsync"])
			appConfig.Vsync = root["Vsync"].as<bool>();

		if(root["ShaderPath"])
			appConfig.ShaderPath = root["ShaderPath"].as<std::string>();

		if(root["ShaderMaterial"]){
			appConfig.ShaderMaterials.clear();
			YAML::Node materialsNode = root["ShaderMaterial"];
			for(auto material : materialsNode){
				ShaderMaterial shaderMaterial;
				shaderMaterial.filePath = material.first.as<std::string>();
				shaderMaterial.entryPoint = material.second.as<std::string>();
				appConfig.ShaderMaterials.push_back(shaderMaterial);
			}
		}


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

		return true;
	}

	void SaveConfig(const std::wstring& file){

		YAML::Emitter out;
		out << YAML::BeginMap;

		out << YAML::Key << "AppType" << YAML::Value << static_cast<int>(appConfig.AppType);
		out << YAML::Key << "StartScene" << YAML::Value << appConfig.startSceneFilePath;
		out << YAML::Key << "Vsync" << YAML::Value << appConfig.Vsync;
		out << YAML::Key << "ShaderPath" << YAML::Value << appConfig.ShaderPath;

		out << YAML::Key << "ShaderMaterial";
		out << YAML::Value << YAML::BeginMap;

		for(const auto& material : appConfig.ShaderMaterials){
			out << YAML::Key << material.filePath;
			out << YAML::Value << material.entryPoint;
		}

		out << YAML::EndMap;


		out << YAML::Key << "FullScreen" << YAML::Value << appConfig.FullScreen;
		out << YAML::Key << "Width" << YAML::Value << appConfig.Width;
		out << YAML::Key << "Height" << YAML::Value << appConfig.Height;
		out << YAML::Key << "MasterVolume" << YAML::Value << appConfig.MasterVolume;
		out << YAML::Key << "BGMVolume" << YAML::Value << appConfig.BGMVolume;
		out << YAML::Key << "SEVolume" << YAML::Value << appConfig.SEVolume;

		out << YAML::EndMap;

		std::ofstream fout(file);
		if(!fout){
			OutputDebugStringW((L"Failed to save config file:" + file).c_str());
			return;
		}
		fout << out.c_str();
	}
};
