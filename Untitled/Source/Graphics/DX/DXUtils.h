#pragma once

#include "Graphics/DX/DXCommon.h"

namespace DXUtils
{
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(ID3D12DescriptorHeap* heap, uint64_t index, uint64_t heapIncrementSize)
	{
		auto handle = heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += (index * heapIncrementSize);
		return handle;
	}

	constexpr inline D3D12_HEAP_PROPERTIES D3D12HeapProperties(D3D12_HEAP_TYPE type)
	{
		return {
			.Type = type,
			.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask = 1,
			.VisibleNodeMask = 1
		};
	}

	constexpr inline D3D12_RESOURCE_DESC ResourceDescTexture2D(DXGI_FORMAT format, uint64_t width, uint32_t height, 
		uint32_t samples = 1, uint16_t mipLevels = 1, uint16_t arraySize = 1)
	{
		return {
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment = 0,
			.Width = width,
			.Height = height,
			.DepthOrArraySize = arraySize,
			.MipLevels = mipLevels,
			.Format = format,
			.SampleDesc = { samples, 0 },
			.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags = D3D12_RESOURCE_FLAG_NONE
		};
	}

	constexpr inline D3D12_RESOURCE_DESC ResourceDescBuffer(uint64_t width, 
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
	{
		return {
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = 0,
			.Width = width,
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc = { 1, 0 },
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = flags
		};
	}

	constexpr inline D3D12_RESOURCE_BARRIER ResourceBarrierTransition(ID3D12Resource* resource, 
		D3D12_RESOURCE_STATES srcState, D3D12_RESOURCE_STATES dstState)
	{
		return {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition {
				.pResource = resource,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = srcState,
				.StateAfter = dstState
			}
		};
	}

	constexpr inline D3D12_ROOT_PARAMETER CreateRootParameter(D3D12_ROOT_DESCRIPTOR_TABLE table)
	{
		return D3D12_ROOT_PARAMETER {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = table,
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};
	}

	constexpr inline D3D12_ROOT_PARAMETER CreateRootParameter(D3D12_ROOT_PARAMETER_TYPE type, 
		uint32_t shaderRegister, uint32_t space)
	{
		return D3D12_ROOT_PARAMETER {
			.ParameterType = type,
			.Descriptor = {
				.ShaderRegister = shaderRegister,
				.RegisterSpace = space
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};
	}

	constexpr inline D3D12_ROOT_PARAMETER CreateRootConstantsParameter(D3D12_ROOT_PARAMETER_TYPE type,
		uint32_t num32Bitvalues, uint32_t shaderRegister, uint32_t space)
	{
		return D3D12_ROOT_PARAMETER {
			.ParameterType = type,
			.Constants = {
				.ShaderRegister = shaderRegister,
				.RegisterSpace = space,
				.Num32BitValues = num32Bitvalues
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
		};
	}

	constexpr inline D3D12_DESCRIPTOR_RANGE CreateDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE type,
		uint32_t count, uint32_t baseShaderRegister, uint32_t space)
	{
		return D3D12_DESCRIPTOR_RANGE {
			.RangeType = type,
			.NumDescriptors = count,
			.BaseShaderRegister = baseShaderRegister,
			.RegisterSpace = space,
			.OffsetInDescriptorsFromTableStart = D3D12_APPEND_ALIGNED_ELEMENT
		};
	}

	constexpr inline uint32_t RoundUp(uint32_t size, uint32_t alignment)
	{
		return (size + (alignment - 1)) & ~(alignment - 1);
	}

	inline void ResetGraphicsCommandList(const GraphicsContext& context)
	{
		context.graphicsCommands->Reset(context.graphicsCommandAllocators[context.backBufferIndex].Get(), nullptr);
	}

	inline void ResetCopyCommandList(const GraphicsContext& context)
	{
		context.copyCommands->Reset(context.copyCommandAllocators[context.backBufferIndex].Get(), nullptr);
	}

	inline void CreateRootSignature(const GraphicsContext& context, const eastl::fixed_vector<D3D12_ROOT_PARAMETER, 64>&& params,
		ID3D12RootSignature** pRootSignature, bool isGlobalRootSignature /*= false*/)
	{
		// Calculate the shader record size
		auto shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		for (const auto& param : params)
		{
			// Constants cost one DWORD per constant
			if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
			{
				shaderRecordSize += sizeof(uint32_t) * param.Constants.Num32BitValues;
			}
			// Descriptor tables cost one DWORD total
			else if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				shaderRecordSize += sizeof(uint32_t);
			}
			// Raw/structured buffer SRV/UAV and CBVs cost two DWORDs
			else
			{
				shaderRecordSize += sizeof(uint64_t);
			}
		}
		shaderRecordSize = DXUtils::RoundUp(shaderRecordSize,
			D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		D3D12_ROOT_SIGNATURE_DESC signatureDesc {
			.NumParameters = static_cast<uint32_t>(params.size()),
			.pParameters = params.data(),
			.NumStaticSamplers = 0,
			.pStaticSamplers = nullptr,
			.Flags = isGlobalRootSignature ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE
		};

		Microsoft::WRL::ComPtr<ID3DBlob> blob;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		DXCHECK(D3D12SerializeRootSignature(&signatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
		context.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			IID_PPV_ARGS(pRootSignature));
	}

	inline void CreateGlobalRootSignature(const GraphicsContext& context, eastl::fixed_vector<D3D12_ROOT_PARAMETER, 64>&& params,
		ID3D12RootSignature** pRootSignature)
	{
		CreateRootSignature(context, eastl::move(params), pRootSignature, true);
	}
}