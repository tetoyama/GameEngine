// =======================================================================
//
// CodeParser.h
//
// C++ソースを CodeChunk へ切り出す軽量パーサ。
//
// 前提と限界（重要）：
//   C++は文脈自由文法ではなく、完全な構文解析には本物のコンパイラフロントエンドが要る。
//   ここが狙うのはそこではなく「検索インデックスとして十分な粒度で切れること」。
//   採る戦略は次の3段。
//     1. SourceMasker でコメント／リテラルを潰す（括弧カウントの汚染を除去）
//     2. 行頭アンカーでシグネチャ行を検出する
//     3. 波括弧の深度追跡で本体の終端を決める
//
//   2 が成立するのは、このリポジトリのコードスタイルが
//   「Type Class::Method(...)」を行頭から書く形で一貫しているため。
//   実測で675件の関数定義がこのアンカーに一致した。
//   汎用パーサではなく「このコードベース向けの実用パーサ」である点に注意。
//
//   検出できないもの（意図的な非対応）：
//     - クラス内に直接書かれたinlineメンバ関数
//       （所属する型宣言チャンクに含まれるため、情報としては失われない）
//     - 名前空間スコープの自由関数
//     - マクロで生成される定義
//
// =======================================================================
#pragma once

#include <string>
#include <vector>

#include "CodeChunk.h"

namespace agentos {

// ---------------------------------
// パース統計
// ---------------------------------
// パーサの取りこぼしを可視化するための計測値。
// 「何を切れたか」と同じくらい「何を切れなかったか」が設計判断に効く。
struct ParseStats {
	int functionCount = 0;
	int typeCount = 0;

	// シグネチャらしき行を見つけたが、波括弧が閉じずに本体を確定できなかった件数。
	// テンプレートやマクロ絡みで壊れた箇所がここに出る。
	int unterminatedCount = 0;

	// 前方宣言と判定して読み飛ばした件数（正常な挙動）。
	int forwardDeclarationCount = 0;

	void Merge(const ParseStats& other) {
		functionCount += other.functionCount;
		typeCount += other.typeCount;
		unterminatedCount += other.unterminatedCount;
		forwardDeclarationCount += other.forwardDeclarationCount;
	}
};

// ソース1ファイルをチャンクへ切り出す。
// relativePath は moduleTag の導出とチャンクの出自記録に使う。
std::vector<CodeChunk> ParseSourceFile(
	const std::string& relativePath,
	const std::string& source,
	ParseStats* outStats = nullptr);

// パスからモジュール見出しを導く。
//   "Source/GameApplication/AgentOS/Core/Store/SqliteDb.cpp" → "AgentOS/Core/Store"
// 先頭の "Source/GameApplication" は全ファイル共通で情報量が無いため落とす。
std::string DeriveModuleTag(const std::string& relativePath);

} // namespace agentos
