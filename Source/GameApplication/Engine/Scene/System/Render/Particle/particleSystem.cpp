// =======================================================================
// 
// particleSystem.cpp
// 
// =======================================================================
#include "particleSystem.h"
#include "Scene.h"
#include "sceneManager.h"

#include "Backends/ImGui/ImGui.h"

#include "Entity/Entity.h"
#include "Registry/componentRegistry.h"

#include "Component/ParticleComponent.h"

#include "DebugTools/debugSystem.h"
#include "Graphics/graphicsContext.h"
#include "Graphics/mainRenderer.h"
#include <Component/transformComponent.h>
#include <d3d11.h>
#include <cmath>

constexpr UINT PARTICLE_THREAD_GROUP_SIZE = 64;

float RandomParticle() {
	return (float)(rand() % 200 - 100) / 100.0f;
}

static void updateParticlesCPU(ParticleComponent* particle, float deltaTime) {
	if (!particle) return;
	for (int i = 0; i < MAXPARTICLE; i++) {
		if (particle->Particle[i].LifeTime > 0.0f) {
			particle->Particle[i].LifeTime -= deltaTime;
			particle->Particle[i].Position += particle->Particle[i].Speed * deltaTime;
			particle->Particle[i].Speed += particle->AddSpeed * deltaTime;
			particle->Particle[i].Speed *= Vector3(powf(particle->MulSpeed.x, deltaTime), powf(particle->MulSpeed.y, deltaTime), powf(particle->MulSpeed.z, deltaTime));
		}
	}
}

struct ParticleParamCB {
	Vector3 AddSpeed;
	float pad0 = 0.0f;
	Vector3 MulSpeed;
	float pad1 = 0.0f;
	float DeltaTime = 0.0f;
	float ParticleLifeTime = 0.0f;
	float pad2[2] = {};
};

