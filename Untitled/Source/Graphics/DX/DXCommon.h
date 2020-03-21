#pragma once

#include "Graphics/DX/DXDescriptorHeap.h"
#include "Graphics/DX/DXCommandQueueManager.h"

#ifdef _DEBUG
#ifndef DXCHECK
#define DXCHECK(x) \
	{ \
		if(FAILED(x)) { assert(false); }; \
	}
#endif
namespace DXUtils
{
	inline void SetName(ID3D12Object* object, LPCWSTR name)
	{
		object->SetName(name);
	}
}
#else
#ifndef DXCHECK
#define DXCHECK
#endif
namespace DXUtils
{
	inline void SetName(ID3D12Object* object, LPCWSTR name)
	{
		// Remove this to optimize out debug names in release
		object->SetName(name);
	}
}
#endif

constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;

class ShaderCompiler;
class ResourceAllocator;
struct GraphicsContext
{
	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

	// Core D3D12 objects
	Microsoft::WRL::ComPtr<ID3D12Device5> device;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> graphicsCommands;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> graphicsCommandAllocators[MAX_FRAMES_IN_FLIGHT];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> copyCommands;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> copyCommandAllocators[MAX_FRAMES_IN_FLIGHT];

	// Swap chain objects
	uint32_t backBufferIndex;
	Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
	Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers[MAX_FRAMES_IN_FLIGHT];

	// Command queue manager
	DXCommandQueueManager queues;

	// Descriptor heap for SRVs/UAVs
	DXDescriptorHeap descriptorHeap;

	// Descriptor heap objects
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RTVDescriptorHeap;
	uint32_t RTVDescriptorHeapIncSize;

	// Rendering objects
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	// D3D12 memory allocator
	eastl::unique_ptr<ResourceAllocator> allocator;

	// Shader compiler
	eastl::unique_ptr<ShaderCompiler> shaderCompiler;
};
