// =======================================================================
//
// MockEmbeddingBackend.h
//
// llama.cppを使わない決定論的な埋め込み実装。
//
// 単なるスタブではなく「意味のあるベクトル」を返すことを狙っている。
// 語トークンと文字trigramをハッシュして次元へ散らすため、
// 語彙が重なるテキストほどコサイン類似度が高くなる。
// これにより検索層のテストが配線確認に留まらず、
// 「関連するチャンクが実際に上位へ来るか」まで検証できる。
//
// 当然ながら本物の埋め込みモデルのような意味的一般化（言い換えへの頑健さ）は無い。
// そこはテトが Service/LlamaEmbeddingBackend を繋いだ後、
// 評価セットで実測して判断する領域。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "IEmbeddingBackend.h"

namespace agentos {

class MockEmbeddingBackend final : public IEmbeddingBackend {
public:
	explicit MockEmbeddingBackend(std::size_t dimensions = 256)
		: m_dimensions(dimensions == 0 ? 1 : dimensions) {}

	std::size_t Dimensions() const override { return m_dimensions; }
	std::size_t MaxInputTokens() const override { return 8192; }
	std::string Name() const override { return "mock-lexical-v1"; }

	Result Embed(
		const std::vector<std::string>& texts,
		std::vector<std::vector<float>>* out) override;

	// 単体テキストの埋め込み（クエリ用の便宜関数）
	std::vector<float> EmbedOne(const std::string& text) const;

private:
	std::size_t m_dimensions;
};

} // namespace agentos
