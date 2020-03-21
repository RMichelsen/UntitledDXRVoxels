#include "PCH.h"
#include "RaytracingShaderTable.h"

#include "Graphics/DX/DXUtils.h"
#include "Graphics/Memory/ResourceAllocator.h"

RaytracingShaderTable::RaytracingShaderTable(ResourceAllocator* const allocator, ID3D12StateObjectProperties* pipelineProps_,
	uint32_t shaderRecordSize, uint32_t shaderRecordCount, const wchar_t* identifier) :
	recordCount(0),
	pipelineProps(pipelineProps_)
{
	// Allocate the shader table with the appropriate dimensions
	if (shaderRecordSize == 0)
	{
		alignedRecordSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
	}
	else
	{
		alignedRecordSize = DXUtils::RoundUp(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	}
	uint32_t alignedTableSize = DXUtils::RoundUp(alignedRecordSize * shaderRecordCount, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	auto bufferDesc = DXUtils::ResourceDescBuffer(static_cast<uint64_t>(alignedTableSize));
	gpuBuffer = allocator->CreateUploadBuffer(&bufferDesc);
	D3D12_RANGE range {
		.Begin = 0,
		.End = 0
	};
	DXCHECK(gpuBuffer.GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&mappedBuffer)));
}

RaytracingShaderTable::~RaytracingShaderTable()
{
	D3D12_RANGE range {
		.Begin = 0,
		.End = 0
	};
	gpuBuffer.GetResource()->Unmap(0, nullptr);
	gpuBuffer.allocation->Release();
}

// Returns the InstanceContributionToHitGroupIndex of this shader record
uint32_t RaytracingShaderTable::InsertShaderRecord(const void* identifier,
	const void* rootArguments, uint32_t rootArgumentsSize)
{
	memcpy(mappedBuffer, identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	memcpy(mappedBuffer + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, rootArguments, rootArgumentsSize);
	mappedBuffer += alignedRecordSize;

	return recordCount++;
}

// Returns the InstanceContributionToHitGroupIndex of this shader record
uint32_t RaytracingShaderTable::InsertEmptyShaderRecord(const void* identifier)
{
	memcpy(mappedBuffer, identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	mappedBuffer += alignedRecordSize;

	return recordCount++;
}

void RaytracingShaderTable::Reset()
{
	// Remap the shader table, effectively resetting the mapped pointer
	gpuBuffer.GetResource()->Unmap(0, nullptr);
	DXCHECK(gpuBuffer.GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&mappedBuffer)));

	recordCount = 0;
}
