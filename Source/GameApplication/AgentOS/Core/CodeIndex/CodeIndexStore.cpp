// =======================================================================
//
// CodeIndexStore.cpp
//
// =======================================================================
#include "CodeIndexStore.h"

#include <cstdio>
#include <unordered_set>

#include "VectorMath.h"

namespace agentos {

namespace {

const char* const kSchema =
	"CREATE TABLE IF NOT EXISTS code_index_meta ("
	"  key   TEXT PRIMARY KEY,"
	"  value TEXT NOT NULL"
	");"
	"CREATE TABLE IF NOT EXISTS code_files ("
	"  path         TEXT PRIMARY KEY,"
	"  content_hash TEXT NOT NULL"
	");"
	"CREATE TABLE IF NOT EXISTS code_chunks ("
	"  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
	"  path       TEXT NOT NULL,"
	"  kind       TEXT NOT NULL,"
	"  module     TEXT NOT NULL,"
	"  name       TEXT NOT NULL,"
	"  start_line INTEGER NOT NULL,"
	"  end_line   INTEGER NOT NULL,"
	"  text       TEXT NOT NULL,"
	"  dim        INTEGER NOT NULL DEFAULT 0,"
	"  embedding  BLOB"
	");"
	// path での一括削除が差分更新のホットパスなので索引を張る。
	"CREATE INDEX IF NOT EXISTS idx_code_chunks_path ON code_chunks(path);"
	// シンボル完全一致検索（ハイブリッドの字句側）で使う。
	"CREATE INDEX IF NOT EXISTS idx_code_chunks_name ON code_chunks(name);";

CodeChunkKind KindFromString(const std::string& s) {
	return (s == "type") ? CodeChunkKind::Type : CodeChunkKind::Function;
}

} // namespace

// =======================================================================
// HashContent
// =======================================================================
std::string CodeIndexStore::HashContent(const std::string& content) {
	// FNV-1a 64bit。衝突耐性は差分検出に足りればよい。
	std::uint64_t h = 1469598103934665603ULL;
	for(const char c : content) {
		h ^= static_cast<std::uint8_t>(c);
		h *= 1099511628211ULL;
	}
	// 長さも混ぜて、単純な置換での衝突を減らす。
	h ^= static_cast<std::uint64_t>(content.size());

	char buf[17];
	std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
	return std::string(buf);
}

// =======================================================================
// Open / Close / EnsureSchema
// =======================================================================
Result CodeIndexStore::Open(const std::string& dbPath) {
	return m_db.Open(dbPath);
}

void CodeIndexStore::Close() {
	m_db.Close();
}

Result CodeIndexStore::EnsureSchema() {
	if(!m_db.IsOpen()) return Result::Fail("CodeIndexStore: DB未オープン");
	return m_db.Exec(kSchema);
}

// =======================================================================
// メタ情報
// =======================================================================
Result CodeIndexStore::GetMeta(const std::string& key, std::string* outValue) {
	if(outValue == nullptr) return Result::Fail("CodeIndexStore::GetMeta: outValueがnullptr");
	outValue->clear();

	Statement stmt;
	Result r = m_db.Prepare("SELECT value FROM code_index_meta WHERE key = ?1;", &stmt);
	if(!r) return r;
	r = stmt.BindText(1, key);
	if(!r) return r;

	if(stmt.Step() == Statement::StepResult::Row) {
		*outValue = stmt.ColumnText(0);
	}
	return Result::Ok();
}

Result CodeIndexStore::SetMeta(const std::string& key, const std::string& value) {
	Statement stmt;
	Result r = m_db.Prepare(
		"INSERT INTO code_index_meta(key, value) VALUES(?1, ?2)"
		" ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
		&stmt);
	if(!r) return r;
	r = stmt.BindText(1, key);
	if(!r) return r;
	r = stmt.BindText(2, value);
	if(!r) return r;

	if(stmt.Step() == Statement::StepResult::Error) {
		return Result::Fail("CodeIndexStore::SetMeta: 書き込み失敗");
	}
	return Result::Ok();
}

// =======================================================================
// GetFileHash
// =======================================================================
Result CodeIndexStore::GetFileHash(const std::string& path, std::string* outHash) {
	if(outHash == nullptr) return Result::Fail("CodeIndexStore::GetFileHash: outHashがnullptr");
	outHash->clear();

	Statement stmt;
	Result r = m_db.Prepare("SELECT content_hash FROM code_files WHERE path = ?1;", &stmt);
	if(!r) return r;
	r = stmt.BindText(1, path);
	if(!r) return r;

	if(stmt.Step() == Statement::StepResult::Row) {
		*outHash = stmt.ColumnText(0);
	}
	return Result::Ok();
}

// =======================================================================
// ReplaceFile
// =======================================================================
Result CodeIndexStore::ReplaceFile(
	const std::string& path,
	const std::string& contentHash,
	const std::vector<CodeChunk>& chunks,
	const std::vector<std::vector<float>>& embeddings) {

	if(!embeddings.empty() && embeddings.size() != chunks.size()) {
		return Result::Fail("CodeIndexStore::ReplaceFile: embeddingsの件数がchunksと一致しない");
	}

	// Transactionはコンストラクタで BEGIN IMMEDIATE を発行する。
	// ここで再度Begin()を呼ぶと二重開始でSQLiteに弾かれる（TaskStoreの用法に合わせる）。
	Transaction tx(m_db);

	// --- 1. 旧チャンクを消す ---
	Result r;
	{
		Statement del;
		r = m_db.Prepare("DELETE FROM code_chunks WHERE path = ?1;", &del);
		if(!r) return r;
		r = del.BindText(1, path);
		if(!r) return r;
		if(del.Step() == Statement::StepResult::Error) {
			return Result::Fail("CodeIndexStore::ReplaceFile: 旧チャンクの削除に失敗");
		}
	}

	// --- 2. 新チャンクを入れる ---
	{
		Statement ins;
		r = m_db.Prepare(
			"INSERT INTO code_chunks"
			"(path, kind, module, name, start_line, end_line, text, dim, embedding)"
			" VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9);",
			&ins);
		if(!r) return r;

		for(std::size_t i = 0; i < chunks.size(); ++i) {
			const CodeChunk& c = chunks[i];

			r = ins.Reset();
			if(!r) return r;

			r = ins.BindText(1, c.filePath);
			if(!r) return r;
			r = ins.BindText(2, ToString(c.kind));
			if(!r) return r;
			r = ins.BindText(3, c.moduleTag);
			if(!r) return r;
			r = ins.BindText(4, c.qualifiedName);
			if(!r) return r;
			r = ins.BindInt64(5, c.startLine);
			if(!r) return r;
			r = ins.BindInt64(6, c.endLine);
			if(!r) return r;
			r = ins.BindText(7, c.text);
			if(!r) return r;

			if(embeddings.empty() || embeddings[i].empty()) {
				r = ins.BindInt64(8, 0);
				if(!r) return r;
				r = ins.BindNull(9);
				if(!r) return r;
			} else {
				const std::vector<std::uint8_t> blob = FloatsToBlob(embeddings[i]);
				r = ins.BindInt64(8, static_cast<std::int64_t>(embeddings[i].size()));
				if(!r) return r;
				r = ins.BindBlob(9, blob.data(), blob.size());
				if(!r) return r;
			}

			if(ins.Step() == Statement::StepResult::Error) {
				return Result::Fail("CodeIndexStore::ReplaceFile: チャンク挿入に失敗");
			}
		}
	}

	// --- 3. ファイルのハッシュを更新 ---
	{
		Statement upd;
		r = m_db.Prepare(
			"INSERT INTO code_files(path, content_hash) VALUES(?1, ?2)"
			" ON CONFLICT(path) DO UPDATE SET content_hash = excluded.content_hash;",
			&upd);
		if(!r) return r;
		r = upd.BindText(1, path);
		if(!r) return r;
		r = upd.BindText(2, contentHash);
		if(!r) return r;
		if(upd.Step() == Statement::StepResult::Error) {
			return Result::Fail("CodeIndexStore::ReplaceFile: ファイル記録の更新に失敗");
		}
	}

	return tx.Commit();
}

// =======================================================================
// RemoveFilesNotIn
// =======================================================================
Result CodeIndexStore::RemoveFilesNotIn(
	const std::vector<std::string>& presentPaths, int* outRemoved) {

	std::unordered_set<std::string> present(presentPaths.begin(), presentPaths.end());

	// 索引済みのパス一覧を取る
	std::vector<std::string> stale;
	{
		Statement stmt;
		Result r = m_db.Prepare("SELECT path FROM code_files;", &stmt);
		if(!r) return r;
		while(stmt.Step() == Statement::StepResult::Row) {
			const std::string path = stmt.ColumnText(0);
			if(present.find(path) == present.end()) {
				stale.push_back(path);
			}
		}
	}

	if(stale.empty()) {
		if(outRemoved) *outRemoved = 0;
		return Result::Ok();
	}

	Transaction tx(m_db);

	Statement delChunks;
	Result r = m_db.Prepare("DELETE FROM code_chunks WHERE path = ?1;", &delChunks);
	if(!r) return r;
	Statement delFile;
	r = m_db.Prepare("DELETE FROM code_files WHERE path = ?1;", &delFile);
	if(!r) return r;

	for(const std::string& path : stale) {
		r = delChunks.Reset();
		if(!r) return r;
		r = delChunks.BindText(1, path);
		if(!r) return r;
		if(delChunks.Step() == Statement::StepResult::Error) {
			return Result::Fail("CodeIndexStore::RemoveFilesNotIn: チャンク削除に失敗");
		}

		r = delFile.Reset();
		if(!r) return r;
		r = delFile.BindText(1, path);
		if(!r) return r;
		if(delFile.Step() == Statement::StepResult::Error) {
			return Result::Fail("CodeIndexStore::RemoveFilesNotIn: ファイル削除に失敗");
		}
	}

	r = tx.Commit();
	if(!r) return r;

	if(outRemoved) *outRemoved = static_cast<int>(stale.size());
	return Result::Ok();
}

// =======================================================================
// LoadAll
// =======================================================================
Result CodeIndexStore::LoadAll(std::vector<StoredChunk>* out) {
	if(out == nullptr) return Result::Fail("CodeIndexStore::LoadAll: outがnullptr");
	out->clear();

	Statement stmt;
	Result r = m_db.Prepare(
		"SELECT id, path, kind, module, name, start_line, end_line, text, dim, embedding"
		" FROM code_chunks ORDER BY id;",
		&stmt);
	if(!r) return r;

	while(stmt.Step() == Statement::StepResult::Row) {
		StoredChunk sc;
		sc.id = stmt.ColumnInt64(0);
		sc.chunk.filePath = stmt.ColumnText(1);
		sc.chunk.kind = KindFromString(stmt.ColumnText(2));
		sc.chunk.moduleTag = stmt.ColumnText(3);
		sc.chunk.qualifiedName = stmt.ColumnText(4);
		sc.chunk.startLine = static_cast<int>(stmt.ColumnInt64(5));
		sc.chunk.endLine = static_cast<int>(stmt.ColumnInt64(6));
		sc.chunk.text = stmt.ColumnText(7);

		const std::int64_t dim = stmt.ColumnInt64(8);
		if(dim > 0 && !stmt.ColumnIsNull(9)) {
			sc.embedding = BlobToFloats(stmt.ColumnBlob(9));
			// 記録された次元と実バイト数が食い違うBLOBは壊れているので捨てる。
			if(sc.embedding.size() != static_cast<std::size_t>(dim)) {
				sc.embedding.clear();
			}
		}

		out->push_back(std::move(sc));
	}

	return Result::Ok();
}

// =======================================================================
// CountChunks
// =======================================================================
Result CodeIndexStore::CountChunks(std::int64_t* out) {
	if(out == nullptr) return Result::Fail("CodeIndexStore::CountChunks: outがnullptr");
	*out = 0;

	Statement stmt;
	Result r = m_db.Prepare("SELECT COUNT(*) FROM code_chunks;", &stmt);
	if(!r) return r;
	if(stmt.Step() == Statement::StepResult::Row) {
		*out = stmt.ColumnInt64(0);
	}
	return Result::Ok();
}

} // namespace agentos
