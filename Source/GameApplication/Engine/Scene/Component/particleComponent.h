// =======================================================================
//
// particleComponent.h
//
// =======================================================================
#pragma once

#include "Interface/IComponent.h"
#include "Backends/myVector3.h"

#define MAXPARTICLE 512

// 個々のパーティクルの実行状態。
struct PARTICLE {
	Vector3 Position = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 Speed = Vector3(0.0f, 0.0f, 0.0f);
	float LifeTime = 0.0f;
};

// パーティクル設定とCPU実行状態を保持するComponent。
// YAMLとInspector実装はParticleComponentOperationsへ分離する。
class ParticleComponent: public IComponent {
public:
	PARTICLE Particle[MAXPARTICLE];

	bool isLoop = true;
	float SpawnInterval = 0.05f;
	int SpawnCount = 100;
	float particleLifeTime = 1.0f;
	float particleSize = 1.0f;
	float particleSpeed = 1.0f;
	float SpawnTimer = 0.0f;

	Vector3 SpawnPosition = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 SpawnPositionRandom = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 StartSpeed = Vector3(0.0f, 10.0f, 0.0f);
	Vector3 StartSpeedRandom = Vector3(3.0f, 0.0f, 3.0f);
	Vector3 AddSpeed = Vector3(0.0f, -15.0f, 0.0f);
	Vector3 MulSpeed = Vector3(1.0f, 1.0f, 1.0f);

	YAML::Node encode() override;
	bool decode(SceneContext* context, const YAML::Node& node) override;
	void inspector(SceneContext* context) override;
};

#include "Operations/ParticleComponentOperations.h"
