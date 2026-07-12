#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchD3D11VertexInputState.h"

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

void ValidateVertexInputStateScope(){
	static_assert(!std::is_copy_constructible_v<
		StaticBatchD3D11VertexInputState>);
	static_assert(!std::is_copy_assignable_v<
		StaticBatchD3D11VertexInputState>);
	static_assert(!std::is_move_constructible_v<
		StaticBatchD3D11VertexInputState>);
	static_assert(!std::is_move_assignable_v<
		StaticBatchD3D11VertexInputState>);

	const std::string scope = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/"
		"StaticBatchD3D11VertexInputState.h"
	);
	assert(scope.find("VSGetShader") != std::string::npos);
	assert(scope.find("IAGetInputLayout") != std::string::npos);
	assert(scope.find("IAGetPrimitiveTopology") != std::string::npos);
	assert(scope.find("VSSetShader") != std::string::npos);
	assert(scope.find("IASetInputLayout") != std::string::npos);
	assert(scope.find("IASetPrimitiveTopology") != std::string::npos);
	assert(scope.find("~StaticBatchD3D11VertexInputState") !=
		std::string::npos);
}

void ValidateShadowSubmissionUsesScope(){
	const std::string submission = ReadTextFile(
		"Source/GameApplication/Engine/Scene/System/Render/StaticBatch/"
		"StaticBatchShadowSubmission.h"
	);
	assert(submission.find(
		"#include \"System/Render/StaticBatch/"
		"StaticBatchD3D11VertexInputState.h\""
	) != std::string::npos);
	assert(submission.find(
		"StaticBatchD3D11VertexInputState vertexInputState"
	) != std::string::npos);
	assert(submission.find("graphics.GetDeviceContext()") !=
		std::string::npos);

	const std::size_t stateCapture = submission.find(
		"StaticBatchD3D11VertexInputState vertexInputState"
	);
	const std::size_t commandBegin = submission.find("commandList->Begin()");
	assert(stateCapture != std::string::npos);
	assert(commandBegin != std::string::npos);
	assert(stateCapture < commandBegin);
}

} // namespace

int main(){
	ValidateVertexInputStateScope();
	ValidateShadowSubmissionUsesScope();
	return 0;
}
