// AudioLoader.h
#pragma once
class AudioLoader {
public:
    AudioLoader();
    ~AudioLoader();
    bool LoadAudio(const std::string& audioPath);
};
