// =======================================================================
// 
// audioData.h
// 
// =======================================================================
#pragma once
#include <xaudio2.h>
#include <string>
// オーディオリソースデータを保持するクラス
class AudioData {
public:
	std::string FilePath = "";
	BYTE* m_SoundData = nullptr;

	int m_Length = 0;
	int m_PlayLength = 0;

	WAVEFORMATEX m_Format = {};

	AudioData(){
		OutputDebugStringA("Created AudioData\n");
	}

	~AudioData(){
		OutputDebugStringA(("Destroyed AudioData: " + FilePath + "\n").c_str());

		delete[] m_SoundData;
		m_SoundData = nullptr;
	}
};
