// =======================================================================
// 
// modelData.h
// 
// =======================================================================
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Shader/Common.hlsl"

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"

#include "modelMaterialTypes.h"
#include "modelAssimpMaterialPropertyNormalization.h"

class GraphicsContext;
struct aiScene;

struct SKINNING_INPUT_VERTEX {
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;
	uint32_t BoneIndex[4];
	float BoneWeight[4];
	DirectX::XMFLOAT4 Diffuse;
};

struct DEFORM_VERTEX {
	aiVector3D Position;
	aiVector3D Normal;
	uint32_t BoneIndex[4];
	float BoneWeight[4];
};

struct BONE {
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
	aiMatrix4x4 OffsetMatrix;
	aiMatrix4x4 WorldMatrix;
};

struct AnimationBlend {
	std::string name;
	float weight = 0.0f;
	float animationStartTime = 0.0f;
	bool isLoop = true;
};

struct AnimationData {
	std::string FilePath;
	const aiScene* Scene = nullptr;
	aiAnimation* Animation = nullptr;
	bool isImported = true;

	void Release(){
		if(isImported && Scene){
			aiReleaseImport(Scene);
			Scene = nullptr;
			Animation = nullptr;
		}
	}
};

// Assimp Meshから抽出したBackend非依存の共有Geometry Source。
// 移行中は既存D3D11 Buffer生成にも使用し、次工程でRenderSystem/RHI側の
// Model Geometry Runtime生成元へ切り替える。
struct ModelMeshGeometryCpuData {
	std::vector<VERTEX_3D> vertices;
	std::vector<std::uint32_t> indices;

	bool IsValid() const noexcept {
		return !vertices.empty() && !indices.empty();
	}
};

struct ModelData {
public:
	ModelData(){
		OutputDebugStringA(("Created ModelData " + FilePath + "\n").c_str());
	}

	~ModelData(){
		OutputDebugStringA(("Destroyed ModelData " + FilePath + "\n").c_str());
		Release();
	}

	void Release();

	std::string FilePath = "";
	bool isBlender = false;
	bool SetTexture = false;
	const aiScene* AiScene = nullptr;

	// Backend非依存CPU Geometry。Native Bufferと寿命を分離し、RHI移行時の
	// 再生成元として保持する。
	std::vector<ModelMeshGeometryCpuData> MeshGeometry;

	// MeshGeometryの内容・並び・Vertex Layout解釈が変化した場合に必ず進める。
	// 要素数が同一の頂点差し替えもRuntime再生成対象にするため、Pointerや
	// vector sizeだけを変更判定へ使用しない。
	std::uint64_t GetGeometryRevision() const noexcept {
		return m_geometryRevision;
	}

	std::uint64_t MarkGeometryDirty() noexcept {
		++m_geometryRevision;
		if(m_geometryRevision == 0){
			++m_geometryRevision;
		}
		return m_geometryRevision;
	}

	// Import時に正規化されたAsset-local定義。配列IndexはRuntime走査用、
	// IDはScene保存とReimport追従用として分離する。
	std::vector<ModelSubMeshDefinition> SubMeshes;
	std::vector<ImportedMaterialDefinition> ImportedMaterials;
	std::vector<ModelMaterialImportDiagnostic> MaterialImportDiagnostics;

	// Loader分割中の移行Bridge。既に明示定義が与えられている場合は保持し、
	// 未正規化のAssimp Sceneだけを最初のRender Extraction前に変換する。
	// AdapterはaiMaterialPropertyを直接読み、Assimp実装Libraryへ依存しない。
	bool EnsureMaterialDefinitionsNormalized(){
		if(m_materialDefinitionsNormalized){
			return true;
		}
		if(!SubMeshes.empty() || !ImportedMaterials.empty()){
			m_materialDefinitionsNormalized = true;
			return true;
		}
		if(!AiScene){
			return false;
		}
		m_materialDefinitionsNormalized =
			ModelAssimpMaterialNormalization::Normalize(
				AiScene,
				ImportedMaterials,
				SubMeshes,
				MaterialImportDiagnostics
			);
		return m_materialDefinitionsNormalized;
	}

