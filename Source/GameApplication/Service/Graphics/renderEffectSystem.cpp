#include "renderEffectSystem.h"
#include "graphicsContext.h"

#ifdef _DEBUG
#pragma comment(lib,"Effekseerd.lib")
#pragma comment(lib,"EffekseerRendererDX11d.lib")
#pragma comment(lib,"EffekseerSoundXAudio2d.lib")
#else
#pragma comment(lib,"Effekseer.lib")
#pragma comment(lib,"EffekseerRendererDX11.lib")
#pragma comment(lib,"EffekseerSoundXAudio2.lib")
#endif

bool RenderEffectSystem::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {

    manager = nullptr; //エフェクトの管理クラス
    renderer = nullptr;  //エフェクトの描画クラス

    //Effekseerのレンダラーとマネージャーを作成
    manager = Effekseer::Manager::Create(9999);
    auto grapth = ::EffekseerRendererDX11::CreateGraphicsDevice(device, context);
    renderer = EffekseerRendererDX11::Renderer::Create(grapth, 9999);

    if (manager == nullptr) {
        MessageBox(NULL, L"マネージャーがNULLです", L"エラー", MB_OK);
        return false;
    }


    //描画モジュールの設定
    manager->SetSpriteRenderer(renderer->CreateSpriteRenderer());
    manager->SetRibbonRenderer(renderer->CreateRibbonRenderer());
    manager->SetRingRenderer(renderer->CreateRingRenderer());
    manager->SetTrackRenderer(renderer->CreateTrackRenderer());
    manager->SetModelRenderer(renderer->CreateModelRenderer());

    //テクスチャ、モデル、カーブ、マテリアルローダーの設定
    manager->SetTextureLoader(renderer->CreateTextureLoader());
    manager->SetModelLoader(renderer->CreateModelLoader());
    manager->SetMaterialLoader(renderer->CreateMaterialLoader());
    manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());

	return true;
}

void RenderEffectSystem::ShutDown() {

	renderer = nullptr;
	manager.Reset();
}
