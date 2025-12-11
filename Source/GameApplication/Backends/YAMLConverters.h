#pragma once
#include <backends/yaml-cpp/yaml.h>

#ifdef _DEBUG
#pragma comment(lib,"yaml-cppd.lib")
#else
#pragma comment(lib,"yaml-cpp.lib")
#endif // _DEBUG

#include <DirectXMath.h>
#include "Backends/myVector2.h"
#include "Backends/myVector3.h"
#include "Resources/Data/textureData.h"
#include "Graphics/graphicsContext.h"

namespace YAML {

	template<>
	struct convert<DirectX::XMFLOAT4> {
		static Node encode(const DirectX::XMFLOAT4& v){
			Node node;
			node["x"] = v.x;
			node["y"] = v.y;
			node["z"] = v.z;
			node["w"] = v.w;
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT4& v){
			if(!node.IsMap()) return false;
			v.x = node["x"].as<float>();
			v.y = node["y"].as<float>();
			v.z = node["z"].as<float>();
			v.w = node["w"].as<float>();
			return true;
		}
	};

	template<>
	struct convert<Vector2> {
		static Node encode(const Vector2& rhs){
			Node node;
			node["x"] = rhs.x;
			node["y"] = rhs.y;
			return node;
		}

		static bool decode(const Node& node, Vector2& rhs){
			if(!node.IsMap() || node.size() != 2) return false;
			rhs.x = node["x"].as<float>();
			rhs.y = node["y"].as<float>();
			return true;
		}
	};

	template<>
	struct convert<Vector3> {
		static Node encode(const Vector3& rhs){
			Node node;
			node["x"] = rhs.x;
			node["y"] = rhs.y;
			node["z"] = rhs.z;
			return node;
		}

		static bool decode(const Node& node, Vector3& rhs){
			if(!node.IsMap() || node.size() != 3) return false;
			rhs.x = node["x"].as<float>();
			rhs.y = node["y"].as<float>();
			rhs.z = node["z"].as<float>();
			return true;
		}
	};

	template<>
	struct convert<MATERIAL> {
		static Node encode(const MATERIAL& mat) {
			Node node;
			node["Ambient"] = mat.Ambient;
			node["Diffuse"] = mat.Diffuse;
			node["Specular"] = mat.Specular;
			node["Emission"] = mat.Emission;
			node["Shininess"] = mat.Shininess;
			node["TextureEnable"] = static_cast<bool>(mat.DiffuseTextureEnable); // BOOL→bool
			return node;
		}

		static bool decode(const Node& node, MATERIAL& mat) {
			if (!node.IsMap()) return false;
			mat.Ambient = node["Ambient"].as<DirectX::XMFLOAT4>();
			mat.Diffuse = node["Diffuse"].as<DirectX::XMFLOAT4>();
			mat.Specular = node["Specular"].as<DirectX::XMFLOAT4>();
			mat.Emission = node["Emission"].as<DirectX::XMFLOAT4>();
			mat.Shininess = node["Shininess"].as<float>();
			mat.DiffuseTextureEnable = node["TextureEnable"].as<bool>() ? TRUE : FALSE;
			return true;
		}
	};

	template<>
	struct YAML::convert<DirectX::XMMATRIX> {
		static Node encode(const DirectX::XMMATRIX& mat) {
			Node node;
			for (int i = 0; i < 4; ++i) {
				Node row;
				for (int j = 0; j < 4; ++j) {
					row.push_back(mat.r[i].m128_f32[j]);
				}
				node.push_back(row);
			}
			return node;
		}

		static bool decode(const Node& node, DirectX::XMMATRIX& mat) {
			if (!node.IsSequence() || node.size() != 4)
				return false;

			for (int i = 0; i < 4; ++i) {
				if (!node[i].IsSequence() || node[i].size() != 4)
					return false;

				for (int j = 0; j < 4; ++j) {
					float value = node[i][j].as<float>();
					mat.r[i].m128_f32[j] = value;
				}
			}
			return true;
		}
	};


	template<>
	struct convert<DirectX::XMFLOAT4X4> {
		static Node encode(const DirectX::XMFLOAT4X4& mat){
			Node node;
			for(int i = 0; i < 4; ++i){
				Node row;
				for(int j = 0; j < 4; ++j){
					row.push_back(mat.m[i][j]);
				}
				node.push_back(row);
			}
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT4X4& mat){
			if(!node.IsSequence() || node.size() != 4)
				return false;

			for(int i = 0; i < 4; ++i){
				if(!node[i].IsSequence() || node[i].size() != 4)
					return false;

				for(int j = 0; j < 4; ++j){
					mat.m[i][j] = node[i][j].as<float>();
				}
			}
			return true;
		}
	};
}
