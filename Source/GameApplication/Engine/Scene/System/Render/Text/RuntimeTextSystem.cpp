#include "RuntimeTextSystem.h"

#include "Component/RuntimeTextComponent.h"
#include "Component/textureComponent.h"
#include "Component/transformComponent.h"
#include "DebugTools/debugSystem.h"
#include "Registry/componentRegistry.h"
#include "Resources/Data/textureData.h"
#include "Service/Graphics/graphicsContext.h"
#include "scene.h"
#include "sceneManager.h"

#include <algorithm>
#include <cstdint>
#include <d2d1helper.h>
#include <sstream>
#include <vector>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace {

std::wstring Utf8ToWide(const std::string& value){
	if(value.empty()) return {};
	int length = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		value.data(),
		static_cast<int>(value.size()),
		nullptr,
		0);
	if(length <= 0){
		length = MultiByteToWideChar(
			CP_UTF8,
			0,
			value.data(),
			static_cast<int>(value.size()),
			nullptr,
			0);
	}
	if(length <= 0) return {};
	std::wstring result(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(
		CP_UTF8,
		0,
		value.data(),
		static_cast<int>(value.size()),
		result.data(),
		length);
	return result;
}

DWRITE_TEXT_ALIGNMENT ResolveHorizontal(RuntimeTextComponent::HorizontalAlignment value){
	switch(value){
	case RuntimeTextComponent::HorizontalAlignment::Center: return DWRITE_TEXT_ALIGNMENT_CENTER;
	case RuntimeTextComponent::HorizontalAlignment::Trailing: return DWRITE_TEXT_ALIGNMENT_TRAILING;
	case RuntimeTextComponent::HorizontalAlignment::Leading:
	default: return DWRITE_TEXT_ALIGNMENT_LEADING;
	}
}

DWRITE_PARAGRAPH_ALIGNMENT ResolveVertical(RuntimeTextComponent::VerticalAlignment value){
	switch(value){
	case RuntimeTextComponent::VerticalAlignment::Center: return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	case RuntimeTextComponent::VerticalAlignment::Far: return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
	case RuntimeTextComponent::VerticalAlignment::Near:
	default: return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
	}
}

std::string HResultMessage(const char* operation, HRESULT result){
	std::ostringstream stream;
	stream << operation << " failed with HRESULT 0x" << std::hex
		<< static_cast<unsigned long>(result);
	return stream.str();
}

} // namespace

void RuntimeTextSystem::Initialize(){
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if(SUCCEEDED(comResult)){
		m_comInitializedHere = true;
	} else if(comResult != RPC_E_CHANGED_MODE){
		if(m_context && m_context->debug){
			m_context->debug->LOG_ERROR(HResultMessage("CoInitializeEx", comResult).c_str());
		}
		return;
	}

	const HRESULT factoryResult = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(m_wicFactory.ReleaseAndGetAddressOf()));
	if(FAILED(factoryResult) && m_context && m_context->debug){
		m_context->debug->LOG_ERROR(HResultMessage("WIC factory creation", factoryResult).c_str());
	}
}

void RuntimeTextSystem::Finalize(){
	m_wicFactory.Reset();
	if(m_comInitializedHere){
		CoUninitialize();
		m_comInitializedHere = false;
	}
}

void RuntimeTextSystem::RegisterTasks(SystemScheduleBuilder& builder){
	using RuntimeTextUpdateQuery = ECSQuery::ComponentQueryView<
		ECSQuery::Write<RuntimeTextComponent>,
		ECSQuery::Write<TextureComponent>,
		ECSQuery::Write<TransformComponent>
	>;

	builder.AddQueryTask<RuntimeTextUpdateQuery>(
		"RuntimeTextSystem.Rasterize.DirtyText",
		SystemTaskDomain::Frame,
		SystemPhase::Early,
		0,
		StructuralAccess::None,
		ThreadAffinity::MainThread,
		[this](const SystemTaskContext&){ Update(); }
	);
}

