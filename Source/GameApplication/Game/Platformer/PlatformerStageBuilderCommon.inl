	static MaterialStyle Grass() {
		return {float4(0.20f, 0.42f, 0.28f, 1.0f), 0.02f, 0.88f,
			float3(0.0f, 0.015f, 0.0f), 0.0f};
	}
	static MaterialStyle Stone() {
		return {float4(0.28f, 0.38f, 0.50f, 1.0f), 0.08f, 0.78f,
			float3(0.0f, 0.01f, 0.03f), 0.0f};
	}
	static MaterialStyle Sand() {
		return {float4(0.62f, 0.50f, 0.30f, 1.0f), 0.02f, 0.90f,
			float3(0.03f, 0.015f, 0.0f), 0.0f};
	}
	static MaterialStyle Orchard() {
		return {float4(0.34f, 0.48f, 0.30f, 1.0f), 0.03f, 0.82f,
			float3(0.01f, 0.03f, 0.0f), 0.0f};
	}
	static MaterialStyle Ruin() {
		return {float4(0.32f, 0.34f, 0.42f, 1.0f), 0.38f, 0.42f,
			float3(0.02f, 0.025f, 0.05f), 0.10f};
	}
	static MaterialStyle Arena() {
		return {float4(0.20f, 0.22f, 0.30f, 1.0f), 0.48f, 0.35f,
			float3(0.02f, 0.015f, 0.06f), 0.15f};
	}
	static MaterialStyle Guide() {
		return {float4(0.12f, 0.55f, 0.72f, 1.0f), 0.18f, 0.34f,
			float3(0.01f, 0.38f, 0.85f), 1.40f};
	}
	static MaterialStyle WarmGlow() {
		return {float4(1.0f, 0.65f, 0.25f, 1.0f), 0.25f, 0.20f,
			float3(1.0f, 0.45f, 0.12f), 3.20f};
	}

	static bool IsCoreEntity(std::string_view name) {
		static constexpr std::array<std::string_view, 8> CoreNames = {
			"PlatformerGame",
			"PlatformerPlayer",
			"PlatformerCamera",
			"Sun",
			"PlatformerSky",
			"PlatformerPlayerFeedback",
			"PlatformerClearFeedback",
			"PlatformerStageBuilder"
		};
		for(std::string_view core : CoreNames) {
			if(name == core) return true;
		}
		return false;
	}

	static void PurgeBakedStage(
		SceneContext* context,
		EntityCommandBuffer& commands
	) {
		const auto namedEntities =
			context->component->FindEntitiesWithComponent<NameComponent>();
		for(Entity entity : namedEntities) {
			auto* name = ComponentRef<NameComponent>(entity, context).TryGet();
			if(!name || IsCoreEntity(name->name)) continue;
			commands.DestroyEntity(entity);
		}
	}

	static void ConfigureCore(SceneContext* context) {
		const Vector3 start(0.0f, 0.05f, 0.0f);
		auto player =
			PlatformerSceneAccess::FindFirst<PlatformerCharacterController>(context);
		if(auto* controller = player.TryGet()) {
			controller->SetCheckpoint(start);
			if(auto* pose =
				ComponentRef<TransformComponent>(player.GetEntityRef()).TryGet()) {
				pose->position = start;
			}
			if(auto* collider =
				ComponentRef<ColliderComponent>(player.GetEntityRef()).TryGet()) {
				if(auto* rigid = collider->pRigidbodyDynamic) {
					rigid->setGlobalPose(
						physx::PxTransform(start.x, start.y, start.z));
					rigid->setLinearVelocity(physx::PxVec3(0.0f));
					rigid->setAngularVelocity(physx::PxVec3(0.0f));
					rigid->wakeUp();
				}
			}
		}

		auto camera =
			PlatformerSceneAccess::FindFirst<CameraComponent>(context);
		if(auto* component = camera.TryGet()) {
			for(auto& effect : component->postEffects) {
				if(effect.name == "PlatformerBrightPass") {
					effect.Param.x = 1.05f;
				}
			}
			component->InvalidatePostEffectGraphCache();
		}

		auto cameraController =
			PlatformerSceneAccess::FindFirst<PlatformerCameraController>(context);
		if(auto* controller = cameraController.TryGet()) {
			controller->courseYawDegrees = 0.0f;
			controller->courseDistance = 9.5f;
			controller->courseHeight = 4.2f;
			controller->wallYawDegrees = 55.0f;
			controller->wallDistance = 12.0f;
			controller->wallHeight = 5.8f;
			controller->bossDistance = 14.5f;
			controller->bossHeight = 7.5f;
			controller->SetProfile(
				PlatformerCameraController::Profile::Course);
		}
	}

	static void SetName(EntityRef entity, const std::string& name) {
		if(auto* component = ComponentRef<NameComponent>(entity).TryGet()) {
			component->name = name;
		}
	}

	static void SetTransform(
		EntityRef entity,
		const Vector3& position,
		const Vector3& scale,
		float pitchDegrees = 0.0f,
		float yawDegrees = 0.0f,
		float rollDegrees = 0.0f
	) {
		auto* transform = ComponentRef<TransformComponent>(entity).TryGet();
		if(!transform) return;
		transform->position = position;
		transform->scale = scale;
		const DirectX::XMVECTOR rotation =
			DirectX::XMQuaternionRotationRollPitchYaw(
				DirectX::XMConvertToRadians(pitchDegrees),
				DirectX::XMConvertToRadians(yawDegrees),
				DirectX::XMConvertToRadians(rollDegrees));
		DirectX::XMFLOAT4 stored;
		DirectX::XMStoreFloat4(&stored, rotation);
		transform->SetRotation(stored);
	}

	static void SetMaterial(
		EntityRef entity,
		const MaterialStyle& style
	) {
		auto* material = ComponentRef<MaterialComponent>(entity).TryGet();
		if(!material) return;
		material->Material.BaseColor = style.baseColor;
		material->Material.Metallic = style.metallic;
		material->Material.Roughness = style.roughness;
		material->Material.AO = 1.0f;
		material->Material.EmissiveColor = style.emissiveColor;
		material->Material.EmissiveIntensity = style.emissiveIntensity;
		material->Material.UseDiffuseTexture = true;
		material->Material.UseNormalTexture = true;
		material->Material.UseEnvironmentMap = true;
	}

	static void QueueBlock(
		EntityCommandBuffer& commands,
		std::string name,
		const Vector3& position,
		const Vector3& scale,
		const MaterialStyle& style,
		float pitchDegrees = 0.0f
	) {
		commands.InstantiatePrefab(
			BlockPrefab,
			[name, position, scale, style, pitchDegrees](EntityRef root) {
				SetName(root, name);
				SetTransform(root, position, scale, pitchDegrees);
				SetMaterial(root, style);
			});
	}

	static void QueueCoin(
		EntityCommandBuffer& commands,
		std::string name,
		const Vector3& position
	) {
		commands.InstantiatePrefab(
			CoinPrefab,
			[name, position](EntityRef root) {
				SetName(root, name);
				SetTransform(root, position, Vector3(0.34f, 0.52f, 0.12f));
			});
	}

	static void QueueEnemy(
		EntityCommandBuffer& commands,
		std::string name,
		const Vector3& position,
		const Vector3& patrolAxis,
		float patrolDistance,
		float patrolSpeed
	) {
		commands.InstantiatePrefab(
			EnemyPrefab,
			[name, position, patrolAxis, patrolDistance, patrolSpeed](
				EntityRef root
			) {
				SetName(root, name);
				SetTransform(root, position, Vector3(0.9f, 0.9f, 0.9f));
				if(auto* enemy =
					ComponentRef<PlatformerEnemy>(root).TryGet()) {
					enemy->patrolAxis = patrolAxis;
					enemy->patrolDistance = patrolDistance;
					enemy->patrolSpeed = patrolSpeed;
				}
			});
	}

	static void QueueCheckpoint(
		EntityCommandBuffer& commands,
		std::string name,
		const Vector3& position
	) {
		commands.InstantiatePrefab(
			CheckpointPrefab,
			[name, position](EntityRef root) {
				SetName(root, name);
				SetTransform(root, position, Vector3(0.45f, 1.2f, 0.45f));
			});
	}

	static void QueueMovingPlatform(
		EntityCommandBuffer& commands,
		std::string name,
		const Vector3& position,
		const Vector3& localOffset,
		float cycleSeconds,
		float phaseOffset
	) {
		commands.InstantiatePrefab(
			MovingPlatformPrefab,
			[name, position, localOffset, cycleSeconds, phaseOffset](
				EntityRef root
			) {
				SetName(root, name);
				SetTransform(root, position, Vector3(3.0f, 0.35f, 3.0f), -6.0f);
				if(auto* platform =
					ComponentRef<PlatformerMovingPlatform>(root).TryGet()) {
					platform->localOffset = localOffset;
					platform->cycleSeconds = cycleSeconds;
					platform->phaseOffset = phaseOffset;
					platform->carryPlayer = true;
				}
			});
	}

	static void QueueCameraZone(
		EntityCommandBuffer& commands,
		std::string name,
		const Vector3& position,
		const Vector3& scale,
		PlatformerCameraController::Profile profile,
		bool restoreOnExit
	) {
		commands.InstantiatePrefab(
			CameraZonePrefab,
			[name, position, scale, profile, restoreOnExit](EntityRef root) {
				SetName(root, name);
				SetTransform(root, position, scale);
				if(auto* zone =
					ComponentRef<PlatformerCameraZone>(root).TryGet()) {
					zone->profile = static_cast<int>(profile);
					zone->exitProfile =
						static_cast<int>(
							PlatformerCameraController::Profile::Course);
					zone->restoreOnExit = restoreOnExit;
				}
			});
	}

	static void QueueTree(
		EntityCommandBuffer& commands,
		std::string name,
		const Vector3& position,
		float scale
	) {
		commands.InstantiatePrefab(
			TreePrefab,
			[name, position, scale](EntityRef root) {
				SetName(root, name);
				SetTransform(root, position, Vector3(scale, scale, scale));
				if(auto* collider =
					ComponentRef<ColliderComponent>(root).TryGet()) {
					for(auto& shape : collider->colliders) {
						shape.isTrigger = true;
						shape.collisionLayer = 8;
					}
				}
			});
	}

	static void QueueProp(
		EntityCommandBuffer& commands,
		const char* prefabPath,
		std::string name,
		const Vector3& position,
		const Vector3& scale,
		const MaterialStyle& style,
		float yawDegrees = 0.0f
	) {
		commands.InstantiatePrefab(
			prefabPath,
			[name, position, scale, style, yawDegrees](EntityRef root) {
				SetName(root, name);
				SetTransform(root, position, scale, 0.0f, yawDegrees);
				SetMaterial(root, style);
			});
	}

	static void QueueBoss(
		EntityCommandBuffer& commands,
		const Vector3& position
	) {
		commands.InstantiatePrefab(
			BossPrefab,
			[position](EntityRef root) {
				SetName(root, "PlatformerBoss");
				SetTransform(root, position, Vector3(2.4f, 2.0f, 2.4f));
				if(auto* collider =
					ComponentRef<ColliderComponent>(root).TryGet()) {
					for(auto& shape : collider->colliders) {
						shape.offset = Vector3(0.0f, 0.0f, 0.0f);
					}
				}
			});
	}

