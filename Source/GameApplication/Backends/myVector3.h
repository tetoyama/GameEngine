#pragma once
#include <math.h>
#include <DirectXMath.h>
class Vector3
{
public:

	union{
		float v[3];
		struct {
			float x, y, z;
		};
	};

	Vector3(){
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}
	Vector3(const Vector3& vec3):x(vec3.x), y(vec3.y), z(vec3.z){}
	Vector3(const float nx, const float ny, const float nz):x(nx), y(ny), z(nz){}
	Vector3(const DirectX::XMFLOAT3& v): x(v.x), y(v.y), z(v.z){}
	Vector3(const DirectX::XMVECTOR& v){
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
		DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(this), v);
	}
	~Vector3(){}

	Vector3& operator=(const Vector3& vec3){
		if(this != &vec3){
			x = vec3.x;
			y = vec3.y;
			z = vec3.z;
		}
		return *this;
	}
	Vector3& operator=(const DirectX::XMFLOAT3& v){
		x = v.x;
		y = v.y;
		z = v.z;
		return *this;
	}
	Vector3& operator=(const DirectX::XMVECTOR& v){
		DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(this), v);
		return *this;
	}

	bool operator==(const Vector3& vec3) const{
		return (x == vec3.x && y == vec3.y && z == vec3.z);
	}

	bool operator!=(const Vector3& vec3) const{
		return !(*this == vec3);
	}

	Vector3 operator+(const Vector3& vec3) const{
		return Vector3(x + vec3.x, y + vec3.y, z + vec3.z);
	}

	Vector3 operator-(const Vector3& vec3) const{
		return Vector3(x - vec3.x, y - vec3.y, z - vec3.z);
	}

	Vector3 operator*(const Vector3& vec3) const{
		return Vector3(x * vec3.x, y * vec3.y, z * vec3.z);
	}

	Vector3 operator/(const Vector3& vec3) const{
		if(vec3.x == 0 || vec3.y == 0 || vec3.z == 0) return Vector3(0, 0, 0); // ゼロ除算を避ける
		return Vector3(x / vec3.x, y / vec3.y, z / vec3.z);
	}

	Vector3 operator*(const float scalar) const{
		return Vector3(x * scalar, y * scalar, z * scalar);
	}

	Vector3 operator/(const float scalar) const{
		if(scalar == 0) return Vector3(0, 0, 0); // ゼロ除算を避ける
		float oneOverA = 1.0f / scalar;
		return Vector3(x * oneOverA, y * oneOverA, z * oneOverA);
	}

	Vector3& operator+=(const Vector3& vec3){
		x += vec3.x;
		y += vec3.y;
		z += vec3.z;
		return *this;
	}

	Vector3& operator-=(const Vector3& vec3){
		x -= vec3.x;
		y -= vec3.y;
		z -= vec3.z;
		return *this;
	}

	Vector3& operator*=(const Vector3& vec3){
		x *= vec3.x;
		y *= vec3.y;
		z *= vec3.z;
		return *this;
	}

	Vector3& operator/=(const Vector3& vec3){
		if(vec3.x == 0 || vec3.y == 0 || vec3.z == 0) return *this; // ゼロ除算を避ける
		x /= vec3.x;
		y /= vec3.y;
		z /= vec3.z;
		return *this;
	}

	Vector3& operator*=(const float scalar){
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

	Vector3& operator/=(const float scalar){
		if(scalar == 0) return *this; // ゼロ除算を避ける
		float oneOverA = 1.0f / scalar;
		x *= oneOverA;
		y *= oneOverA;
		z *= oneOverA;
		return *this;
	}

	void zero(){
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}

	// ベクトルの長さを求める
	float length() const{
		return sqrtf(x * x + y * y + z * z);
	}
	// ベクトルの正規化
	Vector3 normalize() const{
		float len = length();
		if(len == 0) return Vector3(0, 0, 0);
		return Vector3(x / len, y / len, z / len);
	}
	DirectX::XMVECTOR ToXMVECTOR() const{
		return DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(this));
	}

};

inline Vector3 Vec3Cross(const Vector3& a, const Vector3& b){
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}