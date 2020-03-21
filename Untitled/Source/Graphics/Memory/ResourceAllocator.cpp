#include "PCH.h"
#include "ResourceAllocator.h"

#include "Graphics/DX/DXCommon.h"
#include "Graphics/DX/DXUtils.h"

ResourceAllocator::ResourceAllocator(GraphicsContext& context_) :
	context(context_)
{

	D3D12MA::ALLOCATOR_DESC desc {
		.Flags = D3D12MA::ALLOCATOR_FLAGS::ALLOCATOR_FLAG_NONE,
		.pDevice = context.device.Get(),
		.PreferredBlockSize = 0, // 256 MB default
		.pAllocationCallbacks = nullptr,
		.pAdapter = context.adapter.Get()
	};
	DXCHECK(D3D12MA::CreateAllocator(&desc, &allocator));
}

ResourceAllocator::~ResourceAllocator()
{
	ReleaseStagingBuffers();
	allocator->Release();
}

DXTexture2D ResourceAllocator::CreateTexture2D(const D3D12_RESOURCE_DESC* resourceDesc, D3D12_RESOURCE_STATES initResourceState,
	const D3D12_CLEAR_VALUE* clearValue)
{
	DXTexture2D texture {};
	D3D12MA::ALLOCATION_DESC allocDesc {
	.Flags = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED,
	.HeapType = D3D12_HEAP_TYPE_DEFAULT
	};
	DXCHECK(allocator->CreateResource(&allocDesc, resourceDesc, initResourceState, clearValue, 
		&texture.allocation, __uuidof(ID3D12Resource), nullptr));
	DXUtils::SetName(texture.GetResource(), L"Texture2D");

	return texture;
}

DXUploadBuffer ResourceAllocator::CreateUploadBuffer(const D3D12_RESOURCE_DESC* resourceDesc, const uint32_t numInstances /*= 1*/)
{
	DXUploadBuffer buffer { 
		DXBuffer {
			.sizeInBytes = resourceDesc->Width,
			.numInstances = numInstances
		}
	};

	D3D12MA::ALLOCATION_DESC allocDesc {
		.Flags = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_NONE,
		.HeapType = D3D12_HEAP_TYPE_UPLOAD
	};
	DXCHECK(allocator->CreateResource(&allocDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		&buffer.allocation, __uuidof(ID3D12Resource), nullptr));
	DXUtils::SetName(buffer.GetResource(), L"UploadBuffer");

	return buffer;
}

DXUploadBuffer ResourceAllocator::CreateUploadBufferWithData(const D3D12_RESOURCE_DESC* resourceDesc, const void* data, 
	uint32_t numInstances /*= 1*/)
{
	DXUploadBuffer buffer = CreateUploadBuffer(resourceDesc, numInstances);
	buffer.SetData(data);

	return buffer;
}

DXReadBackBuffer ResourceAllocator::CreateReadBackBuffer(const D3D12_RESOURCE_DESC* resourceDesc, const uint32_t numInstances /*= 1*/)
{
	D3D12_RESOURCE_DESC outputBufferDesc = DXUtils::ResourceDescBuffer(resourceDesc->Width, 
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	DXReadBackBuffer buffer {
		DXBuffer {
			.sizeInBytes = resourceDesc->Width,
			.numInstances = numInstances
		}
	};

	buffer.outputBuffer = CreateDeviceLocalBuffer(&outputBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, numInstances);

	D3D12MA::ALLOCATION_DESC allocDesc {
		.Flags = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_NONE,
		.HeapType = D3D12_HEAP_TYPE_READBACK
	};
	DXCHECK(allocator->CreateResource(&allocDesc, resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		&buffer.allocation, __uuidof(ID3D12Resource), nullptr));
	DXUtils::SetName(buffer.GetResource(), L"ReadBackBuffer");

	return buffer;
}

DXDeviceLocalBuffer ResourceAllocator::CreateDeviceLocalBuffer(const D3D12_RESOURCE_DESC* resourceDesc,
	D3D12_RESOURCE_STATES initResourceState, const uint32_t numInstances /*= 1*/)
{
	DXDeviceLocalBuffer buffer {
		DXBuffer {
			.sizeInBytes = resourceDesc->Width,
			.numInstances = numInstances
		}
	};

	D3D12MA::ALLOCATION_DESC allocDesc {
	.Flags = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED,
	.HeapType = D3D12_HEAP_TYPE_DEFAULT
	};
	DXCHECK(allocator->CreateResource(&allocDesc, resourceDesc, initResourceState, nullptr,
		&buffer.allocation, __uuidof(ID3D12Resource), nullptr));
	DXUtils::SetName(buffer.GetResource(), L"DeviceLocal Buffer");

	return buffer;
}

DXDeviceLocalBuffer ResourceAllocator::CreateDeviceLocalBufferWithData(const D3D12_RESOURCE_DESC* resourceDesc, 
	D3D12_RESOURCE_STATES initResourceState, const void* data, uint32_t numInstances /*= 1*/)
{
	// Create the device local buffer
	DXDeviceLocalBuffer buffer = CreateDeviceLocalBuffer(resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, numInstances);

	// Prepare staging buffer
	auto bufferDesc = DXUtils::ResourceDescBuffer(resourceDesc->Width);
	stagingBuffers.push_back(CreateUploadBufferWithData(&bufferDesc, data));

	// Record copy command and barrier
	context.copyCommands->CopyBufferRegion(buffer.allocation->GetResource(), 0,
		stagingBuffers.back().GetResource(), 0, resourceDesc->Width);
	auto barrier = DXUtils::ResourceBarrierTransition(buffer.GetResource(),
		D3D12_RESOURCE_STATE_COPY_DEST, initResourceState);
	context.graphicsCommands->ResourceBarrier(1, &barrier);
	
	return buffer;
}

void ResourceAllocator::ReleaseStagingBuffers()
{
	for (auto& buf : stagingBuffers)
	{
		buf.allocation->Release();
	}
	stagingBuffers.reset_lose_memory();
}
