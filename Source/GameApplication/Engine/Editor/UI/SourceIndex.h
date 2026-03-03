#pragma once

#include <string>

// ============================
// SourceIndex
// ============================
// 起動時に Source/ 以下のソースファイルを全走査し、
// キーワード（クラス名・関数名・ファイルステムなど）→ ファイルパスの
// 逆引き辞書をビルドしてディスクにキャッシュする。
//
// 次回起動時は各ファイルの最終更新時刻を比較し、
// 変更・追加・削除されたファイルのみ再処理する（差分インデックス更新）。
//
// キャッシュファイル: Asset/BRAIN/cache/source_index.txt
//
class SourceIndex {
public:
	// 起動時に呼び出す。バックグラウンドスレッドでインデックスをビルドする。
	// 二重呼び出しは無視される。
	static void BuildAsync();

	// キーワードで関連ファイルを検索する（大文字小文字を無視、部分一致）。
	// インデックスが未完成の場合はステータス文字列を返す。
	static std::string SearchByKeyword(const std::string& keyword);

	// インデックスのステータス文字列を返す（UI 表示やデバッグ用）
	static std::string GetStatus();
};
