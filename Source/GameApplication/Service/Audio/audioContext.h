// =======================================================================
// 
// audioContext.h
// 
// =======================================================================
#pragma once
#include <xaudio2.h>
#include "Service/IService.h"

#pragma comment(lib, "xaudio2.lib")

class DebugLogService;

// XAudio2を用いたオーディオエンジンの管理クラス
class AudioContext: public IService {
private:
	IXAudio2* m_XAudio = nullptr;
	IXAudio2MasteringVoice* m_MasteringVoice = nullptr;
	DebugLogService* m_DebugLog = nullptr;

public:
	explicit AudioContext(DebugLogService* debugLog = nullptr);

	~AudioContext();

	bool Initialize();

	void Shutdown() override;

	IXAudio2* GetXAudio() const{
		return m_XAudio;
	}

	IXAudio2MasteringVoice* GetMasteringVoice() const{
		return m_MasteringVoice;
	}

	IXAudio2SourceVoice* CreateSourceVoice(WAVEFORMATEX* wfx);
};
