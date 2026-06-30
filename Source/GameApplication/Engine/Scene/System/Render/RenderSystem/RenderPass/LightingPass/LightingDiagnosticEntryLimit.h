#pragma once

#include <algorithm>

namespace LightingDiagnosticEntryLimit {

inline int ResolveExpandedCount(
	float encodedCount,
	int firstEntryIndex,
	int activeEntryCount
) noexcept {
	const int remainingEntries = (std::max)(
		activeEntryCount - firstEntryIndex,
		0
	);
	if(remainingEntries == 0){
		return 0;
	}

	const int requestedCount = (std::max)(
		static_cast<int>(encodedCount + 0.5f),
		1
	);
	return (std::min)(requestedCount, remainingEntries);
}

} // namespace LightingDiagnosticEntryLimit
