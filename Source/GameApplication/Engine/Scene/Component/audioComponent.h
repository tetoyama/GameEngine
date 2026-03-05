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
// XAudio2 の IXAudio2SourceVoice を通じて音声の再生・停止・音量制御を行う
// AudioSystem が PlayOnStart フラグを見て再生を開始する
class AudioComponent: public IComponent {
public:
	std::shared_ptr<AudioData> m_AudioData;        // ロード済みオーディオデータ
	std::string FilePath;                           // オーディオファイルのパス（YAML 保存用）

	bool Loop = false;          // ループ再生するか
	float Volume = 1.0f;        // 音量（0.0〜1.0）
	bool PlayOnStart = false;   // シーン開始時に自動再生するか
	bool Playing = false;       // 現在再生中かどうか
	bool isInitialized = false; // AudioSystem によって初期化済みかどうか

	IXAudio2SourceVoice* m_SourceVoice = nullptr; // 再生ハンドル（AudioSystem が生成・管理）

	AudioComponent() = default;

	// デストラクタ: ソースボイスを停止・解放する
	~AudioComponent(){
		if(m_SourceVoice){
			m_SourceVoice->Stop();
			m_SourceVoice->DestroyVoice();
			m_SourceVoice = nullptr;
		}
	}

	// AudioContext を渡して音声を再生する
	// 既存のソースボイスがある場合は停止・解放してから新規生成する
	// 引数:
	//   audioContext - XAudio2 マスタリングボイスを保持するコンテキスト
	// 戻り値: 再生開始に成功した場合 true
	bool Play(AudioContext* audioContext){
		if(!m_AudioData || !m_AudioData->m_SoundData || !audioContext)
			return false;

		// 既存ボイスを破棄して新規ボイスを作成
		if(m_SourceVoice){
			m_SourceVoice->Stop();
			m_SourceVoice->DestroyVoice();
			m_SourceVoice = nullptr;
		}

		// オーディオフォーマットに合ったソースボイスを生成
		m_SourceVoice = audioContext->CreateSourceVoice(&m_AudioData->m_Format);
		if(!m_SourceVoice) return false;

		// バッファを設定してソースボイスに送信
		XAUDIO2_BUFFER buffer = {};
		buffer.AudioBytes = m_AudioData->m_Length;
		buffer.pAudioData = m_AudioData->m_SoundData;
		buffer.PlayBegin = 0;
		buffer.PlayLength = m_AudioData->m_PlayLength;
		buffer.Flags = XAUDIO2_END_OF_STREAM;

		// ループ設定: 無限ループの場合はバッファ全体を繰り返す
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

	// 再生中の音声を停止してソースボイスを解放する
	void Stop(){
		if(m_SourceVoice){
			m_SourceVoice->Stop();
			m_SourceVoice->FlushSourceBuffers(); // バッファをフラッシュして再生残を破棄
			m_SourceVoice->DestroyVoice();
			m_SourceVoice = nullptr;
		}
		Playing = false;
	}

	// 音声を完全にリセットする（ファイルロードし直し時に呼ぶ）
	// 停止・初期化フラグクリア・オーディオデータの解放を行う
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
