#include "PCH.h"
#include "DXDescriptorHeap.h"

#include "Core/Logging.h"
#include "Graphics/DX/DXCommon.h"

DXDescriptorHeap::DXDescriptorHeap(ID3D12Device* device_, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t size) :
	device(device_),
	heapSize(size)
{
		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc {
			.Type = type,
			.NumDescriptors = size,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = 0
		};
		DXCHECK(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&heap)));
		DXUtils::SetName(heap.Get(), L"Descriptor Heap");

		handleIncrement = device->GetDescriptorHandleIncrementSize(type);
}

DXDescriptorHandles DXDescriptorHeap::GetNextHandles(uint32_t overrideIndex)
{
	auto cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
	auto gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();

	uint32_t index = eastl::numeric_limits<uint32_t>::max();
	if (overrideIndex != eastl::numeric_limits<uint32_t>::max())
	{
		UNTITLED_ASSERT(overrideIndex < heapSize && "Override index outside of heap range!");
		index = overrideIndex;
	}
	else
	{
		if (freelist.empty())
		{
			index = heapIndex++;
			UNTITLED_ASSERT(index < heapSize && "Descriptor heap out of indices!");
		}
		else
		{
			index = freelist.back();
			freelist.pop_back();
		}
	}

	cpuHandle.ptr += (static_cast<uint64_t>(index) * handleIncrement);
	gpuHandle.ptr += (static_cast<uint64_t>(index) * handleIncrement);

	return DXDescriptorHandles {
		.owningHeap = this,
		.cpuHandle = cpuHandle,
		.gpuHandle = gpuHandle,
		.heapIndex = index
	};
}


DXDescriptorHandles DXDescriptorHeap::CreateSRV(ID3D12Resource* const resource, 
	const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, uint32_t overrideIndex /*= eastl::numeric_limits<uint32_t>::max()*/)
{
	auto handles = GetNextHandles(overrideIndex);

	// Create the SRV at the location specified by the handles
	device->CreateShaderResourceView(resource, desc, handles.cpuHandle);

	return handles;
}

DXDescriptorHandles DXDescriptorHeap::CreateUAV(ID3D12Resource* const resource, 
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc,  uint32_t overrideIndex /*= eastl::numeric_limits<uint32_t>::max()*/)
{
	auto handles = GetNextHandles(overrideIndex);

	// Create the UAV at the location specified by the handles
	device->CreateUnorderedAccessView(resource, nullptr, desc, handles.cpuHandle);

	return handles;
}