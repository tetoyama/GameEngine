// =======================================================================
//
// VectorMath.cpp
//
// =======================================================================
#include "VectorMath.h"

#include <cmath>
#include <cstring>

namespace agentos {

void L2Normalize(std::vector<float>* v) {
	if(v == nullptr || v->empty()) return;

	double sumSquares = 0.0;
	for(const float x : *v) {
		sumSquares += static_cast<double>(x) * static_cast<double>(x);
	}
	if(sumSquares <= 0.0) return; // ゼロベクトルは割れないのでそのまま返す

	const double inv = 1.0 / std::sqrt(sumSquares);
	for(float& x : *v) {
		x = static_cast<float>(static_cast<double>(x) * inv);
	}
}

float DotProduct(const float* a, const float* b, std::size_t dim) {
	if(a == nullptr || b == nullptr) return 0.0f;
	// 誤差の蓄積を避けるため、累算はdoubleで行う。
	double sum = 0.0;
	for(std::size_t i = 0; i < dim; ++i) {
		sum += static_cast<double>(a[i]) * static_cast<double>(b[i]);
	}
	return static_cast<float>(sum);
}

float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
	if(a.empty() || a.size() != b.size()) return 0.0f;

	double dot = 0.0;
	double na = 0.0;
	double nb = 0.0;
	for(std::size_t i = 0; i < a.size(); ++i) {
		const double x = static_cast<double>(a[i]);
		const double y = static_cast<double>(b[i]);
		dot += x * y;
		na += x * x;
		nb += y * y;
	}
	if(na <= 0.0 || nb <= 0.0) return 0.0f;
	return static_cast<float>(dot / (std::sqrt(na) * std::sqrt(nb)));
}

std::vector<std::uint8_t> FloatsToBlob(const std::vector<float>& v) {
	std::vector<std::uint8_t> blob(v.size() * sizeof(float));
	if(!v.empty()) {
		std::memcpy(blob.data(), v.data(), blob.size());
	}
	return blob;
}

std::vector<float> BlobToFloats(const std::vector<std::uint8_t>& blob) {
	// 端数があるBLOBは壊れているとみなし、空を返す。
	if(blob.empty() || (blob.size() % sizeof(float)) != 0) return {};

	std::vector<float> v(blob.size() / sizeof(float));
	std::memcpy(v.data(), blob.data(), blob.size());
	return v;
}

} // namespace agentos
