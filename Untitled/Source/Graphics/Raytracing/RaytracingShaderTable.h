#pragma once

#include "Graphics/DX/DXBuffer.h"
#include "Graphics/Raytracing/RaytracingShader.h"

class RaytracingShaderTable
{
public:
	RaytracingShaderTable() = default;
	RaytracingShaderTable(ResourceAllocator* const allocator, ID3D12StateObjectProperties* pipelineProps,
		uint32_t shaderRecordSize, uint32_t shaderRecordCount, const wchar_t* identifier);
	~RaytracingShaderTable();

	inline const uint32_t GetCurrentTableOffset() { return recordCount; }
	inline const uint32_t GetTotalSizeInBytes() { return alignedRecordSize * recordCount;  }
	inline const uint32_t GetAlignedRecordSize() { return alignedRecordSize; }
	inline const D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() { return gpuBuffer.GetGPUAddress(); }
	uint32_t InsertShaderRecord(const void* identifier, const void* rootArguments, uint32_t rootArgumentsSize);
	uint32_t InsertEmptyShaderRecord(const void* identifier);
	void Reset();

private:
	ID3D12StateObjectProperties* pipelineProps;
	DXUploadBuffer gpuBuffer;
	uint8_t* mappedBuffer;
	uint32_t alignedRecordSize;
	uint32_t recordCount;
};

