// =======================================================================
//
// CodeParser.cpp
//
// =======================================================================
#include "CodeParser.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "SourceMasker.h"

namespace agentos {

namespace {

// 本体探索の打ち切り行数。これを超える単一の関数／型は事実上存在せず、
// 超えた場合はパース崩れ（マクロ等）とみなして諦める。
constexpr int kMaxBodyLines = 2000;

// ---------------------------------
// 文字種
// ---------------------------------
bool IsIdentStart(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool IsIdentChar(char c) noexcept {
	return IsIdentStart(c) || (c >= '0' && c <= '9');
}

bool IsSpace(char c) noexcept {
	return c == ' ' || c == '\t' || c == '\r';
}

// ---------------------------------
// 文字列ユーティリティ
// ---------------------------------
std::string TrimCopy(const std::string& s) {
	std::size_t b = 0;
	std::size_t e = s.size();
	while(b < e && IsSpace(s[b])) ++b;
	while(e > b && IsSpace(s[e - 1])) --e;
	return s.substr(b, e - b);
}

// 位置posから識別子を読み取る。読めなければ空文字列。
std::string ReadIdentifier(const std::string& line, std::size_t pos) {
	if(pos >= line.size() || !IsIdentStart(line[pos])) return {};
	std::size_t e = pos;
	while(e < line.size() && IsIdentChar(line[e])) ++e;
	return line.substr(pos, e - pos);
}

// 行の先頭トークン（最初の識別子）を返す。
std::string FirstToken(const std::string& line) {
	std::size_t b = 0;
	while(b < line.size() && IsSpace(line[b])) ++b;
	return ReadIdentifier(line, b);
}

// 定義行ではありえない先頭トークン。
// 制御構文や宣言子で始まる行を早期に除外し、誤検出を減らす。
bool IsNonDefinitionStarter(const std::string& token) {
	static const char* const kStarters[] = {
		"return", "if", "for", "while", "switch", "case", "do", "else",
		"using", "typedef", "friend", "namespace", "enum", "extern",
		"static_assert", "public", "private", "protected", "template",
		"delete", "goto", "throw", "catch", "try", "operator",
	};
	for(const char* s : kStarters) {
		if(token == s) return true;
	}
	return false;
}

// ---------------------------------
// 本体の終端を求める
// ---------------------------------
// startLine から前方へ走査し、最初の '{' から深度が0へ戻る行を返す。
// '{' より先に '}' 以外の深度0の ';' が来た場合は宣言とみなし -1 を返す。
//
// 戻り値: 終端行のindex。確定できなければ -1。
int FindBodyEnd(const std::vector<std::string>& masked, int startLine) {
	int depth = 0;
	bool opened = false;
	const int limit = std::min(static_cast<int>(masked.size()), startLine + kMaxBodyLines);

	for(int i = startLine; i < limit; ++i) {
		for(const char c : masked[static_cast<std::size_t>(i)]) {
			if(c == '{') {
				++depth;
				opened = true;
			} else if(c == '}') {
				--depth;
				if(opened && depth <= 0) return i;
			} else if(c == ';' && !opened) {
				// 本体に入る前のセミコロン = 宣言だった
				return -1;
			}
		}
	}
	return -1;
}

// ---------------------------------
// 関数定義シグネチャの検出
// ---------------------------------
// 行頭アンカー付きで「Type Class::Method(」の形を探す。
// マスク済みの行に対して呼ぶこと。
//
// 見つかれば "Class::Method" を返す。見つからなければ空文字列。
std::string DetectFunctionSignature(const std::string& masked) {
	if(masked.empty()) return {};

	// 行頭アンカー。インデントされた行はクラス内メンバ等なので対象外。
	if(!IsIdentStart(masked[0])) return {};

	if(masked.find("::") == std::string::npos) return {};
	if(masked.find('(') == std::string::npos) return {};

	if(IsNonDefinitionStarter(FirstToken(masked))) return {};

	// 定義のシグネチャなら、修飾名は行内で最初の '(' より前に現れる。
	// これを課さないと、1行に収まった関数の本体内の呼び出しを拾ってしまう。
	//   char LowerAscii(unsigned char c) { return std::tolower(c); }
	// が "std::tolower" という定義として誤検出された（実走査で発見）。
	const std::size_t firstParen = masked.find('(');

	// 左から順に "::" を探し、最初に「識別子::[~]識別子(」の形になる箇所を採る。
	std::size_t search = 0;
	while(true) {
		const std::size_t sep = masked.find("::", search);
		if(sep == std::string::npos) return {};
		if(sep > firstParen) return {}; // 最初の '(' より後ろは本体側
		search = sep + 2;

		// "::" の直前が識別子であること（= クラス名／名前空間名）
		if(sep == 0) continue;
		std::size_t nameEnd = sep;
		std::size_t nameBegin = nameEnd;
		while(nameBegin > 0 && IsIdentChar(masked[nameBegin - 1])) --nameBegin;
		if(nameBegin == nameEnd) continue;

		// 直前が識別子の一部として繋がる "::" の連鎖（A::B::C）は、
		// 最後の "::" まで進めてから判定したいので、次のループへ委ねる。
		std::size_t after = sep + 2;

		// デストラクタ
		bool destructor = false;
		if(after < masked.size() && masked[after] == '~') {
			destructor = true;
			++after;
		}

		const std::string method = ReadIdentifier(masked, after);
		if(method.empty()) continue;

		std::size_t p = after + method.size();
		while(p < masked.size() && IsSpace(masked[p])) ++p;
		if(p >= masked.size() || masked[p] != '(') continue;

		// 修飾の連鎖をすべて拾う（A::B::method → "A::B::method"）
		std::size_t qualBegin = nameBegin;
		while(qualBegin >= 2) {
			// qualBegin の直前が "::" ならさらに前の識別子まで戻る
			if(masked[qualBegin - 1] != ':' || masked[qualBegin - 2] != ':') break;
			std::size_t prevEnd = qualBegin - 2;
			std::size_t prevBegin = prevEnd;
			while(prevBegin > 0 && IsIdentChar(masked[prevBegin - 1])) --prevBegin;
			if(prevBegin == prevEnd) break;
			qualBegin = prevBegin;
		}

		std::string qualified = masked.substr(qualBegin, sep - qualBegin);
		qualified += "::";
		if(destructor) qualified += '~';
		qualified += method;
		return qualified;
	}
}

// 全大文字＋数字＋アンダースコアのみで構成されるか。
// エクスポートマクロ（DLLEXPORT / MYAPI 等）の判定に使う。
bool IsMacroLikeToken(const std::string& token) {
	if(token.empty()) return false;
	bool hasUpper = false;
	for(const char c : token) {
		if(c >= 'a' && c <= 'z') return false;
		if(c >= 'A' && c <= 'Z') hasUpper = true;
	}
	return hasUpper;
}

// ---------------------------------
// 型宣言の検出
// ---------------------------------
// "class X" / "struct X" を探す。見つかれば型名を返す。
std::string DetectTypeDeclaration(const std::string& masked) {
	const std::string trimmed = TrimCopy(masked);
	if(trimmed.empty()) return {};

	std::size_t pos = 0;
	if(trimmed.compare(0, 6, "class ") == 0) {
		pos = 6;
	} else if(trimmed.compare(0, 7, "struct ") == 0) {
		pos = 7;
	} else {
		return {};
	}

	while(pos < trimmed.size() && IsSpace(trimmed[pos])) ++pos;

	std::string name = ReadIdentifier(trimmed, pos);
	if(name.empty()) return {};

	// 識別子が2つ続く場合、どちらが型名かは形で決まる。
	//   "class MYAPI Foo"   → エクスポートマクロ。型名は後ろ
	//   "class Foo final"   → 継承禁止指定。型名は前
	// マクロは慣習的に全大文字なので、それを判定材料にする。
	// （この分岐を誤ると class Foo final が全て "final" という
	//   同一名に潰れ、索引上で衝突する。実走査で実際に踏んだ。）
	std::size_t next = pos + name.size();
	while(next < trimmed.size() && IsSpace(trimmed[next])) ++next;
	const std::string second = ReadIdentifier(trimmed, next);
	if(!second.empty() && IsMacroLikeToken(name)) {
		name = second;
	}

	return name;
}

// ---------------------------------
// 名前空間の開始検出
// ---------------------------------
// "namespace X {" / "namespace {" を検出する。
// 戻り値は開始したかどうか。名前は outName へ（無名なら空）。
bool DetectNamespaceOpen(const std::string& masked, std::string* outName) {
	const std::string trimmed = TrimCopy(masked);
	if(trimmed.compare(0, 10, "namespace ") != 0 &&
	   trimmed.compare(0, 10, "namespace{") != 0) {
		return false;
	}
	if(trimmed.find('{') == std::string::npos) return false;
	// "namespace A = B;" はエイリアス。開き括弧が無いので上で弾かれる。

	std::size_t pos = 9;
	while(pos < trimmed.size() && IsSpace(trimmed[pos])) ++pos;
	*outName = ReadIdentifier(trimmed, pos);
	return true;
}

// ---------------------------------
// 行分割（改行位置は保存済みなので単純分割でよい）
// ---------------------------------
std::vector<std::string> SplitLines(const std::string& text) {
	std::vector<std::string> lines;
	std::string cur;
	for(const char c : text) {
		if(c == '\n') {
			lines.push_back(cur);
			cur.clear();
		} else {
			cur.push_back(c);
		}
	}
	lines.push_back(std::move(cur));
	return lines;
}

std::string JoinLines(const std::vector<std::string>& lines, int from, int to) {
	std::string out;
	for(int i = from; i <= to && i < static_cast<int>(lines.size()); ++i) {
		if(i > from) out.push_back('\n');
		out += lines[static_cast<std::size_t>(i)];
	}
	return out;
}

std::string JoinNamespaces(const std::vector<std::pair<int, std::string>>& stack) {
	std::string out;
	for(const auto& entry : stack) {
		if(entry.second.empty()) continue; // 無名名前空間は修飾に出さない
		if(!out.empty()) out += "::";
		out += entry.second;
	}
	return out;
}

} // namespace

// =======================================================================
// DeriveModuleTag
// =======================================================================
std::string DeriveModuleTag(const std::string& relativePath) {
	std::string path = relativePath;
	for(char& c : path) {
		if(c == '\\') c = '/';
	}

	// 全ファイル共通の接頭辞は情報量が無いので削る。
	static const char* const kPrefixes[] = {
		"Source/GameApplication/",
		"Source/",
		"./",
	};
	for(const char* prefix : kPrefixes) {
		const std::size_t len = std::char_traits<char>::length(prefix);
		if(path.compare(0, len, prefix) == 0) {
			path = path.substr(len);
			break;
		}
	}

	const std::size_t slash = path.find_last_of('/');
	if(slash == std::string::npos) return "(root)";
	return path.substr(0, slash);
}

// =======================================================================
// ParseSourceFile
// =======================================================================
std::vector<CodeChunk> ParseSourceFile(
	const std::string& relativePath,
	const std::string& source,
	ParseStats* outStats) {

	ParseStats stats;
	std::vector<CodeChunk> chunks;

	// 1. リテラル／コメントを潰す。以降の括弧勘定はこの写しに対して行う。
	const std::string masked = MaskLiteralsAndComments(source);

	const std::vector<std::string> rawLines = SplitLines(source);
	const std::vector<std::string> maskedLines = SplitLines(masked);

	// マスクは長さと改行位置を保つ契約なので、行数は必ず一致する。
	// 万一崩れたら以降の行番号が全部ずれるため、ここで諦める。
	if(rawLines.size() != maskedLines.size()) {
		if(outStats) outStats->Merge(stats);
		return chunks;
	}

	const std::string moduleTag = DeriveModuleTag(relativePath);
	const int lineCount = static_cast<int>(maskedLines.size());

	int depth = 0;
	std::vector<std::pair<int, std::string>> nsStack;

	for(int i = 0; i < lineCount; ++i) {
		const std::string& m = maskedLines[static_cast<std::size_t>(i)];

		// --- 名前空間の開始を先に見る（この行の修飾には含めない） ---
		std::string nsName;
		const bool opensNamespace = DetectNamespaceOpen(m, &nsName);

		if(!opensNamespace) {
			const std::string nsPrefix = JoinNamespaces(nsStack);

			// --- 関数定義 ---
			const std::string funcName = DetectFunctionSignature(m);
			if(!funcName.empty()) {
				const int end = FindBodyEnd(maskedLines, i);
				if(end < 0) {
					// 宣言だったか、括弧が閉じなかったか。
					// 前者は正常。後者はパース崩れなので分けて数えたいが、
					// 行単位では区別しきれないため FindBodyEnd の内訳で判断する。
					++stats.forwardDeclarationCount;
				} else {
					CodeChunk chunk;
					chunk.kind = CodeChunkKind::Function;
					chunk.filePath = relativePath;
					chunk.moduleTag = moduleTag;
					chunk.qualifiedName = nsPrefix.empty() ? funcName : (nsPrefix + "::" + funcName);
					chunk.startLine = i + 1;
					chunk.endLine = end + 1;
					chunk.text = JoinLines(rawLines, i, end);
					chunks.push_back(std::move(chunk));
					++stats.functionCount;
				}
			} else {
				// --- 型宣言 ---
				const std::string typeName = DetectTypeDeclaration(m);
				if(!typeName.empty()) {
					const int end = FindBodyEnd(maskedLines, i);
					if(end < 0) {
						// "class Foo;" の前方宣言、または "struct tm t;" の変数宣言
						++stats.forwardDeclarationCount;
					} else if(end - i < 1) {
						// 1行に収まる型宣言は実質中身が無い。索引に載せる価値が薄い。
						++stats.forwardDeclarationCount;
					} else {
						CodeChunk chunk;
						chunk.kind = CodeChunkKind::Type;
						chunk.filePath = relativePath;
						chunk.moduleTag = moduleTag;
						chunk.qualifiedName = nsPrefix.empty() ? typeName : (nsPrefix + "::" + typeName);
						chunk.startLine = i + 1;
						chunk.endLine = end + 1;
						chunk.text = JoinLines(rawLines, i, end);
						chunks.push_back(std::move(chunk));
						++stats.typeCount;
					}
				}
			}
		}

		// --- この行の括弧で深度を更新 ---
		for(const char c : m) {
			if(c == '{') ++depth;
			else if(c == '}') --depth;
		}

		if(opensNamespace) {
			nsStack.emplace_back(depth, nsName);
		}

		// 閉じた名前空間を取り除く
		while(!nsStack.empty() && depth < nsStack.back().first) {
			nsStack.pop_back();
		}
	}

	if(outStats) outStats->Merge(stats);
	return chunks;
}

} // namespace agentos
