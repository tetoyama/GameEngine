// =======================================================================
// 
// audioComponent.h
// 
// =======================================================================
#pragma once
#include <memory>
#include <string>
#include <xaudio2.h>

#include "Interface/IComponent.h"

#include "Service/Audio/audioContext.h"
#include "Resources/Data/AudioData.h"
#include "scene.h"
#include "sceneManager.h"
#include "Resources/resourceService.h"

class AudioContext;

// オーディオの再生を管理するコンポーネント
class AudioComponent: public IComponent {
public:
	std::shared_ptr<AudioData> m_AudioData;
	std::string FilePath;

	bool Loop = false;
	float Volume = 1.0f;
	bool PlayOnStart = false;
	bool Playing = false;
	bool isInitialized = false;

	IXAudio2SourceVoice* m_SourceVoice = nullptr; // 再生ハンドルは実行時に取得する

	AudioComponent() = default;
	~AudioComponent(){
		if(m_SourceVoice){
			m_SourceVoice->Stop();
			m_SourceVoice->DestroyVoice();
			m_SourceVoice = nullptr;
		}
	}

	// AudioContextを外から渡して再生開始
	bool Play(AudioContext* audioContext){
		if(!m_AudioData || !m_AudioData->m_SoundData || !audioContext)
			return false;

		if(m_SourceVoice){
			m_SourceVoice->Stop();
			m_SourceVoice->DestroyVoice();
			m_SourceVoice = nullptr;
		}

		m_SourceVoice = audioContext->CreateSourceVoice(&m_AudioData->m_Format);
		if(!m_SourceVoice) return false;

		XAUDIO2_BUFFER buffer = {};
		buffer.AudioBytes = m_AudioData->m_Length;
		buffer.pAudioData = m_AudioData->m_SoundData;
		buffer.PlayBegin = 0;
		buffer.PlayLength = m_AudioData->m_PlayLength;
		buffer.Flags = XAUDIO2_END_OF_STREAM;

		if(Loop){
			buffer.LoopBegin = 0;
			buffer.LoopLength = m_AudioData->m_PlayLength;
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}

		HRESULT hr = m_SourceVoice->SubmitSourceBuffer(&buffer);
		if(FAILED(hr)){
			m_SourceVoice->DestroyVoice();
			m_SourceVoice = nullptr;
			return false;
		}

		m_SourceVoice->Start();
		m_SourceVoice->SetVolume(Volume);
		Playing = true;

		return true;
	}

	void Stop(){
		if(m_SourceVoice){
			m_SourceVoice->Stop();
			m_SourceVoice->FlushSourceBuffers();
			m_SourceVoice->DestroyVoice();
			m_SourceVoice = nullptr;
		}
		Playing = false;
	}

	void Reset(){
		Stop();
		isInitialized = false;
		m_AudioData.reset();
	}

	YAML::Node encode() override{
		YAML::Node node;
		node["FilePath"] = FilePath;
		node["Loop"] = Loop;
		node["Volume"] = Volume;
		node["PlayOnStart"] = PlayOnStart;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node.IsMap()) return false;
		if(node["FilePath"]) FilePath = node["FilePath"].as<std::string>();
		if(node["Loop"]) Loop = node["Loop"].as<bool>();
		if(node["Volume"]) Volume = node["Volume"].as<float>();
		if(node["PlayOnStart"]) PlayOnStart = node["PlayOnStart"].as<bool>();
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::PushID(this);

		char filepathBuffer[256] = "";
		strncpy_s(filepathBuffer, sizeof(filepathBuffer), FilePath.c_str(), _TRUNCATE);

		ImGui::BeginGroup();

		ImGui::Text("Audio");
		ImGui::SameLine(100);
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 24.0f);
		if(ImGui::InputText("##AudioInput", filepathBuffer, sizeof(filepathBuffer))){
			Reset();
			FilePath = std::string(filepathBuffer);
		}
		ImGui::PopItemWidth();

		ImGui::SameLine();
		if(ImGui::SmallButton("x")){
			Reset();
			FilePath.clear();
		}
		ImGui::EndGroup();

		if(ImGui::BeginDragDropTarget()){
			if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")){
				Reset();
				FilePath = (const char*)payload->Data;
			}
			ImGui::EndDragDropTarget();
		}

		if(ImGui::UndoCheckbox("Loop", &Loop)){
			Reset();
		}
		
		ImGui::SameLine();
		
		if(ImGui::UndoCheckbox("Play On Start", &PlayOnStart)){
			Reset();
		}

		ImGui::AlignTextToFramePadding();
		ImGui::Text("Volume");
		ImGui::SameLine(100);
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
		if(ImGui::UndoSliderFloat("##VolumeSlider", &Volume, 0.0f, 1.0f)){
			if(m_SourceVoice){
				m_SourceVoice->SetVolume(Volume);
			}
		}
		ImGui::PopItemWidth();

		ImGui::PopID();
	}
};
