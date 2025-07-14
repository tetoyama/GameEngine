#pragma once
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
	DirectX::XMFLOAT4	Diffuse = {0.8f, 0.8f, 0.8f, 1.0f};
	DirectX::XMFLOAT4	Specular = {0.04f, 0.04f, 0.04f, 1.0f};
	DirectX::XMFLOAT4	Emission = {0.0f, 0.0f, 0.0f, 1.0f};
	float		Shininess = 64.0f;
	BOOL		DiffuseTextureEnable = false;
	BOOL		NormalTextureEnable = false;
	float		Dummy{};
};

#define LIGHT_TYPE_NONE			0
#define LIGHT_TYPE_DIRECTIONAL	1
#define LIGHT_TYPE_POINT		2
#define LIGHT_TYPE_SPOT			3

struct LIGHT
{
	BOOL		Enable = true;
	UINT		LightType = 0;
	BOOL		Dummy[2];
	DirectX::XMFLOAT4	Direction = DirectX::XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT4	Diffuse = DirectX::XMFLOAT4(0.9f, 0.9f, 1.0f, 1);
	DirectX::XMFLOAT4	Ambient = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);

	DirectX::XMFLOAT4	Position = DirectX::XMFLOAT4(0, 1, 0, 0);
	DirectX::XMFLOAT4	PointLightParam = DirectX::XMFLOAT4(100.0f, 0, 0, 0);
	DirectX::XMFLOAT4	Angle = DirectX::XMFLOAT4(DirectX::XM_PI / 180.0f * 60.0f, 0, 0, 0);

	DirectX::XMFLOAT4	SkyColor = DirectX::XMFLOAT4(0.8f, 0.8f, 1.0f, 0.1f);
	DirectX::XMFLOAT4	GroundColor = DirectX::XMFLOAT4(1.0f, 0.8f, 0.5f, 0.05f);
	DirectX::XMFLOAT4	GroundNormal = DirectX::XMFLOAT4(0, 1, 0, 0);

};

struct CAMERA
{
	DirectX::XMFLOAT4	CameraPosition;
};

struct Parameter
{
	DirectX::XMFLOAT4	Parameter;
};

#define MAX_BONES 128

struct AnimationCB {
	float animationTime;						// 現在のアニメーション時刻（秒）
	int boneCount;								// 使用中のボーン数
	float dummy[2];
	DirectX::XMMATRIX boneMatrices[MAX_BONES];	// アニメーション済みのボーン行列（モデル空間）
};
