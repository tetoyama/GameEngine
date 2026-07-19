// =======================================================================
//
// EditorIconLibrary.h
// Shared editor icon atlas access.
//
// =======================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "Backends/ImGui/imgui.h"
#include "Resources/Data/textureData.h"
#include "Resources/Loader/textureLoader.h"
#include "Resources/resourceService.h"

enum class EditorIcon : std::uint8_t {
	Add = 0,
	Assets,
	Collider,
	Component,
	Console,
	Entity,
	Hierarchy,
	Inspector,
	Layers,
	Name,
	Performance,
	Transform,
	Count,
};

struct EditorIconImage {
	ImTextureRef texture{};
	ImVec2 uv0{0.0f, 0.0f};
	ImVec2 uv1{1.0f, 1.0f};

	bool IsValid() const {
		return texture._TexID != (ImTextureID)0;
	}
};

class EditorIconLibrary {
public:
	void Initialize(ResourceService* resources) {
		if(!resources) return;
		m_atlas = resources->Load<TextureData>(
			"Asset/Texture/UI/Editor/EditorIcons.png"
		);
	}

	void Shutdown() {
		m_atlas.reset();
	}

	EditorIconImage Get(EditorIcon icon) const {
		EditorIconImage image;
		if(!m_atlas || !m_atlas->pTexture.Get()) return image;

		constexpr int columns = 4;
		constexpr int rows = 3;
		const int index = static_cast<int>(icon);
		if(index < 0 || index >= static_cast<int>(EditorIcon::Count)){
			return image;
		}

		const int column = index % columns;
		const int row = index / columns;
		image.texture._TexID = (ImTextureID)m_atlas->pTexture.Get();
		image.uv0 = ImVec2(
			static_cast<float>(column) / static_cast<float>(columns),
			static_cast<float>(row) / static_cast<float>(rows)
		);
		image.uv1 = ImVec2(
			static_cast<float>(column + 1) / static_cast<float>(columns),
			static_cast<float>(row + 1) / static_cast<float>(rows)
		);
		return image;
	}

	EditorIconImage GetForComponent(std::string_view componentName) const {
		if(componentName == "NameComponent"){
			return Get(EditorIcon::Name);
		}
		if(componentName == "TransformComponent"){
			return Get(EditorIcon::Transform);
		}
		if(componentName == "ColliderComponent"){
			return Get(EditorIcon::Collider);
		}
		return Get(EditorIcon::Component);
	}

private:
	std::shared_ptr<TextureData> m_atlas;
};
