/// Description:
/// ファイルパスから拡張子を取得、および指定拡張子との一致判定を行う実装

#include "checkFileExtention.h"

//---------------------------------------------------------------------
// @brief  ファイルパスから拡張子を取得する（std::string 版）
// @param  filePath  対象のファイルパス
// @return 先頭の '.' を含む拡張子
//         '.' が存在しない場合は空文字列を返す
//---------------------------------------------------------------------
std::string GetFileExtension(const std::string& filePath){
	// 最後に出現する '.' の位置を取得
	const size_t dotPos = filePath.find_last_of('.');

	// '.' が見つからない場合は拡張子なし
	if(dotPos == std::string::npos){
		return "";
	}

	// '.' 以降を拡張子として返す
	return filePath.substr(dotPos);
}

//---------------------------------------------------------------------
// @brief  指定された拡張子を持つかどうかを判定する（std::string 版）
// @param  filePath   判定対象のファイルパス
// @param  extension  判定したい拡張子（".png" または "png" を想定）
// @return 一致すれば true
//---------------------------------------------------------------------
bool HasExtension(const std::string& filePath, const std::string& extension){
	// 対象ファイルの拡張子を取得
	const std::string ext = GetFileExtension(filePath);

	// ".png" と "png" の両形式に対応
	return (ext == extension) ||
		(ext == ("." + extension));
}

//---------------------------------------------------------------------
// @brief  ファイルパスから拡張子を取得する（std::wstring 版）
// @param  filePath  対象のファイルパス
// @return 先頭の L'.' を含む拡張子
//         '.' が存在しない場合は空文字列を返す
//---------------------------------------------------------------------
std::wstring GetFileExtension(const std::wstring& filePath){
	// 最後に出現する '.' の位置を取得
	const size_t dotPos = filePath.find_last_of(L'.');

	// '.' が見つからない場合は拡張子なし
	if(dotPos == std::wstring::npos){
		return L"";
	}

	// '.' 以降を拡張子として返す
	return filePath.substr(dotPos);
}

//---------------------------------------------------------------------
// @brief  指定された拡張子を持つかどうかを判定する（std::wstring 版）
// @param  filePath   判定対象のファイルパス
// @param  extension  判定したい拡張子（L".png" または L"png" を想定）
// @return 一致すれば true
//---------------------------------------------------------------------
bool HasExtension(const std::wstring& filePath, const std::wstring& extension){
	// 対象ファイルの拡張子を取得
	const std::wstring ext = GetFileExtension(filePath);

	// L".png" と L"png" の両形式に対応
	return (ext == extension) ||
		(ext == (L"." + extension));
}