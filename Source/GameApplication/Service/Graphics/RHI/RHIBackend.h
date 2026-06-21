#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "RHICapabilities.h"
#include "RHIInterfaces.h"

namespace RHI {

// OS固有Window型をRHI公開契約へ漏らさないための非所有Handle。
// Win32ではwindow=HWND、Vulkan/X11等ではdisplayも使用できる。
struct NativeWindowHandle {
	void* window = nullptr;
	void* display = nullptr;
};

struct DeviceCreateDesc {
	NativeWindowHandle nativeWindow;
	SwapChainDesc swapChain;
	uint32_t adapterIndex = 0;
	bool enableDebugLayer = false;
	bool preferHighPerformanceAdapter = true;
};

struct AdapterInfo {
	uint32_t index = 0;
	std::string name;
	uint64_t dedicatedVideoMemory = 0;
	uint64_t sharedSystemMemory = 0;
	bool software = false;
};

// API単位の生成・Adapter列挙責務。
// RendererはD3D11/Vulkan等のBackend具象型を参照せず、このFactory契約だけを使う。
class IRHIBackend {
public:
	virtual ~IRHIBackend() = default;

	virtual BackendType GetType() const noexcept = 0;
	virtual std::string_view GetName() const noexcept = 0;
	virtual bool IsSupported() const = 0;
	virtual std::vector<AdapterInfo> EnumerateAdapters() const = 0;
	virtual std::unique_ptr<IRHIDevice> CreateDevice(
		const DeviceCreateDesc& desc
	) = 0;
};

using BackendFactory = std::unique_ptr<IRHIBackend>(*)();

// 利用可能なBackendを実行時に選択するRegistry。
// D3D11、D3D12、Vulkanはそれぞれ別翻訳単位からFactoryを登録する。
class BackendRegistry {
public:
	static BackendRegistry& Instance(){
		static BackendRegistry registry;
		return registry;
	}

	bool Register(BackendType type, BackendFactory factory){
		if(!factory || FindFactory(type)) return false;
		m_entries.push_back({type, factory});
		return true;
	}

	std::unique_ptr<IRHIBackend> Create(BackendType type) const {
		BackendFactory factory = FindFactory(type);
		return factory ? factory() : nullptr;
	}

	bool IsRegistered(BackendType type) const {
		return FindFactory(type) != nullptr;
	}

private:
	struct Entry {
		BackendType type;
		BackendFactory factory;
	};

	BackendFactory FindFactory(BackendType type) const {
		for(const Entry& entry : m_entries){
			if(entry.type == type) return entry.factory;
		}
		return nullptr;
	}

	std::vector<Entry> m_entries;
};

} // namespace RHI
