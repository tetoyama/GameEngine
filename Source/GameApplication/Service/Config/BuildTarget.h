#pragma once
#include <string_view>

enum class BuildTarget { Windows, MacOS };
inline constexpr std::string_view BuildTargetName(BuildTarget target) noexcept {
    return target == BuildTarget::MacOS ? "macOS" : "Windows";
}
inline constexpr std::string_view BuildTargetPreset(BuildTarget target) noexcept {
    return target == BuildTarget::MacOS ? "macos" : "windows";
}
