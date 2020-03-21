#pragma once

struct DXDescriptorHeap;
struct DXDescriptorHandles
{
	DXDescriptorHeap* owningHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	uint32_t heapIndex = eastl::numeric_limits<uint32_t>::max();
};

struct DXDescriptorHeap
{
	ID3D12Device* device = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
	uint32_t heapSize = 0;
	uint32_t heapIndex = 0;
	uint32_t handleIncrement = 0;
	eastl::vector<uint32_t> freelist;

	DXDescriptorHeap() = default;
	DXDescriptorHeap(ID3D12Device* device_, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t size);

	inline ID3D12DescriptorHeap* Get()
	{
		return heap.Get();
	}
	
	inline ID3D12DescriptorHeap* const* GetAddressOf()
	{
		return heap.GetAddressOf();
	}

	DXDescriptorHandles GetNextHandles(uint32_t overrideIndex);

	DXDescriptorHandles CreateSRV(ID3D12Resource* const resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc,
		uint32_t overrideIndex = eastl::numeric_limits<uint32_t>::max());
	DXDescriptorHandles CreateUAV(ID3D12Resource* const resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc,
		uint32_t overrideIndex = eastl::numeric_limits<uint32_t>::max());

};