#pragma once

#include <backends/yaml-cpp/yaml.h>
#include "buildSetting.h"

#ifdef _DEBUG
#pragma comment(lib,"yaml-cppd.lib")
#else
#pragma comment(lib,"yaml-cpp.lib")
#endif // _DEBUG

#include <DirectXMath.h>
#include "Backends/myVector2.h"
#include "Backends/myVector3.h"

#include "Resources/Data/textureData.h"
#include "Shader/Common.hlsl"
#include "Graphics/graphicsContext.h"

//======================================================================
// YAML 変換定義
// 各種数学構造体・マテリアル構造体を YAML へシリアライズ / デシリアライズする
//======================================================================
namespace YAML{
	//==================================================================
	// float4
	//==================================================================
	template<>
	struct convert<float4>
	{
		// float4 → YAML
		static Node encode(const float4& v){
			Node node;
			node["x"] = v.x;
			node["y"] = v.y;
			node["z"] = v.z;
			node["w"] = v.w;
			return node;
		}

		// YAML → float4
		static bool decode(const Node& node, float4& v){
			if(!node.IsMap()) return false;

			v.x = node["x"].as<float>();
			v.y = node["y"].as<float>();
			v.z = node["z"].as<float>();
			v.w = node["w"].as<float>();

			return true;
		}
	};

	//==================================================================
	// float3
	//==================================================================
	template<>
	struct convert<float3>
	{
		static Node encode(const float3& v){
			Node node;
			node["x"] = v.x;
			node["y"] = v.y;
			node["z"] = v.z;
			return node;
		}

		static bool decode(const Node& node, float3& v){
			if(!node.IsMap()) return false;

			v.x = node["x"].as<float>();
			v.y = node["y"].as<float>();
			v.z = node["z"].as<float>();

			return true;
		}
	};

	//==================================================================
	// float2
	//==================================================================
	template<>
	struct convert<float2>
	{
		static Node encode(const float2& v){
			Node node;
			node["x"] = v.x;
			node["y"] = v.y;
			return node;
		}

		static bool decode(const Node& node, float2& v){
			if(!node.IsMap()) return false;

			v.x = node["x"].as<float>();
			v.y = node["y"].as<float>();

			return true;
		}
	};

	//==================================================================
	// Vector2
	//==================================================================
	template<>
	struct convert<Vector2>
	{
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

	//==================================================================
	// Vector3
	//==================================================================
	template<>
	struct convert<Vector3>
	{
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

	//==================================================================
	// MATERIAL
	//==================================================================
	template<>
	struct convert<MATERIAL>
	{
		// MATERIAL → YAML
		static Node encode(const MATERIAL& mat){
			Node node;

			node["BaseColor"] = mat.BaseColor;
			node["Metallic"] = mat.Metallic;
			node["Roughness"] = mat.Roughness;
			node["AO"] = mat.AO;
			node["EmissiveColor"] = mat.EmissiveColor;
			node["EmissiveIntensity"] = mat.EmissiveIntensity;

			// ビットフラグを個別の bool として書き出す
			node["UseDiffuseTexture"] =
				(mat.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0;

			node["UseNormalTexture"] =
				(mat.MaterialFlags & MATERIAL_FLAG_USE_NORMAL_TEXTURE) != 0;

			return node;
		}

		// YAML → MATERIAL
		static bool decode(const Node& node, MATERIAL& mat){
			if(!node.IsMap()) return false;

			// 各項目が存在する場合のみ読み込む
			if(node["BaseColor"])
				mat.BaseColor = node["BaseColor"].as<DirectX::XMFLOAT4>();

			if(node["Metallic"])
				mat.Metallic = node["Metallic"].as<float>();

			if(node["Roughness"])
				mat.Roughness = node["Roughness"].as<float>();

			if(node["AO"])
				mat.AO = node["AO"].as<float>();

			if(node["EmissiveColor"])
				mat.EmissiveColor = node["EmissiveColor"].as<DirectX::XMFLOAT3>();

			if(node["EmissiveIntensity"])
				mat.EmissiveIntensity = node["EmissiveIntensity"].as<float>();

			// bool 値からビットマスクを再構築
			mat.MaterialFlags = 0;

			if(node["UseDiffuseTexture"])
				mat.MaterialFlags |= MATERIAL_FLAG_USE_DIFFUSE_TEXTURE;

			if(node["UseNormalTexture"])
				mat.MaterialFlags |= MATERIAL_FLAG_USE_NORMAL_TEXTURE;

			return true;
		}
	};

	//==================================================================
	// DirectX::XMMATRIX
	//==================================================================
	template<>
	struct convert<DirectX::XMMATRIX>
	{
		// 4x4 行列を [ [row0], [row1], ... ] 形式で保存
		static Node encode(const DirectX::XMMATRIX& mat){
			Node node;

			for(int i = 0; i < 4; ++i){
				Node row;
				for(int j = 0; j < 4; ++j){
					row.push_back(mat.r[i].m128_f32[j]);
				}
				node.push_back(row);
			}

			return node;
		}

		static bool decode(const Node& node, DirectX::XMMATRIX& mat){
			if(!node.IsSequence() || node.size() != 4)
				return false;

			for(int i = 0; i < 4; ++i){
				if(!node[i].IsSequence() || node[i].size() != 4)
					return false;

				for(int j = 0; j < 4; ++j){
					mat.r[i].m128_f32[j] = node[i][j].as<float>();
				}
			}

			return true;
		}
	};

	//==================================================================
	// DirectX::XMFLOAT4X4
	//==================================================================
	template<>
	struct convert<DirectX::XMFLOAT4X4>
	{
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