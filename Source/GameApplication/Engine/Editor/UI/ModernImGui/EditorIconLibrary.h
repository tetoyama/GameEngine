// =======================================================================
//
// EditorIconLibrary.h
// Resolution-independent editor icon descriptors.
//
// =======================================================================
#pragma once

#include <cstdint>
#include <string_view>

#include "Backends/ImGui/imgui.h"
#include "Resources/Data/textureData.h"

class ResourceService;

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
	// Editor glyphs are drawn as vector primitives. Texture fields remain for
	// controls that intentionally pass a runtime texture through TextureIcon().
	EditorIcon vectorIcon = EditorIcon::Count;
	ImTextureRef texture{};
	ImVec2 uv0{0.0f, 0.0f};
	ImVec2 uv1{1.0f, 1.0f};

	bool IsVector() const {
		return vectorIcon != EditorIcon::Count;
	}

	bool IsValid() const {
		return IsVector() || texture._TexID != (ImTextureID)0;
	}
};

class EditorIconLibrary {
public:
	// Kept for EditorService lifecycle compatibility. Vector icons require no
	// resource loading and are available immediately at every DPI scale.
	void Initialize(ResourceService*) {}
	void Shutdown() {}

	EditorIconImage Get(EditorIcon icon) const {
		EditorIconImage image;
		const int index = static_cast<int>(icon);
		if(index < 0 || index >= static_cast<int>(EditorIcon::Count)){
			return image;
		}
		image.vectorIcon = icon;
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
};
