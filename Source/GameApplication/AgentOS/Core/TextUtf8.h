// =======================================================================
//
// TextUtf8.h
//
// UTF-8を壊さない文字列操作。
//
// 背景（実機で踏んだクラッシュ）:
//   Evidenceやプロンプトの文字列は `substr(0, n)` でバイト単位に
//   切り詰められていた。対象がほぼASCIIだった頃は問題にならなかったが、
//   CodeSearchがソースコードの抜粋（日本語コメントを含む）を
//   Evidenceへ載せるようになった結果、切断点が多バイト文字の途中に落ち、
//   不正なUTF-8が生成されるようになった。
//
//   nlohmann::json の dump() は不正なUTF-8を見つけると例外を投げる。
//   AgentOSのワーカースレッドはこれを捕まえないため、
//   terminate → abort でプロセスごと落ちていた。
//
//   スタックはこう出る:
//     evidence_prompt::Compress -> json::dump -> serializer::dump_escaped -> throw
//
// 対策は2層。
//   1. 切り詰めを文字境界で行う（このヘッダ）
//   2. dump時は error_handler_t::replace を指定し、
//      万一不正な文字列が混ざってもプロセスを落とさない
//
// =======================================================================
#pragma once

#include <cstddef>
#include <string>

namespace agentos {

// pos がUTF-8の継続バイト(10xxxxxx)を指しているか。
inline bool IsUtf8ContinuationByte(unsigned char byte) noexcept {
	return (byte & 0xC0) == 0x80;
}

// byteIndex 以下で、文字の切れ目になる最大の位置を返す。
// 返り値は「そこまでを含めて切ってよいバイト数」。
inline std::size_t Utf8SafeCutPoint(const std::string& text, std::size_t byteIndex) noexcept {
	if(byteIndex >= text.size()) return text.size();

	// text[cut] は切り出しに含まれない最初のバイト。
	// それが継続バイトなら文字の途中なので、先頭側へ戻る。
	std::size_t cut = byteIndex;
	while(cut > 0 && IsUtf8ContinuationByte(static_cast<unsigned char>(text[cut]))) {
		--cut;
	}
	return cut;
}

// UTF-8を壊さずに maxBytes 以下へ切り詰める。
// 切り詰めた場合のみ suffix を付ける。
// suffix自体の長さは maxBytes に含めない（呼び出し側の意図を単純に保つため）。
inline std::string TruncateUtf8(
	const std::string& text,
	std::size_t maxBytes,
	const std::string& suffix = std::string()) {

	if(text.size() <= maxBytes) return text;
	const std::size_t cut = Utf8SafeCutPoint(text, maxBytes);
	return text.substr(0, cut) + suffix;
}

} // namespace agentos
