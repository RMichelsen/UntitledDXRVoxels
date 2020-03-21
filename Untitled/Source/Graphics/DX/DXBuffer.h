#pragma once

#include "Graphics/DX/DXCommon.h"
#include "Graphics/DX/DXDescriptorHeap.h"
#include "Graphics/DX/DXUtils.h"

// Wrappers around a D3D12MA allocation for buffer types
struct DXBuffer
{
	uint64_t sizeInBytes;
	uint32_t numInstances;
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

	inline void CreateSRV(uint32_t stride, DXDescriptorHeap* const heap,
		uint32_t overrideIndex = eastl::numeric_limits<uint32_t>::max())
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Buffer = {
				.FirstElement = 0,
				.NumElements = static_cast<uint32_t>(sizeInBytes / stride),
				.StructureByteStride = stride,
				.Flags = D3D12_BUFFER_SRV_FLAG_NONE
			}
		};
		handles = heap->CreateSRV(GetResource(), &srvDesc, overrideIndex);
	}

	inline void CreateUAV(uint32_t stride, DXDescriptorHeap* const heap,
		uint32_t overrideIndex = eastl::numeric_limits<uint32_t>::max())
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
			.Buffer = {
				.FirstElement = 0,
				.NumElements = static_cast<uint32_t>(sizeInBytes / stride),
				.StructureByteStride = stride,
				.Flags = D3D12_BUFFER_UAV_FLAG_NONE
			}
		};
		handles = heap->CreateUAV(GetResource(), &uavDesc, overrideIndex);
	}

	inline void Release()
	{
		if (handles.owningHeap != nullptr)
		{
			handles.owningHeap->freelist.push_back(handles.heapIndex);
		}

		allocation->Release();
	}
};

struct DXUploadBuffer : public DXBuffer
{
	inline void SetData(const void* data, const uint64_t size, const uint32_t instance = 0)
	{
		uint8_t* destination;
		D3D12_RANGE range {
			.Begin = 0,
			.End = 0
		};
		DXCHECK(GetResource()->Map(0, &range, reinterpret_cast<void**>(&destination)));
		memcpy(destination + (instance * sizeInBytes), data, size);
		GetResource()->Unmap(0, &range);
	}

	inline void SetData(const void* data, const uint32_t instance = 0)
	{
		SetData(data, sizeInBytes, instance);
	}
};

struct DXDeviceLocalBuffer : public DXBuffer
{
};

struct DXReadBackBuffer : public DXBuffer
{
	DXDeviceLocalBuffer outputBuffer;
	void* mappedData;

	inline void CreateSRV(uint32_t stride, DXDescriptorHeap* const heap,
		uint32_t overrideIndex = eastl::numeric_limits<uint32_t>::max())
	{
		outputBuffer.CreateSRV(stride, heap, overrideIndex);
	}

	inline void CreateUAV(uint32_t stride, DXDescriptorHeap* const heap,
		uint32_t overrideIndex = eastl::numeric_limits<uint32_t>::max())
	{
		outputBuffer.CreateUAV(stride, heap, overrideIndex);
	}

	inline void IssueBufferReadBack(ID3D12GraphicsCommandList4* const commandList)
	{
		D3D12_RESOURCE_BARRIER barrier = DXUtils::ResourceBarrierTransition(outputBuffer.GetResource(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyResource(GetResource(), outputBuffer.GetResource());
	}

	template<typename T>
	inline T* StartBufferRead(uint32_t instance = 0)
	{
		uint32_t startOfRange = instance * static_cast<uint32_t>(sizeInBytes);
		D3D12_RANGE readBackRange {
			.Begin = startOfRange,
			.End = startOfRange + sizeInBytes
		};

		DXCHECK(GetResource()->Map(0, &readBackRange, &mappedData));
		return reinterpret_cast<T*>(mappedData);
	}

	inline void EndBufferRead()
	{
		D3D12_RANGE emptyRange {
			.Begin = 0,
			.End = 0
		};
		GetResource()->Unmap(0, &emptyRange);
		mappedData = nullptr;
	}

	inline void Release()
	{
		outputBuffer.Release();
		if (handles.owningHeap != nullptr)
		{
			handles.owningHeap->freelist.push_back(handles.heapIndex);
		}

		allocation->Release();
	}
};