void RuntimeTextSystem::Update(){
	if(!m_context || !m_context->sceneManager || !m_wicFactory) return;

	for(const auto& [name, scene] : m_context->sceneManager->GetActiveScenes()){
		(void)name;
		if(!scene) continue;
		SceneContext* sceneContext = scene->GetSceneContext();
		if(!sceneContext || !sceneContext->component) continue;
		const auto entities = sceneContext->component->FindEntitiesWithComponent<RuntimeTextComponent>();
		for(const Entity entity : entities){
			auto* text = sceneContext->component->GetComponent<RuntimeTextComponent>(entity);
			auto* texture = sceneContext->component->GetComponent<TextureComponent>(entity);
			if(!text || !texture || !text->NeedsRasterization()) continue;

			std::shared_ptr<TextureData> generated;
			std::string error;
			if(!Rasterize(*text, generated, &error)){
				if(m_context->debug){
					m_context->debug->LOG_ERROR(("RuntimeText: " + error).c_str());
				}
				continue;
			}

			texture->m_TextureData = std::move(generated);
			texture->UV_Slice_X = 1.0f;
			texture->UV_Slice_Y = 1.0f;
			texture->AnimationNum = 0;
			if(text->AutoSizeTransform){
				if(auto* transform = sceneContext->component->GetComponent<TransformComponent>(entity)){
					transform->scale.x = static_cast<float>(text->PixelWidth);
					transform->scale.y = static_cast<float>(text->PixelHeight);
				}
			}
			text->MarkRasterized();
		}
	}
}

