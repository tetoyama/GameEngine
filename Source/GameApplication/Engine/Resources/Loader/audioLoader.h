// =======================================================================
// 
// audioLoader.h
// 
// =======================================================================
#pragma once

#include "ResourceLoader.h"
#include <xaudio2.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <memory>
#include <string>

#include "Resources/Data/audioData.h"
#include "Audio/audioContext.h"
#include "Backends/checkFileExtention.h"

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "winmm.lib")

inline std::shared_ptr<AudioData> LoadAudioFromWav(const std::string& filePath, AudioContext* context){
	HMMIO m_Hmmio= nullptr;
	MMCKINFO m_RiffChunk= {};
	MMCKINFO m_FormatChunk= {};
	MMCKINFO m_DataChunk= {};
	WAVEFORMATEX m_Wfx= {};

	hmmio = mmioOpenA((LPSTR)filePath.c_str(), nullptr, MMIO_READ);
	if(!hmmio) return nullptr;

	riffChunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	if(mmioDescend(hmmio, &riffChunk, nullptr, MMIO_FINDRIFF) != MMSYSERR_NOERROR){
		mmioClose(hmmio, 0);
		return m_Nullptr;
	}

	formatChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if(mmioDescend(hmmio, &formatChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR){
		mmioClose(hmmio, 0);
		return m_Nullptr;
	}

	if(formatChunk.cksize < sizeof(WAVEFORMATEX)){
		PCMWAVEFORMAT m_Pcmwf= {};
		mmioRead(hmmio, (HPSTR)&pcmwf, sizeof(pcmwf));
		memcpy(&wfx, &pcmwf, sizeof(pcmwf));
		wfx.cbSize = 0;
	} else{
		mmioRead(hmmio, (HPSTR)&wfx, sizeof(wfx));
	}
	mmioAscend(hmmio, &formatChunk, 0);

	dataChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if(mmioDescend(hmmio, &dataChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR){
		mmioClose(hmmio, 0);
		return m_Nullptr;
	}

	auto m_Audio= std::make_shared<AudioData>();
	audio->FilePath = filePath;
	audio->m_Length = dataChunk.cksize;
	audio->m_PlayLength = dataChunk.cksize / wfx.nBlockAlign;
	audio->m_SoundData = new BYTE[dataChunk.cksize];
	audio->m_Format = wfx;

	mmioRead(hmmio, (HPSTR)audio->m_SoundData, dataChunk.cksize);

	mmioClose(hmmio, 0);

	return m_Audio;
}


template<>
inline void ResourceLoader<AudioData>::SetupLoadFunc(void* contextPtr){
	OutputDebugStringA("SetupLoadFunc AudioData called\n");

	auto m_Context= static_cast<AudioContext*>(contextPtr);
	SetLoadFunction([=](const std::string& path, std::shared_ptr<void> /*args*/) {
		return LoadAudioFromWav(path, context);
	});
}
