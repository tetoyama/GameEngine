// =======================================================================
//
// CodeSearch.h
//
// コード索引に対する検索層。
//
// 2系統を並列に走らせ、Reciprocal Rank Fusion で統合する。
//
//   字句検索（Lexical）
//     シンボル名の完全一致・部分一致と、語彙の重なり。
//     「ApplyCodePatch の実装はどこ」のような、名前を知っているクエリに強い。
//     埋め込みは原理的にこれを超えられない。
//
//   ベクトル検索（Vector）
//     埋め込みのコサイン類似度。全件走査。
//     「当たり判定を作ってる箇所」のような、語彙が一致しないクエリに強い。
//     一方で Serialize と Deserialize が近い座標に来るなど、
//     論理的な区別は苦手。
//
// 片方だけでは落ちるクエリが必ずあるため、両方を持って融合する。
// これがコードRAGでハイブリッドが定番である理由。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "CodeIndexStore.h"

namespace agentos {

// ---------------------------------
// 検索結果1件
// ---------------------------------
struct SearchHit {
	std::size_t index = 0; // SearchIndex内のチャンク添字
	float score = 0.0f;

	// 診断用。どちらの経路で何位だったか（0 = ヒットせず）。
	int lexicalRank = 0;
	int vectorRank = 0;
};

// ---------------------------------
// SearchIndex
// ---------------------------------
// 索引全体をメモリに載せ、検索を提供する。
// チャンク1,448件・1024次元でも6MB程度なので全部載せて問題ない。
class SearchIndex {
public:
	// StoredChunk列を受け取って構築する。ベクトルは連続領域へ詰め直され、
	// 全走査時のキャッシュ効率が上がる。
	Result Build(std::vector<StoredChunk> chunks);

	std::size_t Size() const noexcept { return m_chunks.size(); }
	std::size_t Dimensions() const noexcept { return m_dimensions; }

	// ベクトルを持つチャンク数。索引が未ベクトル化かどうかの判定に使う。
	std::size_t EmbeddedCount() const noexcept { return m_embeddedCount; }

	const StoredChunk& At(std::size_t index) const { return m_chunks[index]; }

	// --- 字句検索 ---
	// シンボル名の一致を最優先し、次に語彙の重なりで順位付けする。
	std::vector<SearchHit> SearchLexical(const std::string& query, std::size_t topK) const;

	// --- ベクトル検索 ---
	// queryVector は正規化済みであること。全件と内積を取る。
	std::vector<SearchHit> SearchVector(
		const std::vector<float>& queryVector, std::size_t topK) const;

	// --- ハイブリッド ---
	// 2系統の順位を Reciprocal Rank Fusion で統合する。
	//   score = Σ 1 / (rrfK + rank)
	// スコアの絶対値ではなく順位のみを使うため、
	// 尺度の異なる2系統をそのまま混ぜられるのが利点。
	std::vector<SearchHit> SearchHybrid(
		const std::string& query,
		const std::vector<float>& queryVector,
		std::size_t topK,
		std::size_t candidatePool = 50,
		double rrfK = 60.0) const;

private:
	std::vector<StoredChunk> m_chunks;

	// 全ベクトルの連続配置（行優先 chunkIndex * dim）
	std::vector<float> m_matrix;
	std::vector<char> m_hasEmbedding;

	std::size_t m_dimensions = 0;
	std::size_t m_embeddedCount = 0;

	// 各チャンクの語彙集合（小文字化済み・重複除去済み）
	std::vector<std::vector<std::string>> m_tokens;

	// トークン → IDF（逆文書頻度）。
	// 単純な語の重なり率だと、日本語bigramの「して」「いる」のような
	// 頻出語が効いてしまい「日本語コメントが多いだけのチャンク」が上位に来る。
	// 実データで実際にそうなったため、稀な語ほど重く数える重み付けを入れている。
	std::unordered_map<std::string, float> m_idf;

	// 平均文書長。長い文書ほど「たまたま語を含む」確率が上がるため、
	// これを基準に長さ正規化をかける（BM25と同じ考え方）。
	double m_averageDocLength = 1.0;

	float IdfOf(const std::string& token) const;
};

// テキストを検索用トークンへ分解する。
// 識別子はそのまま1語にし、加えてCamelCase / snake_case を分割した語も足す。
// "ApplyCodePatch" が "apply" "code" "patch" でも引けるようにするため。
std::vector<std::string> TokenizeForSearch(const std::string& text);

} // namespace agentos