bool RuntimeTextSystem::Rasterize(
	const RuntimeTextComponent& component,
	std::shared_ptr<TextureData>& output,
	std::string* error){
	auto fail = [&](const std::string& message){
		if(error) *error = message;
		return false;
	};
	if(!m_context || !m_context->graphics) return fail("GraphicsContext is unavailable");
	ID3D11Device* device = m_context->graphics->GetDevice();
	ID2D1Factory* d2dFactory = m_context->graphics->GetD2DFactory();
	IDWriteFactory* dwriteFactory = m_context->graphics->GetDWriteFactory();
	if(!device || !d2dFactory || !dwriteFactory || !m_wicFactory){
		return fail("D3D11, Direct2D, DirectWrite, or WIC is unavailable");
	}

	const UINT width = static_cast<UINT>(std::clamp(component.PixelWidth, 1, 4096));
	const UINT height = static_cast<UINT>(std::clamp(component.PixelHeight, 1, 4096));

	Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
	HRESULT result = m_wicFactory->CreateBitmap(
		width,
		height,
		GUID_WICPixelFormat32bppPBGRA,
		WICBitmapCacheOnLoad,
		bitmap.GetAddressOf());
	if(FAILED(result)) return fail(HResultMessage("IWICImagingFactory::CreateBitmap", result));

	const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_SOFTWARE,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
		96.0f,
		96.0f,
		D2D1_RENDER_TARGET_USAGE_NONE,
		D2D1_FEATURE_LEVEL_DEFAULT);
	Microsoft::WRL::ComPtr<ID2D1RenderTarget> renderTarget;
	result = d2dFactory->CreateWicBitmapRenderTarget(
		bitmap.Get(),
		properties,
		renderTarget.GetAddressOf());
	if(FAILED(result)) return fail(HResultMessage("CreateWicBitmapRenderTarget", result));

	std::wstring fontFamily = Utf8ToWide(component.FontFamily);
	if(fontFamily.empty()) fontFamily = L"Yu Gothic UI";
	Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
	result = dwriteFactory->CreateTextFormat(
		fontFamily.c_str(),
		nullptr,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		std::clamp(component.FontSize, 4.0f, 256.0f),
		L"ja-jp",
		textFormat.GetAddressOf());
	if(FAILED(result)) return fail(HResultMessage("IDWriteFactory::CreateTextFormat", result));

	textFormat->SetTextAlignment(ResolveHorizontal(component.Horizontal));
	textFormat->SetParagraphAlignment(ResolveVertical(component.Vertical));
	textFormat->SetWordWrapping(component.WordWrap
		? DWRITE_WORD_WRAPPING_WRAP
		: DWRITE_WORD_WRAPPING_NO_WRAP);

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
	result = renderTarget->CreateSolidColorBrush(
		D2D1::ColorF(
			std::clamp(component.ColorR, 0.0f, 1.0f),
			std::clamp(component.ColorG, 0.0f, 1.0f),
			std::clamp(component.ColorB, 0.0f, 1.0f),
			std::clamp(component.ColorA, 0.0f, 1.0f)),
		brush.GetAddressOf());
	if(FAILED(result)) return fail(HResultMessage("CreateSolidColorBrush", result));

	renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	renderTarget->BeginDraw();
	renderTarget->Clear(D2D1::ColorF(0.0f, 0.0f));
	const std::wstring text = Utf8ToWide(component.Text);
	if(!text.empty()){
		renderTarget->DrawTextW(
			text.data(),
			static_cast<UINT32>(text.size()),
			textFormat.Get(),
			D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
			brush.Get(),
			D2D1_DRAW_TEXT_OPTIONS_CLIP,
			DWRITE_MEASURING_MODE_NATURAL);
	}
	result = renderTarget->EndDraw();
	if(FAILED(result)) return fail(HResultMessage("ID2D1RenderTarget::EndDraw", result));

	WICRect lockRect{0, 0, static_cast<INT>(width), static_cast<INT>(height)};
	Microsoft::WRL::ComPtr<IWICBitmapLock> bitmapLock;
	result = bitmap->Lock(&lockRect, WICBitmapLockRead, bitmapLock.GetAddressOf());
	if(FAILED(result)) return fail(HResultMessage("IWICBitmap::Lock", result));
	UINT sourceStride = 0;
	UINT sourceSize = 0;
	BYTE* source = nullptr;
	if(FAILED(bitmapLock->GetStride(&sourceStride)) ||
		FAILED(bitmapLock->GetDataPointer(&sourceSize, &source)) ||
		!source){
		return fail("WIC bitmap lock did not expose pixel data");
	}

	const UINT destinationStride = width * 4U;
	std::vector<std::uint8_t> rgba(static_cast<std::size_t>(destinationStride) * height);
	for(UINT y = 0; y < height; ++y){
		const BYTE* sourceRow = source + static_cast<std::size_t>(sourceStride) * y;
		std::uint8_t* destinationRow = rgba.data() + static_cast<std::size_t>(destinationStride) * y;
		for(UINT x = 0; x < width; ++x){
			const std::uint8_t bluePremultiplied = sourceRow[x * 4U + 0U];
			const std::uint8_t greenPremultiplied = sourceRow[x * 4U + 1U];
			const std::uint8_t redPremultiplied = sourceRow[x * 4U + 2U];
			const std::uint8_t alpha = sourceRow[x * 4U + 3U];
			auto unpremultiply = [alpha](std::uint8_t value){
				if(alpha == 0) return static_cast<std::uint8_t>(0);
				return static_cast<std::uint8_t>(std::min(255U,
					(static_cast<unsigned int>(value) * 255U + alpha / 2U) / alpha));
			};
			destinationRow[x * 4U + 0U] = unpremultiply(redPremultiplied);
			destinationRow[x * 4U + 1U] = unpremultiply(greenPremultiplied);
			destinationRow[x * 4U + 2U] = unpremultiply(bluePremultiplied);
			destinationRow[x * 4U + 3U] = alpha;
		}
	}

	D3D11_TEXTURE2D_DESC textureDescription{};
	textureDescription.Width = width;
	textureDescription.Height = height;
	textureDescription.MipLevels = 1;
	textureDescription.ArraySize = 1;
	textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDescription.SampleDesc.Count = 1;
	textureDescription.Usage = D3D11_USAGE_IMMUTABLE;
	textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initialData{};
	initialData.pSysMem = rgba.data();
	initialData.SysMemPitch = destinationStride;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	result = device->CreateTexture2D(&textureDescription, &initialData, texture.GetAddressOf());
	if(FAILED(result)) return fail(HResultMessage("ID3D11Device::CreateTexture2D", result));

	auto generated = std::make_shared<TextureData>();
	result = device->CreateShaderResourceView(texture.Get(), nullptr, generated->pTexture.GetAddressOf());
	if(FAILED(result)) return fail(HResultMessage("ID3D11Device::CreateShaderResourceView", result));
	generated->Width = static_cast<int>(width);
	generated->Height = static_cast<int>(height);
	generated->FilePath = "runtime://text/" + std::to_string(component.CalculateSignature());
	output = std::move(generated);
	return true;
}
