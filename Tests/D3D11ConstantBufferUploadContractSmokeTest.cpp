#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

#include "Graphics/D3D11ConstantBufferUpload.h"

namespace {

std::string ReadTextFile(const char* path){
	std::ifstream stream(path, std::ios::binary);
	assert(stream && "contract source file must exist");
	return std::string(
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()
	);
}

std::size_t RequireToken(
	const std::string& text,
	const std::string& token,
	std::size_t offset = 0
){
	const std::size_t position = text.find(token, offset);
	assert(position != std::string::npos);
	return position;
}

void ValidateDescriptors(){
	static_assert(D3D11ConstantBufferUpload::IsValidByteWidth(16));
	static_assert(D3D11ConstantBufferUpload::IsValidByteWidth(256));
	static_assert(!D3D11ConstantBufferUpload::IsValidByteWidth(0));
	static_assert(!D3D11ConstantBufferUpload::IsValidByteWidth(15));

	const D3D11_BUFFER_DESC dynamicDesc =
		D3D11ConstantBufferUpload::MakeBufferDesc(
			256,
			D3D11ConstantBufferUploadStrategy::DynamicMap
		);
	assert(dynamicDesc.ByteWidth == 256);
	assert(dynamicDesc.Usage == D3D11_USAGE_DYNAMIC);
	assert(dynamicDesc.BindFlags == D3D11_BIND_CONSTANT_BUFFER);
	assert(dynamicDesc.CPUAccessFlags == D3D11_CPU_ACCESS_WRITE);

	const D3D11_BUFFER_DESC defaultDesc =
		D3D11ConstantBufferUpload::MakeBufferDesc(
			256,
			D3D11ConstantBufferUploadStrategy::UpdateSubresource
		);
	assert(defaultDesc.ByteWidth == 256);
	assert(defaultDesc.Usage == D3D11_USAGE_DEFAULT);
	assert(defaultDesc.BindFlags == D3D11_BIND_CONSTANT_BUFFER);
	assert(defaultDesc.CPUAccessFlags == 0);
}

void ValidateReleaseFastPathContract(){
	const std::string source = ReadTextFile(
		"Source/GameApplication/Service/Graphics/D3D11ConstantBufferUpload.h"
	);
	const std::size_t upload = RequireToken(source, "static bool UploadBytes(");
	const std::size_t debugBegin = RequireToken(source, "#ifndef NDEBUG", upload);
	const std::size_t getDesc = RequireToken(source, "buffer->GetDesc", debugBegin);
	const std::size_t debugEnd = RequireToken(source, "#endif", getDesc);
	const std::size_t map = RequireToken(source, "context->Map(", debugEnd);
	const std::size_t copy = RequireToken(source, "std::memcpy(", map);
	const std::size_t unmap = RequireToken(source, "context->Unmap(", copy);

	assert(debugBegin < getDesc);
	assert(getDesc < debugEnd);
	assert(debugEnd < map);
	assert(map < copy);
	assert(copy < unmap);
}

} // namespace

int main(){
	ValidateDescriptors();
	ValidateReleaseFastPathContract();
	return 0;
}
