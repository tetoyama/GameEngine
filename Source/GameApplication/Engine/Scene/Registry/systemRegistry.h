// =======================================================================
// 
// systemRegistry.h
// 
// =======================================================================
#pragma once

#include <vector>
#include <memory>
#include "Interface/ISystem.h"   // ISystem の定義（Initialize, Start, Update, FixedUpdate, Draw など）

/**
 * @brief 各種 System を登録し、一括で初期化・起動・更新・描画を行うマネージャクラス
 */
class SystemRegistry {
public:
	SystemRegistry() = default;
	~SystemRegistry() = default;

	/**
	 * @brief System を登録する
	 *
	 * System は std::unique_ptr<ISystem> で管理する。外側から渡されたものは
	 * SystemRegistry が所有権を持つ。
	 */
	void RegisterSystem(std::unique_ptr<ISystem> system){
		m_systems.emplace_back(std::move(system));
	}

	/**
	 * @brief 全 System の初期化を呼び出す
	 */
	void InitializeAll(){
		for(auto& sys : m_systems){
			sys->Initialize();
		}
	}

	/**
	 * @brief 全 System の Start を呼び出す
	 */
	void StartAll(){
		for(auto& sys : m_systems){
			sys->Start();
		}
	}

	/**
	 * @brief 全 System の FixedUpdate を呼び出す
	 * @param fdt 固定タイムステップ
	 */
	void FixedUpdateAll(float fdt){
		for(auto& sys : m_systems){
			sys->FixedUpdate(fdt);
		}
	}

	/**
	 * @brief 全 System の Update を呼び出す
	 * @param dt デルタタイム
	 */
	void UpdateAll(float dt){
		for(auto& sys : m_systems){
			sys->Update(dt);
		}
	}

	/**
	 * @brief 全 System の Update を呼び出す
	 * @param dt デルタタイム
	 */
	void EditorUpdateAll(float dt){
		for(auto& sys : m_systems){
			sys->EditorUpdate(dt);
		}
	}

	/**
	 * @brief 全 System の Draw を呼び出す
	 */
	void DrawAll(){
		for(auto& sys : m_systems){
			sys->Draw();
		}
	}

	void StopAll() {
		for (auto& sys : m_systems) {
			sys->Stop();
		}
	}

	void FinalizeAll(){
		for(auto& sys : m_systems){
			sys->Finalize();
		}
	}

	template<typename T>
	T* GetSystem(){
		for(auto& sys : m_systems){
			if(auto casted = dynamic_cast<T*>(sys.get())){
				return casted;
			}
		}
		return nullptr;
	}

	std::vector<std::unique_ptr<ISystem>>& GetSystems() {
		return systems;
	}

	// -------------------------
   // YAML Encode / Decode
   // -------------------------

   // 保存
	void EncodeAll(YAML::Node& rootNode) const{
		YAML::Node systemsNode = rootNode["Systems"];

		for(const auto& sys : m_systems){

			const char* name = sys->GetSystemName();
			if(!name) continue;

			YAML::Node sysNode;
			sysNode = sys->encode();

			systemsNode[name] = sysNode;
		}

		rootNode["Systems"] = systemsNode;
	}

	// 読み込み
	void DecodeAll(const YAML::Node& rootNode){
		if(!rootNode["Systems"]) return;

		const YAML::Node systemsNode = rootNode["Systems"];

		for(auto& sys : m_systems){
			const char* name = sys->GetSystemName();
			if(!name) continue;

			const YAML::Node sysNode = systemsNode[name];
			if(!sysNode) continue;

			sys->decode(sysNode);
		}
	}
private:
	std::vector<std::unique_ptr<ISystem>> m_Systems;
};
