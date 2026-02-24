#pragma once

#include <string>
#include <windows.h>

//---------------------------------------------------------------------
// @brief  std::string（ANSI/SJIS）を std::wstring に変換する
// @param  oString  変換元のマルチバイト文字列
// @return 変換後のワイド文字列
//
// MultiByteToWideChar を使用して ANSI（CP_ACP）から
// UTF-16（wchar_t）へ変換する。
//---------------------------------------------------------------------
inline std::wstring StringToWString(std::string oString){
	// 必要なワイド文字バッファサイズを取得（終端 null 含む）
	const int bufferSize = MultiByteToWideChar(
		CP_ACP,                // 変換元コードページ（ANSI）
		0,
		oString.c_str(),
		-1,                    // null 終端まで変換
		nullptr,
		0                      // サイズ取得のみ
	);

	// 変換先バッファを確保
	wchar_t* wideBuffer = new wchar_t[bufferSize];

	// 実際の変換処理
	MultiByteToWideChar(
		CP_ACP,
		0,
		oString.c_str(),
		-1,
		wideBuffer,
		bufferSize
	);

	// null 終端を除いた範囲で std::wstring を構築
	std::wstring result(wideBuffer, wideBuffer + bufferSize - 1);

	// 確保したバッファを解放
	delete[] wideBuffer;

	return result;
}