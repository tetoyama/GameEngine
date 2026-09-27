// =======================================================================
//
// VectorMath.h
//
// 埋め込みベクトルの基本演算。
//
// ここが「ベクトル検索」の実体で、想像より遥かに小さい。
// 実測でチャンクは1,448件。1024次元float32でも全体で5.9MBしかなく、
// 全件をメモリに載せて総当たりで内積を取っても150万回の積和で済む。
// 単スレッドで1ms未満のため、sqlite-vec等の近似最近傍探索は不要
// （Docs/AgentOS/04_Execution_Engine_Roadmap.md のRAG前提2の結論）。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace agentos {

// ベクトルの長さを1に揃える。ゼロベクトルは変更しない。
// 正規化しておくと、コサイン類似度が単なる内積になり計算が軽くなる。
void L2Normalize(std::vector<float>* v);

// 内積。両者が正規化済みならこれがそのままコサイン類似度。
float DotProduct(const float* a, const float* b, std::size_t dim);

// 正規化の有無に依存しないコサイン類似度。テストや検証用。
float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

// float配列 ⇔ バイト列。SQLiteのBLOBへ格納するための変換。
// エンディアンはネイティブのまま扱う（索引ファイルは同一マシン内で完結する前提）。
std::vector<std::uint8_t> FloatsToBlob(const std::vector<float>& v);
std::vector<float> BlobToFloats(const std::vector<std::uint8_t>& blob);

} // namespace agentos
