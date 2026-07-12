#pragma once

#include "Interface/IComponent.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

class RuntimeTextComponent final : public IComponent {
public:
	enum class HorizontalAlignment : std::uint8_t {
		Leading = 0,
		Center,
		Trailing
	};

	enum class VerticalAlignment : std::uint8_t {
		Near = 0,
		Center,
		Far
	};

	std::string Text;
	std::string FontFamily = "Yu Gothic UI";
	float FontSize = 32.0f;
	int PixelWidth = 512;
	int PixelHeight = 96;
	float ColorR = 1.0f;
	float ColorG = 1.0f;
	float ColorB = 1.0f;
	float ColorA = 1.0f;
	HorizontalAlignment Horizontal = HorizontalAlignment::Leading;
	VerticalAlignment Vertical = VerticalAlignment::Near;
	bool WordWrap = true;
	bool AutoSizeTransform = true;

	void SetText(std::string value){
		if(Text == value) return;
		Text = std::move(value);
		MarkDirty();
	}

	void MarkDirty() noexcept{
		m_renderedSignature = 0;
	}

	bool NeedsRasterization() const noexcept{
		return m_renderedSignature != CalculateSignature();
	}

	void MarkRasterized() noexcept{
		m_renderedSignature = CalculateSignature();
	}

	std::uint64_t CalculateSignature() const noexcept{
		constexpr std::uint64_t offset = 1469598103934665603ULL;
		constexpr std::uint64_t prime = 1099511628211ULL;
		std::uint64_t hash = offset;
		auto appendBytes = [&](const void* data, std::size_t size){
			const auto* bytes = static_cast<const unsigned char*>(data);
			for(std::size_t index = 0; index < size; ++index){
				hash ^= bytes[index];
				hash *= prime;
			}
		};
		appendBytes(Text.data(), Text.size());
		appendBytes(FontFamily.data(), FontFamily.size());
		appendBytes(&FontSize, sizeof(FontSize));
		appendBytes(&PixelWidth, sizeof(PixelWidth));
		appendBytes(&PixelHeight, sizeof(PixelHeight));
		appendBytes(&ColorR, sizeof(ColorR));
		appendBytes(&ColorG, sizeof(ColorG));
		appendBytes(&ColorB, sizeof(ColorB));
		appendBytes(&ColorA, sizeof(ColorA));
		appendBytes(&Horizontal, sizeof(Horizontal));
		appendBytes(&Vertical, sizeof(Vertical));
		appendBytes(&WordWrap, sizeof(WordWrap));
		return hash == 0 ? 1 : hash;
	}

	YAML::Node encode() override{
		YAML::Node node;
		node["Text"] = Text;
		node["FontFamily"] = FontFamily;
		node["FontSize"] = FontSize;
		node["PixelWidth"] = PixelWidth;
		node["PixelHeight"] = PixelHeight;
		node["ColorR"] = ColorR;
		node["ColorG"] = ColorG;
		node["ColorB"] = ColorB;
		node["ColorA"] = ColorA;
		node["Horizontal"] = static_cast<int>(Horizontal);
		node["Vertical"] = static_cast<int>(Vertical);
		node["WordWrap"] = WordWrap;
		node["AutoSizeTransform"] = AutoSizeTransform;
		return node;
	}

	bool decode(SceneContext*, const YAML::Node& node) override{
		if(!node.IsMap()) return false;
		if(node["Text"]) Text = node["Text"].as<std::string>();
		if(node["FontFamily"]) FontFamily = node["FontFamily"].as<std::string>();
		if(node["FontSize"]) FontSize = node["FontSize"].as<float>();
		if(node["PixelWidth"]) PixelWidth = node["PixelWidth"].as<int>();
		if(node["PixelHeight"]) PixelHeight = node["PixelHeight"].as<int>();
		if(node["ColorR"]) ColorR = node["ColorR"].as<float>();
		if(node["ColorG"]) ColorG = node["ColorG"].as<float>();
		if(node["ColorB"]) ColorB = node["ColorB"].as<float>();
		if(node["ColorA"]) ColorA = node["ColorA"].as<float>();
		if(node["Horizontal"]) Horizontal = static_cast<HorizontalAlignment>(
			std::clamp(node["Horizontal"].as<int>(), 0, 2));
		if(node["Vertical"]) Vertical = static_cast<VerticalAlignment>(
			std::clamp(node["Vertical"].as<int>(), 0, 2));
		if(node["WordWrap"]) WordWrap = node["WordWrap"].as<bool>();
		if(node["AutoSizeTransform"]) AutoSizeTransform = node["AutoSizeTransform"].as<bool>();
		PixelWidth = std::clamp(PixelWidth, 1, 4096);
		PixelHeight = std::clamp(PixelHeight, 1, 4096);
		FontSize = std::clamp(FontSize, 4.0f, 256.0f);
		ColorR = std::clamp(ColorR, 0.0f, 1.0f);
		ColorG = std::clamp(ColorG, 0.0f, 1.0f);
		ColorB = std::clamp(ColorB, 0.0f, 1.0f);
		ColorA = std::clamp(ColorA, 0.0f, 1.0f);
		MarkDirty();
		return true;
	}

	void inspector(SceneContext*) override{
		bool changed = false;
		changed |= ImGui::UndoInputText("Text", &Text);
		changed |= ImGui::UndoInputText("Font Family", &FontFamily);
		changed |= ImGui::UndoDragFloat("Font Size", &FontSize, 1.0f, 4.0f, 256.0f);
		changed |= ImGui::UndoDragInt("Pixel Width", &PixelWidth, 1.0f, 1, 4096);
		changed |= ImGui::UndoDragInt("Pixel Height", &PixelHeight, 1.0f, 1, 4096);
		changed |= ImGui::UndoDragFloat("Color R", &ColorR, 0.01f, 0.0f, 1.0f);
		changed |= ImGui::UndoDragFloat("Color G", &ColorG, 0.01f, 0.0f, 1.0f);
		changed |= ImGui::UndoDragFloat("Color B", &ColorB, 0.01f, 0.0f, 1.0f);
		changed |= ImGui::UndoDragFloat("Color A", &ColorA, 0.01f, 0.0f, 1.0f);
		int horizontal = static_cast<int>(Horizontal);
		int vertical = static_cast<int>(Vertical);
		changed |= ImGui::UndoDragInt("Horizontal", &horizontal, 1.0f, 0, 2);
		changed |= ImGui::UndoDragInt("Vertical", &vertical, 1.0f, 0, 2);
		changed |= ImGui::UndoCheckbox("Word Wrap", &WordWrap);
		changed |= ImGui::UndoCheckbox("Auto Size Transform", &AutoSizeTransform);
		Horizontal = static_cast<HorizontalAlignment>(std::clamp(horizontal, 0, 2));
		Vertical = static_cast<VerticalAlignment>(std::clamp(vertical, 0, 2));
		if(changed){
			PixelWidth = std::clamp(PixelWidth, 1, 4096);
			PixelHeight = std::clamp(PixelHeight, 1, 4096);
			FontSize = std::clamp(FontSize, 4.0f, 256.0f);
			MarkDirty();
		}
	}

private:
	std::uint64_t m_renderedSignature = 0;
};
