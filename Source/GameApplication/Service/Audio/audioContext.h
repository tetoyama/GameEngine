// =======================================================================
// 
// audioContext.h
// 
// =======================================================================
#pragma once
#include <xaudio2.h>
#include "Service/IService.h"

#pragma comment(lib, "xaudio2.lib")

class AudioContext: public IService {
private:
	IXAudio2* m_XAudio = nullptr;
	IXAudio2MasteringVoice* m_MasteringVoice = nullptr;

public:
	AudioContext() = default;

	~AudioContext(){
		Shutdown();
	}

	bool Initialize(){
		if(!m_XAudio){
			HRESULT hr = XAudio2Create(&m_XAudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
			if(FAILED(hr)){
				OutputDebugStringA("Failed to create XAudio2 engine\n");
				return false;
			}

			hr = m_XAudio->CreateMasteringVoice(&m_MasteringVoice);
			if(FAILED(hr)){
				OutputDebugStringA("Failed to create MasteringVoice\n");
				m_XAudio->Release();
				m_XAudio = nullptr;
				return false;
			}

			hr = m_XAudio->StartEngine();
			if(FAILED(hr)){
				OutputDebugStringA("Failed to start XAudio2 engine\n");
				m_MasteringVoice->DestroyVoice();
				m_MasteringVoice = nullptr;
				m_XAudio->Release();
				m_XAudio = nullptr;
				return false;
			}
		}
		return true;
	}

	void Shutdown() override{
		if(m_MasteringVoice){
			m_MasteringVoice->DestroyVoice();
			m_MasteringVoice = nullptr;
		}
		if(m_XAudio){
			m_XAudio->StopEngine();
			m_XAudio->Release();
			m_XAudio = nullptr;
		}
	}

	IXAudio2* GetXAudio() const{
		return m_XAudio;
	}

	IXAudio2MasteringVoice* GetMasteringVoice() const{
		return m_MasteringVoice;
	}

	IXAudio2SourceVoice* CreateSourceVoice(WAVEFORMATEX* wfx){
		if(!m_XAudio) return nullptr;

		char buf[256];
		sprintf_s(buf, "CreateSourceVoice: format=%d, channels=%d, sampleRate=%d, bitsPerSample=%d\n",
				  wfx->wFormatTag, wfx->nChannels, wfx->nSamplesPerSec, wfx->wBitsPerSample);
		OutputDebugStringA(buf);

		IXAudio2SourceVoice* voice = nullptr;
		HRESULT hr = m_XAudio->CreateSourceVoice(&voice, wfx);
		if(FAILED(hr)){
			OutputDebugStringA("Failed to create SourceVoice\n");
			return nullptr;
		}
		return voice;
	}
};
