// =======================================================================
// 
// particleSystem.h
// 
// =======================================================================
#pragma once
#include "Interface/ISystem.h"
#include "Entity/Entity.h" // Entityの定義をインクルード
#include <d3d11.h>
#include <wrl/client.h>
#include <unordered_map>

struct SceneManagerContext;
class TransformComponent;
class SpriteRendererComponent;
class ParticleComponent;
// パーティクルの生成・更新を管理するシステム
class ParticleSystem : public ISystem {
public:

	const char* GetSystemName() const override{
		return "ParticleSystem";
	}

	ParticleSystem(SceneManagerContext* context) : m_context(context) {}
	~ParticleSystem() {}
	void Initialize() override;
	void Finalize()override;

	void Start() override;
	void Update(float deltaTime) override;

private:
	SceneManagerContext* m_context;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_particleParamCB;

	struct ParticleGPUResource {
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		Microsoft::WRL::ComPtr<ID3D11Buffer> readback;
		bool needsUpload = true;
	};
	std::unordered_map<ParticleComponent*, ParticleGPUResource> m_gpuResources;

	ParticleGPUResource* EnsureGPUResource(ParticleComponent* particle, ID3D11Device* device);
	void UploadParticlesToGPU(const ParticleGPUResource& resource, ParticleComponent* particle, ID3D11DeviceContext* context);
	void ReadbackParticles(const ParticleGPUResource& resource, ParticleComponent* particle, ID3D11DeviceContext* context);
	void UpdateParticleParamCB(const ParticleComponent* particle, float deltaTime, ID3D11DeviceContext* context);
};
