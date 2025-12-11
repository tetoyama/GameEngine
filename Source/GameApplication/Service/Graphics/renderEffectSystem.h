// EffectSystem.h
#pragma once

#include "Backends/Effekseer/Effekseer.h"
#include "Backends/Effekseer/EffekseerRendererDX11.h"

class RenderEffectSystem {
public:
    RenderEffectSystem(){}
    ~RenderEffectSystem(){}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void ShutDown();

    Effekseer::ManagerRef manager; //エフェクトの管理クラス
    EffekseerRendererDX11::RendererRef renderer;  //エフェクトの描画クラス
private:


};
