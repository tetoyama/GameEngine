// =======================================================================
// 
// transformComponent.h
// 
// =======================================================================
#pragma once
#include "Interface/IComponent.h"
#include "Backends/myVector3.h"
#include "Backends/YAMLConverters.h"
#include "Backends/ImGuiFunc.h"
#include "Registry/componentRegistry.h"
#include "Graphics/mainRenderer.h"
#include "Scene/scene.h"

#include "DebugTools/ImGuiSystem.h"
#include "Scene/sceneManager.h"
#include "System/Render/RenderSystem/RenderPass/RenderPassContext.h"

#include "2DspriteRendererComponent.h"

#include <DirectXMath.h>

// エンティティの位置・回転・スケールを管理するコンポーネント
class TransformComponent: public IComponent {
private:
	Vector3 rotationEuler; // インスペクタ表示や加算操作に使うオイラー角キャッシュ
	DirectX::XMFLOAT4 rotation = {0, 0, 0, 1}; // クォータニオン

public:
	Vector3 position = Vector3(0, 0, 0); // ローカル位置
	Vector3 scale = Vector3(1, 1, 1);    // ローカルスケール
	Entity parent = 0;                   // 親エンティティ ID（0 は親なし）
	std::vector<Entity> children; // 子エンティティのリスト（シリアライズ対象外・Scene が再構築する）
	// rotation を XMVECTOR として取得
	DirectX::XMVECTOR rotationVector() const{
		return DirectX::XMLoadFloat4(&rotation);
	}

	void SetRotationX(float x){
		Vector3 euler = GetRotationEuler();
		euler.x = x;
		SetRotationEuler(euler);
	}

	void SetRotationY(float y){
		Vector3 euler = GetRotationEuler();
		euler.y = y;
		SetRotationEuler(euler);
	}

	void SetRotationZ(float z){
		Vector3 euler = GetRotationEuler();
		euler.z = z;
		SetRotationEuler(euler);
	}

	void AddRotationX(float x){
		Vector3 euler = GetRotationEuler();
		euler.x += x;
		SetRotationEuler(euler);
	}

	void AddRotationY(float y){
		Vector3 euler = GetRotationEuler();
		euler.y += y;
		SetRotationEuler(euler);
	}

	void AddRotationZ(float z){
		Vector3 euler = GetRotationEuler();
		euler.z += z;
		SetRotationEuler(euler);
	}

	// オイラー角 (ラジアン) から回転を設定する
	void SetRotationEuler(const Vector3& euler){
		rotationEuler = euler;
		DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(
			euler.x, // Pitch (X軸)
			euler.y, // Yaw   (Y軸)
			euler.z  // Roll  (Z軸)
		);
		DirectX::XMStoreFloat4(&rotation, q);
	}

	void SetRotation(const DirectX::XMFLOAT4 q){
		rotation = q;
		rotationEuler = GetRotationEuler();
	}

	const DirectX::XMFLOAT4& GetRotation() const{
		return rotation;
	}

