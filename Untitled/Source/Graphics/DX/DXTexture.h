#pragma once

struct DXTexture2D
{
	D3D12MA::Allocation* allocation;
	DXDescriptorHandles handles;

	inline ID3D12Resource* GetResource()
	{
		return allocation->GetResource();
	}

	inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress()
	{
		return allocation->GetResource()->GetGPUVirtualAddress();
	}

	inline void CreateSRV(DXDescriptorHeap* const heap)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
		};
		handles = heap->CreateSRV(GetResource(), &srvDesc);
	}

	inline void CreateUAV(DXDescriptorHeap* const heap, uint32_t overrideIndex = eastl::numeric_limits<uint32_t>::max())
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D
		};
		handles = heap->CreateUAV(GetResource(), &uavDesc, overrideIndex);
	}
};