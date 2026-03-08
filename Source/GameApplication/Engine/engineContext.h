// =======================================================================
// 
// engineContext.h
// 
// =======================================================================

#pragma once
#include <memory>
#include <windows.h>
#include <unordered_map>
#include <typeindex>
#include <vector>
#include "Service/IService.h"

// サービスの依存注入コンテナ（DI コンテナ）
// 全エンジンサービスを型をキーとして管理し、型安全な取得・登録を提供する
// サービスの生成は EngineContextBuilder、終了は Shutdown() が担う
class EngineContext{

public:
	// 全サービスを登録順の逆順でシャットダウンする
	void Shutdown();

	// 指定した型のサービスをコンテナに登録する
	// 同じ型が既に登録されている場合は上書きしない
	// T は IService を継承していなければならない
	// 登録順は Shutdown 時の逆順破棄に利用される
	template<typename T>
	void Register(std::shared_ptr<T> instance) {
		static_assert(std::is_base_of<IService, T>::value, "サービスはIServiceを継承してください");
		if (!instance) return;
		auto type = std::type_index(typeid(T));
		if (m_Services.count(type)) return;
		m_Services[type] = instance;
		m_ServiceOrder.push_back(type);
	}

	// 指定した型のサービスをコンテナから取得する
	// 登録されていない場合は nullptr を返し、デバッグ出力に警告を出す
	template<typename T>
	inline std::shared_ptr<T> Get() const{
		auto it = m_Services.find(std::type_index(typeid(T)));
		if(it != m_Services.end()){
			return std::static_pointer_cast<T>(it->second);
		}
		OutputDebugStringA("EngineContext:取得に失敗しました。\n");
		return nullptr;
	}

private:
    std::unordered_map<std::type_index, std::shared_ptr<IService>> m_Services; // 型 → サービスインスタンスのマップ
    std::vector<std::type_index> m_ServiceOrder;                                // 登録順（Shutdown 時の逆順処理に使用）
};

// EngineContext の全サービスを生成・登録してコンテキストを構築するビルダークラス
class EngineContextBuilder
{
public:
	// 全サービスを登録した EngineContext を生成して返す
	std::shared_ptr<EngineContext> Build();
};

