// =======================================================================
//
// CameraPostEffectGraph.h
//
// CameraComponent定義後にincludeする実装ヘッダー。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace CameraPostEffectGraph {

inline void Invalidate(CameraComponent& camera){
	camera.postEffectGraphDirty = true;
}

inline void Rebuild(CameraComponent& camera){
	camera.cachedSortedPostEffectIndices.clear();
	camera.cachedResolvedPostEffectInputs.assign(
		camera.postEffects.size(),
		{}
	);

	for(size_t index = 0; index < camera.postEffects.size(); ++index){
		camera.cachedResolvedPostEffectInputs[index].assign(
			camera.postEffects[index].inputPins.size(),
			-1
		);
	}

	const int effectCount = static_cast<int>(camera.postEffects.size());
	const int inputNode = effectCount;
	const int outputNode = effectCount + 1;

	std::unordered_map<int, std::vector<int>> adjacency;
	std::unordered_map<int, int> indegree;
	std::unordered_set<int> nodes;

	for(const CameraPostEffectLink& link : camera.postEffectLinks){
		int startNode = link.startNode;
		int endNode = link.endNode;

		if(endNode >= 0 && endNode < effectCount){
			auto& resolvedInputs =
				camera.cachedResolvedPostEffectInputs[endNode];
			const auto& inputPins = camera.postEffects[endNode].inputPins;
			const auto pinIterator = std::find(
				inputPins.begin(),
				inputPins.end(),
				link.endAttr
			);

			if(pinIterator != inputPins.end()){
				const size_t slot = static_cast<size_t>(
					pinIterator - inputPins.begin()
				);
				resolvedInputs[slot] = startNode < 0 ? -2 : startNode;
			}
		}

		if(startNode == -1) startNode = inputNode;
		if(endNode == -2) endNode = outputNode;

		if(startNode >= 0 && startNode <= outputNode &&
			endNode >= 0 && endNode <= outputNode){
			adjacency[startNode].push_back(endNode);
			++indegree[endNode];
			nodes.insert(startNode);
			nodes.insert(endNode);
		}
	}

	std::queue<int> readyNodes;
	for(int node : nodes){
		if(!indegree.contains(node)){
			readyNodes.push(node);
		}
	}

	while(!readyNodes.empty()){
		const int node = readyNodes.front();
		readyNodes.pop();
		camera.cachedSortedPostEffectIndices.push_back(node);

		for(int nextNode : adjacency[node]){
			int& nextIndegree = indegree[nextNode];
			--nextIndegree;
			if(nextIndegree == 0){
				readyNodes.push(nextNode);
			}
		}
	}

	if(camera.cachedSortedPostEffectIndices.size() != nodes.size()){
		OutputDebugStringA("Warning: post effect graph has cycles!\n");
		for(int node : nodes){
			if(std::find(
				camera.cachedSortedPostEffectIndices.begin(),
				camera.cachedSortedPostEffectIndices.end(),
				node
			) == camera.cachedSortedPostEffectIndices.end()){
				camera.cachedSortedPostEffectIndices.push_back(node);
			}
		}
	}

	for(int& node : camera.cachedSortedPostEffectIndices){
		if(node == inputNode) node = -1;
		if(node == outputNode) node = -2;
	}

	camera.postEffectGraphDirty = false;
}

inline const std::vector<int>& GetExecutionOrder(CameraComponent& camera){
	if(camera.postEffectGraphDirty){
		Rebuild(camera);
	}
	return camera.cachedSortedPostEffectIndices;
}

inline const std::vector<int>& GetResolvedInputs(
	CameraComponent& camera,
	int effectIndex
){
	static const std::vector<int> empty;
	if(camera.postEffectGraphDirty){
		Rebuild(camera);
	}
	if(effectIndex < 0 ||
		effectIndex >= static_cast<int>(camera.cachedResolvedPostEffectInputs.size())){
		return empty;
	}
	return camera.cachedResolvedPostEffectInputs[effectIndex];
}

} // namespace CameraPostEffectGraph

inline void CameraComponent::InvalidatePostEffectGraphCache(){
	CameraPostEffectGraph::Invalidate(*this);
}

inline void CameraComponent::RebuildPostEffectGraphCache(){
	CameraPostEffectGraph::Rebuild(*this);
}

inline const std::vector<int>& CameraComponent::TopologicalSortPostEffects(){
	return CameraPostEffectGraph::GetExecutionOrder(*this);
}

inline const std::vector<int>& CameraComponent::GetResolvedPostEffectInputs(
	int effectIndex
){
	return CameraPostEffectGraph::GetResolvedInputs(*this, effectIndex);
}
