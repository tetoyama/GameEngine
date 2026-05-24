// =======================================================================
// 
// renderSystem.h
// 
// =======================================================================
#pragma once

#include "Interface/ISystem.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Scene/Entity/Entity.h"
#include "System/Render/RenderSystem/renderLayer.h"

struct SceneManagerContext;
struct PixelShaderData;
struct VertexShaderData;
struct RenderableContext;
struct RenderTarget;
struct CameraEntityData;
struct TextureData;

class IRenderable;
class IRenderPass;

class PlayerPass;
class EditorPass;

class ComponentRegistry;
class TransformComponent;

class PostEffectShader;

//======================================================================
// ポストエフェクト情報
//======================================================================
struct PostEffect
{
	PostEffectShader* shader;  // 使用するポストエフェクトシェーダ
	std::string       name;    // UI表示用名称
	bool              enabled; // 有効 / 無効
};

//======================================================================
// シェーダーマテリアル定義
//======================================================================
struct ShaderMaterial
{
	std::string filePath;   // HLSL ファイルパス
	std::string entryPoint; // エントリーポイント関数名
};

//======================================================================
// RenderSystem
// 描画処理全体を管理するシステム
//======================================================================
class RenderSystem: public ISystem
{
public:

	//------------------------------------------------------------------
	// システム名取得
	//------------------------------------------------------------------
	const char* GetSystemName() const override{
		return "RenderSystem";
	}

	//------------------------------------------------------------------
	// コンストラクタ
	// デフォルトのマテリアル設定を初期登録する
	//------------------------------------------------------------------
	RenderSystem(SceneManagerContext* context)
		: m_pContext(context){
		ShaderMaterials.clear();

		// Unlit マテリアル
		ShaderMaterial unlitMaterial;
		unlitMaterial.filePath = "UnlitShader.hlsli";
		unlitMaterial.entryPoint = "ShadeMaterial_Unlit";

		// PBR マテリアル
		ShaderMaterial pbrMaterial;
		pbrMaterial.filePath = "PBRShader.hlsli";
		pbrMaterial.entryPoint = "ShadeMaterial_PBR";

		ShaderMaterials.push_back(unlitMaterial);
		ShaderMaterials.push_back(pbrMaterial);
	}

	~RenderSystem(){}

	//------------------------------------------------------------------
	// ISystem 実装
	//------------------------------------------------------------------
	void Initialize() override;
	void Finalize() override;

	void Update(float deltaTime) override;
	void EditorUpdate(float deltaTime) override;
	void Draw() override;

	bool decode(const YAML::Node& node) override;
	YAML::Node encode() override;

	void SystemSetting() override;
	bool HasSystemSetting() const override { return true; }

	//------------------------------------------------------------------
	// 指定型の Renderable を取得
	// 登録済みリストから dynamic_cast で検索する
	//------------------------------------------------------------------
	template<typename T>
	T* GetRenderable(){
		static_assert(std::is_base_of<IRenderable, T>::value,
					  "T must inherit from IRenderable");

		for(auto& r : m_renderables){
			if(auto p = dynamic_cast<T*>(r.get())){
				return p;
			}
		}

		return nullptr;
	}

	//------------------------------------------------------------------
	// パス関連
	//------------------------------------------------------------------
	PlayerPass* m_PlayerPass = nullptr;
	bool* pShowPlayer = nullptr;

	// 各 RenderLayer の表示状態（Player）
	bool playerRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] =
	{
		true, true, true, true, true, false
	};

	EditorPass* m_EditorPass = nullptr;
	bool* pShowEditor = nullptr;

	// 各 RenderLayer の表示状態（Editor）
	bool editorRenderLayerVisible[(int)RenderLayer::MaxRenderLayer] =
	{
		true, true, true, true, true, true,
	};

	//------------------------------------------------------------------
	// ピクセルシェーダ再コンパイル
	//------------------------------------------------------------------
	void ReCompilePixelShaders();

	//------------------------------------------------------------------
	// 使用中のシェーダ取得
	//------------------------------------------------------------------
	PixelShaderData* GetDeferredPS(){
		return DeferredPS.get();
	}

	PixelShaderData* GetForwardPS(){
		return ForwardPS.get();
	}

	// フォワードパス向けに環境マップテクスチャ・サンプラーを返す
	std::shared_ptr<TextureData> GetEnvironmentMap() const;
	ID3D11SamplerState*          GetEnvMapSampler()  const;

	//------------------------------------------------------------------
	// 登録済みシェーダマテリアル一覧
	//------------------------------------------------------------------
	std::vector<ShaderMaterial> shaderMaterials;

private:

	//------------------------------------------------------------------
	// カメラ情報取得
	//------------------------------------------------------------------
	const CameraEntityData FindCameraEntity();

	//------------------------------------------------------------------
	// スカイスフィアから環境マップを自動更新
	//------------------------------------------------------------------
	void UpdateSkyBoxEnvironmentMap();

	//------------------------------------------------------------------
	// UI 制御関連
	//------------------------------------------------------------------
	void ControlButton();
	void DrawRenderLayerToggleUI();

	void EditorView();
	void PlayerView();

private:

	SceneManagerContext* m_pContext = nullptr;

	// 登録済み Renderable 一覧
	std::vector<std::shared_ptr<IRenderable>> m_Renderables;

	// ポストエフェクト用コピーシェーダ
	PostEffectShader* pCopyShader = nullptr;

	// 自動生成シェーダ出力パス
	std::string m_ShaderPath= "Source/Shader/AutoGen/";

	// UI ボタン用テクスチャ
	std::shared_ptr<TextureData> m_PlayButtonTexture;
	std::shared_ptr<TextureData> m_PauseButtonTexture;
	std::shared_ptr<TextureData> m_StopButtonTexture;
	std::shared_ptr<TextureData> m_StepButtonTexture;

	// 描画パイプライン用ピクセルシェーダ
	std::shared_ptr<PixelShaderData> m_DeferredPs;
	std::shared_ptr<PixelShaderData> m_ForwardPs;
};
