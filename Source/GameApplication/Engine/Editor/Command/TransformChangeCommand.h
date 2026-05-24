// =======================================================================
// 
// TransformChangeCommand.h
// 
// =======================================================================
#pragma once

#include "ICommand.h"
#include "Entity/Entity.h"
#include "Scene/scene.h"
#include "Scene/Component/transformComponent.h"
#include <Backends/myVector3.h>
#include <DirectXMath.h>

class TransformChangeCommand : public ICommand {
public:
	TransformChangeCommand(SceneContext* context, Entity entity,
		Vector3 oldPos, DirectX::XMFLOAT4 oldRot, Vector3 oldScale,
		Vector3 newPos, DirectX::XMFLOAT4 newRot, Vector3 newScale)
		: m_pContext(context)
		, m_entity(entity)
		, m_oldPos(oldPos), m_oldRot(oldRot), m_oldScale(oldScale)
		, m_newPos(newPos), m_newRot(newRot), m_newScale(newScale)
	{}

	void Execute() override {
		auto* t = _GetTransform();
		if (!t) return;
		t->position = m_newPos;
		t->SetRotation(m_newRot);
		t->scale = m_newScale;
	}

	void Undo() override {
		auto* t = _GetTransform();
		if (!t) return;
		t->position = m_oldPos;
		t->SetRotation(m_oldRot);
		t->scale = m_oldScale;
	}

	std::string GetDescription() const override { return "トランスフォームを変更"; }

private:
	TransformComponent* _GetTransform() {
		if (!m_pContext || !m_pContext->entity->IsAlive(m_entity)) return nullptr;
		return m_pContext->component->GetComponent<TransformComponent>(m_entity);
	}

	SceneContext*        m_pContext;
	Entity m_Entity;
	Vector3              m_oldPos, m_NewPos;
	DirectX::XMFLOAT4   m_oldRot, m_NewRot;
	Vector3              m_oldScale, m_NewScale;
};
