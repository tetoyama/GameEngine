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

	void FinalizeAll(){
		for(auto& sys : m_systems){
			sys->Finalize();
		}
	}
private:
	std::vector<std::unique_ptr<ISystem>> m_systems;
};
