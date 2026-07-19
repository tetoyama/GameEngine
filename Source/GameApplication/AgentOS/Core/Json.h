// =======================================================================
//
// Json.h
//
// AgentOS Core用のJSONラッパ。
// llama.cpp用にvendor済みのnlohmann/jsonを再利用する。
// （Linuxテスト時は -I Source/GameApplication/Backends/llama/vendor でも解決可能）
//
// =======================================================================
#pragma once

#include "../../Backends/llama/vendor/nlohmann/json.hpp"

namespace agentos {

using Json = nlohmann::json;

} // namespace agentos
