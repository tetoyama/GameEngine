#pragma once


#include <string>

struct AgentConfig {
    int maxTokens = 1024;        // 最大生成トークン数
    float temperature = 0.7f;    // 温度パラメータ（サンプリングの多様性）
    float topP = 0.9f;           // nucleus sampling
    int topK = 40;               // top-k sampling
    bool useStreaming = true;    // 応答をストリーム形式で受け取るか
};
