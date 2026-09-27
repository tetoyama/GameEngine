// =======================================================================
//
// CapabilitySet.cpp
//
// =======================================================================
#include "CapabilitySet.h"

#include <iomanip>
#include <random>
#include <sstream>

namespace agentos {

namespace {

// 32桁の16進文字列（128bit）をプロセス内乱数から生成する。
// 推測困難性が目的であり暗号論的強度は要求しない（プロセス内トークンのため）。
std::string GenerateSecret() {
	static thread_local std::mt19937_64 rng{std::random_device{}()};
	std::uniform_int_distribution<std::uint64_t> dist;
	const std::uint64_t high = dist(rng);
	const std::uint64_t low = dist(rng);

	std::ostringstream oss;
	oss << std::hex << std::setfill('0')
	    << std::setw(16) << high
	    << std::setw(16) << low;
	return oss.str();
}

} // namespace

CapabilityToken CapabilityRegistry::IssueToken(
	const AgentId& agent,
	std::vector<ToolName> allowedTools,
	PermissionLevel maxPermission) {

	Grant grant;
	grant.owner = agent;
	grant.allowedTools = std::move(allowedTools);
	grant.maxPermission = maxPermission;

	std::string secret = GenerateSecret();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		grants_[secret] = std::move(grant);
	}

	CapabilityToken token;
	token.owner = agent;
	token.secret = std::move(secret);
	return token;
}

Result CapabilityRegistry::Validate(const CapabilityToken& token, const ToolName& tool, PermissionLevel requiredLevel) const {
	std::lock_guard<std::mutex> lock(mutex_);

	auto it = grants_.find(token.secret);
	if (it == grants_.end()) {
		return Result::Fail("invalid capability token");
	}

	const Grant& grant = it->second;
	if (grant.owner != token.owner) {
		return Result::Fail("capability token owner mismatch");
	}

	bool toolAllowed = false;
	for (const auto& allowed : grant.allowedTools) {
		if (allowed == "*" || allowed == tool) {
			toolAllowed = true;
			break;
		}
	}
	if (!toolAllowed) {
		return Result::Fail("tool not permitted by capability: " + tool);
	}

	if (static_cast<std::uint8_t>(requiredLevel) > static_cast<std::uint8_t>(grant.maxPermission)) {
		return Result::Fail("insufficient permission level for tool: " + tool);
	}

	return Result::Ok();
}

void CapabilityRegistry::Revoke(const CapabilityToken& token) {
	std::lock_guard<std::mutex> lock(mutex_);
	grants_.erase(token.secret);
}

} // namespace agentos
