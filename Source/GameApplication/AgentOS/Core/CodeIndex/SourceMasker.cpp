// =======================================================================
//
// SourceMasker.cpp
//
// =======================================================================
#include "SourceMasker.h"

#include <cstddef>

namespace agentos {

namespace {

// 生文字列の区切り子は最大16文字（C++規格上の上限）。
constexpr std::size_t kMaxRawDelimiter = 16;

} // namespace

std::string MaskLiteralsAndComments(const std::string& source) {
	std::string out = source;
	const std::size_t n = source.size();

	// [from, to) を空白で潰す。改行だけは残して行構造を保つ。
	const auto blank = [&out, n](std::size_t from, std::size_t to) {
		for(std::size_t k = from; k < to && k < n; ++k) {
			if(out[k] != '\n') out[k] = ' ';
		}
	};

	std::size_t i = 0;
	while(i < n) {
		const char c = source[i];

		// ---- 行コメント ----
		if(c == '/' && i + 1 < n && source[i + 1] == '/') {
			std::size_t j = i;
			while(j < n && source[j] != '\n') ++j;
			blank(i, j);
			i = j;
			continue;
		}

		// ---- ブロックコメント ----
		if(c == '/' && i + 1 < n && source[i + 1] == '*') {
			std::size_t j = i + 2;
			while(j + 1 < n && !(source[j] == '*' && source[j + 1] == '/')) ++j;
			j = (j + 1 < n) ? (j + 2) : n;
			blank(i, j);
			i = j;
			continue;
		}

		// ---- 生文字列 R"delim( ... )delim" ----
		// 通常の文字列より先に判定する必要がある。中身にエスケープが
		// 効かないため、通常の文字列として処理すると終端を誤る。
		if(c == 'R' && i + 1 < n && source[i + 1] == '"') {
			std::size_t d = i + 2;
			std::string delim;
			while(d < n && source[d] != '(' && delim.size() < kMaxRawDelimiter) {
				delim.push_back(source[d]);
				++d;
			}
			if(d < n && source[d] == '(') {
				const std::string close = ")" + delim + "\"";
				const std::size_t end = source.find(close, d + 1);
				const std::size_t stop = (end == std::string::npos) ? n : (end + close.size());
				blank(i, stop);
				i = stop;
				continue;
			}
			// '(' が見つからなければ生文字列ではない。通常処理へ落とす。
		}

		// ---- 文字列 / 文字リテラル ----
		if(c == '"' || c == '\'') {
			const char quote = c;
			std::size_t j = i + 1;
			while(j < n) {
				if(source[j] == '\\') {
					j += 2; // エスケープ。次の1文字を読み飛ばす。
					continue;
				}
				if(source[j] == quote) {
					++j;
					break;
				}
				// 未終端リテラルで暴走しないための保険。
				// 行継続(\)は上のエスケープ処理で既に吸収されている。
				if(source[j] == '\n') break;
				++j;
			}
			blank(i, j);
			i = j;
			continue;
		}

		++i;
	}

	return out;
}

} // namespace agentos