ParticleSystem::ParticleGPUResource* ParticleSystem::EnsureGPUResource(ParticleComponent* particle, ID3D11Device* device) {
	auto& resource = m_gpuResources[particle];
	if (resource.buffer) {
		return &resource;
	}
	if (!device) {
		return nullptr;
	}

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(PARTICLE) * MAXPARTICLE;
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(PARTICLE);

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = particle->Particle;

	HRESULT hr = device->CreateBuffer(&bufferDesc, &initData, resource.buffer.GetAddressOf());
	if (FAILED(hr)) {
		resource = {};
		m_gpuResources.erase(particle);
		OutputDebugStringA("Failed to create particle GPU buffer\n");
		return nullptr;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.NumElements = MAXPARTICLE;
	hr = device->CreateUnorderedAccessView(resource.buffer.Get(), &uavDesc, resource.uav.GetAddressOf());
	if (FAILED(hr)) {
		resource = {};
		m_gpuResources.erase(particle);
		OutputDebugStringA("Failed to create particle UAV\n");
		return nullptr;
	}

	D3D11_BUFFER_DESC stagingDesc{};
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.ByteWidth = sizeof(PARTICLE) * MAXPARTICLE;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.BindFlags = 0;
	stagingDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	stagingDesc.StructureByteStride = sizeof(PARTICLE);
	hr = device->CreateBuffer(&stagingDesc, nullptr, resource.readback.GetAddressOf());
	if (FAILED(hr)) {
		resource = {};
		m_gpuResources.erase(particle);
		OutputDebugStringA("Failed to create particle readback buffer\n");
		return nullptr;
	}

	return &resource;
}

void ParticleSystem::UploadParticlesToGPU(const ParticleGPUResource& resource, ParticleComponent* particle, ID3D11DeviceContext* context) {
	// UAV buffers cannot be mapped for CPU writes; keep the existing design and sync via UpdateSubresource.
	context->UpdateSubresource(resource.buffer.Get(), 0, nullptr, particle->Particle, 0, 0);
}

void ParticleSystem::ReadbackParticles(const ParticleGPUResource& resource, ParticleComponent* particle, ID3D11DeviceContext* context) {
	// Rendering consumes the CPU-side Particle array, so results are read back every frame to stay consistent.
	context->CopyResource(resource.readback.Get(), resource.buffer.Get());
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(context->Map(resource.readback.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
		memcpy(particle->Particle, mapped.pData, sizeof(PARTICLE) * MAXPARTICLE);
		context->Unmap(resource.readback.Get(), 0);
	}
}

void ParticleSystem::UpdateParticleParamCB(const ParticleComponent* particle, float deltaTime, ID3D11DeviceContext* context) {
	if (!m_particleParamCB) {
		return;
	}
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(context->Map(m_particleParamCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		ParticleParamCB param{};
		param.AddSpeed = particle->AddSpeed;
		param.MulSpeed = particle->MulSpeed;
		param.DeltaTime = deltaTime;
		param.ParticleLifeTime = particle->particleLifeTime;
		memcpy(mapped.pData, &param, sizeof(ParticleParamCB));
		context->Unmap(m_particleParamCB.Get(), 0);
	}
}

void ParticleSystem::Initialize() {
	ID3D11Device* device = m_context->graphics ? m_context->graphics->GetDevice() : nullptr;
	if (!device) {
		return;
	}

	D3D11_BUFFER_DESC cbDesc{};
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof(ParticleParamCB);
	if (FAILED(device->CreateBuffer(&cbDesc, nullptr, m_particleParamCB.GetAddressOf()))) {
		OutputDebugStringA("Failed to create particle param constant buffer\n");
		m_particleParamCB.Reset();
	}
}

void ParticleSystem::Finalize() {
	m_gpuResources.clear();
	m_particleParamCB.Reset();
}

void ParticleSystem::Start() {
	m_gpuResources.clear();
	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& Entities = context->component->FindEntitiesWithComponent<ParticleComponent>();
		if (Entities.empty()) {
			continue;
		}
		for (Entity entity : Entities) {
			ParticleComponent* particle = context->component->GetComponent<ParticleComponent>(entity);
			if (!particle) {
				continue;
			}
			particle->SpawnTimer = 0.0f;

			for (int i = 0; i < MAXPARTICLE; i++) {
				if (particle->isLoop) {
					particle->Particle[i].LifeTime = 0.0f;
				} else {
					if (particle->SpawnCount > i) {
						particle->Particle[i].LifeTime = particle->particleLifeTime;
						particle->Particle[i].Position = particle->SpawnPosition + Vector3(particle->SpawnPositionRandom.x * RandomParticle(), particle->SpawnPositionRandom.y * RandomParticle(), particle->SpawnPositionRandom.z * RandomParticle());
						particle->Particle[i].Speed = particle->StartSpeed + Vector3(particle->StartSpeedRandom.x * RandomParticle(), particle->StartSpeedRandom.y * RandomParticle(), particle->StartSpeedRandom.z * RandomParticle());
					}
				}
			}
		}
	}
}

void ParticleSystem::Update(float deltaTime){
	auto graphicsContext = m_context->graphics;
	ID3D11DeviceContext* deviceContext = graphicsContext ? graphicsContext->GetDeviceContext() : nullptr;
	ID3D11ComputeShader* particleCS = graphicsContext ? graphicsContext->GetParticleUpdateShader() : nullptr;

	for (auto& [name, scene] : m_context->sceneManager->GetActiveScenes()) {
		auto context = scene->GetSceneContext();
		const auto& Entities = context->component->FindEntitiesWithComponent<ParticleComponent>();
		if (Entities.empty()) {
			continue;
		}
		for (Entity entity : Entities) {
			ParticleComponent* particle = context->component->GetComponent<ParticleComponent>(entity);
			if (!particle) {
				continue;
			}
			particle->SpawnTimer += deltaTime;

			TransformComponent* transform = context->component->GetComponent<TransformComponent>(entity);
			if (transform) {
				ParticleGPUResource* gpuResourcePtr = nullptr;

				if (particleCS && deviceContext && m_particleParamCB) {
					gpuResourcePtr = EnsureGPUResource(particle, graphicsContext->GetDevice());
					if (gpuResourcePtr && gpuResourcePtr->buffer && gpuResourcePtr->uav && gpuResourcePtr->readback) {
						if (gpuResourcePtr->needsUpload) {
							UploadParticlesToGPU(*gpuResourcePtr, particle, deviceContext);
						}
						UpdateParticleParamCB(particle, deltaTime, deviceContext);

						deviceContext->CSSetUnorderedAccessViews(0, 1, gpuResourcePtr->uav.GetAddressOf(), nullptr);
						ID3D11Buffer* cbuffers[] = { m_particleParamCB.Get() };
						deviceContext->CSSetConstantBuffers(0, 1, cbuffers);
						deviceContext->CSSetShader(particleCS, nullptr, 0);

						const UINT group = (MAXPARTICLE + PARTICLE_THREAD_GROUP_SIZE - 1) / PARTICLE_THREAD_GROUP_SIZE;
						deviceContext->Dispatch(group, 1, 1);

						ID3D11UnorderedAccessView* nullUAV = nullptr;
						deviceContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
						ID3D11Buffer* nullCB = nullptr;
						deviceContext->CSSetConstantBuffers(0, 1, &nullCB);
						deviceContext->CSSetShader(nullptr, nullptr, 0);

						// Rendering path consumes CPU-side particle data, so keep the arrays in sync.
						ReadbackParticles(*gpuResourcePtr, particle, deviceContext);
						gpuResourcePtr->needsUpload = false;
					} else {
						updateParticlesCPU(particle, deltaTime);
					}
				} else {
					updateParticlesCPU(particle, deltaTime);
				}

				bool hasNewParticles = false;
				if (particle->SpawnInterval == 0.0f) { continue; }
				while (particle->SpawnTimer > particle->SpawnInterval) {
					particle->SpawnTimer -= particle->SpawnInterval;
					for (int i = 0; i < MAXPARTICLE; i++) {
						if (particle->isLoop && particle->Particle[i].LifeTime <= 0.0f) {
							particle->Particle[i].LifeTime = particle->particleLifeTime;
							particle->Particle[i].Position = particle->SpawnPosition + Vector3(particle->SpawnPositionRandom.x * RandomParticle(), particle->SpawnPositionRandom.y * RandomParticle(), particle->SpawnPositionRandom.z * RandomParticle());
							particle->Particle[i].Speed = particle->StartSpeed + Vector3(particle->StartSpeedRandom.x * RandomParticle(), particle->StartSpeedRandom.y * RandomParticle(), particle->StartSpeedRandom.z * RandomParticle());
							hasNewParticles = true;
							break;
						}
					}
				}

				if (gpuResourcePtr && hasNewParticles) {
					gpuResourcePtr->needsUpload = true;
				}
			}
		}

	}
}