	void InvalidateMaterialDefinitions() noexcept {
		SubMeshes.clear();
		ImportedMaterials.clear();
		MaterialImportDiagnostics.clear();
		m_materialDefinitionsNormalized = false;
	}

	bool AreMaterialDefinitionsNormalized() const noexcept {
		return m_materialDefinitionsNormalized;
	}

	std::size_t ResolvedSubMeshCount() const noexcept {
		return SubMeshes.empty() ? MeshGeometry.size() : SubMeshes.size();
	}

	const ModelSubMeshDefinition* FindSubMesh(
		ModelSubMeshID id
	) const noexcept {
		if(id == InvalidModelSubMeshID){
			return nullptr;
		}
		for(const ModelSubMeshDefinition& subMesh : SubMeshes){
			if(subMesh.id == id){
				return &subMesh;
			}
		}
		return nullptr;
	}

	const ImportedMaterialDefinition* FindImportedMaterial(
		ImportedMaterialID id
	) const noexcept {
		if(id == InvalidImportedMaterialID){
			return nullptr;
		}
		for(const ImportedMaterialDefinition& material : ImportedMaterials){
			if(material.id == id){
				return &material;
			}
		}
		return nullptr;
	}

	// Legacy通常描画互換。生成・所有は段階的にRenderSystem/RHIへ移す。
	std::vector<ID3D11Buffer*> VertexBuffer;
	std::vector<ID3D11Buffer*> IndexBuffer;
	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;

	std::vector<BONE> m_Bones;
	std::unordered_map<std::string, uint32_t> m_BoneIndexMap;
	bool enableRootMotion = false;

	std::unordered_map<std::string, AnimationData> m_Animation;
	std::vector<DEFORM_VERTEX>* m_DeformVertex = nullptr;

	std::vector<ID3D11Buffer*> m_SkinInputBuffer;
	std::vector<ID3D11ShaderResourceView*> m_SkinInputSRV;
	std::vector<ID3D11Buffer*> m_SkinOutputUAVBuffer;
	std::vector<ID3D11UnorderedAccessView*> m_SkinOutputUAV;
	std::vector<ID3D11Buffer*> m_SkinOutputVB;

	ID3D11Buffer* m_BoneCB = nullptr;
	ID3D11Buffer* m_InfoCB = nullptr;

	void CreateSkinningBuffers(GraphicsContext* ctx);
	void UpdateAndDispatchSkinning(
		GraphicsContext* ctx,
		std::vector<ID3D11Buffer*>& dynamicVertexBuffers
	);

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Parent);

	void LoadAnimation(const char* FileName, const char* Name);

	bool HasImportedAnimationSource(const std::string& filePath) const {
		for(const auto& [name, animation] : m_Animation){
			(void)name;
			if(animation.isImported && animation.FilePath == filePath){
				return true;
			}
		}
		return false;
	}

	// 共有ModelDataへ同じSource Pathを重複Importしない公開入口。
	void LoadAnimationSource(const char* fileName, const char* name){
		if(!fileName || !name || fileName[0] == '\0' || name[0] == '\0'){
			return;
		}
		if(HasImportedAnimationSource(fileName)){
			return;
		}
		LoadAnimation(fileName, name);
	}

	void RemoveAnimation(const std::string& name){
		auto it = m_Animation.find(name);
		if(it != m_Animation.end()){
			it->second.Release();
			m_Animation.erase(it);
		}
	}

	void UpdateBoneAnimation(
		const std::vector<AnimationBlend>& anims,
		float frame
	);

	void CPU_Skinning(
		const std::vector<DEFORM_VERTEX>& deformVertices,
		const aiMesh* mesh,
		VERTEX_3D* outVertex
	) const;

private:
	std::uint64_t m_geometryRevision = 1;
	bool m_materialDefinitionsNormalized = false;
};