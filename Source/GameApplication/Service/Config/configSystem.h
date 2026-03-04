// =======================================================================
// 
// configSystem.h
// 
// =======================================================================
#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include "YAMLConverters.h"
#include "buildSetting.h"
#include "appConfig.h"

// アプリケーション・エディタ設定ファイルの読み込み・保存を管理するサービス
class ConfigService: public IService {
public:
	ConfigService(){
	}
	~ConfigService(){
	}

	bool Initialize(){

		if(LoadEditorConfig(EDITOR_CONFIG_PATH)){
			OutputDebugStringW(L"Editor config loaded successfully.\n");
		} else{
			OutputDebugStringW(L"Failed to load editor config. Using default settings.\n");
			return false;
		}

		if(LoadApplicationConfig(APPLICATION_CONFIG_PATH)){
			OutputDebugStringW(L"Application config loaded successfully.\n");
		} else{
			OutputDebugStringW(L"Failed to load application config. Using default settings.\n");
			return false;
		}

		return true;
	}

	void Shutdown() override{

		SaveEditorConfig(EDITOR_CONFIG_PATH);
		SaveApplicationConfig(APPLICATION_CONFIG_PATH);
	}

	APPCONFIG appConfig;
	YAML::Node editorConfig;

	bool LoadEditorConfig(const std::wstring& file){
		std::ifstream fin(file);
		if(!fin){
			OutputDebugStringW((L"Config file not found: " + file).c_str());
			return false;
		}

		editorConfig = YAML::Load(fin);

		return true;
	}

	void SaveEditorConfig(const std::wstring& file){
		std::ofstream fout(file);
		if(!fout){
			OutputDebugStringW((L"Failed to save config file:" + file).c_str());
			return;
		}
		fout << editorConfig;
		fout.close();
	}

	bool LoadApplicationConfig(const std::wstring& file){

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
};
