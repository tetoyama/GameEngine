// =======================================================================
//
// CodeSearch.cpp
//
// =======================================================================
#include "CodeSearch.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <climits>
#include <unordered_map>
#include <unordered_set>

#include "VectorMath.h"

namespace agentos {

namespace {

// 文書長正規化の強さ（BM25のb）。0で無効、1で完全に長さで割る。
// 全文検索の慣例値0.75をそのまま使う。
constexpr double kLengthNormB = 0.75;

bool IsWordChar(unsigned char c) {
	return std::isalnum(c) != 0 || c == '_';
}

bool IsUpper(unsigned char c) { return c >= 'A' && c <= 'Z'; }
bool IsLower(unsigned char c) { return c >= 'a' && c <= 'z'; }

char LowerAscii(unsigned char c) { return static_cast<char>(std::tolower(c)); }

std::string ToLower(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for(const char c : s) out.push_back(LowerAscii(static_cast<unsigned char>(c)));
	return out;
}

// UTF-8の先頭バイトから、その文字のバイト数を得る。
// 不正なバイトは1として進め、走査が止まらないようにする。
std::size_t Utf8CharLen(unsigned char c) {
	if(c < 0x80) return 1;
	if((c & 0xE0) == 0xC0) return 2;
	if((c & 0xF0) == 0xE0) return 3;
	if((c & 0xF8) == 0xF0) return 4;
	return 1;
}

// 非ASCII（日本語など）の連なりをbigramへ割る。
//
// 日本語は空白で語が区切られないため、ASCII前提の分かち書きでは
// トークンが1つも取れない。全文検索で定番のbigram索引を使う。
//   "当たり判定" → 当た, たり, り判, 判定
// クエリ側も同じ規則で割れば、部分一致が自然に成立する。
//
// この対応が要るのは実測に基づく。コメント行の44.7%が日本語であり、
// テトのクエリも日本語で来る。ここを落とすと字句検索が半身になる。
void EmitCjkBigrams(const std::vector<std::string>& chars, std::vector<std::string>* out) {
	if(chars.empty()) return;
	if(chars.size() == 1) {
		out->push_back(chars[0]);
		return;
	}
	for(std::size_t i = 0; i + 1 < chars.size(); ++i) {
		out->push_back(chars[i] + chars[i + 1]);
	}
}

// CamelCase / snake_case を構成語へ割る。
// "ApplyCodePatch" → apply, code, patch
// "m_frameCounter" → m, frame, counter
void SplitCompound(const std::string& word, std::vector<std::string>* out) {
	std::string cur;
	for(std::size_t i = 0; i < word.size(); ++i) {
		const unsigned char c = static_cast<unsigned char>(word[i]);

		if(c == '_') {
			if(!cur.empty()) { out->push_back(cur); cur.clear(); }
			continue;
		}

		// 小文字→大文字の境界で切る（applyCode → apply / Code）
		if(IsUpper(c) && !cur.empty()) {
			const unsigned char prev = static_cast<unsigned char>(word[i - 1]);
			const bool prevLower = IsLower(prev);
			// 連続大文字の直後に小文字が来る境界でも切る（HTTPServer → HTTP / Server）
			const bool acronymEnd =
				IsUpper(prev) && (i + 1 < word.size()) &&
				IsLower(static_cast<unsigned char>(word[i + 1]));
			if(prevLower || acronymEnd) {
				out->push_back(cur);
				cur.clear();
			}
		}
		cur.push_back(LowerAscii(c));
	}
	if(!cur.empty()) out->push_back(cur);
}

} // namespace

// =======================================================================
// TokenizeForSearch
// =======================================================================
std::vector<std::string> TokenizeForSearch(const std::string& text) {
	std::vector<std::string> tokens;
	std::string word;                 // ASCII識別子の蓄積
	std::vector<std::string> cjkRun;  // 非ASCII文字の連なり

	const auto flushWord = [&]() {
		if(word.empty()) return;
		tokens.push_back(ToLower(word));
		// 分割語も足す。元の語を残すのは完全一致を優先したいため。
		std::vector<std::string> parts;
		SplitCompound(word, &parts);
		for(std::string& p : parts) {
			if(p.size() >= 2 && p != tokens.back()) tokens.push_back(std::move(p));
		}
		word.clear();
	};

	const auto flushCjk = [&]() {
		if(cjkRun.empty()) return;
		EmitCjkBigrams(cjkRun, &tokens);
		cjkRun.clear();
	};

	// UTF-8を文字単位で走査する。ASCIIと非ASCIIで扱いを分ける。
	std::size_t i = 0;
	while(i < text.size()) {
		const unsigned char c = static_cast<unsigned char>(text[i]);
		const std::size_t len = Utf8CharLen(c);

		if(len == 1) {
			flushCjk();
			if(IsWordChar(c)) word.push_back(text[i]);
			else flushWord();
		} else {
			flushWord();
			// 末尾が途切れた不正シーケンスは切り捨てる
			if(i + len > text.size()) break;
			cjkRun.push_back(text.substr(i, len));
		}
		i += len;
	}
	flushWord();
	flushCjk();

	// 重複を除く（出現頻度は使わず、集合としての重なりで測る）
	std::sort(tokens.begin(), tokens.end());
	tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
	return tokens;
}

// =======================================================================
// SearchIndex::Build
// =======================================================================
Result SearchIndex::Build(std::vector<StoredChunk> chunks) {
	m_chunks = std::move(chunks);
	m_matrix.clear();
	m_hasEmbedding.assign(m_chunks.size(), 0);
	m_dimensions = 0;
	m_embeddedCount = 0;

	// 次元数は最初に見つかったベクトルで決め、以降は一致を要求する。
	for(const StoredChunk& sc : m_chunks) {
		if(!sc.embedding.empty()) {
			m_dimensions = sc.embedding.size();
			break;
		}
	}

	if(m_dimensions > 0) {
		m_matrix.assign(m_chunks.size() * m_dimensions, 0.0f);
		for(std::size_t i = 0; i < m_chunks.size(); ++i) {
			const std::vector<float>& e = m_chunks[i].embedding;
			if(e.empty()) continue;
			if(e.size() != m_dimensions) {
				return Result::Fail(
					"SearchIndex::Build: 次元数が混在している（索引の再構築が必要）: "
					+ m_chunks[i].chunk.qualifiedName);
			}
			std::copy(e.begin(), e.end(), m_matrix.begin() + static_cast<std::ptrdiff_t>(i * m_dimensions));
			m_hasEmbedding[i] = 1;
			++m_embeddedCount;
		}
	}

	// 語彙は検索のたびに作り直さず、構築時に1回だけ用意する。
	m_tokens.clear();
	m_tokens.reserve(m_chunks.size());
	for(const StoredChunk& sc : m_chunks) {
		// 名前とモジュールは本文より情報密度が高いので必ず含める。
		std::string material = sc.chunk.qualifiedName + " " + sc.chunk.moduleTag + " " + sc.chunk.text;
		m_tokens.push_back(TokenizeForSearch(material));
	}

	// --- IDF（逆文書頻度）を求める ---
	// これが無いと日本語bigramの「して」「いる」のような頻出語が支配し、
	// 「日本語コメントが多いだけのチャンク」が上位を占める（実データで確認した）。
	std::unordered_map<std::string, std::size_t> documentFrequency;
	for(const std::vector<std::string>& docTokens : m_tokens) {
		// m_tokens は重複除去済みなので、そのまま数えれば文書頻度になる。
		for(const std::string& t : docTokens) ++documentFrequency[t];
	}

	// 平均文書長。長さ正規化の基準になる。
	std::size_t totalLength = 0;
	for(const std::vector<std::string>& docTokens : m_tokens) totalLength += docTokens.size();
	m_averageDocLength = m_tokens.empty()
		? 1.0
		: (static_cast<double>(totalLength) / static_cast<double>(m_tokens.size()));
	if(m_averageDocLength <= 0.0) m_averageDocLength = 1.0;

	m_idf.clear();
	m_idf.reserve(documentFrequency.size());
	const double n = static_cast<double>(m_tokens.size());
	for(const auto& kv : documentFrequency) {
		const double df = static_cast<double>(kv.second);
		// BM25と同じ形。全文書に出る語でも0や負にならないよう +1 してある。
		const double idf = std::log((n - df + 0.5) / (df + 0.5) + 1.0);
		m_idf.emplace(kv.first, static_cast<float>(idf));
	}

	return Result::Ok();
}

float SearchIndex::IdfOf(const std::string& token) const {
	const auto it = m_idf.find(token);
	// 索引に無い語（クエリ固有の語）は最も稀とみなす。
	// 見たことのない識別子ほど検索意図を強く表すため。
	if(it == m_idf.end()) {
		const double n = static_cast<double>(m_tokens.empty() ? 1 : m_tokens.size());
		return static_cast<float>(std::log(n + 1.0));
	}
	return it->second;
}

// =======================================================================
// SearchIndex::SearchLexical
// =======================================================================
std::vector<SearchHit> SearchIndex::SearchLexical(
	const std::string& query, std::size_t topK) const {

	const std::vector<std::string> queryTokens = TokenizeForSearch(query);
	if(queryTokens.empty()) return {};

	const std::string loweredQuery = ToLower(query);

	// クエリ側のIDF総和。被覆率の分母になる。
	double queryIdfTotal = 0.0;
	std::vector<float> queryIdf;
	queryIdf.reserve(queryTokens.size());
	for(const std::string& qt : queryTokens) {
		const float idf = IdfOf(qt);
		queryIdf.push_back(idf);
		queryIdfTotal += idf;
	}
	if(queryIdfTotal <= 0.0) return {};

	std::vector<SearchHit> scored;
	scored.reserve(m_chunks.size());

	for(std::size_t i = 0; i < m_chunks.size(); ++i) {
		const std::vector<std::string>& docTokens = m_tokens[i];
		if(docTokens.empty()) continue;

		// --- 語彙の重なりをIDFで重み付けする ---
		// 単純な件数だと「して」「いる」のような頻出bigramが支配してしまう。
		// 稀な語ほど検索意図を強く表すので、そちらを重く数える。
		double matchedIdf = 0.0;
		for(std::size_t q = 0; q < queryTokens.size(); ++q) {
			if(std::binary_search(docTokens.begin(), docTokens.end(), queryTokens[q])) {
				matchedIdf += queryIdf[q];
			}
		}
		if(matchedIdf <= 0.0) continue;

		// --- 文書長で正規化する（BM25のb項に相当） ---
		// これが無いと、語彙集合の大きい巨大チャンクが「たまたま多くの語を
		// 含む」だけで上位を占める。実データでは1,000行超のクラス宣言や
		// 長い関数が常に1位に居座った。
		const double docLen = static_cast<double>(docTokens.size());
		const double norm = (1.0 - kLengthNormB) + kLengthNormB * (docLen / m_averageDocLength);

		float score = static_cast<float>((matchedIdf / queryIdfTotal) / norm);

		// --- シンボル名の一致を強く優遇する ---
		// 完全一致検索がベクトルに勝てる唯一にして最大の領域なので、
		// ここは明示的に重みを付ける。
		const std::string loweredName = ToLower(m_chunks[i].chunk.qualifiedName);
		if(!loweredName.empty()) {
			if(loweredQuery.find(loweredName) != std::string::npos) {
				score += 2.0f; // クエリが修飾名を丸ごと含む
			} else {
				const std::size_t sep = loweredName.rfind("::");
				const std::string shortName =
					(sep == std::string::npos) ? loweredName : loweredName.substr(sep + 2);
				if(!shortName.empty() && loweredQuery.find(shortName) != std::string::npos) {
					score += 1.0f; // 末尾のシンボル名が含まれる
				}
			}
		}

		SearchHit hit;
		hit.index = i;
		hit.score = score;
		scored.push_back(hit);
	}

	std::stable_sort(scored.begin(), scored.end(),
	                 [](const SearchHit& a, const SearchHit& b) { return a.score > b.score; });
	if(scored.size() > topK) scored.resize(topK);

	for(std::size_t r = 0; r < scored.size(); ++r) {
		scored[r].lexicalRank = static_cast<int>(r) + 1;
	}
	return scored;
}

// =======================================================================
// SearchIndex::SearchVector
// =======================================================================
std::vector<SearchHit> SearchIndex::SearchVector(
	const std::vector<float>& queryVector, std::size_t topK) const {

	if(m_dimensions == 0 || queryVector.size() != m_dimensions) return {};

	std::vector<SearchHit> scored;
	scored.reserve(m_embeddedCount);

	// 全件走査。1,448件×1024次元でも150万回の積和で、1ms未満で終わる。
	for(std::size_t i = 0; i < m_chunks.size(); ++i) {
		if(!m_hasEmbedding[i]) continue;
		const float* row = m_matrix.data() + i * m_dimensions;
		SearchHit hit;
		hit.index = i;
		hit.score = DotProduct(queryVector.data(), row, m_dimensions);
		scored.push_back(hit);
	}

	std::stable_sort(scored.begin(), scored.end(),
	                 [](const SearchHit& a, const SearchHit& b) { return a.score > b.score; });
	if(scored.size() > topK) scored.resize(topK);

	for(std::size_t r = 0; r < scored.size(); ++r) {
		scored[r].vectorRank = static_cast<int>(r) + 1;
	}
	return scored;
}

// =======================================================================
// SearchIndex::SearchHybrid
// =======================================================================
std::vector<SearchHit> SearchIndex::SearchHybrid(
	const std::string& query,
	const std::vector<float>& queryVector,
	std::size_t topK,
	std::size_t candidatePool,
	double rrfK) const {

	const std::size_t pool = std::max(candidatePool, topK);

	const std::vector<SearchHit> lexical = SearchLexical(query, pool);
	const std::vector<SearchHit> vector = SearchVector(queryVector, pool);

	// index → 融合スコアと各経路の順位
	std::unordered_map<std::size_t, SearchHit> fused;

	const auto accumulate = [&](const std::vector<SearchHit>& hits, bool isLexical) {
		for(std::size_t r = 0; r < hits.size(); ++r) {
			const std::size_t rank = r + 1;
			SearchHit& entry = fused[hits[r].index];
			entry.index = hits[r].index;
			// RRF: 順位のみを使うため、尺度の違う2系統をそのまま混ぜられる。
			entry.score += static_cast<float>(1.0 / (rrfK + static_cast<double>(rank)));
			if(isLexical) entry.lexicalRank = static_cast<int>(rank);
			else entry.vectorRank = static_cast<int>(rank);
		}
	};

	accumulate(lexical, true);
	accumulate(vector, false);

	std::vector<SearchHit> merged;
	merged.reserve(fused.size());
	for(const auto& kv : fused) merged.push_back(kv.second);

	// RRFは2経路の順位が鏡像になると（片方がL1V2、他方がL2V1など）
	// 完全に同点になる。この決着を添字順という無意味な基準に委ねたくない。
	//
	// 同点時は字句側を優先する。コード検索ではシンボル名の完全一致が
	// 埋め込み類似度より高精度な信号であり、埋め込みは Serialize と
	// Deserialize を近接させるような取り違えを起こしうるため。
	std::sort(merged.begin(), merged.end(),
	          [](const SearchHit& a, const SearchHit& b) {
		          if(a.score != b.score) return a.score > b.score;

		          // rank 0 は「その経路では圏外」を意味する。
		          const int aLex = (a.lexicalRank > 0) ? a.lexicalRank : INT_MAX;
		          const int bLex = (b.lexicalRank > 0) ? b.lexicalRank : INT_MAX;
		          if(aLex != bLex) return aLex < bLex;

		          return a.index < b.index; // 最後は決定論性のためだけの基準
	          });

	if(merged.size() > topK) merged.resize(topK);
	return merged;
}

} // namespace agentos
