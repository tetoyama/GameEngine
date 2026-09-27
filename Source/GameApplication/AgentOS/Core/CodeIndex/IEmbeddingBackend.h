// =======================================================================
//
// IEmbeddingBackend.h
//
// テキストを固定長ベクトルへ変換する層の抽象。
//
// 既存の Core/Llm/ILlmBackend.h と同じ分離方針を踏襲している。
//   Core/CodeIndex/IEmbeddingBackend.h   ← 抽象（ここ）
//   Core/CodeIndex/MockEmbeddingBackend  ← Linuxテスト用の決定論的実装
//   Service/LlamaEmbeddingBackend        ← llama.cpp実装（Core外・MSVC必須）
//
// この分離によって、索引・永続化・検索という本体側のロジックを
// llama.cppに触れずに検証できる。実機依存は「llama.cppを叩く数十行」だけに閉じる。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../AgentOsTypes.h"

namespace agentos {

class IEmbeddingBackend {
public:
	virtual ~IEmbeddingBackend() = default;

	// 出力ベクトルの次元数。索引の全チャンクで一致していなければならない。
	virtual std::size_t Dimensions() const = 0;

	// モデルが受け付ける最大入力トークン数。
	// これを超えるチャンクは末尾が切り捨てられるため、
	// 呼び出し側は事前に分割するか、切り捨てを許容するかを判断する。
	// 実測ではEmbeddingGemma-300M(2048)で全チャンクの5.3%が超過する。
	virtual std::size_t MaxInputTokens() const = 0;

	// 索引に記録するモデル識別子。
	// モデルを差し替えたのに古いベクトルが残っている、という事故を防ぐために使う。
	virtual std::string Name() const = 0;

	// texts を順序通りに埋め込む。out は texts と同じ件数・同じ順序で返る。
	// 返るベクトルはL2正規化済みであること（コサイン類似度を内積で計算するため）。
	virtual Result Embed(
		const std::vector<std::string>& texts,
		std::vector<std::vector<float>>* out) = 0;
};

} // namespace agentos
