// =======================================================================
//
// CodeIndexStore.h
//
// コード索引のSQLite永続化層。
//
// ベクトルを独立バイナリではなくSQLiteのBLOBへ入れる理由：
//   決め手は差分更新。コードは毎日変わるため、索引は
//   「このファイルの分だけ消して入れ直す」を頻繁に行う。
//   SQLiteなら DELETE + INSERT をトランザクションで囲むだけで済み、
//   メタデータとベクトルの整合も自動的に保たれる。
//   独立バイナリだと削除跡の穴を自前で管理することになり、
//   実質的に簡易ストレージアロケータを書く羽目になる。
//   読み込みが数千回のstepになる点は起動時1回・数十msなので無視できる。
//
// 検索そのものはメモリ上の全走査で行う（VectorMath.h のコメント参照）。
// このクラスの役割は「起動時に全部読む」「変更分だけ書き戻す」の2つ。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../AgentOsTypes.h"
#include "../Store/SqliteDb.h"
#include "CodeChunk.h"

namespace agentos {

// ---------------------------------
// 索引に載った1件
// ---------------------------------
struct StoredChunk {
	std::int64_t id = 0;
	CodeChunk chunk;
	std::vector<float> embedding; // 空 = 未ベクトル化
};

// ---------------------------------
// CodeIndexStore
// ---------------------------------
class CodeIndexStore {
public:
	Result Open(const std::string& dbPath);
	void Close();
	bool IsOpen() const noexcept { return m_db.IsOpen(); }

	// テーブルとインデックスを用意する。既存なら何もしない。
	Result EnsureSchema();

	// 索引が使っている埋め込みモデルの識別子。
	// これが変わったらベクトルは全て無効なので、呼び出し側は再構築を選べる。
	Result GetMeta(const std::string& key, std::string* outValue);
	Result SetMeta(const std::string& key, const std::string& value);

	// 記録済みのファイル内容ハッシュ。未登録なら空文字列。
	Result GetFileHash(const std::string& path, std::string* outHash);

	// 1ファイル分を丸ごと置き換える（差分更新の単位）。
	// embeddings は空でもよい（chunksと同数か0のどちらか）。
	// トランザクションで囲まれるため、途中で失敗しても中途半端な状態は残らない。
	Result ReplaceFile(
		const std::string& path,
		const std::string& contentHash,
		const std::vector<CodeChunk>& chunks,
		const std::vector<std::vector<float>>& embeddings);

	// 索引にあるが今回の走査に現れなかったファイルを削除する。
	// 削除されたソースが索引に残り続けるのを防ぐ。
	Result RemoveFilesNotIn(const std::vector<std::string>& presentPaths, int* outRemoved);

	// 全件をメモリへ読み出す。検索はこの配列に対する全走査で行う。
	Result LoadAll(std::vector<StoredChunk>* out);

	Result CountChunks(std::int64_t* out);

	// 内容ハッシュ。差分判定にのみ使うので暗号強度は不要。
	static std::string HashContent(const std::string& content);

private:
	SqliteDb m_db;
};

} // namespace agentos
