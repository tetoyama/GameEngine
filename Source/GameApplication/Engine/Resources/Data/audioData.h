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
	BYTE* soundData = nullptr;

	int length = 0;
	int playLength = 0;

	WAVEFORMATEX format = {};

	AudioData(){
		OutputDebugStringA("Created AudioData\n");
	}

	~AudioData(){
		OutputDebugStringA(("Destroyed AudioData: " + FilePath + "\n").c_str());

		delete[] soundData;
		soundData = nullptr;
	}
};
