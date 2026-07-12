#include "RuntimeTextSystem.h"

#include "Component/RuntimeTextComponent.h"
#include "Component/entityNameComponent.h"
#include "Component/textureComponent.h"
#include "Component/transformComponent.h"
#include "DebugTools/debugSystem.h"
#include "Registry/componentRegistry.h"
#include "Resources/Data/textureData.h"
#include "Service/Graphics/graphicsContext.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <d2d1helper.h>
#include <sstream>
#include <unordered_set>
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

float Saturate(float value){
	return std::clamp(value, 0.0f, 1.0f);
}

float EaseOutCubic(float value){
	const float inverse = 1.0f - Saturate(value);
	return 1.0f - inverse * inverse * inverse;
}

float EaseOutBack(float value){
	constexpr float c1 = 1.70158f;
	constexpr float c3 = c1 + 1.0f;
	const float shifted = Saturate(value) - 1.0f;
	return 1.0f + c3 * shifted * shifted * shifted + c1 * shifted * shifted;
}

bool Contains(const std::string& text, const char* token){
	return text.find(token) != std::string::npos;
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
	m_motionStates.clear();
	m_lastCueEntity = 0;
	m_cueKind = CueKind::None;
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
		[this](const SystemTaskContext&){ ProcessDirtyText(); }
	);
}

