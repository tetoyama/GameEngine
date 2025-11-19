#pragma once
#include "commonDefine.h"

struct VERTEX_3D
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT3 Tangent;
	DirectX::XMFLOAT4 Diffuse;
	DirectX::XMFLOAT2 TexCoord;
};

struct UVMatrix {
	DirectX::XMFLOAT2 Start = {0.0f,0.0f};
	DirectX::XMFLOAT2 End = {1.0f,1.0f};
};

struct MATERIAL
{
	DirectX::XMFLOAT4	Ambient = {0.1f, 0.1f, 0.1f, 1.0f};
	DirectX::XMFLOAT4	Diffuse = {1.0f, 1.0f, 1.0f, 1.0f};
	DirectX::XMFLOAT4	Specular = {0.04f, 0.04f, 0.04f, 1.0f};
	DirectX::XMFLOAT4	Emission = {0.0f, 0.0f, 0.0f, 1.0f};
	float		Shininess = 64.0f;
	BOOL		DiffuseTextureEnable = false;
	BOOL		NormalTextureEnable = false;
	float		Dummy{};
};

struct LIGHT
{
	UINT		Enable = 0;
	UINT		LightType = 0;
	UINT		CastShadow = 0;
	UINT		Dummy = 0;

	DirectX::XMFLOAT4	Position = DirectX::XMFLOAT4(0, 1, 0, 0);
	DirectX::XMFLOAT4	Direction = DirectX::XMFLOAT4(0, -1, 0, 0);

	DirectX::XMFLOAT4	Diffuse = DirectX::XMFLOAT4(0.9f, 0.9f, 1.0f, 1);
	DirectX::XMFLOAT4	Ambient = DirectX::XMFLOAT4(0.01f, 0.01f, 0.01f, 1.0f);

	DirectX::XMMATRIX LightView = DirectX::XMMATRIX{
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	};
	DirectX::XMMATRIX LightProjection = DirectX::XMMATRIX{
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	};

	DirectX::XMFLOAT4	Param = DirectX::XMFLOAT4(100.0f, 0, 0, 0);
};

struct CAMERA
{
	DirectX::XMFLOAT4	CameraPosition;
};

struct Parameter
{
	DirectX::XMFLOAT4	Parameter;
};
