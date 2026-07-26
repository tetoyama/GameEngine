// =======================================================================
// 
// llamaModelLoader.h
// 
// =======================================================================
#pragma once

#include "ResourceLoader.h"

#include <memory>
#include <string>
#include <tuple>

#include "../Data/llamaModelData.h"

#include <llama.h>

// ---------------------------------
// GPUオフロード層数
// ---------------------------------
// llama_model_params::n_gpu_layers はモデルのロード時にしか指定できない。
// そのためCPU/GPUの切り替えにはモデルの再ロードが必要になる。
//
// 既定は0（CPUのみ）。ゲームエンジンに埋め込む以上、
// 何もしなければVRAMを奪わない側に倒しておく。
// GPUを使いたい場合のみ明示的に層数を指定する（-1で全層）。
//
// 注意: GPUバックエンド（ggml-cuda.dll 等）が登録されていなければ、
// ここで層数を指定しても黙ってCPUへ落ちる。
// 検出状況は LLAMAService::GetBackendInfo() で確認できる。
struct LLAMAModelLoadArgs {
	int gpuLayers = 0;
};

inline std::shared_ptr<LLAMAModelData> LoadLLAMAModelFromFile(
	const std::string& filePath, int gpuLayers = 0){
	std::shared_ptr<LLAMAModelData> modelData = std::make_shared<LLAMAModelData>();
	modelData->m_path = filePath;

	OutputDebugStringA(("Loading LLAMA model: " + filePath +
		" (gpuLayers=" + std::to_string(gpuLayers) + ")\n").c_str());

	llama_model_params mParams = llama_model_default_params();
	mParams.n_gpu_layers = gpuLayers;

	// llama_model_load_from_file は C API なので例外は投げない。
	// 失敗時は単に nullptr を返すだけなので、try/catch ではなく戻り値チェックで判定する。
	modelData->m_model = llama_model_load_from_file(filePath.c_str(), mParams);
	if(!modelData->m_model){
		OutputDebugStringA(("LLAMA load failed (model is null): " + filePath + "\n").c_str());
		return nullptr;
	}

	// llama_vocab は model から取得
	modelData->m_vocab = llama_model_get_vocab(modelData->m_model);
	if(!modelData->m_vocab){
		// vocab が取れないのは異常系。モデルだけ作って vocab が null のまま
		// 後段（トークナイズ等）に渡ると未定義動作の温床になるので、ここで弾く。
		OutputDebugStringA(("LLAMA load failed (vocab is null): " + filePath + "\n").c_str());
		llama_model_free(modelData->m_model);
		modelData->m_model = nullptr;
		return nullptr;
	}

	return modelData;
}

// ResourceLoader 用の設定
template<>
inline void ResourceLoader<LLAMAModelData>::SetupLoadFunc(void* /*unused*/){
	OutputDebugStringA("SetupLoadFunc LLAMAModelData called\n");

	// ResourceLoaderは可変長引数を std::tuple へ包んだ shared_ptr を渡してくる。
	// このローダーは常に Load<LLAMAModelData>(path, gpuLayers) の形で
	// 呼ばれる契約とし、tuple<int> として受け取る。
	// （引数無しで呼ぶと tuple<> が渡り型が食い違うため、呼び出し側は必ず層数を渡すこと）
	//
	// なおキャッシュキーには引数も含まれるので、CPU用とGPU用の
	// 同一モデルは別エントリとして共存する。切り替え時に旧側を
	// Unloadしないとメモリを二重に抱えることになる点に注意。
	SetLoadFunction([](const std::string& path, std::shared_ptr<void> args){
		int gpuLayers = 0;
		if(args){
			gpuLayers = std::get<0>(*std::static_pointer_cast<std::tuple<int>>(args));
		}
		return LoadLLAMAModelFromFile(path, gpuLayers);
					});
}