void RuntimeTextSystem::ProcessDirtyText(){
	if(!m_context || !m_context->sceneManager || !m_wicFactory) return;

	const auto now = std::chrono::steady_clock::now();
	std::unordered_set<std::uint64_t> aliveEntities;

	for(const auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		(void)sceneName;
		if(!scene) continue;
		SceneContext* sceneContext = scene->GetSceneContext();
		if(!sceneContext || !sceneContext->component) continue;
		const auto entities = sceneContext->component->FindEntitiesWithComponent<RuntimeTextComponent>();

		// A newly recreated status entity is an action-bound presentation trigger.
		// The controller already rebuilds the runtime UI after every accepted action,
		// so no rules or hidden state need to leak into this presentation layer.
		for(const Entity entity : entities){
			auto* text = sceneContext->component->GetComponent<RuntimeTextComponent>(entity);
			auto* name = sceneContext->component->GetComponent<NameComponent>(entity);
			if(!text || !name) continue;
			if(name->name != "Status" && name->name != "ReorderStatus" && name->name != "GameSet") continue;

			const std::uint64_t key = entity.GetPackedValue();
			if(key == m_lastCueEntity) continue;
			m_lastCueEntity = key;

			CueKind cue = CueKind::None;
			if(name->name == "GameSet" || Contains(text->Text, "GAME SET")){
				cue = CueKind::GameSet;
			} else if(text->Text.starts_with("戦闘:")){
				cue = CueKind::Battle;
			} else if(text->Text.starts_with("偵察:")){
				cue = CueKind::Scout;
			} else if(Contains(text->Text, "中央再編")){
				cue = CueKind::Reorder;
			} else if(Contains(text->Text, "移動")){
				cue = CueKind::Move;
			}
			if(cue != CueKind::None){
				m_cueKind = cue;
				m_cueStarted = now;
			}
		}

		for(const Entity entity : entities){
			auto* text = sceneContext->component->GetComponent<RuntimeTextComponent>(entity);
			auto* texture = sceneContext->component->GetComponent<TextureComponent>(entity);
			auto* transform = sceneContext->component->GetComponent<TransformComponent>(entity);
			auto* name = sceneContext->component->GetComponent<NameComponent>(entity);
			if(!text || !texture) continue;

			if(text->NeedsRasterization()){
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
				if(text->AutoSizeTransform && transform){
					const float viewportWidth = static_cast<float>(
						std::max<UINT>(1U, m_context->graphics ? m_context->graphics->m_width : 1U));
					const float viewportHeight = static_cast<float>(
						std::max<UINT>(1U, m_context->graphics ? m_context->graphics->m_height : 1U));
					transform->scale.x = static_cast<float>(text->PixelWidth) / viewportWidth;
					transform->scale.y = static_cast<float>(text->PixelHeight) / viewportHeight;
				}
				text->MarkRasterized();
			}

			if(!transform) continue;
			const std::uint64_t key = entity.GetPackedValue();
			aliveEntities.insert(key);

			auto motionIterator = m_motionStates.find(key);
			if(motionIterator == m_motionStates.end()){
				MotionState motion;
				motion.basePositionX = transform->position.x;
				motion.basePositionY = transform->position.y;
				motion.baseScaleX = transform->scale.x;
				motion.baseScaleY = transform->scale.y;
				motion.started = now;

				const std::string componentName = name ? name->name : std::string{};
				if(componentName == "BoardCell"){
					motion.kind = MotionKind::BoardPulse;
					motion.durationSeconds = 0.24f;
					motion.delaySeconds = static_cast<float>(entity.index % 7U) * 0.012f;
				} else if(componentName == "Piece"){
					motion.kind = MotionKind::PiecePop;
					motion.durationSeconds = 0.28f;
					motion.delaySeconds = static_cast<float>(entity.index % 3U) * 0.018f;
				} else if(componentName == "DeckCard" || componentName == "ReorderCard"){
					motion.kind = MotionKind::CardDeal;
					motion.durationSeconds = 0.32f;
					motion.delaySeconds = static_cast<float>(entity.index % 6U) * 0.025f;
				} else if(componentName == "Title" || componentName == "ModeTitle" ||
					componentName == "RulesTitle" || componentName == "DeckTitle" ||
					componentName == "IntroTitle" || componentName == "ReorderTitle"){
					motion.kind = MotionKind::TitleReveal;
					motion.durationSeconds = 0.46f;
				} else if(componentName == "GameSet"){
					motion.kind = MotionKind::GameSet;
					motion.durationSeconds = 0.72f;
				} else if(componentName == "Winner"){
					motion.kind = MotionKind::Winner;
					motion.durationSeconds = 0.56f;
					motion.delaySeconds = 0.16f;
				} else if(componentName == "Status" || componentName == "ReorderStatus"){
					if(text->Text.starts_with("戦闘:")) motion.kind = MotionKind::StatusImpact;
					else if(text->Text.starts_with("偵察:")) motion.kind = MotionKind::StatusScout;
					else if(Contains(text->Text, "移動")) motion.kind = MotionKind::StatusMove;
					else motion.kind = MotionKind::SoftPop;
					motion.durationSeconds = 0.34f;
				} else if(componentName == "Button" || componentName == "HistoryLine" ||
					componentName == "AiReasoning" || componentName == "Turn" ||
					componentName == "InputMode"){
					motion.kind = MotionKind::SoftPop;
					motion.durationSeconds = 0.20f;
					motion.delaySeconds = static_cast<float>(entity.index % 4U) * 0.015f;
				}
				motionIterator = m_motionStates.emplace(key, motion).first;
			}

			MotionState& motion = motionIterator->second;
			transform->position.x = motion.basePositionX;
			transform->position.y = motion.basePositionY;
			transform->scale.x = motion.baseScaleX;
			transform->scale.y = motion.baseScaleY;

			const float elapsed = std::chrono::duration<float>(now - motion.started).count();
			const float localTime = elapsed - motion.delaySeconds;
			const float normalized = motion.durationSeconds > 0.0f
				? Saturate(localTime / motion.durationSeconds)
				: 1.0f;
			const float back = EaseOutBack(normalized);
			const float cubic = EaseOutCubic(normalized);
			auto applyScale = [&](float amount){
				transform->scale.x = motion.baseScaleX * amount;
				transform->scale.y = motion.baseScaleY * amount;
			};

			switch(motion.kind){
			case MotionKind::SoftPop:
				applyScale(0.88f + 0.12f * back);
				break;
			case MotionKind::BoardPulse:
				applyScale(0.84f + 0.16f * back);
				break;
			case MotionKind::PiecePop:
				applyScale(0.64f + 0.36f * back);
				transform->position.y += (1.0f - cubic) * 0.018f;
				break;
			case MotionKind::CardDeal:
				applyScale(0.70f + 0.30f * back);
				transform->position.y += (1.0f - cubic) * 0.035f;
				break;
			case MotionKind::TitleReveal:
				applyScale(0.72f + 0.28f * back);
				transform->position.y += (1.0f - cubic) * 0.032f;
				break;
			case MotionKind::StatusImpact:
				applyScale(0.66f + 0.34f * back);
				break;
			case MotionKind::StatusScout:
				applyScale(0.78f + 0.22f * back);
				break;
			case MotionKind::StatusMove:
				applyScale(0.84f + 0.16f * back);
				transform->position.x -= (1.0f - cubic) * 0.025f;
				break;
			case MotionKind::GameSet:
				applyScale(0.34f + 0.66f * back);
				transform->position.y += (1.0f - cubic) * 0.065f;
				break;
			case MotionKind::Winner:
				applyScale(0.58f + 0.42f * back);
				break;
			case MotionKind::None:
			default:
				break;
			}

			const std::string componentName = name ? name->name : std::string{};
			const float cueElapsed = std::chrono::duration<float>(now - m_cueStarted).count();
			switch(m_cueKind){
			case CueKind::Battle:
				if(cueElapsed < 0.42f){
					const float cueT = Saturate(cueElapsed / 0.42f);
					const float fade = 1.0f - cueT;
					const float shake = std::sin(cueT * 72.0f) * fade;
					const float impact = std::sin(cueT * 3.14159265f);
					if(componentName == "Piece"){
						transform->position.x += shake * 0.010f;
						transform->scale.x *= 1.0f + impact * 0.18f;
						transform->scale.y *= 1.0f + impact * 0.18f;
					} else if(componentName == "BoardCell"){
						transform->position.x += shake * 0.0035f;
						transform->scale.x *= 1.0f + impact * 0.055f;
						transform->scale.y *= 1.0f + impact * 0.055f;
					} else if(componentName == "Status"){
						transform->position.x += shake * 0.014f;
						transform->position.y -= (1.0f - EaseOutCubic(cueT)) * 0.22f;
						transform->scale.x *= 1.0f + fade * 0.72f + impact * 0.20f;
						transform->scale.y *= 1.0f + fade * 0.72f + impact * 0.20f;
					}
				}
				break;
			case CueKind::Scout:
				if(cueElapsed < 0.52f){
					const float cueT = Saturate(cueElapsed / 0.52f);
					const float fade = 1.0f - cueT;
					const float wave = std::sin(cueT * 18.0f + static_cast<float>(entity.index % 5U));
					if(componentName == "Piece"){
						transform->position.y += wave * fade * 0.008f;
					} else if(componentName == "BoardCell"){
						const float pulse = std::sin(cueT * 3.14159265f);
						transform->scale.x *= 1.0f + pulse * 0.075f;
						transform->scale.y *= 1.0f + pulse * 0.075f;
					} else if(componentName == "Status"){
						transform->position.y -= (1.0f - EaseOutCubic(cueT)) * 0.13f;
						transform->scale.x *= 1.0f + fade * 0.38f;
						transform->scale.y *= 1.0f + fade * 0.38f;
					}
				}
				break;
			case CueKind::Move:
				if(cueElapsed < 0.30f){
					const float cueT = Saturate(cueElapsed / 0.30f);
					const float impact = std::sin(cueT * 3.14159265f);
					if(componentName == "Piece"){
						transform->position.y -= impact * 0.012f;
						transform->scale.x *= 1.0f + impact * 0.10f;
						transform->scale.y *= 1.0f + impact * 0.10f;
					}
				}
				break;
			case CueKind::Reorder:
				if(cueElapsed < 0.64f && (componentName == "ReorderCard" || componentName == "DeckCard")){
					const float cueT = Saturate(cueElapsed / 0.64f);
					const float fade = 1.0f - cueT;
					transform->position.y -= std::sin(cueT * 15.0f + static_cast<float>(entity.index % 6U)) * fade * 0.014f;
				}
				break;
			case CueKind::GameSet:
				if(cueElapsed < 1.10f){
					const float cueT = Saturate(cueElapsed / 1.10f);
					const float pulse = std::sin(cueT * 3.14159265f);
					if(componentName == "GameSet"){
						transform->scale.x *= 1.0f + pulse * 0.28f;
						transform->scale.y *= 1.0f + pulse * 0.28f;
					} else if(componentName == "Winner"){
						transform->scale.x *= 1.0f + pulse * 0.14f;
						transform->scale.y *= 1.0f + pulse * 0.14f;
					}
				}
				break;
			case CueKind::None:
			default:
				break;
			}
		}
	}

	for(auto iterator = m_motionStates.begin(); iterator != m_motionStates.end();){
		if(aliveEntities.find(iterator->first) == aliveEntities.end()){
			iterator = m_motionStates.erase(iterator);
		} else {
			++iterator;
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

	const bool strongEmphasis =
		Contains(component.Text, "GAME SET") ||
		Contains(component.Text, " WIN") ||
		component.Text.starts_with("戦闘:") ||
		Contains(component.Text, "中央再編");
	const bool mediumEmphasis = strongEmphasis || component.FontSize >= 28.0f;

	std::wstring fontFamily = Utf8ToWide(component.FontFamily);
	if(fontFamily.empty()) fontFamily = L"Yu Gothic UI";
	Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
	result = dwriteFactory->CreateTextFormat(
		fontFamily.c_str(),
		nullptr,
		strongEmphasis ? DWRITE_FONT_WEIGHT_EXTRA_BOLD :
			(mediumEmphasis ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL),
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

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> shadowBrush;
	result = renderTarget->CreateSolidColorBrush(
		D2D1::ColorF(0.015f, 0.02f, 0.04f, std::clamp(component.ColorA * 0.78f, 0.0f, 0.9f)),
		shadowBrush.GetAddressOf());
	if(FAILED(result)) return fail(HResultMessage("Create shadow brush", result));

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> glowBrush;
	if(strongEmphasis){
		result = renderTarget->CreateSolidColorBrush(
			D2D1::ColorF(
				std::clamp(component.ColorR, 0.0f, 1.0f),
				std::clamp(component.ColorG, 0.0f, 1.0f),
				std::clamp(component.ColorB, 0.0f, 1.0f),
				std::clamp(component.ColorA * 0.23f, 0.0f, 0.35f)),
			glowBrush.GetAddressOf());
		if(FAILED(result)) return fail(HResultMessage("Create glow brush", result));
	}

	renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	renderTarget->BeginDraw();
	renderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
	const std::wstring text = Utf8ToWide(component.Text);
	if(!text.empty()){
		auto drawAt = [&](float offsetX, float offsetY, ID2D1Brush* targetBrush){
			renderTarget->DrawText(
				text.data(),
				static_cast<UINT32>(text.size()),
				textFormat.Get(),
				D2D1::RectF(
					offsetX,
					offsetY,
					static_cast<float>(width) + offsetX,
					static_cast<float>(height) + offsetY),
				targetBrush,
				D2D1_DRAW_TEXT_OPTIONS_CLIP,
				DWRITE_MEASURING_MODE_NATURAL);
		};

		if(strongEmphasis && glowBrush){
			drawAt(-3.0f, 0.0f, glowBrush.Get());
			drawAt(3.0f, 0.0f, glowBrush.Get());
			drawAt(0.0f, -3.0f, glowBrush.Get());
			drawAt(0.0f, 3.0f, glowBrush.Get());
			drawAt(-2.0f, -2.0f, glowBrush.Get());
			drawAt(2.0f, 2.0f, glowBrush.Get());
		}
		if(mediumEmphasis){
			drawAt(-1.0f, 0.0f, shadowBrush.Get());
			drawAt(1.0f, 0.0f, shadowBrush.Get());
			drawAt(0.0f, -1.0f, shadowBrush.Get());
			drawAt(0.0f, 1.0f, shadowBrush.Get());
		}
		drawAt(2.0f, 3.0f, shadowBrush.Get());
		drawAt(0.0f, 0.0f, brush.Get());
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
