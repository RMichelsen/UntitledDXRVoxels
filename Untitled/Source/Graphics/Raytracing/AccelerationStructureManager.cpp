#include "PCH.h"
#include "AccelerationStructureManager.h"

#include "Core/Logging.h"
#include "Graphics/DX/DXCommon.h"
#include "Graphics/DX/DXUtils.h"
#include "Graphics/Memory/ResourceAllocator.h"
#include "Graphics/Raytracing/RaytracingSharedHlsl.h"

AccelerationStructureManager::AccelerationStructureManager(GraphicsContext& context_) : 
	context(context_)
{
	auto bufferDesc =  DXUtils::ResourceDescBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * MAX_NUM_TOTAL_BLAS_INSTANCES);
	BLInstanceDescriptorsGPU = context.allocator->CreateUploadBuffer(&bufferDesc);

	bufferDesc = DXUtils::ResourceDescBuffer(MAX_SCRATCHBUFFER_SIZE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	// Allocate the scratch buffer
	scratchBuffer = context.allocator->CreateDeviceLocalBuffer(&bufferDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	DXUtils::SetName(scratchBuffer.GetResource(), L"ScratchBuffer");

	// Allocate the TLAS buffer
	TLAccelerationStructure.ASBuffer = context.allocator->CreateDeviceLocalBuffer(&bufferDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	DXUtils::SetName(TLAccelerationStructure.ASBuffer.GetResource(), L"TLAS");
}

AccelerationStructureManager::~AccelerationStructureManager()
{
	BLInstanceDescriptorsGPU.Release();
	for (auto& BLAS : BLAccelerationStructures)
	{
		BLAS.ASBuffer.Release();
	}
	TLAccelerationStructure.ASBuffer.Release();
	scratchBuffer.Release();
}

BLASHandle AccelerationStructureManager::AddBLAS(eastl::vector<AccelerationStructureGeometry>&& geometries)
{
	auto handle = BLAccelerationStructures.Insert({});
	auto& BLAS = BLAccelerationStructures[handle];

	// Add the geometry
	for (auto& geometry : geometries)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc {
			.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
			.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
			.Triangles {
				.IndexFormat = DXGI_FORMAT_R32_UINT,
				.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
				.IndexCount = static_cast<uint32_t>(geometry.indices.sizeInBytes) / sizeof(uint32_t),
				.VertexCount = static_cast<uint32_t>(geometry.vertices.sizeInBytes) / sizeof(Vertex),
				.IndexBuffer = geometry.indices.GetGPUAddress(),
				.VertexBuffer {
					.StartAddress = geometry.vertices.GetGPUAddress(),
					.StrideInBytes = sizeof(Vertex)
				}
			}
		};
		BLAS.geometryDescriptions.push_back(geometryDesc);
		BLAS.geometryInstances.push_back(geometry);
	}

	// Compute the prebuild info for the BLAS
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO BLASPrebuildInfo {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS BLASInputs {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
		.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
		.NumDescs = static_cast<uint32_t>(BLAS.geometryDescriptions.size()),
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.pGeometryDescs = BLAS.geometryDescriptions.data()
	};
	context.device->GetRaytracingAccelerationStructurePrebuildInfo(&BLASInputs,
		&BLASPrebuildInfo);
	UNTITLED_ASSERT(BLASPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	requiredScratchSize = eastl::max(
		eastl::max(BLASPrebuildInfo.ScratchDataSizeInBytes, BLASPrebuildInfo.UpdateScratchDataSizeInBytes),
		requiredScratchSize);

	// Allocate the acceleration structure
	auto bufferDesc = DXUtils::ResourceDescBuffer(BLASPrebuildInfo.ResultDataMaxSizeInBytes,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	BLAS.ASBuffer = context.allocator->CreateDeviceLocalBuffer(&bufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	DXUtils::SetName(BLAS.ASBuffer.GetResource(), L"BLAS");

	BLAS.built = false;
	return handle;
}

void AccelerationStructureManager::BuildBLAS(BLASHandle handle, DXDescriptorHeap* descriptorHeap)
{
	UNTITLED_ASSERT(requiredScratchSize < MAX_SCRATCHBUFFER_SIZE && "Required scratch buffer size exceeds fixed size scratch buffer limit!");

	// Build bottom level acceleration structure
	auto& BLAS = BLAccelerationStructures[handle];
	UNTITLED_ASSERT(!BLAS.built && "BLAS is already built!");

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS BLASInputs {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
		.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
		.NumDescs = static_cast<uint32_t>(BLAS.geometryDescriptions.size()),
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.pGeometryDescs = BLAS.geometryDescriptions.data()
	};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BLASBuildDesc {
		.DestAccelerationStructureData = BLAS.ASBuffer.GetGPUAddress(),
		.Inputs = BLASInputs,
		.ScratchAccelerationStructureData = scratchBuffer.GetGPUAddress()
	};

	PIXBeginEvent(context.graphicsCommands.Get(), PIX_COLOR_DEFAULT, L"Build BLAS");
	context.graphicsCommands->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());
	context.graphicsCommands->BuildRaytracingAccelerationStructure(&BLASBuildDesc, 0, nullptr);

	// Since a single scratch resource is used to build all BLAS,
	// it is necessary to insert barriers between builds. This could
	// be optimized away by allocating multiple scratch buffers to allow
	// the GPU to schedule multiple builds simultaneously.
	D3D12_RESOURCE_BARRIER barrier {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
		.UAV {
			.pResource = BLAS.ASBuffer.GetResource()
		}
	};
	context.graphicsCommands->ResourceBarrier(1, &barrier);
	PIXEndEvent(context.graphicsCommands.Get());

	BLAS.built = true;
}

void AccelerationStructureManager::RebuildBLAS(BLASHandle handle, eastl::vector<AccelerationStructureGeometry>&& geometries, 
	DXDescriptorHeap* descriptorHeap)
{
	auto& BLAS = BLAccelerationStructures[handle];
	UNTITLED_ASSERT(BLAS.built && "Cannot rebuild a BLAS that has not been built!");

	BLAS.geometryDescriptions.clear();
	BLAS.geometryInstances.clear();

	for (auto& geometry : geometries)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc {
			.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
			.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
			.Triangles {
				.IndexFormat = DXGI_FORMAT_R32_UINT,
				.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
				.IndexCount = static_cast<uint32_t>(geometry.indices.sizeInBytes) / sizeof(uint32_t),
				.VertexCount = static_cast<uint32_t>(geometry.vertices.sizeInBytes) / sizeof(Vertex),
				.IndexBuffer = geometry.indices.GetGPUAddress(),
				.VertexBuffer {
					.StartAddress = geometry.vertices.GetGPUAddress(),
					.StrideInBytes = sizeof(Vertex)
				}
			}
		};
		BLAS.geometryDescriptions.push_back(geometryDesc);
		BLAS.geometryInstances.push_back(geometry);
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS BLASInputs {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
		.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
		.NumDescs = static_cast<uint32_t>(BLAS.geometryDescriptions.size()),
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.pGeometryDescs = BLAS.geometryDescriptions.data()
	};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BLASBuildDesc {
		.DestAccelerationStructureData = BLAS.ASBuffer.GetGPUAddress(),
		.Inputs = BLASInputs,
		.SourceAccelerationStructureData = BLAS.ASBuffer.GetGPUAddress(),
		.ScratchAccelerationStructureData = scratchBuffer.GetGPUAddress()
	};

	PIXBeginEvent(context.graphicsCommands.Get(), PIX_COLOR_DEFAULT, L"Rebuild BLAS");
	context.graphicsCommands->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());
	context.graphicsCommands->BuildRaytracingAccelerationStructure(&BLASBuildDesc, 0, nullptr);

	// Since a single scratch resource is used to build all BLAS,
	// it is necessary to insert barriers between builds. This could
	// be optimized away by allocating multiple scratch buffers to allow
	// the GPU to schedule multiple builds simultaneously.
	D3D12_RESOURCE_BARRIER barrier {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
		.UAV {
			.pResource = BLAS.ASBuffer.GetResource()
		}
	};
	context.graphicsCommands->ResourceBarrier(1, &barrier);
	PIXEndEvent(context.graphicsCommands.Get());
}

BLASInstanceHandle AccelerationStructureManager::AddBLASInstance(BLASHandle handle, DirectX::XMMATRIX transform /*= MATRIX_IDENTITY*/)
{
	auto& BLAS = BLAccelerationStructures[handle];

	auto instanceHandle = BLInstanceDescriptorsCPU.Insert(D3D12_RAYTRACING_INSTANCE_DESC {
			.InstanceMask = 0xFF,
			.InstanceContributionToHitGroupIndex = BLAS.instanceContributionToHitGroupIndex,
			.AccelerationStructure = BLAS.ASBuffer.GetGPUAddress()
		});

	DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4*>(BLInstanceDescriptorsCPU[instanceHandle].Transform), transform);
	return instanceHandle;
}

void AccelerationStructureManager::BuildTLAS(DXDescriptorHeap* descriptorHeap)
{
	UNTITLED_ASSERT(requiredScratchSize < MAX_SCRATCHBUFFER_SIZE && "Required scratch buffer size exceeds fixed size scratch buffer limit!");

	// Copy the bottom level instance descriptors to the GPU
	BLInstanceDescriptorsGPU.SetData(BLInstanceDescriptorsCPU.data(),
		BLInstanceDescriptorsCPU.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

	// Build the top level acceleration structure
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS TLASInputs {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
		.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
		.NumDescs = static_cast<uint32_t>(BLInstanceDescriptorsCPU.size()),
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.InstanceDescs = BLInstanceDescriptorsGPU.GetGPUAddress()
	};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC TLASBuildDesc {
		.DestAccelerationStructureData = TLAccelerationStructure.ASBuffer.GetGPUAddress(),
		.Inputs = TLASInputs,
		.ScratchAccelerationStructureData = scratchBuffer.GetGPUAddress()
	};

	PIXBeginEvent(context.graphicsCommands.Get(), PIX_COLOR_DEFAULT, L"Build TLAS");
	context.graphicsCommands->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());
	context.graphicsCommands->BuildRaytracingAccelerationStructure(&TLASBuildDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER barrier {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
		.UAV {
			.pResource = TLAccelerationStructure.ASBuffer.GetResource()
		}
	};
	context.graphicsCommands->ResourceBarrier(1, &barrier);
	PIXEndEvent(context.graphicsCommands.Get());
}