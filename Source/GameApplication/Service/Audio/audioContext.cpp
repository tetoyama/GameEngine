// =======================================================================
// 
// audioContext.cpp
// 
// =======================================================================
#include "audioContext.h"

#include <Windows.h>
#include "Service/DebugTools/DebugSystem.h"

#define AUDIO_LOG(level, msg) do { if(m_DebugLog) { m_DebugLog->Log(level, msg, __FUNCTION__, __FILE__, __LINE__); } } while(0)

AudioContext::AudioContext(DebugLogService* debugLog)
	: m_DebugLog(debugLog){}

AudioContext::~AudioContext(){
	Shutdown();
}

bool AudioContext::Initialize(){
	AUDIO_LOG(LogLevel::Info, "AudioContext の初期化を開始します");
	if(!m_XAudio){
		HRESULT hr = XAudio2Create(&m_XAudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if(FAILED(hr)){
			AUDIO_LOG(LogLevel::Error, "XAudio2 エンジンの生成に失敗しました");
			OutputDebugStringA("Failed to create XAudio2 engine\n");
			return false;
		}

		hr = m_XAudio->CreateMasteringVoice(&m_MasteringVoice);
		if(FAILED(hr)){
			AUDIO_LOG(LogLevel::Error, "MasteringVoice の生成に失敗しました");
			OutputDebugStringA("Failed to create MasteringVoice\n");
			m_XAudio->Release();
			m_XAudio = nullptr;
			return false;
		}

		hr = m_XAudio->StartEngine();
		if(FAILED(hr)){
			AUDIO_LOG(LogLevel::Error, "XAudio2 エンジンの開始に失敗しました");
			OutputDebugStringA("Failed to start XAudio2 engine\n");
			m_MasteringVoice->DestroyVoice();
			m_MasteringVoice = nullptr;
			m_XAudio->Release();
			m_XAudio = nullptr;
			return false;
		}
	}
	m_IsShutdown = false;
	AUDIO_LOG(LogLevel::Info, "AudioContext の初期化が完了しました");
	return true;
}

void AudioContext::Shutdown(){
	if(m_IsShutdown){
		return;
	}
	m_IsShutdown = true;

	AUDIO_LOG(LogLevel::Info, "AudioContext を終了します");
	if(m_MasteringVoice){
		m_MasteringVoice->DestroyVoice();
		m_MasteringVoice = nullptr;
	}
	if(m_XAudio){
		m_XAudio->StopEngine();
		m_XAudio->Release();
		m_XAudio = nullptr;
	}
	AUDIO_LOG(LogLevel::Info, "AudioContext の終了が完了しました");
}

IXAudio2SourceVoice* AudioContext::CreateSourceVoice(WAVEFORMATEX* wfx){
	if(!m_XAudio) return nullptr;

	AUDIO_LOG(LogLevel::Trace, "SourceVoice を生成します");

	char buf[256];
	sprintf_s(buf, "CreateSourceVoice: format=%d, channels=%d, sampleRate=%d, bitsPerSample=%d\n",
			  wfx->wFormatTag, wfx->nChannels, wfx->nSamplesPerSec, wfx->wBitsPerSample);
	OutputDebugStringA(buf);

	IXAudio2SourceVoice* voice = nullptr;
	HRESULT hr = m_XAudio->CreateSourceVoice(&voice, wfx);
	if(FAILED(hr)){
		AUDIO_LOG(LogLevel::Error, "SourceVoice の生成に失敗しました");
		OutputDebugStringA("Failed to create SourceVoice\n");
		return nullptr;
	}
	AUDIO_LOG(LogLevel::Debug, "SourceVoice の生成が完了しました");
	return voice;
}

#undef AUDIO_LOG
