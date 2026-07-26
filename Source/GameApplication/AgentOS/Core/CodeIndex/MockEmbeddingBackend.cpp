// =======================================================================
//
// MockEmbeddingBackend.cpp
//
// =======================================================================
#include "MockEmbeddingBackend.h"

#include <cctype>
#include <cstdint>

#include "VectorMath.h"

namespace agentos {

namespace {

// FNV-1a 64bit。決定論的であればよいので暗号強度は不要。
std::uint64_t Fnv1a(const char* data, std::size_t len, std::uint64_t seed) {
	std::uint64_t h = seed;
	for(std::size_t i = 0; i < len; ++i) {
		h ^= static_cast<std::uint8_t>(data[i]);
		h *= 1099511628211ULL;
	}
	return h;
}

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;

bool IsWordChar(unsigned char c) {
	return std::isalnum(c) != 0 || c == '_';
}

char LowerAscii(unsigned char c) {
	return static_cast<char>(std::tolower(c));
}

} // namespace

std::vector<float> MockEmbeddingBackend::EmbedOne(const std::string& text) const {
	std::vector<float> v(m_dimensions, 0.0f);

	// --- 1. 語トークン ---
	// 識別子・単語単位。CamelCaseはそのまま1語として扱う。
	std::string token;
	const auto flush = [&]() {
		if(token.empty()) return;
		const std::uint64_t h = Fnv1a(token.data(), token.size(), kFnvOffset);
		v[h % m_dimensions] += 1.0f;
		token.clear();
	};

	std::string lowered;
	lowered.reserve(text.size());
	for(const char raw : text) {
		const unsigned char c = static_cast<unsigned char>(raw);
		lowered.push_back(LowerAscii(c));
		if(IsWordChar(c)) {
			token.push_back(LowerAscii(c));
		} else {
			flush();
		}
	}
	flush();

	// --- 2. 文字trigram ---
	// 部分一致（Serialize と Serializer など）に反応させるため。
	// 語トークンより重みを落とし、主軸はあくまで語彙一致に置く。
	if(lowered.size() >= 3) {
		for(std::size_t i = 0; i + 3 <= lowered.size(); ++i) {
			const std::uint64_t h = Fnv1a(lowered.data() + i, 3, kFnvOffset ^ 0x9E3779B9ULL);
			v[h % m_dimensions] += 0.25f;
		}
	}

	// コサイン類似度を内積で計算できるようにする。
	// 同時に、長いチャンクほどベクトルが大きくなる偏りも取り除かれる。
	L2Normalize(&v);
	return v;
}

Result MockEmbeddingBackend::Embed(
	const std::vector<std::string>& texts,
	std::vector<std::vector<float>>* out) {

	if(out == nullptr) {
		return Result::Fail("MockEmbeddingBackend::Embed: outがnullptr");
	}

	out->clear();
	out->reserve(texts.size());
	for(const std::string& text : texts) {
		out->push_back(EmbedOne(text));
	}
	return Result::Ok();
}

} // namespace agentos
