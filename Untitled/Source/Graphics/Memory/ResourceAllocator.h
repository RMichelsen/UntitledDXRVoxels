#pragma once

#include "Graphics/DX/DXBuffer.h"
#include "Graphics/DX/DXDescriptorHeap.h"
#include "Graphics/DX/DXTexture.h"
#include "Graphics/DX/DXUtils.h"

constexpr size_t MAX_STAGING_BUFFERS = 1024;

class ResourceAllocator
{
public:
	ResourceAllocator(GraphicsContext& context_);
	~ResourceAllocator();

	DXTexture2D CreateTexture2D(const D3D12_RESOURCE_DESC* resourceDesc, D3D12_RESOURCE_STATES initResourceState,
		const D3D12_CLEAR_VALUE* clearValue);

	DXUploadBuffer CreateUploadBuffer(const D3D12_RESOURCE_DESC* resourceDesc, uint32_t numInstances = 1);
	DXUploadBuffer CreateUploadBufferWithData(const D3D12_RESOURCE_DESC* resourceDesc, const void* data, 
		uint32_t numInstances = 1);

	DXReadBackBuffer CreateReadBackBuffer(const D3D12_RESOURCE_DESC* resourceDesc, uint32_t numInstances = 1);

	DXDeviceLocalBuffer CreateDeviceLocalBuffer(const D3D12_RESOURCE_DESC* resourceDesc, 
		D3D12_RESOURCE_STATES initResourceState, uint32_t numInstances = 1);
	DXDeviceLocalBuffer CreateDeviceLocalBufferWithData(const D3D12_RESOURCE_DESC* resourceDesc,
		D3D12_RESOURCE_STATES initResourceState, const void* data, uint32_t numInstances = 1);

	void ReleaseStagingBuffers();

private:
	GraphicsContext& context;
	D3D12MA::Allocator* allocator;

	eastl::fixed_vector<DXUploadBuffer, MAX_STAGING_BUFFERS> stagingBuffers;
};