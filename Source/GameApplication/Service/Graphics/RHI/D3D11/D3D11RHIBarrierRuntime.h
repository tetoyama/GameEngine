#pragma once

namespace RHI {

inline bool D3D11RHICommandList::ResourceBarrier(
	std::span<const ResourceBarrierDesc> barriers
){
	if(!m_recording || !m_owner || !m_owner->m_context ||
		m_renderPassActive){
		return false;
	}

	// 先に全Barrierを検証し、途中失敗で状態だけ一部更新されることを防ぐ。
	for(const ResourceBarrierDesc& barrier : barriers){
		if(!barrier.HasExactlyOneResource()) return false;
		if(barrier.type == ResourceBarrierType::Transition &&
			barrier.after == ResourceState::Undefined){
			return false;
		}

		if(barrier.buffer){
			D3D11BufferResource* resource = m_owner->Find(barrier.buffer);
			if(!resource || barrier.subresource != AllSubresources){
				return false;
			}
			if(barrier.type == ResourceBarrierType::Transition){
				if(barrier.before != ResourceState::Undefined &&
					resource->state != barrier.before){
					return false;
				}
			}
			else if(!HasAnyFlag(
				resource->state,
				ResourceState::UnorderedAccess
			)){
				return false;
			}
			continue;
		}

		D3D11TextureResource* resource = m_owner->Find(barrier.texture);
		if(!resource || resource->subresourceStates.empty()) return false;
		if(barrier.subresource != AllSubresources &&
			barrier.subresource >= resource->subresourceStates.size()){
			return false;
		}

		auto validateState = [&](ResourceState current){
			if(barrier.type == ResourceBarrierType::Transition){
				return barrier.before == ResourceState::Undefined ||
					current == barrier.before;
			}
			return HasAnyFlag(current, ResourceState::UnorderedAccess);
		};

		if(barrier.subresource == AllSubresources){
			for(ResourceState current : resource->subresourceStates){
				if(!validateState(current)) return false;
			}
		}
		else if(!validateState(
			resource->subresourceStates[barrier.subresource]
		)){
			return false;
		}
	}

	for(const ResourceBarrierDesc& barrier : barriers){
		if(barrier.type != ResourceBarrierType::Transition){
			// D3D11は明示UAV Barrierを持たない。状態検証のみ行う。
			continue;
		}

		if(barrier.buffer){
			m_owner->Find(barrier.buffer)->state = barrier.after;
			continue;
		}

		D3D11TextureResource* resource = m_owner->Find(barrier.texture);
		if(barrier.subresource == AllSubresources){
			for(ResourceState& state : resource->subresourceStates){
				state = barrier.after;
			}
		}
		else{
			resource->subresourceStates[barrier.subresource] = barrier.after;
		}
	}

	// D3D11のNative Resource TransitionはRuntimeが暗黙管理する。
	// ここではD3D12 / Vulkanと共通の論理状態だけを更新する。
	return true;
}

} // namespace RHI
