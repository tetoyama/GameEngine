// ConfigSystem.h
#pragma once
#include <string>
#include <filesystem>
#include <iostream>
#include "YAMLConverters.h"

enum APPTYPE{
	Editor = 0,
	Player,
	DebugPlayer,
	DebugEditor,
};

struct APPCONFIG
{
	// アプリケーションの種類
	APPTYPE AppType = Editor;

	// Video Settings
	bool Vsync = false;
	
	bool FullScreen = false;
	int Width = 1280;
	int Height = 720;

	// Audio Settings
	float MasterVolume = 1.0f;
	float BGMVolume = 1.0f;
	float SEVolume = 1.0f;

	// Other Settings

};


class ConfigSystem: public IService {
public:
	ConfigSystem(){
	}
	~ConfigSystem(){
	}

	bool Initialize(){
		LoadConfig(L"config.yaml");
		return true;
	}

	void Shutdown() override{
		SaveConfig(L"config.yaml");
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

		if(appConfig.AppType == APPTYPE::DebugEditor){
			appConfig.AppType = APPTYPE::DebugPlayer;
		}

		return true;
	}

	void SaveConfig(const std::wstring& file){

		if(appConfig.AppType == APPTYPE::DebugPlayer){
			appConfig.AppType = APPTYPE::Editor;
		}

		YAML::Emitter out;
		out << YAML::BeginMap;

		out << YAML::Key << "AppType" << YAML::Value << static_cast<int>(appConfig.AppType);
		out << YAML::Key << "Vsync" << YAML::Value << appConfig.Vsync;
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