	// オイラー角 (ラジアン) を取得する
	// 内部でクォータニオン→オイラー角変換を行う（詳細は実装内コメント参照）
	Vector3 GetRotationEuler() const{
		DirectX::XMVECTOR q = rotationVector();
		DirectX::XMFLOAT4 qf;
		DirectX::XMStoreFloat4(&qf, q);

		// クォータニオン → オイラー角変換（XYZ 軸順）
		float ysqr = qf.y * qf.y;

		// Roll（X 軸回転）の計算
		float t0 = +2.0f * (qf.w * qf.x + qf.y * qf.z);
		float t1 = +1.0f - 2.0f * (qf.x * qf.x + ysqr);
		float roll = atan2f(t0, t1);

		// Pitch（Y 軸回転）の計算（ジンバルロック回避のためクランプを適用）
		float t2 = +2.0f * (qf.w * qf.y - qf.z * qf.x);
		t2 = (t2 > 1.0f) ? 1.0f : t2;    // 数値誤差による asinf の定義域外を防ぐ
		t2 = (t2 < -1.0f) ? -1.0f : t2;
		float pitch = asinf(t2);

		// Yaw（Z 軸回転）の計算
		float t3 = +2.0f * (qf.w * qf.z + qf.x * qf.y);
		float t4 = +1.0f - 2.0f * (ysqr + qf.z * qf.z);
		float yaw = atan2f(t3, t4);

		return Vector3(roll, pitch, yaw);
	}
	YAML::Node encode() override{
		YAML::Node node;
		node["Position"] = position;
		node["Rotation"] = std::vector<float>{rotation.x, rotation.y, rotation.z, rotation.w};
		node["Scale"] = scale;
		node["Parent"] = (int)parent;
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override{
		if(!node.IsMap()){
			return false;
		}

		if(node["Position"]){
			position = node["Position"].as<Vector3>();
		}
		if(node["Rotation"] && node["Rotation"].IsSequence() && node["Rotation"].size() == 4){
			auto q = node["Rotation"].as<std::vector<float>>();
			rotation = {q[0], q[1], q[2], q[3]};
			rotationEuler = GetRotationEuler();
		}
		if(node["Scale"]){
			scale = node["Scale"].as<Vector3>();
		}
		if(node["Parent"]){
			parent = Entity(node["Parent"].as<int>());
		}
		return true;
	}

	void inspector(SceneContext* context) override{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

		// スケールを一様倍率として扱うかどうかを UI 状態として保持する
		static bool isUniformLocked = false;

		// ----------- Position -----------
		ImGui::UndoDragVec3("Position", position);
		// ----------- Rotation (Euler UI) -----------
		{
			if(ImGui::UndoDragVec3("Rotation", rotationEuler)){
				DirectX::XMVECTOR qNew = DirectX::XMQuaternionRotationRollPitchYaw(
					rotationEuler.x, rotationEuler.y, rotationEuler.z
				);
				DirectX::XMStoreFloat4(&rotation, qNew);
			}

		}


		// ----------- Scale -----------
		ImGui::UndoCheckbox("##isUniformLocked", &isUniformLocked);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("isUniformLocked?");
		ImGui::SameLine();
		// 変更前のスケールを保存
		Vector3 oldScale = scale;

		// UndoDragVec3 を呼ぶ
		bool changed = ImGui::UndoDragVec3("Scale", scale);

		if(isUniformLocked && changed){
			// 比率を維持
			Vector3 ratio;
			ratio.x = oldScale.x != 0.0f ? scale.x / oldScale.x : 1.0f;
			ratio.y = oldScale.y != 0.0f ? scale.y / oldScale.y : 1.0f;
			ratio.z = oldScale.z != 0.0f ? scale.z / oldScale.z : 1.0f;

			// どの軸が変更されたかを判定（X軸優先など任意で決める）
			if(scale.x != oldScale.x){
				scale.y = oldScale.y * ratio.x;
				scale.z = oldScale.z * ratio.x;
			} else if(scale.y != oldScale.y){
				scale.x = oldScale.x * ratio.y;
				scale.z = oldScale.z * ratio.y;
			} else if(scale.z != oldScale.z){
				scale.x = oldScale.x * ratio.z;
				scale.y = oldScale.y * ratio.z;
			}
		}


		ImGui::InputInt("Parent Entity", (int*)&parent);

		// 子エンティティを読み取り専用で表示
		if (!children.empty()) {
			ImGui::Text("Children (%d):", (int)children.size());
			ImGui::Indent();
			for (Entity child : children) {
				ImGui::Text("Entity %u", child);
			}
			ImGui::Unindent();
		} else {
			ImGui::Text("Children: none");
		}

		ImGui::PopStyleVar(); // ItemSpacing
	}

	// 前方向ベクトル（ローカル Z+ 軸をクォータニオンで回転した結果）
	Vector3 front() const{
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotation);
		DirectX::XMVECTOR f = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 0, 1, 0), q);
		DirectX::XMFLOAT3 out;
		XMStoreFloat3(&out, f);
		return Vector3(out.x, out.y, out.z);
	}

	// 上方向ベクトル（ローカル Y+ 軸をクォータニオンで回転した結果）
	Vector3 up() const{
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotation);
		DirectX::XMVECTOR u = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 1, 0, 0), q);
		DirectX::XMFLOAT3 out;
		XMStoreFloat3(&out, u);
		return Vector3(out.x, out.y, out.z);
	}

	// 右方向ベクトル（ローカル X+ 軸をクォータニオンで回転した結果）
	Vector3 right() const{
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotation);
		DirectX::XMVECTOR r = DirectX::XMVector3Rotate(DirectX::XMVectorSet(1, 0, 0, 0), q);
		DirectX::XMFLOAT3 out;
		XMStoreFloat3(&out, r);
		return Vector3(out.x, out.y, out.z);
	}

	// ワールド変換行列を再帰的に計算する
	// ローカル行列（SRT: Scale * Rotate * Translate）を親の変換と合成する
	// 親が存在する場合は再帰的に親のワールド行列を計算して乗算する
	DirectX::XMMATRIX CalculateWorldMatrix(const TransformComponent* transform,ComponentRegistry* componentregistry) const{

		// ローカル変換行列: Scale → Rotation → Translation の順に合成
		DirectX::XMMATRIX local =
			DirectX::XMMatrixScaling(transform->scale.x, transform->scale.y, transform->scale.z) *
			DirectX::XMMatrixRotationQuaternion(XMLoadFloat4(&transform->rotation)) *
			DirectX::XMMatrixTranslation(transform->position.x, transform->position.y, transform->position.z);

		if(transform->parent != 0){
			auto* parentTransform = componentregistry->GetComponent<TransformComponent>(transform->parent);
			// 循環参照を防ぐため、親の parent が自分自身でないことを確認してから再帰
			if(parentTransform && parentTransform->parent != transform->parent){
				return local * CalculateWorldMatrix(parentTransform, componentregistry);
			}
		}
		return local;
	}

	// ワールド座標系での位置を取得する
	// ワールド変換行列の平行移動成分（第4行）を抽出して返す
	Vector3 GetWorldPosition(ComponentRegistry* componentregistry) const{
		// 自分自身を起点に WorldMatrix を計算
		DirectX::XMMATRIX world = CalculateWorldMatrix(
			this,
			componentregistry
		);

		// 行列の第4行（_41, _42, _43）が平行移動成分
		DirectX::XMFLOAT4X4 m;
		DirectX::XMStoreFloat4x4(&m, world);

		return Vector3(m._41, m._42, m._43);
	}

	// 2D UI 用の矩形トランスフォームを計算する
	// 仮想 UI 座標（参照解像度 1×1 基準）をビューポートのピクセル座標に変換する
	// 引数:
	//   viewportSize      - 現在のビューポートサイズ（ピクセル）
	//   sprite            - アンカーとピボット情報を持つスプライトコンポーネント
	//   originalTransform - 仮想 UI 空間での元のトランスフォーム
	// 戻り値: ピクセル空間に変換されたトランスフォーム
	TransformComponent CalculateRectTransform(
		const Vector2& viewportSize,
		const SpriteRendererComponent& sprite,
		const TransformComponent& originalTransform
	){
		TransformComponent adjustedTransform = originalTransform;

		Vector2 screenSize = viewportSize;

		// 仮想UI基準解像度（正規化座標 0〜1 を使用）
		const Vector2 referenceResolution = {1.0f, 1.0f};

		// アスペクト比補正係数（横方向のみ補正してアスペクト比を維持）
		float screenAspect = screenSize.x / screenSize.y;
		float referenceAspect = referenceResolution.x / referenceResolution.y;
		float aspectRatioScaleX = referenceAspect / screenAspect;

		// アンカー位置（画面サイズ基準のピクセル座標）
		Vector2 anchoredPosition = {
			viewportSize.x * sprite.anchor.x,
			viewportSize.y * sprite.anchor.y
		};

		// スプライトのピクセルサイズを仮想スケールから計算（アスペクト比補正込み）
		Vector2 adjustedScale = {
			originalTransform.scale.x * aspectRatioScaleX / referenceResolution.x * viewportSize.x,
			originalTransform.scale.y / referenceResolution.y * viewportSize.y
		};

		// ピボット補正（スプライト原点を pivot 位置にオフセット）
		Vector2 pivotOffset = {
			adjustedScale.x * -sprite.pivot.x,
			adjustedScale.y * -sprite.pivot.y
		};

		// 仮想UI座標 → ピクセル座標への変換（アスペクト比補正付き）
		Vector2 positionOffset = {
			originalTransform.position.x * aspectRatioScaleX / referenceResolution.x * viewportSize.x,
			originalTransform.position.y / referenceResolution.y * viewportSize.y
		};

		// 最終ピクセル位置 = アンカー位置 + ピボット補正 + ポジションオフセット
		Vector2 finalPosition = {
			anchoredPosition.x - pivotOffset.x + positionOffset.x,
			anchoredPosition.y - pivotOffset.y + positionOffset.y
		};

		adjustedTransform.position = Vector3(finalPosition.x, finalPosition.y, originalTransform.position.z);
		adjustedTransform.scale = Vector3(adjustedScale.x, adjustedScale.y, originalTransform.scale.z);

		return adjustedTransform;
	}

	// CalculateRectTransform の逆変換
	// ピクセル座標を仮想 UI 座標（参照解像度 1×1 基準）に戻す
	// エディターでのドラッグ操作後に仮想座標を更新する際に使用
	// 引数:
	//   viewportSize      - 現在のビューポートサイズ（ピクセル）
	//   sprite            - アンカーとピボット情報を持つスプライトコンポーネント
	//   adjustedTransform - ピクセル空間でのトランスフォーム
	// 戻り値: 仮想 UI 空間に逆変換されたトランスフォーム
	TransformComponent ReverseCalculateRectTransform(
		const Vector2& viewportSize,
		const SpriteRendererComponent& sprite,
		const TransformComponent& adjustedTransform
	) {
		TransformComponent virtualTransform = adjustedTransform;

		const Vector2 referenceResolution = { 1.0f, 1.0f };

		// アスペクト比補正係数（CalculateRectTransform と同じ計算）
		float screenAspect = viewportSize.x / viewportSize.y;
		float referenceAspect = referenceResolution.x / referenceResolution.y;
		float aspectRatioScaleX = referenceAspect / screenAspect;

		// アンカー位置（ピクセル）
		Vector2 anchoredPosition = {
			viewportSize.x * sprite.anchor.x,
			viewportSize.y * sprite.anchor.y
		};

		// 実スケール → 仮想スケールへの逆変換
		Vector2 virtualScale = {
			adjustedTransform.scale.x / viewportSize.x * referenceResolution.x / aspectRatioScaleX,
			adjustedTransform.scale.y / viewportSize.y * referenceResolution.y
		};

		// ピボット補正（ピクセル単位）
		Vector2 pivotOffset = {
			adjustedTransform.scale.x * -sprite.pivot.x,
			adjustedTransform.scale.y * -sprite.pivot.y
		};

		// 実座標からアンカー基準の差分を計算（ピクセル空間）
		Vector2 anchoredDiff = {
			adjustedTransform.position.x - anchoredPosition.x + pivotOffset.x,
			adjustedTransform.position.y - anchoredPosition.y + pivotOffset.y
		};

		// ピクセル座標 → 仮想UI座標への逆変換（アスペクト比補正付き）
		Vector2 virtualPosition = {
			anchoredDiff.x / (viewportSize.x) * referenceResolution.x / aspectRatioScaleX,
			anchoredDiff.y / (viewportSize.y) * referenceResolution.y
		};

		// 仮想座標に反映
		virtualTransform.position = Vector3(virtualPosition.x, virtualPosition.y, adjustedTransform.position.z);
		virtualTransform.scale = Vector3(virtualScale.x, virtualScale.y, adjustedTransform.scale.z);

		return virtualTransform;
	}

};
