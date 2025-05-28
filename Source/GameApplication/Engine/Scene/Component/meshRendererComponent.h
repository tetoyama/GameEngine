// Engine/Scene/Component/meshRendererComponent.h
#pragma once
#include "Component.h"
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>

// Add the missing GetVertexBuffer method to the MeshData struct  
struct MeshData {  
public:  
   UINT GetIndexCount() const {  
       return indexCount;  
   }  

   ID3D11Buffer* GetVertexBuffer() const {  
       return vertexBuffer.Get();  
   }  

   ID3D11Buffer* GetIndexBuffer() const {  
       return indexBuffer.Get();  
   }  

   UINT GetVertexStride() const {  
       return vertexStride;  
   }  

   Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;  
   Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;  
   UINT indexCount = 0;  
   UINT vertexStride = 0;  
};

class MeshRendererComponent: public Component {
public:
	MeshRendererComponent() = default;
	virtual ~MeshRendererComponent() = default;

	std::shared_ptr<MeshData> mesh;
	// 必要に応じて他の描画パラメータも追加
};
